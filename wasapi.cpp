
#include <Windows.h>
#include <stdio.h>
#include <assert.h>

#include <wtypes.h>
#include <winerror.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <Endpointvolume.h>
#include <mmreg.h>

#include "wasapi.h"

#define EXIT_ON_ERROR(res)   { if(res<0) { printf("wasapi.cpp:%d EXIT_ON_ERROR()\n", __LINE__); goto Exit; } }

#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#define SAFE_FREE(p)		 { if(p) { free (p);       (p)=NULL; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }


#define RFTIMES_PER_MILLISEC	10000	//// REFERENCE_TIME (100 ns) Time Units per Millisecond. 

static IAudioClient			* m_pAudioClient_Capture   = NULL;
static IAudioClient			* m_pAudioClient_Render    = NULL;
static IAudioCaptureClient	* m_pCaptureClient = NULL;
static IAudioRenderClient   * m_pRenderClient  = NULL;
static IMMDevice			* m_pCaptureEndpointDevice= NULL;
static IMMDevice			* m_pRenderEndpointDevice = NULL;

static bool	m_fAudioCaptureStarted = false;
static bool	m_fAudioRenderStarted  = false;

		HANDLE m_hAudioCaptureEvent	;
		HANDLE m_hAudioRenderEvent	;
		HANDLE m_hStopCaptureThreadEvent;
		HANDLE m_hStopRenderThreadEvent	;
static	HANDLE m_hCaptureThreadStopedEvent	;
static	HANDLE m_hRenderThreadStopedEvent	;

static	UINT32 m_CaptureBufferFrameCount = 0;
static	UINT32 m_RenderBufferFrameCount  = 0;

IAudioCircleBuffer * m_pCaptureBuffer = NULL;
IAudioCircleBuffer * m_pRenderBuffer  = NULL;


/**********************************************************************************************
 *					Circle Buffer for Cature and Render                                       *
 **********************************************************************************************/

#define MAX_LEN_CIRCLE	8
#define GRID_SIZE		(2*48*20)	//Suppose maximum 48kHz @ Stero @ 20ms
#define CIRCLE_BUFF_SIZE	(GRID_SIZE*MAX_LEN_CIRCLE)


IAudioCircleBuffer::IAudioCircleBuffer()
{
	iReadPos = 0;
	iWritePos = 0;
	iLostFrmCount = 0;

	m_pIAudioCircleBuffer = NULL;
	m_pIAudioCircleBuffer = (float*)malloc(CIRCLE_BUFF_SIZE * sizeof(float));
	assert(m_pIAudioCircleBuffer!=NULL);
	memset(m_pIAudioCircleBuffer, 0, CIRCLE_BUFF_SIZE * sizeof(float));
}

IAudioCircleBuffer::~IAudioCircleBuffer()
{
	if (m_pIAudioCircleBuffer){
		free(m_pIAudioCircleBuffer);
		m_pIAudioCircleBuffer = NULL;
	}
}


unsigned IAudioCircleBuffer::GetLostFrmCount(void)
{
	return iLostFrmCount;
}

unsigned IAudioCircleBuffer::GetReadIndex(void)
{
	return iReadPos/GRID_SIZE;
}

unsigned IAudioCircleBuffer::GetWriteIndex(void)
{
	return iWritePos/GRID_SIZE;
}

bool IAudioCircleBuffer::IsDataAvailable()
{
	if (iReadPos != iWritePos)
		return true;
	else
		return false;
}

bool IAudioCircleBuffer::GetData(void *pReadTo)
{
	float * pBuffer = NULL;
	if (iReadPos != iWritePos){
		pBuffer = &m_pIAudioCircleBuffer[iReadPos];
		memcpy( pReadTo, pBuffer, m_iFrameSize_10ms*sizeof(float)*2 );	// Stero
		iReadPos += GRID_SIZE;
		if (iReadPos >= CIRCLE_BUFF_SIZE){
			iReadPos = 0;
		}
		return true;
	}
	else
		return false;
}

