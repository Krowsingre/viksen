#pragma once
// Streams one World stem from disk with AVAudioFile (AAC/M4A, CAF, WAV...),
// converting to the engine's sample rate with AVAudioConverter when needed.
// Used only on the World feeder thread.

#include <memory>
#include <string>

#include "core/worlds/WorldFeeder.h"

#ifdef __OBJC__
@class NSURL;
#else
typedef struct objc_object NSURL;
#endif

namespace atmos::ios {

// Returns nullptr (and sets error) if the file cannot be opened.
std::unique_ptr<core::StemDecoder> makeStemDecoder(NSURL* url, double outputRate, std::string& error);

} // namespace atmos::ios
