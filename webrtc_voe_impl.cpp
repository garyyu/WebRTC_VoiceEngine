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
 *
 */

#include <assert.h>
#include "webrtc_voe_impl.h"
#include "common_audio/resampler/include/resampler.h"
#include "webrtc/modules/audio_processing/splitting_filter.h"


#ifndef WEBRTC_AEC_AGGRESSIVENESS
    #define WEBRTC_AEC_AGGRESSIVENESS kAecNlpAggressive	//kAecNlpConservative, kAecNlpModerate, kAecNlpAggressive
#endif

#ifndef WEBRTC_USE_NS
	#define WEBRTC_USE_NS VOE_FALSE	//True: Enable NS, False: Disable NS.
#endif

#ifndef WEBRTC_NS_POLICY
    #define WEBRTC_NS_POLICY 2	//0: Mild, 1: Medium , 2: Aggressive
#endif

#if WEBRTC_AEC_USE_MOBILE == 1
	#include <modules/audio_processing/aecm/include/echo_control_mobile.h>
	#define W_WebRtcAec_Create WebRtcAecm_Create
	#define W_WebRtcAec_Free WebRtcAecm_Free
	#define W_WebRtcAec_get_error_code WebRtcAecm_get_error_code
	#define W_WebRtcAec_Init(INST, CR) WebRtcAecm_Init(INST, CR)
	#define W_WebRtcAec_BufferFarend WebRtcAecm_BufferFarend
#else
	#include <modules/audio_processing/aec/include/echo_cancellation.h>
	#define W_WebRtcAec_Create WebRtcAec_Create
	#define W_WebRtcAec_Free WebRtcAec_Free
	#define W_WebRtcAec_get_error_code WebRtcAec_get_error_code
	#define W_WebRtcAec_Init(INST, CR) WebRtcAec_Init(INST, CR, CR)
	#define W_WebRtcAec_BufferFarend WebRtcAec_BufferFarend
#endif

#include <modules/audio_processing/ns/include/noise_suppression.h>

#ifdef _DEBUG
#define VALIDATE                                       \
    if (res != 0){                                     \
		printf("##%s(%i) ERROR:\n",__FILE__, __LINE__);								\
		printf("       %s  error, code = %i\n",__FUNCTION__, base->LastError());	\
    }

#else
#define VALIDATE 
#endif

#define SAFE_FREE(p)	{ if (p) { free(p); p=NULL; } }

bool webrtc_use_ns = WEBRTC_USE_NS;

//-----------------------------------------------------------------------------------//

#define print_webrtc_aec_error(c,d)	_print_webrtc_aec_error(__FILE__, __LINE__, c, d)
static void _print_webrtc_aec_error(const char* filename, int linenum, const char* tag, void *AEC_inst) {
	unsigned status = W_WebRtcAec_get_error_code(AEC_inst);
    printf("%s:%d-WebRTC AEC ERROR (%s) %d\n", filename, linenum, tag, status);
}

//-----------------------------------------------------------------------------------//

/*
enum {
  kSamplesPer8kHzChannel = 80,
  kSamplesPer16kHzChannel = 160,
  kSamplesPer32kHzChannel = 320
};

struct MixedAudioChannel {
  MixedAudioChannel() {
    memset(data, 0, sizeof(data));
  }

  int16_t data[kSamplesPer32kHzChannel];
};

struct SplitAudioChannel {
  SplitAudioChannel() {
    memset(low_pass_data, 0, sizeof(low_pass_data));
    memset(high_pass_data, 0, sizeof(high_pass_data));
    memset(analysis_filter_state1, 0, sizeof(analysis_filter_state1));
    memset(analysis_filter_state2, 0, sizeof(analysis_filter_state2));
    memset(synthesis_filter_state1, 0, sizeof(synthesis_filter_state1));
    memset(synthesis_filter_state2, 0, sizeof(synthesis_filter_state2));
  }

  int16_t low_pass_data[kSamplesPer16kHzChannel];
  int16_t high_pass_data[kSamplesPer16kHzChannel];

  int32_t analysis_filter_state1[6];
  int32_t analysis_filter_state2[6];
  int32_t synthesis_filter_state1[6];
  int32_t synthesis_filter_state2[6];
};
*/

//-----------------------------------------------------------------------------------//
//--- High Pass Filter ---//

const int16_t kFilterCoefficients8kHz[5] =
    {3798, -7596, 3798, 7807, -3733};

const int16_t kFilterCoefficients[5] =
    {4012, -8024, 4012, 8002, -3913};

struct FilterState {
  int16_t y[4];
  int16_t x[2];
  const int16_t* ba;
};