void IAudioCircleBuffer::PutData(void *pData, UINT32 iNumFramesToRead)
{
	float * pBuffer = NULL;
	unsigned iNextWritePos = 0;
	iNextWritePos = iWritePos + GRID_SIZE;
	if (iNextWritePos >= CIRCLE_BUFF_SIZE){
		iNextWritePos = 0;
	}

	if (iNextWritePos == iReadPos){
		//Buffer full, Throw this frame and count it.
		iLostFrmCount++;
		//printf("\nIAudioCircleBuffer::PutData() R/W=%d/%d. iLostFrmCount=%d\n", GetReadIndex(), GetWriteIndex(), iLostFrmCount);
	}
	else{
		pBuffer = &m_pIAudioCircleBuffer[iWritePos];
		assert(iNumFramesToRead <= GRID_SIZE);

		memcpy( pBuffer, pData, iNumFramesToRead*sizeof(float)*2 );	// Stero
		iWritePos = iNextWritePos;
	}

	return;
}

/**********************************************************************************************
 *					IAudioClient Interface                                                    *
 **********************************************************************************************/

#define DisplayWasapiError(C,D)	_DisplayWasapiError(__FILE__, __LINE__, C, D)
static void _DisplayWasapiError(const char* filename, int linenum, HRESULT res, char * method)
{ 	
    char *text = 0;
    switch(res){
        case S_OK: return;
        case E_POINTER                              :text ="E_POINTER"; break;
        case E_INVALIDARG                           :text ="E_INVALIDARG"; break;
        case AUDCLNT_E_NOT_INITIALIZED              :text ="AUDCLNT_E_NOT_INITIALIZED"; break;
        case AUDCLNT_E_ALREADY_INITIALIZED          :text ="AUDCLNT_E_ALREADY_INITIALIZED"; break;
        case AUDCLNT_E_WRONG_ENDPOINT_TYPE          :text ="AUDCLNT_E_WRONG_ENDPOINT_TYPE"; break;
        case AUDCLNT_E_DEVICE_INVALIDATED           :text ="AUDCLNT_E_DEVICE_INVALIDATED"; break;
        case AUDCLNT_E_NOT_STOPPED                  :text ="AUDCLNT_E_NOT_STOPPED"; break;
        case AUDCLNT_E_BUFFER_TOO_LARGE             :text ="AUDCLNT_E_BUFFER_TOO_LARGE"; break;
        case AUDCLNT_E_OUT_OF_ORDER                 :text ="AUDCLNT_E_OUT_OF_ORDER"; break;
        case AUDCLNT_E_UNSUPPORTED_FORMAT           :text ="AUDCLNT_E_UNSUPPORTED_FORMAT"; break;
        case AUDCLNT_E_INVALID_SIZE                 :text ="AUDCLNT_E_INVALID_SIZE"; break;
        case AUDCLNT_E_DEVICE_IN_USE                :text ="AUDCLNT_E_DEVICE_IN_USE"; break;
        case AUDCLNT_E_BUFFER_OPERATION_PENDING     :text ="AUDCLNT_E_BUFFER_OPERATION_PENDING"; break;
        case AUDCLNT_E_THREAD_NOT_REGISTERED        :text ="AUDCLNT_E_THREAD_NOT_REGISTERED"; break;
        case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED   :text ="AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED"; break;
        case AUDCLNT_E_ENDPOINT_CREATE_FAILED       :text ="AUDCLNT_E_ENDPOINT_CREATE_FAILED"; break;
		case AUDCLNT_E_SERVICE_NOT_RUNNING			:text ="AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
		case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED     :text ="AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED"; break;
		case AUDCLNT_E_EXCLUSIVE_MODE_ONLY          :text ="AUDCLNT_E_EXCLUSIVE_MODE_ONLY"; break;
		case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL :text ="AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL"; break;
		case AUDCLNT_E_EVENTHANDLE_NOT_SET          :text ="AUDCLNT_E_EVENTHANDLE_NOT_SET"; break;
		case AUDCLNT_E_INCORRECT_BUFFER_SIZE        :text ="AUDCLNT_E_INCORRECT_BUFFER_SIZE"; break;
		case AUDCLNT_E_BUFFER_SIZE_ERROR            :text ="AUDCLNT_E_BUFFER_SIZE_ERROR"; break;
		case AUDCLNT_E_CPUUSAGE_EXCEEDED            :text ="AUDCLNT_E_CPUUSAGE_EXCEEDED"; break;
		case AUDCLNT_S_BUFFER_EMPTY					:text ="AUDCLNT_S_BUFFER_EMPTY"; break;
		case AUDCLNT_S_THREAD_ALREADY_REGISTERED	:text ="AUDCLNT_S_THREAD_ALREADY_REGISTERED"; break;
		case AUDCLNT_S_POSITION_STALLED				:text ="AUDCLNT_S_POSITION_STALLED"; break;
		default: 
			text =" Unkown Error!";
        break;

    }
	printf("%s:%d-ERROR in %s / WASAPI ERROR HRESULT: %d : %s\n", filename, linenum, method, res, text);
}

