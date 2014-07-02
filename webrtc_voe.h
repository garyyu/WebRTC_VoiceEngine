/*
 *  Copyright (c) 2013 Gary Yu. All Rights Reserved.
 *
 *	URL: https://github.com/garyyu/WebRTC_VoiceEngine
 *
 *  Use of this source code is governed by a New BSD license which can be found in the LICENSE file
 *  in the root of the source tree. Refer to README.md.
 *  For WebRTC License & Patents information, please read files LICENSE.webrtc and PATENTS.webrtc.
 */

/*
 * This file contains the Wrapper of WebRTC Voice Engine.
 * You just need this header file , lib file , and dll file in the root folder.
 *
 */

#ifndef __WRTC_VOICE_ENGINE_H__
#define __WRTC_VOICE_ENGINE_H__

#define _WEBRTC_API_EXPORTS	// For DLL Building.
#define _WEBRTC_FOR_PC

#if		defined(_WEBRTC_API_EXPORTS)
#define WEBRTC_API  __declspec(dllexport)
#elif	defined(_WEBRTC_API_IMPORTS)
#define WEBRTC_API  __declspec(dllimport)
#else
#define	WEBRTC_API
#endif


#define VOE_TRUE	 1
#define VOE_FALSE    0

typedef short int16_t;

typedef struct webrtc_ec
{
	void*		AEC_inst;
    void*		NS_inst;
	void*		HP_FilterState;
    unsigned	samples_per_frame;
    unsigned	echo_tail;
    unsigned	echo_skew;
    unsigned    clock_rate;
    unsigned 	blockLen10ms;
    int16_t*	tmp_frame;
    int16_t*	tmp_frame2;
} webrtc_ec;


typedef struct {
  int instant;
  int average;
  int max;
  int min;
} MyAecLevel;

typedef struct {
  MyAecLevel rerl;
  MyAecLevel erl;
  MyAecLevel erle;
  MyAecLevel aNlp;
} MyAecMetrics;


#if defined(_WEBRTC_FOR_PC)
#define WEBRTC_AEC_USE_MOBILE 0
#else
#define WEBRTC_AEC_USE_MOBILE 1
#endif

/************************************************************************/
/*                        Volume Control API                            */
/************************************************************************/

class WEBRTC_API WebRTCVolumeCtlImpl
{
private:
    void* m_voe;			//VoiceEngine*
    void* m_base;			//VoEBase*
    void* volume_control;	//VoEVolumeControl*
	//void* m_apm;			//VoEAudioProcessing*
	//void* m_hardware;		//VoEHardware*

public:

    WebRTCVolumeCtlImpl();
    ~WebRTCVolumeCtlImpl();

    int webrtc_voe_init();
    void webrtc_voe_deinit();

	/*--- Microphone Level Control. Valid range is [0,255]. ---*/
	int SetMicVolume(unsigned int level);	
	int GetMicVolume(unsigned int &level);		

	/*--- Speaker Level Control. Valid range is [0,255]. ---*/
	int SetSpkVolume(unsigned int volume);	
	int GetSpkVolume(unsigned int &volume);				
};


class WEBRTC_API MyAudioLevel
{
public:
    MyAudioLevel();
    ~MyAudioLevel();

    signed char Level() const;
    signed short Count() const;
    signed short LevelFullRange() const;
    void Clear();

    void ComputeLevel(const signed short* audioFrame, int length);

private:
    enum { kUpdateFrequency = 10};

    signed short _absMax;
    signed short _count;
    signed char _currentLevel;
    signed short _currentLevelFullRange;

	signed short My_WebRtcSpl_MaxAbsValueW16C(const signed short* vector, int length);

};


/************************************************************************/
/*                              Main AEC API                            */
/************************************************************************/

extern "C" int  WEBRTC_API		webrtc_aec_create(
                        unsigned clock_rate,
                        unsigned channel_count,
                        unsigned samples_per_frame,
                        unsigned tail_ms,
                        unsigned options,
                        void **p_echo );

extern "C" int  WEBRTC_API		webrtc_aec_destroy(void *state );

extern "C" void WEBRTC_API		webrtc_aec_reset(void *state );
extern "C" int  WEBRTC_API		webrtc_aec_cancel_echo(void *state,
                        int16_t *rec_frm,
                        const int16_t *play_frm,
						unsigned framing,
                        unsigned options,
                        void *reserved );

extern "C" int  WEBRTC_API		webrtc_aec_get_metrics( 
						void *state, 
						void *_aec_metrics );


/************************************************************************/
/*                              Main Resampler API                      */
/************************************************************************/

extern "C" int WEBRTC_API		webrtc_resampler_create(
                        int inFreq, 
						int outFreq,
						void **p_resampler
						);

extern "C" int WEBRTC_API		webrtc_resampler_destroy(void *state );

extern "C" int WEBRTC_API		webrtc_resampler_reset( void *state, int inFreq, int outFreq );
extern "C" int WEBRTC_API		webrtc_resampler_process(void *state,
                        const int16_t* samplesIn, 
						int lengthIn, 
						int16_t* samplesOut,
						int maxLen, int &outLen 
						 );

/************************************************************************/
/*         Voice processing configure: (AEC, NS, AGC, VAD)              */
/************************************************************************/
	/*
	**vad: mode 
	**0: low
	**1:
	**2:
	**3: most high
	*/
	int WebRTCVoe_SetVADStatus(int channelsid, bool b, int mode = 1);

	/*
	**agc: mode 
	**0: previously set mode
	**1: platform default
	**2: adaptive mode for use when analog volume control exists (e.g. for PC softphone)
	**3: scaling takes place in the digital domain (e.g. for conference servers and embedded devices)
	**4: can be used on embedded devices where the capture signal level is predictable
	*/
	int WebRTCVoe_SetAgcStatus(bool b, int mode = 1);

	/*
	**EC mode
	**0: previously set mode
	**1: platform default
	**2: conferencing default (aggressive AEC)
	**3: Acoustic Echo Cancellation
	**4: AEC mobile
	*/
	int WebRTCVoe_SetEcStatus(bool b, int mode = 3);

	/*
	**NS mode
	**0: previously set mode
	**1: platform default
	**2: conferencing default
	**3: lowest suppression
	**4: Moderate Suppression
	**5: High Suppression
	**6: highest suppression
	*/
	int WebRTCVoe_SetNsStatus(bool b, int mode = 4);

	int WebRTCVoe_GetVADStatus(int channelsid, bool &b, int &mode);
	int WebRTCVoe_GetAgcStatus(bool &b, int &mode);
	int WebRTCVoe_GetEcStatus(bool &b, int &mode) ;
	int WebRTCVoe_GetNsStatus(bool &b, int &mode) ;



#endif //__WRTC_VOICE_ENGINE_H__