static int InitializeFilter(FilterState* hpf, int sample_rate_hz) {
  assert(hpf != NULL);

  if (sample_rate_hz == 8000) {
    hpf->ba = kFilterCoefficients8kHz;
  } else {
    hpf->ba = kFilterCoefficients;
  }

  WebRtcSpl_MemSetW16(hpf->x, 0, 2);
  WebRtcSpl_MemSetW16(hpf->y, 0, 4);

  return 0;
}


static int Filter(FilterState* hpf, int16_t* data, int length) {
  assert(hpf != NULL);

  int32_t tmp_int32 = 0;
  int16_t* y = hpf->y;
  int16_t* x = hpf->x;
  const int16_t* ba = hpf->ba;

  for (int i = 0; i < length; i++) {
    //  y[i] = b[0] * x[i] + b[1] * x[i-1] + b[2] * x[i-2]
    //         + -a[1] * y[i-1] + -a[2] * y[i-2];

    tmp_int32 =
        WEBRTC_SPL_MUL_16_16(y[1], ba[3]); // -a[1] * y[i-1] (low part)
    tmp_int32 +=
        WEBRTC_SPL_MUL_16_16(y[3], ba[4]); // -a[2] * y[i-2] (low part)
    tmp_int32 = (tmp_int32 >> 15);
    tmp_int32 +=
        WEBRTC_SPL_MUL_16_16(y[0], ba[3]); // -a[1] * y[i-1] (high part)
    tmp_int32 +=
        WEBRTC_SPL_MUL_16_16(y[2], ba[4]); // -a[2] * y[i-2] (high part)
    tmp_int32 = (tmp_int32 << 1);

    tmp_int32 += WEBRTC_SPL_MUL_16_16(data[i], ba[0]); // b[0]*x[0]
    tmp_int32 += WEBRTC_SPL_MUL_16_16(x[0], ba[1]);    // b[1]*x[i-1]
    tmp_int32 += WEBRTC_SPL_MUL_16_16(x[1], ba[2]);    // b[2]*x[i-2]

    // Update state (input part)
    x[1] = x[0];
    x[0] = data[i];

    // Update state (filtered part)
    y[2] = y[0];
    y[3] = y[1];
    y[0] = static_cast<int16_t>(tmp_int32 >> 13);
    y[1] = static_cast<int16_t>((tmp_int32 -
        WEBRTC_SPL_LSHIFT_W32(static_cast<int32_t>(y[0]), 13)) << 2);

    // Rounding in Q12, i.e. add 2^11
    tmp_int32 += 2048;

    // Saturate (to 2^27) so that the HP filtered signal does not overflow
    tmp_int32 = WEBRTC_SPL_SAT(static_cast<int32_t>(134217727),
                               tmp_int32,
                               static_cast<int32_t>(-134217728));

    // Convert back to Q0 and use rounding
    data[i] = (int16_t)WEBRTC_SPL_RSHIFT_W32(tmp_int32, 12);

  }

  return 0;
}

//-----------------------------------------------------------------------------------//

/*
 * Create the AEC.
 */
