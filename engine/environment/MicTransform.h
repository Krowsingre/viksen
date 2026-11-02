#pragma once
// Real-time transformation of environmental (microphone) input.
// Phase 1 chain: tone filter -> feedback echo, with wet/dry. Additional
// transformations (granular delay, reverse buffer, pitch/formant) will plug in here.
// Latency: zero added for the dry/filtered path; echo is a musical delay effect.

#include "engine/audio/Parameter.h"
#include "engine/dsp/Biquad.h"
#include "engine/dsp/DelayLine.h"

namespace atmos {

class MicTransform {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // Any thread.
    SmoothedParameter cutoffHz{1800.0f};   // low-pass tone ("submerged" when low)
    SmoothedParameter echoSeconds{0.38f};
    SmoothedParameter feedback{0.45f};     // clamped to < 0.95 for stability
    SmoothedParameter wet{0.6f};

    // Audio thread: mono in, mono out (same length). In-place allowed.
    void process(const float* in, float* out, int numFrames) noexcept;

private:
    double sampleRate_ = 48000.0;
    Biquad tone_;
    float lastCutoff_ = -1.0f;
    DelayLine delay_;
};

} // namespace atmos
