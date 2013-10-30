
#ifndef _WASAPI_H
#define _WASAPI_H

int InitAudioCaptureRender(unsigned &nSampleRate);
int StartAudio();
int CloseAudio();

class IAudioCircleBuffer{
public:
	IAudioCircleBuffer();
	~IAudioCircleBuffer();

	unsigned GetLostFrmCount(void);
	unsigned GetReadIndex(void);
	unsigned GetWriteIndex(void);

	bool GetData(void *pReadTo);
	void PutData(void *pData, UINT32 iNumFramesToRead);

	bool IsDataAvailable();

private:
	 float *m_pIAudioCircleBuffer;
	 unsigned iReadPos ;
	 unsigned iWritePos;
	 unsigned iLostFrmCount ;

public:
	UINT32 m_iFrameSize_10ms;	//Unit is Frame of IAudioClient. One 'Frame' means nBlockAlign Bytes.
};

extern IAudioCircleBuffer * m_pCaptureBuffer;
extern IAudioCircleBuffer * m_pRenderBuffer ;


#endif	//_WASAPI_H