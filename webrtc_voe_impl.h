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


#ifndef __WRTC_VOICE_ENGINE_IMPL_H__
#define __WRTC_VOICE_ENGINE_IMPL_H__


#include <cstdlib>
#include <vector>

#include "webrtc_voe.h"

#include "common_types.h"
#include "voe_errors.h"
#include "voe_base.h"
#include "voe_codec.h"
#include "voe_volume_control.h"
#include "voe_audio_processing.h"
#include "voe_file.h"
#include "voe_hardware.h"
#include "voe_network.h"
#include "engine_configurations.h"
#include "voe_neteq_stats.h"
#include "voe_external_media.h"
#include "voe_encryption.h"
#include "voe_rtp_rtcp.h"
#include "voe_video_sync.h"
#include "channel_transport.h"

using namespace webrtc;
using namespace test;

typedef int8_t              WebRtc_Word8;
typedef int16_t             WebRtc_Word16;
typedef int32_t             WebRtc_Word32;
typedef int64_t             WebRtc_Word64;
typedef uint8_t             WebRtc_UWord8;
typedef uint16_t            WebRtc_UWord16;
typedef uint32_t            WebRtc_UWord32;
typedef uint64_t            WebRtc_UWord64;

#define MAX_FRAMING 960		// 60ms@16kHz = 960

/*
class WebRTCVoiceEngineImpl:public WebRTCVoiceEngine
{
private:
    VoiceEngine* m_voe;
    VoEBase* base;
    VoECodec* codec;

public:

    WebRTCVoiceEngineImpl();
    virtual ~WebRTCVoiceEngineImpl();

    //Voice Processing Configurations
    virtual int WebRTCVoe_SetVADStatus(int channelsid, bool b, int mode);
    virtual int WebRTCVoe_SetAgcStatus(bool b, int mode);
    virtual int WebRTCVoe_SetEcStatus(bool b, int mode);
    virtual int WebRTCVoe_SetNsStatus(bool b, int mode);
    virtual int WebRTCVoe_GetVADStatus(int channelsid, bool &b, int &mode);
    virtual int WebRTCVoe_GetAgcStatus(bool &b, int &mode);
    virtual int WebRTCVoe_GetEcStatus(bool &b, int &mode);
    virtual int WebRTCVoe_GetNsStatus(bool &b, int &mode);

};
*/

#endif //__WRTC_VOICE_ENGINE_IMPL_H__