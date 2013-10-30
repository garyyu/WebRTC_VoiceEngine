//#ifdef _DEBUG

#include <Windows.h>
#include <iostream>
#include <math.h>
#include <assert.h>
#include <conio.h>
#include <string.h>
#include "webrtc_voe.h"

#include "wasapi.h"

using namespace std;

FILE *rfile = NULL;
FILE *wfile = NULL;

void PrintOptions()
{
    cout << "WebRTC AEC Demo" <<endl;
    cout << "1. Realtime Delay  Test"<<endl;
    cout << "2. Realtime Dialog Test"<<endl;
    cout << "3. Local File Test"<<endl;
    cout << "0. End"<<endl;
    cout << "    Please select your test item:";
}

#define Float2Int16(flSample) (int16_t)floor(32768*flSample+0.5f)
#define IntToFloat(iSample)   (float)(iSample/32768.0f)


void RealTimeDialogTest();
void RealTimeDelayTest();
void LocalFileTest();

int main(int argc, char **argv)
{

    PrintOptions();
    int i = -1;
    cin >> i;
    while (i != 0)
    {
        switch(i)
        {
        case 1: RealTimeDelayTest();break;
		case 2: RealTimeDialogTest();break;
        case 3: LocalFileTest();break;
        default:break;
        }
        
        PrintOptions();
        cin >> i;
    }

    return 0;
}

void LocalFileTest()
{   
	unsigned i = 0;
	size_t	 read_size = 0;
	unsigned total_size= 0;

	webrtc_ec * echo = NULL;
	unsigned	clock_rate = 16000;			//16kHz
	unsigned	samples_per_frame = 16*10;	//16kHz*10ms
	unsigned	system_delay = 0;

	int16_t	  * file_buffer = NULL;
	int16_t   * farend_buffer  = NULL;
	int16_t   * nearend_buffer = NULL;

    char strFileIn [128] = "stero_in.pcm";	//Left  Channel: Farend Signal, Right Channel: Nearend Input Signal (with echo)
    char strFileOut[128] = "stero_out.pcm";	//Right Channel: same as input, Right Channel: Nearend Output after AEC.

    cout << "    Please give input file name:";
	cin >> strFileIn;

    fopen_s(&rfile, strFileIn, "rb");
	if (rfile == NULL){
		printf("file %s open fail.\n", strFileIn);
		return;
	}
    fopen_s(&wfile, strFileOut,"wb");
	assert(wfile!=NULL);

    cout << "    Sound Card Clock Rate (kHz)?:";
	cin >> clock_rate;
	if ((clock_rate!=8) && (clock_rate!=16) && (clock_rate!=32) && (clock_rate!=48))
	{
		printf("Not Supported for your %d kHz.\n", clock_rate);
		fclose(rfile); rfile=NULL;
		fclose(wfile); wfile=NULL;
		return;
	}
	samples_per_frame = clock_rate*10;	//10ms sample numbers
	clock_rate *= 1000;	// kHz -> Hz

    cout << "    System Delay (sound card buffer & application playout buffer) (ms)?:";
	cin >> system_delay;
	if (system_delay>320){	//To Be check
		printf("Not Supported for your system delay %d (ms).\n", system_delay);	
		fclose(rfile); rfile=NULL;
		fclose(wfile); wfile=NULL;
		return;
	}

	if (0 != webrtc_aec_create(
                        clock_rate,
                        1,		// channel_count
                        samples_per_frame,	// clock_rate(kHz)*10(ms)
                        system_delay,		// system_delay (ms)
                        0,		// options,
                        (void**)&echo ) )
	{
		printf("%s:%d-Error on webrtc_aec_create()!\n", __FILE__, __LINE__);
		fclose(rfile);
		fclose(wfile);
		return;
	}

	file_buffer = (int16_t *)malloc( samples_per_frame * 2 * 2 );	//2 Bytes/Sample, 2 Channels: Left for Farend, Right for Nearend.
	assert( file_buffer != NULL );
	farend_buffer  = (int16_t *)malloc( samples_per_frame * 2 );	//2 Bytes/Sample
	nearend_buffer = (int16_t *)malloc( samples_per_frame * 2 );	//2 Bytes/Sample
	assert( farend_buffer != NULL );
	assert( nearend_buffer!= NULL );

	while(1)
	{
		read_size = fread(file_buffer, sizeof(int16_t), 2*samples_per_frame, rfile);
		total_size += read_size;
		if (read_size != (2*samples_per_frame)){
			printf("File End. Total %d Bytes.\n", total_size<<1);
			break;
		}

		// Split into Farend and Nearend Signals
		for (i=0; i<samples_per_frame; i++){
			farend_buffer [i] = file_buffer[(i<<1)+0];
			nearend_buffer[i] = file_buffer[(i<<1)+1];
		}

		// Echo Processing
		if (0 != webrtc_aec_cancel_echo( echo,
						   nearend_buffer,	// rec_frm
						   farend_buffer,	// play_frm
						   0,	// options
						   NULL	// reserved 
						   ) )
		{
			printf("%s:%d-Error on webrtc_aec_cancel_echo()!\n", __FILE__, __LINE__);
			break;
		}

		// Merge Again to Stero, Left: Farend, Right: Nearend.
		for (i=0; i<samples_per_frame; i++){
			file_buffer[(i<<1)+1] = nearend_buffer[i];
		}

		// Save to Output File
		fwrite(file_buffer, sizeof(int16_t), 2*samples_per_frame, wfile);
	}

	webrtc_aec_destroy( echo );

	fclose(rfile); rfile=NULL;
	fclose(wfile); wfile=NULL;

	free(file_buffer);
	free(farend_buffer);
	free(nearend_buffer);
	return;
}

