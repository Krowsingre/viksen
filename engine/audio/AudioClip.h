#pragma once
// Immutable decoded audio held in memory. Created off the audio thread, handed to
// the engine by pointer, and destroyed off the audio thread (see AudioEngine).
// Leading silence is part of the clip and is never trimmed implicitly.

#include <memory>

#include "engine/audio/AudioBuffer.h"

namespace atmos {

struct AudioClip {
    AudioBuffer samples;   // non-interleaved float, nominal range [-1, 1]
    double sampleRate = 48000.0;

    int numChannels() const noexcept { return samples.numChannels(); }
    int numFrames() const noexcept { return samples.numFrames(); }
    double durationSeconds() const noexcept { return sampleRate > 0 ? numFrames() / sampleRate : 0.0; }
};

using ClipPtr = std::shared_ptr<const AudioClip>;

} // namespace atmos