static int CreateThreadNotification ();


const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

static int InitEndpointDevice(void){
    HRESULT		hr = S_OK;
	UINT		count = 0;
	bool		isFound = false;

    //IMMDeviceCollection *	pCollection = NULL;
    //IPropertyStore *		pProps = NULL;
    IMMDeviceEnumerator *	pEnumerator = NULL;
	
	//printf("InitEndpointDevice() func enter.\n" );

	//Create the list of audio device
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	EXIT_ON_ERROR(hr);

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
	EXIT_ON_ERROR(hr);

	hr = pEnumerator->GetDefaultAudioEndpoint(eCapture,	eCommunications, &m_pCaptureEndpointDevice);
	EXIT_ON_ERROR(hr);

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender,	eCommunications, &m_pRenderEndpointDevice);
	EXIT_ON_ERROR(hr);

Exit :
	if (FAILED(hr))	{
		DisplayWasapiError(hr, "InitEndpointDevice");
	}
	//SAFE_RELEASE(pProps);
	SAFE_RELEASE(pEnumerator);
    //SAFE_RELEASE(pCollection);
	return SUCCEEDED(hr);
}


static int InitAudioService(unsigned &nSampleRate)
{
	HRESULT hr = S_OK;
	WAVEFORMATEX *pwfx = NULL;

	if (!InitEndpointDevice())
		return 0;

	/**********************************************************************************************
	 *					Audio Capture Part                                                        *
	 **********************************************************************************************/

    hr = m_pCaptureEndpointDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&m_pAudioClient_Capture);
    EXIT_ON_ERROR(hr);
	if (!m_pAudioClient_Capture ){
		printf("%s:%d-IAudioClient Activate Failure!\n", __FILE__, __LINE__);
		return 0;
	}

    hr = m_pAudioClient_Capture->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr);

	{
		printf("\nCapture Device MixFormat:\n");
		printf("MixFormat: wFormatTag=%d\n"		, pwfx->wFormatTag);
		printf("MixFormat: nChannels=%d\n"		, pwfx->nChannels);
		printf("MixFormat: nSamplesPerSec=%d\n"	, pwfx->nSamplesPerSec);
		printf("MixFormat: nAvgBytesPerSec=%d\n", pwfx->nAvgBytesPerSec);
		printf("MixFormat: nBlockAlign=%d\n"	, pwfx->nBlockAlign);
		printf("MixFormat: wBitsPerSample=%d\n"	, pwfx->wBitsPerSample);
		printf("MixFormat: cbSize=%d\n"			, pwfx->cbSize);
		assert(pwfx->nChannels==2);
	}
	m_pCaptureBuffer->m_iFrameSize_10ms = pwfx->nSamplesPerSec / 100;

	nSampleRate = pwfx->nSamplesPerSec;

    // Create a stream with the our format
    hr = m_pAudioClient_Capture->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                20 * RFTIMES_PER_MILLISEC,		// buffer duration: 10ms
                                20 * RFTIMES_PER_MILLISEC,		// periodicity
		                        pwfx,							// wave format
                                NULL);							// session GUID //FD_WIZARD
	EXIT_ON_ERROR(hr);

    // Get the actual size of the allocated buffer.
    hr = m_pAudioClient_Capture->GetBufferSize(&m_CaptureBufferFrameCount);
    EXIT_ON_ERROR(hr);
	printf("InitAudioService(): actual allocated capture buffer size: %d\n", m_CaptureBufferFrameCount);

	hr = m_pAudioClient_Capture->SetEventHandle( m_hAudioCaptureEvent );
	EXIT_ON_ERROR(hr);

	// Get the capture client
	hr = m_pAudioClient_Capture->GetService(__uuidof (IAudioCaptureClient), (void**)&m_pCaptureClient );
    EXIT_ON_ERROR(hr);

	/**********************************************************************************************
	 *					Audio Render Part                                                         *
	 **********************************************************************************************/

    hr = m_pRenderEndpointDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&m_pAudioClient_Render);
    EXIT_ON_ERROR(hr);
	if (!m_pAudioClient_Render ){
		printf("%s:%d-IAudioClient Activate Failure!\n", __FILE__, __LINE__);
		return 0;
	}

    hr = m_pAudioClient_Render->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr);

	{
		printf("\nRender Device MixFormat:\n");
		printf("MixFormat: wFormatTag=%d\n"		, pwfx->wFormatTag);
		printf("MixFormat: nChannels=%d\n"		, pwfx->nChannels);
		printf("MixFormat: nSamplesPerSec=%d\n"	, pwfx->nSamplesPerSec);
		printf("MixFormat: nAvgBytesPerSec=%d\n", pwfx->nAvgBytesPerSec);
		printf("MixFormat: nBlockAlign=%d\n"	, pwfx->nBlockAlign);
		printf("MixFormat: wBitsPerSample=%d\n"	, pwfx->wBitsPerSample);
		printf("MixFormat: cbSize=%d\n"			, pwfx->cbSize);
		assert(pwfx->nChannels==2);
	}
	m_pRenderBuffer->m_iFrameSize_10ms = pwfx->nSamplesPerSec / 100;

    // Create a stream
    hr = m_pAudioClient_Render->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                20 * RFTIMES_PER_MILLISEC,		// buffer duration: 10ms
                                20 * RFTIMES_PER_MILLISEC,		// periodicity
		                        pwfx,							// wave format
                                NULL);							// session GUID //FD_WIZARD
	EXIT_ON_ERROR(hr);

    // Get the actual size of the allocated buffer.
    hr = m_pAudioClient_Render->GetBufferSize(&m_RenderBufferFrameCount);
    EXIT_ON_ERROR(hr);
	printf("InitAudioService(): actual allocated render buffer size: %d\n", m_RenderBufferFrameCount);

	hr = m_pAudioClient_Render->SetEventHandle( m_hAudioRenderEvent );
	EXIT_ON_ERROR(hr);

    // Get the render Client
    hr = m_pAudioClient_Render->GetService(__uuidof (IAudioRenderClient), (void**)&m_pRenderClient );
    EXIT_ON_ERROR(hr);

	/**********************************************************************************************
	 *					Start Audio Capturing & Rendering in the same time                        *
	 **********************************************************************************************/

	CreateThreadNotification();

	// Before Starting Rendering, Make sure Render Buffer is filled for the 1st time with silence.
	{
		BYTE *pData = NULL;

		// Grab all the available space in the shared buffer.
		hr = m_pRenderClient->GetBuffer(m_RenderBufferFrameCount, &pData);
		EXIT_ON_ERROR(hr)

		hr = m_pRenderClient->ReleaseBuffer(m_RenderBufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
		EXIT_ON_ERROR(hr)
	}

Exit :
	if(FAILED(hr)){
		DisplayWasapiError(hr, "InitAudioService()");
	}

	return SUCCEEDED(hr);
}

