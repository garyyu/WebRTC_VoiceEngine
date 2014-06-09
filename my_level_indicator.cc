/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/typedefs.h"
#include "webrtc_voe.h"
#include <stdio.h>

// Number of bars on the indicator.
// Note that the number of elements is specified because we are indexing it
// in the range of 0-32
const int8_t permutation[33] =
    {0,1,2,3,4,4,5,5,5,5,6,6,6,6,6,7,7,7,7,8,8,8,9,9,9,9,9,9,9,9,9,9,9};

MyAudioLevel::MyAudioLevel() :
    //_critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _absMax(0),
    _count(0),
    _currentLevel(0),
    _currentLevelFullRange(0) {
}

MyAudioLevel::~MyAudioLevel() {
    //delete &_critSect;
}

void MyAudioLevel::Clear()
{
    //CriticalSectionScoped cs(&_critSect);
    _absMax = 0;
    _count = 0;
    _currentLevel = 0;
    _currentLevelFullRange = 0;
}


// Maximum absolute value of word16 vector. C version for generic platforms.
int16_t MyAudioLevel::My_WebRtcSpl_MaxAbsValueW16C(const int16_t* vector, int length) {
  int i = 0, absolute = 0, maximum = 0;

  if (vector == NULL || length <= 0) {
    return -1;
  }

  for (i = 0; i < length; i++) {
    absolute = (int)vector[i];
	if (absolute<0 ) absolute = -absolute; 

    if (absolute > maximum) {
      maximum = absolute;
    }
  }

  // Guard the case for abs(-32768).
  if (maximum > 32767) {
    maximum = 32767;
  }

  return (int16_t)maximum;
}

void MyAudioLevel::ComputeLevel(const int16_t* audioFrame, int length)
{
    int16_t absValue(0);

    // Check speech level (works for 2 channels as well)
    absValue = My_WebRtcSpl_MaxAbsValueW16C(
        audioFrame,
        length);

    // Protect member access using a lock since this method is called on a
    // dedicated audio thread in the RecordedDataIsAvailable() callback.
    //CriticalSectionScoped cs(&_critSect);

    if (absValue > _absMax)
    _absMax = absValue;

    // Update level approximately 10 times per second
    if (_count++ == kUpdateFrequency)
    {
        _currentLevelFullRange = _absMax;

        _count = 0;

        // Highest value for a int16_t is 0x7fff = 32767
        // Divide with 1000 to get in the range of 0-32 which is the range of
        // the permutation vector
        int32_t position = _absMax/1000;

        // Make it less likely that the bar stays at position 0. I.e. only if
        // its in the range 0-250 (instead of 0-1000)
        if ((position == 0) && (_absMax > 250))
        {
            position = 1;
        }
        _currentLevel = permutation[position];

        // Decay the absolute maximum (divide by 4)
        _absMax >>= 2;
    }
}

int8_t MyAudioLevel::Level() const
{
    //CriticalSectionScoped cs(&_critSect);
    return _currentLevel;
}

int16_t MyAudioLevel::Count() const
{
    //CriticalSectionScoped cs(&_critSect);
    return _count;
}

int16_t MyAudioLevel::LevelFullRange() const
{
    //CriticalSectionScoped cs(&_critSect);
    return _currentLevelFullRange;
}

