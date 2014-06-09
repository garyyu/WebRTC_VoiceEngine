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
#include "trace.h"

//    VoiceEngine* m_voe;
//    VoEBase* m_base;
//    VoEVolumeControl* volume_control;
//	  VoEAudioProcessing* m_apm;
//	  VoEHardware* m_hardware;

#define kMyWebRTCTraceId	111

WebRTCVolumeCtlImpl::WebRTCVolumeCtlImpl():
		m_voe(NULL),
		m_base(NULL),
		//m_apm(NULL),
		volume_control(NULL)
		//m_hardware(NULL)
{
	return;
}

		
WebRTCVolumeCtlImpl::~WebRTCVolumeCtlImpl()
{
    webrtc_voe_deinit();

	return;
}


int WebRTCVolumeCtlImpl::webrtc_voe_init()
{
    m_voe			= webrtc::VoiceEngine::Create();
    m_base			= webrtc::VoEBase::GetInterface( (webrtc::VoiceEngine*)m_voe );
    //m_apm			= webrtc::VoEAudioProcessing::GetInterface((webrtc::VoiceEngine*)m_voe);
    volume_control	= webrtc::VoEVolumeControl::GetInterface( (webrtc::VoiceEngine*)m_voe );
    //m_hardware	= webrtc::VoEHardware::GetInterface((webrtc::VoiceEngine*)m_voe);

    //CHECK(m_voe != NULL, "Voice engine instance failed to be created");
    //CHECK(m_base != NULL, "Failed to acquire base interface");
    //CHECK(volume_control != NULL, "Failed to acquire volume interface");

	if ((m_voe == NULL) || (m_base==NULL) || (volume_control==NULL) /*|| (m_apm==NULL) || (m_hardware==NULL)*/){
		return 1;
	}

#if defined(_DEBUG)
	webrtc::VoiceEngine::SetTraceFile("C:\\TEMP\\WebRTC-Trace.log", true);
    webrtc::VoiceEngine::SetTraceFilter(kTraceAll);
#else
    //Turn Off WebRTC Traces
    webrtc::VoiceEngine::SetTraceFilter(kTraceNone);
#endif

    return ((webrtc::VoEBase*)m_base)->Init();
}


void WebRTCVolumeCtlImpl::webrtc_voe_deinit()
{
	if (m_base){
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): start terminate VoEBase.");
		((webrtc::VoEBase*)m_base)->Terminate();
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): VoEBase terminated.");
	}
	else{
		return;
	}

    if (m_base){
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): start release VoEBase.");
        ((webrtc::VoEBase*)m_base)->Release();
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): VoEBase released.");
		m_base = NULL;
	}

    if (volume_control){
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): start release VoEVolumeControl.");
        ((webrtc::VoEVolumeControl*)volume_control)->Release();
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): VoEVolumeControl released.");
		volume_control = NULL;
	}

	/*
    if (m_apm){
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): start release VoEAudioProcessing.");
        ((webrtc::VoEAudioProcessing*)m_apm)->Release();
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): VoEAudioProcessing released.");
		m_apm = NULL;
	}

    if (m_hardware){
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): start release VoEHardware.");
        ((webrtc::VoEHardware*)m_hardware)->Release();
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): VoEHardware released.");
		m_hardware = NULL;
	}*/

	webrtc::VoiceEngine* voe = (webrtc::VoiceEngine*)m_voe;
	if (m_voe){
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): start Delete VoiceEngine.");
		webrtc::VoiceEngine::Delete(voe);
		WEBRTC_TRACE(kTraceApiCall, kTraceVoice, kMyWebRTCTraceId, "webrtc_voe_deinit(): VoiceEngine Deleted.");
		m_voe = NULL;
	}

	return;
}

/*--- Microphone Level Control. Valid range is [0,255]. ---*/
int WebRTCVolumeCtlImpl::SetMicVolume(unsigned int level)
{
  if (volume_control==NULL)
	  return 1;

  // Set to 0 first in case the mic is above 100%.
  ((webrtc::VoEVolumeControl*)volume_control)->SetMicVolume(0);

  if (((webrtc::VoEVolumeControl*)volume_control)->SetMicVolume(level) != 0) {
    //failure
    return 1;
  }

  return 0;
}

/*--- Microphone Level Control. Valid range is [0,255]. ---*/
int WebRTCVolumeCtlImpl::GetMicVolume(unsigned int &level)	
{
  if (volume_control==NULL)
	  return 1;

  level = 0;
  if (((webrtc::VoEVolumeControl*)volume_control)->GetMicVolume(level) != 0) {
    //failure
    return 1;
  }

  return 0;
}

/*--- Speaker Level Control. Valid range is [0,255]. ---*/
int WebRTCVolumeCtlImpl::SetSpkVolume(unsigned int volume)
{
  if (volume_control==NULL)
	  return 1;

  if (((webrtc::VoEVolumeControl*)volume_control)->SetSpeakerVolume(volume) != 0) {
    //failure
    return 1;
  }

  return 0;
}

/*--- Speaker Level Control. Valid range is [0,255]. ---*/
int WebRTCVolumeCtlImpl::GetSpkVolume(unsigned int &volume)	
{
  if (volume_control==NULL)
	  return 1;

  volume = 0;
  if (((webrtc::VoEVolumeControl*)volume_control)->GetSpeakerVolume(volume) != 0) {
    //failure
    return 1;
  }

  return 0;
}