int StartAudio()
{     
	HRESULT hr = S_OK;

	// Start up the render stream.
	printf("%s:%d-start the audio rendering\n", __FILE__, __LINE__ );
	hr = m_pAudioClient_Render->Start();
	EXIT_ON_ERROR(hr);
	m_fAudioRenderStarted  = true;
      
	// Start up the capture stream.
	printf("%s:%d-start the audio capturing\n", __FILE__, __LINE__ );
	hr = m_pAudioClient_Capture->Start();
	EXIT_ON_ERROR(hr);
	m_fAudioCaptureStarted = true;

Exit :
	if(FAILED(hr)){
		DisplayWasapiError(hr, "StartAudio()");
	}

	return SUCCEEDED(hr);
}


static void ProcessCaptureStream()
{
	HRESULT hr;

    UINT32 numFramesAvailable = 0;
    UINT32 packetLength = 0;
    BYTE *pData = NULL;
    DWORD flags = 0;

    hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
    EXIT_ON_ERROR(hr)

    while (packetLength != 0)
    {
        // Get the available data in the shared buffer.
        hr = m_pCaptureClient->GetBuffer(
                                &pData,
                                &numFramesAvailable,
                                &flags, NULL, NULL);
        EXIT_ON_ERROR(hr)

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
        {
            pData = NULL;  // Tell CopyData to write silence.
			//TODO:
        }

        // Copy the available capture data
		if (pData){
			assert(numFramesAvailable == m_pCaptureBuffer->m_iFrameSize_10ms);
			m_pCaptureBuffer->PutData( pData, numFramesAvailable);
			//printf("IAudioCircleBuffer::PutData() R/W=%d/%d\n", m_pCaptureBuffer->GetReadIndex(), m_pCaptureBuffer->GetWriteIndex());
		}
 
        hr = m_pCaptureClient->ReleaseBuffer(numFramesAvailable);
        EXIT_ON_ERROR(hr)

        hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr)
    }

