#pragma once
// Master safety limiter. Instant-attack peak limiter with smooth release, linked
// across channels, followed by a hard ceiling clamp. Guarantees |out| <= ceiling.
// This is a hearing-safety stage, not a mastering limiter.

#include "engine/audio/AudioBuffer.h"

namespace atmos {

class SafetyLimiter {
public:
    void prepare(double sampleRate, float ceilingDb = -1.0f, float releaseMs = 150.0f) noexcept;
    void setCeilingDb(float db) noexcept;
    void process(AudioBuffer& buffer, int numFrames) noexcept;
    void reset() noexcept { gain_ = 1.0f; lastReduction_ = 1.0f; }

    float ceiling() const noexcept { return ceiling_; }
    // Minimum gain applied during the last block (1.0 = no reduction).
    float lastGainReduction() const noexcept { return lastReduction_; }

private:
    float ceiling_ = 0.891f;
    float releaseCoeff_ = 0.9999f;
    float gain_ = 1.0f;
    float lastReduction_ = 1.0f;
};

} // namespace atmos