extern "C" int WEBRTC_API webrtc_aec_create(
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned tail_ms,
				     unsigned options,
				     void **p_echo )
{
	webrtc_ec *echo;
    //int sampling_rate;
    int status;

    *p_echo = NULL;

    echo = (webrtc_ec *) malloc(sizeof(webrtc_ec));
    assert(echo != NULL);
	memset(echo, 0, sizeof(webrtc_ec));

    // Alloc memory
    status = W_WebRtcAec_Create(&echo->AEC_inst);
    if(status){
    	return -1;	//No Memory
    }
	printf("%s:%d-Create webRTC AEC with clock rate %d\n", __FILE__, __LINE__, clock_rate);

    // Init
    status = W_WebRtcAec_Init(echo->AEC_inst,
    		clock_rate);

	if(status != 0) {
        if (echo->AEC_inst) {
            print_webrtc_aec_error("Init", echo->AEC_inst);
            W_WebRtcAec_Free(echo->AEC_inst);
            echo->AEC_inst = NULL;
        }
    	return -2;	//Init Failure
    }

	// Set configuration -- sample code for future use
	// For now keep default values
/*
#if WEBRTC_AEC_USE_MOBILE == 1
	AecmConfig aecm_config;
	aecm_config.cngMode = AecmTrue;
	aecm_config.echoMode = 4;

    status = WebRtcAecm_set_config(echo->AEC_inst, aecm_config);
    if(status != 0) {
        print_webrtc_aec_error("Init config", echo->AEC_inst);
        WebRtcAec_Free(echo->AEC_inst);
    	return PJ_EBUG;
    }
#else
    AecConfig aec_config;
    aec_config.nlpMode = PJMEDIA_WEBRTC_AEC_AGGRESSIVENESS;
    aec_config.skewMode = kAecTrue;
    aec_config.metricsMode = kAecFalse;
    aec_config.delay_logging = kAecFalse;

    status = WebRtcAec_set_config(echo->AEC_inst, aec_config);
    if(status != 0) {
        print_webrtc_aec_error("Init config", echo->AEC_inst);
        WebRtcAec_Free(echo->AEC_inst);
    	return PJ_EBUG;
    }
#endif
*/

	if (webrtc_use_ns == VOE_TRUE){
		status = WebRtcNs_Create((NsHandle **)&echo->NS_inst);
		if(status != 0) {
			return -1;	// No Memory
		}

		status = WebRtcNs_Init((NsHandle *)echo->NS_inst, clock_rate);
		if(status != 0) {
			if(echo->AEC_inst){
				W_WebRtcAec_Free(echo->AEC_inst);
				echo->AEC_inst = NULL;
			}

			if (echo->NS_inst) {
				printf("%s:%d-Could not initialize noise suppressor", __FILE__, __LINE__);
				WebRtcNs_Free((NsHandle *)echo->NS_inst);
				echo->NS_inst = NULL;
			}
			return -1;	//Init Failure
		}

		status = WebRtcNs_set_policy((NsHandle *)echo->NS_inst, WEBRTC_NS_POLICY);
		if (status != 0) {
			printf("%s:%d-Could not set noise suppressor policy", __FILE__, __LINE__);
		}
	}else{
		echo->NS_inst = NULL;
	}

	// Initialize High Pass Filter
	echo->HP_FilterState = (FilterState*)malloc(sizeof(FilterState));
	InitializeFilter((FilterState*)(echo->HP_FilterState), clock_rate);

    echo->samples_per_frame = samples_per_frame;
    echo->echo_tail = tail_ms;
    echo->echo_skew = 0;
    echo->clock_rate = clock_rate;
    echo->blockLen10ms = (10 * channel_count * clock_rate / 1000);

    /* Create temporary frames for echo cancellation */
    echo->tmp_frame  = (int16_t*) malloc(sizeof(int16_t)*MAX_FRAMING);
    assert(echo->tmp_frame != NULL);
    echo->tmp_frame2 = (int16_t*) malloc(sizeof(int16_t)*MAX_FRAMING);
    assert(echo->tmp_frame2 != NULL);

    /* Done */
    *p_echo = echo;
    return 0;
}


/*
 * Destroy AEC
 */
extern "C" int WEBRTC_API webrtc_aec_destroy(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    assert(echo);

    if (echo->AEC_inst) {
    	W_WebRtcAec_Free(echo->AEC_inst);
    	echo->AEC_inst = NULL;
    }
    if (echo->NS_inst) {
        WebRtcNs_Free((NsHandle *)echo->NS_inst);
        echo->NS_inst = NULL;
    }
    SAFE_FREE(echo->tmp_frame );
    SAFE_FREE(echo->tmp_frame2);
	SAFE_FREE(echo->HP_FilterState);

	SAFE_FREE(echo);

    return 0;
}


/*
 * Reset AEC
 */
extern "C" void WEBRTC_API webrtc_aec_reset(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    assert(echo != NULL);
    int status;
    /* re-initialize the EC */
	status = W_WebRtcAec_Init(echo->AEC_inst, echo->clock_rate);
    if(status != 0) {
        print_webrtc_aec_error("re-Init", echo->AEC_inst);
        return;
    } else {

#if WEBRTC_AEC_USE_MOBILE == 1
    	AecmConfig aecm_config;
    	aecm_config.cngMode = AecmTrue;
    	aecm_config.echoMode = 4;

        status = WebRtcAecm_set_config(echo->AEC_inst, aecm_config);
        if(status != 0) {
            print_webrtc_aec_error("re-Init config", echo->AEC_inst);
            return;
        }
#else
        AecConfig aec_config;
        aec_config.nlpMode = WEBRTC_AEC_AGGRESSIVENESS;
        aec_config.skewMode = kAecTrue;
        aec_config.metricsMode = kAecFalse;

        status = WebRtcAec_set_config(echo->AEC_inst, aec_config);
        if(status != 0) {
            print_webrtc_aec_error("re-Init config", echo->AEC_inst);
            return;
        }
#endif
    }
    printf("%s:%d-WebRTC AEC reset succeeded", __FILE__, __LINE__);
}


