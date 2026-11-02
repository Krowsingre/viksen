#pragma once
// Real-time transformation of environmental (microphone) input.
// Chain: tone filter -> frequency shifter (blend) -> feedback echo (wet/dry).
// Further transformations (granular delay, reverse buffer, pitch/formant) plug in here.
// Latency: the frequency shifter delays the whole transformed path by
// FrequencyShifter::kHalfTaps samples (about 2.6 ms at 48 kHz); the raw mic bus is not delayed.

#include "engine/audio/Parameter.h"
#include "engine/dsp/Biquad.h"
#include "engine/dsp/DelayLine.h"
#include "engine/dsp/FrequencyShifter.h"

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
    SmoothedParameter shiftHz{-180.0f};    // frequency shift amount (Hz, may be negative)
    SmoothedParameter shiftMix{0.0f};      // 0 = off, 1 = fully shifted

    // Audio thread: mono in, mono out (same length). In-place allowed.
    void process(const float* in, float* out, int numFrames) noexcept;

private:
    double sampleRate_ = 48000.0;
    Biquad tone_;
    float lastCutoff_ = -1.0f;
    DelayLine delay_;
    FrequencyShifter shifter_;
};

} // namespace atmos