void RealTimeDialogTest()
{
	unsigned elapse_time = 0;
	unsigned i = 0;
	size_t	 read_size = 0;
	unsigned total_size= 0;

	webrtc_ec * echo = NULL;
	void * codec2sndcard_Resampler = NULL;
	void * sndcard2codec_Resampler = NULL;

	unsigned	sndcard_clock_rate;
	unsigned	sndcard_samples_per_frame;

	unsigned	clock_rate = 16000;			//16kHz
	unsigned	samples_per_frame = 16*10;	//16kHz*10ms
	unsigned	system_delay = 0;

	int16_t	  * infile_buffer  = NULL;
	int16_t	  * outfile_buffer = NULL;
	int16_t   * farend_buffer  = NULL;
	int16_t   * nearend_buffer = NULL;

	int16_t   * resampler_buffer = NULL;

	float * pCaptureData = NULL;
	float * pRenderData  = NULL;
	bool	result;
	bool	bAutoTest = false;

    char strFilePlay  [128] = "playout.pcm";	//Mono, Which will be used as the playout sound.
    char strFileRecord[128] = "recorded.pcm";	//Stero. Left Channel: same contents as Play file, Right Channel: Recorded Sound after AEC.

	InitAudioCaptureRender(sndcard_clock_rate);
	printf("Sound Card IAudioClient Clock Rate = %d Hz\n", sndcard_clock_rate);
	sndcard_samples_per_frame = sndcard_clock_rate/100;	//10ms sample numbers

	char autotest[128];
    cout << "    Do you want Auto Test? (Y/N):";
	cin >> autotest;
	if ((autotest[0]=='Y') || (autotest[0]=='y'))
	{
		printf("OK. Automatic Test Running now...\n");
		strcpy_s( strFilePlay, "001A_16k.pcm" );
		clock_rate = 16;
		system_delay = 80;
		bAutoTest = true;
	}

	if (bAutoTest==false){
		cout << "    Please give input file name:";
		cin >> strFilePlay;
	}

    fopen_s(&rfile, strFilePlay, "rb");
	if (rfile == NULL){
		printf("file %s open fail.\n", strFilePlay);
		CloseAudio();
		return;
	}

	if (bAutoTest==false){
		cout << "    Input File Clock Sample Rate (kHz)?:";
		cin >> clock_rate;
	}
	if ((clock_rate!=8) && (clock_rate!=16))
	{
		printf("Not Supported for your %d kHz. This AEC library only support 8kHz/16kHz\n", clock_rate);
		fclose(rfile); rfile=NULL;
		fclose(wfile); wfile=NULL;
		CloseAudio();
		return;
	}
	clock_rate *= 1000;
	samples_per_frame = clock_rate/100;	//10ms sample numbers

    fopen_s(&wfile, strFileRecord,"wb");
	assert(wfile!=NULL);

	if (bAutoTest==false){
		cout << "    System Delay (sound card buffer & application playout buffer) (ms)?:";
		cin >> system_delay;
	}
	if (system_delay>320){	//To Be check
		printf("Not Supported for your system delay %d (ms).\n", system_delay);	
		fclose(rfile); rfile=NULL;
		fclose(wfile); wfile=NULL;
		CloseAudio();
		return;
	}

	printf("Two Resamplers are created between %d(Hz) and %d(Hz).\n", clock_rate, sndcard_clock_rate);
	if ( sndcard_clock_rate==44100 )
	{
		//WebRTC Resampler only support 44k!
		webrtc_resampler_create( clock_rate, 44000,		 &codec2sndcard_Resampler );
		webrtc_resampler_create( 44000,      clock_rate, &sndcard2codec_Resampler );
	}
	else
	{
		webrtc_resampler_create( clock_rate,		 sndcard_clock_rate, &codec2sndcard_Resampler );
		webrtc_resampler_create( sndcard_clock_rate, clock_rate,		 &sndcard2codec_Resampler );
	}

	resampler_buffer = (int16_t *)malloc( sndcard_samples_per_frame * sizeof(int16_t) );
	assert(resampler_buffer != NULL);

	if (0 != webrtc_aec_create(
                        clock_rate,
                        1,		// channel_count
                        samples_per_frame,	// clock_rate(kHz)*10(ms)
                        system_delay,		// system_delay (ms)
                        0,		// options,
                        (void**)&echo ) )
	{
		printf("%s:%d-Error on webrtc_aec_create()!\n", __FILE__, __LINE__);
		fclose(rfile);
		fclose(wfile);
		CloseAudio();
		webrtc_resampler_destroy(codec2sndcard_Resampler);
		webrtc_resampler_destroy(sndcard2codec_Resampler);
		return;
	}

	infile_buffer     = (int16_t *)malloc( samples_per_frame * sizeof(int16_t) );	//Mono.
	assert( infile_buffer != NULL );
	outfile_buffer    = (int16_t *)malloc( samples_per_frame * sizeof(int16_t) *2 );//Stero. Put input to the Left Channel, and put Captured voice to the Right Channel.
	assert( outfile_buffer != NULL );
	farend_buffer  = (int16_t *)malloc( samples_per_frame * sizeof(int16_t) );
	nearend_buffer = (int16_t *)malloc( samples_per_frame * sizeof(int16_t) );
	assert( farend_buffer != NULL );
	assert( nearend_buffer!= NULL );

	pCaptureData = (float*)malloc(m_pCaptureBuffer->m_iFrameSize_10ms * sizeof(float) * 2);
	assert(pCaptureData!=NULL);
	pRenderData  = (float*)malloc(m_pRenderBuffer ->m_iFrameSize_10ms * sizeof(float) * 2);
	assert(pRenderData!=NULL);

	StartAudio();
	while(1)
	{
		Sleep(10);

		// Capture One Frame
		do{
			result = m_pCaptureBuffer->GetData(pCaptureData);
			if (result)
			{
				/*---- Each time when Captured one Frame, we need Playout one Frame to sync it. ----*/

				// Simulate Receiving one Packet and Playout 
				{
					read_size = fread(farend_buffer, sizeof(int16_t), samples_per_frame, rfile);
					total_size += read_size;
					if (read_size != samples_per_frame){
						goto Exit;
					}

					// Resample from Codec to Soundcard
					int outLen = 0;
					webrtc_resampler_process(codec2sndcard_Resampler,
                        farend_buffer, 
						samples_per_frame, 
						resampler_buffer,
						sndcard_samples_per_frame, outLen 
						 );
					//assert(outLen == sndcard_samples_per_frame);
					if ( sndcard_clock_rate==44100 ){
						// Special for 44.1kHz. Here temporary copy the last sample.
						resampler_buffer[outLen] = resampler_buffer[outLen-1];
					}

					// Playout One Frame
					{
						//Convert Mono Sound into Stero.
						for (i=0; i<sndcard_samples_per_frame; i++){
							pRenderData[(i<<1)+0] = IntToFloat( resampler_buffer[i] );
							pRenderData[(i<<1)+1] = IntToFloat( resampler_buffer[i] );
						}
						m_pRenderBuffer->PutData( pRenderData, sndcard_samples_per_frame );
					}
				}

				// Convert SoundCard Captured Floating Format to INT16 Format
				for (i=0; i<sndcard_samples_per_frame; i++){
					resampler_buffer[i] = Float2Int16( pCaptureData[(i<<1)] );	// Use Left Channel
				}

				// Resample from Soundcard to Codec (8kHz/16kHz)
				int outLen = 0;
				if ( sndcard_clock_rate==44100 ){
					//WebRTC Resampler only support 44k!
				}
				webrtc_resampler_process(sndcard2codec_Resampler,
							resampler_buffer, 
							(sndcard_clock_rate==44100? 440:sndcard_samples_per_frame),	//WebRTC Resampler only support 44k instead of 44.1k
							nearend_buffer,
							samples_per_frame, outLen 
						);
				//assert(outLen == samples_per_frame);

				// Echo Processing
				if (0 != webrtc_aec_cancel_echo( echo,
								   nearend_buffer,	// rec_frm
								   farend_buffer,	// play_frm
								   0,	// options
								   NULL	// reserved 
								   ) )
				{
					printf("%s:%d-Error on webrtc_aec_cancel_echo()!\n", __FILE__, __LINE__);
					goto Exit;
				}

				// Save Playout & Captured Data in Same File for Checking the Delay
				for (i=0; i<samples_per_frame; i++){
					outfile_buffer[(i<<1)+0] = farend_buffer [i];
					outfile_buffer[(i<<1)+1] = nearend_buffer[i];
				}
				fwrite(outfile_buffer, sizeof(int16_t), 2*samples_per_frame, wfile);
			}

		} while (result==true); //Loop to make sure all captured data has been read.

		elapse_time += 10;
		
		printf("\rCapture(R/W=%d/%d). Render(R/W=%d/%d). Captured Voice %d(ms)", 
				m_pCaptureBuffer->GetReadIndex(), m_pCaptureBuffer->GetWriteIndex(),
				m_pRenderBuffer ->GetReadIndex(), m_pRenderBuffer ->GetWriteIndex(),
				elapse_time);

		if (_kbhit()){	//Quit on Any Key Press
			break;
		}
	}

Exit:
	CloseAudio();
	webrtc_resampler_destroy(codec2sndcard_Resampler);
	webrtc_resampler_destroy(sndcard2codec_Resampler);

	printf("\n");
	printf("Lost Frame Counter for Capturing: %d\n", m_pCaptureBuffer->GetLostFrmCount());
	printf("Lost Frame Counter for Rendering: %d\n", m_pRenderBuffer ->GetLostFrmCount());

	webrtc_aec_destroy( echo );

	if (rfile) { fclose(rfile); rfile=NULL; }
	if (wfile) { fclose(wfile); wfile=NULL; }

	if (infile_buffer ) free( infile_buffer	);
	if (outfile_buffer) free( outfile_buffer);
	if (farend_buffer ) free( farend_buffer	);
	if (nearend_buffer) free( nearend_buffer);
	if (pCaptureData  ) free( pCaptureData	);
	if (pRenderData   ) free( pRenderData	);

	return;
}


