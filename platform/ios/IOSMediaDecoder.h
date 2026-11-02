#pragma once
// Decodes any AVFoundation-supported file (WAV, AIFF, CAF, FLAC, MP3, AAC/M4A, ALAC)
// into an in-memory float clip at the file's native rate. Never modifies the file.

#include <string>

#include "core/assets/WavFile.h"
#include "engine/audio/AudioClip.h"

#ifdef __OBJC__
@class NSURL;
#else
typedef struct objc_object NSURL;
#endif

namespace atmos::ios {

bool decodeAudioFile(NSURL* url, AudioClip& clip, core::AudioFileInfo& info, std::string& error);

} // namespace atmos::ios