/*
 * Perform echo cancellation.
 */
extern "C" int WEBRTC_API webrtc_aec_cancel_echo( void *state,
					   int16_t *rec_frm,
					   const int16_t *play_frm,
					   unsigned framing,
					   unsigned options,
					   void *reserved )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    int status;
    unsigned i; //, tail_factor;

    /* Sanity checks */
    assert(echo && rec_frm && play_frm && options==0 && reserved==NULL);
	if ((echo==NULL) || (rec_frm==NULL) || (play_frm==NULL) || (framing>MAX_FRAMING)){
		return -1;
	}
	echo->samples_per_frame = framing;	//for better flexibility, framing can be changed dynamically.

	//tail_factor = echo->samples_per_frame / echo->blockLen10ms;
    for(i=0; i < echo->samples_per_frame; i+= echo->blockLen10ms) {

		/* Feed farend buffer */
		status = W_WebRtcAec_BufferFarend(echo->AEC_inst, &play_frm[i], echo->blockLen10ms);
		if(status != 0) {
			print_webrtc_aec_error("buffer farend", echo->AEC_inst);
			return -1;
		}

		/* high pass filter */
		Filter((FilterState*)(echo->HP_FilterState), &rec_frm[i], echo->blockLen10ms);

		/* Process echo cancellation */
#if WEBRTC_AEC_USE_MOBILE == 1
		status = WebRtcAecm_Process(echo->AEC_inst,
							(WebRtc_Word16 *) (&rec_frm[i]),
							(WebRtc_Word16 *) (&echo->tmp_frame[i]),
							echo->blockLen10ms,
							echo->echo_tail / tail_factor);
#else
		status = WebRtcAec_Process(echo->AEC_inst,
							(WebRtc_Word16 *) (&rec_frm[i]),
							NULL,
							(WebRtc_Word16 *) (&echo->tmp_frame[i]),
							NULL,
							echo->blockLen10ms,
							(options==0?echo->echo_tail:options),	// echo->echo_tail / tail_factor,
							echo->echo_skew);
#endif
		if(status != 0){
			print_webrtc_aec_error("Process echo", echo->AEC_inst);
			return -1;
		}

    	if(echo->NS_inst){
			/* Noise suppression */
			status = WebRtcNs_Process((NsHandle *)echo->NS_inst,
									  (WebRtc_Word16 *) (&echo->tmp_frame[i]),
									  NULL,
									  (WebRtc_Word16 *) (&echo->tmp_frame2[i]),
									  NULL);
			if (status != 0) {
				printf("%s:%d-Error suppressing noise", __FILE__, __LINE__);
				return -1;
			}
    	}
    }


    /* Copy temporary buffer back to original rec_frm */
	memcpy(rec_frm, 
			(echo->NS_inst)?(echo->tmp_frame2):(echo->tmp_frame),
			(echo->samples_per_frame)<<1);

    return 0;
}


/**********************************************************************************************
 *					WebRTC Resampler API                                                      *
 **********************************************************************************************/

extern "C" int	WEBRTC_API webrtc_resampler_create(
                        int inFreq, 
						int outFreq,
						void **p_resampler
						)
{
	Resampler * p_objResampler = NULL;

	p_objResampler = new Resampler(inFreq, outFreq, kResamplerSynchronous);
	assert(p_objResampler!=NULL);

	*p_resampler = p_objResampler;
	return 0;
}

extern "C" int	WEBRTC_API webrtc_resampler_destroy( void *state )
{
	Resampler * p_objResampler = (Resampler *)state;
	if (p_objResampler!=NULL){
		delete p_objResampler;
		return 0;
	}
	else
		return -1;
}

extern "C" int	WEBRTC_API webrtc_resampler_reset(void *state, int inFreq, int outFreq)
{
	Resampler * p_objResampler = (Resampler *)state;
	if (p_objResampler!=NULL){
		p_objResampler->Reset(inFreq, outFreq, kResamplerSynchronous);
		return 0;
	}
	else
		return -1;
}

extern "C" int	WEBRTC_API webrtc_resampler_process(void *state,
                        const int16_t* samplesIn, 
						int lengthIn, 
						int16_t* samplesOut,
						int maxLen, int &outLen 
						)
{
	int iRetVal = 0;
	Resampler * p_objResampler = (Resampler *)state;
	if (p_objResampler!=NULL){
		iRetVal = p_objResampler->Push(samplesIn, lengthIn, samplesOut, maxLen, outLen);
		return iRetVal;
	}
	else
		return -1;

}

