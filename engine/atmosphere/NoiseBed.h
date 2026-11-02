#pragma once
// Synthesized atmospheric bed: decorrelated stereo pink noise, colour filter and slow
// deterministic "breathing" amplitude drift. Lets a user create atmosphere with no
// imported audio and no microphone. Fully deterministic for a given seed.

#include <cstdint>

#include "engine/audio/AudioBuffer.h"
#include "engine/audio/Parameter.h"
#include "engine/dsp/Biquad.h"

namespace atmos {

class NoiseBed {
public:
    void prepare(double sampleRate);
    void setSeed(uint64_t seed) noexcept;

    // Any thread.
    SmoothedParameter level{0.25f};        // linear
    SmoothedParameter colourHz{900.0f};    // low-pass cutoff
    SmoothedParameter driftDepth{0.5f};    // 0..1 amplitude breathing depth
    SmoothedParameter driftRateHz{0.07f};

    // Audio thread: adds into the first two channels of `out`.
    void processAdd(AudioBuffer& out, int numFrames) noexcept;

private:
    struct Pink {
        uint64_t state = 0x9E3779B97F4A7C15ull;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float next() noexcept;
    };
    double sampleRate_ = 48000.0;
    Pink pink_[2];
    Biquad filter_[2];
    float lastColour_ = -1.0f;
    double phase_ = 0.0;
};

} // namespace atmos