Exit :
	if(FAILED(hr)){
		DisplayWasapiError(hr, "ProcessCaptureStream");
	}

	return;
}

static void ProcessRenderStream()
{
	HRESULT hr;

    UINT32 numFramesAvailable;
    UINT32 numFramesPadding;
    BYTE *pData;
    float * pRenderData = NULL;
	bool result;

	if (!m_pRenderBuffer->IsDataAvailable())	// Wait data to playout but no available data! Possible reasons: Packet Lost, Jitter.
	{
		//Force a silence packet instead
		pRenderData = (float*)malloc( m_pRenderBuffer->m_iFrameSize_10ms * sizeof(float) * 2 );	 //Stero
		assert(pRenderData!=NULL);
		memset(pRenderData, 0, m_pRenderBuffer->m_iFrameSize_10ms * sizeof(float) * 2 );
		m_pRenderBuffer->PutData( pRenderData, m_pRenderBuffer->m_iFrameSize_10ms );
		free(pRenderData);
		pRenderData = NULL;
	}

	while( m_pRenderBuffer->IsDataAvailable() )
	{
		// See how much buffer space is available.
		hr = m_pAudioClient_Render->GetCurrentPadding(&numFramesPadding);
		EXIT_ON_ERROR(hr)

		numFramesAvailable = m_RenderBufferFrameCount - numFramesPadding;
		if (numFramesAvailable >= m_pRenderBuffer->m_iFrameSize_10ms)
		{
			// Grab all the available space in the shared buffer.
			hr = m_pRenderClient->GetBuffer(m_pRenderBuffer->m_iFrameSize_10ms, &pData);
			EXIT_ON_ERROR(hr)

			// Copy the available render data to the audio client buffer.
			if (pData){
				result = m_pRenderBuffer->GetData(pData);
				assert(result == true);
			}
 
			hr = m_pRenderClient->ReleaseBuffer(m_pRenderBuffer->m_iFrameSize_10ms, 0);
			EXIT_ON_ERROR(hr)
		}
		else{
			break;
		}
	} 

Exit :
	if(FAILED(hr)){
		DisplayWasapiError(hr, "ProcessRenderStream");
	}

	return;
}