void RealTimeDelayTest()
{
	unsigned elapse_time = 0;
	unsigned i = 0;
	size_t	 read_size = 0;
	unsigned total_size= 0;

	webrtc_ec * echo = NULL;
	unsigned	clock_rate ;
	unsigned	samples_per_frame;
	unsigned	system_delay = 0;

	int16_t	  * file_buffer = NULL;
	int16_t   * farend_buffer  = NULL;
	int16_t   * nearend_buffer = NULL;

	float * pCaptureData = NULL;
	float * pRenderData  = NULL;
	bool	result;

    char strFilePlay  [128] = "delaytest.pcm";	//Mono, Which will be used as the playout sound.
    char strFileRecord[128] = "recorded.pcm";	//Stero. Left Channel: same contents as Play file, Right Channel: Recorded Sound

	InitAudioCaptureRender(clock_rate);
	printf("Sound Card IAudioClient Clock Rate = %d Hz\n", clock_rate);
	samples_per_frame = clock_rate/100;	//10ms sample numbers

	sprintf_s(strFilePlay, "delaytest%d.pcm", clock_rate);
    fopen_s(&rfile, strFilePlay, "rb");
	if (rfile == NULL){
		printf("file %s open fail.\n", strFilePlay);
		CloseAudio();
		return;
	}

    fopen_s(&wfile, strFileRecord,"wb");
	assert(wfile!=NULL);

	file_buffer    = (int16_t *)malloc( samples_per_frame * sizeof(int16_t)  * 2);//Stero.
	assert( file_buffer != NULL );
	farend_buffer  = (int16_t *)malloc( samples_per_frame * sizeof(int16_t) );
	nearend_buffer = (int16_t *)malloc( samples_per_frame * sizeof(int16_t) );
	assert( farend_buffer != NULL );
	assert( nearend_buffer!= NULL );

	pCaptureData = (float*)malloc(m_pCaptureBuffer->m_iFrameSize_10ms * sizeof(float) * 2);
	assert(pCaptureData!=NULL);
	pRenderData  = (float*)malloc(m_pRenderBuffer ->m_iFrameSize_10ms * sizeof(float) * 2);
	assert(pRenderData!=NULL);

	StartAudio();
	while(1)
	{
		Sleep(10);

		// Capture One Frame
		do{
			result = m_pCaptureBuffer->GetData(pCaptureData);
			if (result)
			{
				/*---- Each time when Captured one Frame, we need Playout one Frame to sync it. ----*/

				// Simulate Receiving one Packet and Playout 
				{
					read_size = fread(farend_buffer, sizeof(int16_t), samples_per_frame, rfile);
					total_size += read_size;
					if (read_size != samples_per_frame){
						goto Exit;
					}

					// Playout One Frame
					{
						//Convert Mono Sound into Stero.
						for (i=0; i<samples_per_frame; i++){
							pRenderData[(i<<1)+0] = IntToFloat( farend_buffer[i] );
							pRenderData[(i<<1)+1] = IntToFloat( farend_buffer[i] );
						}
						m_pRenderBuffer->PutData( pRenderData, samples_per_frame );
					}
				}

				// Save Playout & Captured Data in Same File for Checking the Delay
				for (i=0; i<samples_per_frame; i++){
					file_buffer[(i<<1)+0] = farend_buffer[i];
					file_buffer[(i<<1)+1] = Float2Int16( pCaptureData[(i<<1)+1] );
				}
				fwrite(file_buffer, sizeof(int16_t), 2*samples_per_frame, wfile);
			}

		} while (result==true); //Loop to make sure all captured data has been read.

		elapse_time += 10;
		
		printf("\rCapture(R/W=%d/%d). Render(R/W=%d/%d). Captured Voice %d(ms)", 
				m_pCaptureBuffer->GetReadIndex(), m_pCaptureBuffer->GetWriteIndex(),
				m_pRenderBuffer ->GetReadIndex(), m_pRenderBuffer ->GetWriteIndex(),
				elapse_time);

		if (_kbhit()){	//Quit on Any Key Press
			break;
		}
	}

Exit:
	CloseAudio();

	printf("Lost Frame Counter for Capturing: %d\n", m_pCaptureBuffer->GetLostFrmCount());
	printf("Lost Frame Counter for Rendering: %d\n", m_pRenderBuffer ->GetLostFrmCount());

	if (rfile) { fclose(rfile); rfile=NULL; }
	if (wfile) { fclose(wfile); wfile=NULL; }

	if (file_buffer   ) free(file_buffer);
	if (farend_buffer ) free(farend_buffer);
	if (nearend_buffer) free(nearend_buffer);
	if (pCaptureData  ) free(pCaptureData);
	if (pRenderData   ) free(pRenderData );

	return;
}

//#endif