DWORD WINAPI CaptureNotificationProc( LPVOID lpParam )
{
	bool bDone = false ;
	HANDLE  hTask = NULL;
	DWORD waitResult;

	while(!bDone)
	{
		//check if need exit
		if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hStopCaptureThreadEvent,0) )
		{
			printf("m_hStopCaptureThreadEvent is signaled\n") ;
			bDone = true;
			break;
		}

		//---- Capture Event ----/
		waitResult = WaitForSingleObject(m_hAudioCaptureEvent, 60);     // 60ms Timeout
		if (WAIT_OBJECT_0==waitResult)		// Signaled
		{
			ProcessCaptureStream();
		}
		else if (WAIT_TIMEOUT==waitResult)	// Timeout
		{  
			//TODO:
			if ( m_fAudioCaptureStarted ){
				printf("\nNotificationProc(): m_hAudioCaptureEvent Wait Timeout!\n");
			}
		}
		else{
			printf("Wait Failure on Audio Capturing Signal !\n");
			bDone = true;
			break;
		}
	} //end of while

	m_pAudioClient_Capture->Stop();

	printf("End of Capture Notification Thread\n") ;
	if (!SetEvent( m_hCaptureThreadStopedEvent )){
		printf("%s:%d-SetEvent() Failure! m_hCaptureThreadStopedEvent can't send.\n", __FILE__, __LINE__);
	}

	return 0;
}

DWORD WINAPI RenderNotificationProc( LPVOID lpParam )
{
	bool bDone = false ;
	HANDLE  hTask = NULL;
	DWORD waitResult;

	while(!bDone)
	{
		//check if need exit
		if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hStopRenderThreadEvent,0) )
		{
			printf("m_hStopRenderThreadEvent is signaled\n") ;
			bDone = true;
			break;
		}

		//---- Render Event ----/
		waitResult = WaitForSingleObject(m_hAudioRenderEvent, 60);     // 60ms Timeout
		if (WAIT_OBJECT_0==waitResult)		// Signaled
		{
			ProcessRenderStream();
		}
		else if (WAIT_TIMEOUT==waitResult)	// Timeout
		{  
			//TODO:
			if ( m_fAudioRenderStarted ){
				printf("\nNotificationProc(): m_hAudioRenderEvent Wait Timeout!\n");
			}
		}
		else{
			printf("Wait Failure on Audio Rendering Signal !\n");
			bDone = true;
		}
	} //end of while

	m_pAudioClient_Render ->Stop();

	printf("End of Render Notification Thread\n") ;
	if (!SetEvent( m_hRenderThreadStopedEvent )){
		printf("%s:%d-SetEvent() Failure! m_hRenderThreadStopedEvent can't send.\n", __FILE__, __LINE__);
	}

	return 0;
}

		
static int CreateThreadNotification ()
{
    HANDLE hThread;

	/*---- Rendering Thread ----*/

	hThread = CreateThread( NULL, 0, 
           RenderNotificationProc, NULL, 0, NULL);  
    assert( hThread != NULL);
 	
	if ( !SetThreadPriority( hThread , THREAD_PRIORITY_TIME_CRITICAL) ){
		printf("%s:%d-SetThreadPriority() failure.\n", __FILE__, __LINE__);
	}

	/*---- Capturing Thread ----*/

	hThread = CreateThread( NULL, 0, 
           CaptureNotificationProc, NULL, 0, NULL);  
    assert( hThread != NULL);
 	
	if ( !SetThreadPriority( hThread , THREAD_PRIORITY_TIME_CRITICAL) ){
		printf("%s:%d-SetThreadPriority() failure.\n", __FILE__, __LINE__);
	}
       
	return 1;
}


int InitAudioCaptureRender(unsigned &nSampleRate)
{
	HRESULT hr = S_OK;

	// Initialize Circle Buffer.
	m_pCaptureBuffer = new IAudioCircleBuffer;
	m_pRenderBuffer  = new IAudioCircleBuffer;
	assert(m_pCaptureBuffer);
	assert(m_pRenderBuffer);

	m_hAudioCaptureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hAudioCaptureEvent != NULL);

	m_hAudioRenderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hAudioRenderEvent != NULL);

	m_hStopCaptureThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hStopCaptureThreadEvent != NULL);

	m_hStopRenderThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hStopRenderThreadEvent != NULL);

	m_hCaptureThreadStopedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hCaptureThreadStopedEvent != NULL);

	m_hRenderThreadStopedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_hRenderThreadStopedEvent != NULL);
 
	hr = InitAudioService(nSampleRate);
    EXIT_ON_ERROR(hr);

Exit :
	if(FAILED(hr)){
		DisplayWasapiError(hr, "InitAudioCapture");
	}

	return SUCCEEDED(hr);
}

int CloseAudio()
{
	DWORD waitResult;
	if (!SetEvent(m_hStopCaptureThreadEvent)){
		printf("SetEvent() Failure! Thread Stop Event can't Send.\n");
	}
	if (!SetEvent(m_hStopRenderThreadEvent)){
		printf("SetEvent() Failure! Thread Stop Event can't Send.\n");
	}
	printf("\n");

	waitResult =WaitForSingleObject(m_hCaptureThreadStopedEvent, 1000);     // 1 second Timeout
	if (WAIT_OBJECT_0!=waitResult)		// Signaled
	{
		if (WAIT_TIMEOUT==waitResult)	// Timeout
			printf("CloseAudio(): Wait timeout for Thread exit.\n");
		else
			printf("CloseAudio(): Wait Error for Thread exit.\n");
	}

	waitResult =WaitForSingleObject(m_hRenderThreadStopedEvent, 1000);     // 1 second Timeout
	if (WAIT_OBJECT_0!=waitResult)		// Signaled
	{
		if (WAIT_TIMEOUT==waitResult)	// Timeout
			printf("CloseAudio(): Wait timeout for Thread exit.\n");
		else
			printf("CloseAudio(): Wait Error for Thread exit.\n");
	}

	SAFE_RELEASE(m_pCaptureEndpointDevice);
	SAFE_RELEASE(m_pRenderEndpointDevice );
	SAFE_RELEASE(m_pAudioClient_Capture);
	SAFE_RELEASE(m_pAudioClient_Render );
	SAFE_RELEASE(m_pCaptureClient);
	SAFE_RELEASE(m_pRenderClient );

	if ( m_hStopCaptureThreadEvent )	CloseHandle(m_hStopCaptureThreadEvent);
	if ( m_hStopRenderThreadEvent )		CloseHandle(m_hStopRenderThreadEvent);
	if ( m_hAudioCaptureEvent )			CloseHandle(m_hAudioCaptureEvent);
	if ( m_hAudioRenderEvent )			CloseHandle(m_hAudioRenderEvent);
	if ( m_hCaptureThreadStopedEvent )	CloseHandle(m_hCaptureThreadStopedEvent);
	if ( m_hRenderThreadStopedEvent )	CloseHandle(m_hRenderThreadStopedEvent);

	if (m_pCaptureBuffer) delete m_pCaptureBuffer;
	if (m_pRenderBuffer ) delete m_pRenderBuffer;

	return 0;
}
