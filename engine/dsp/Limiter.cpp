#include "engine/dsp/Limiter.h"

#include <algorithm>
#include <cmath>

#include "engine/dsp/Smoother.h"

namespace atmos {

void SafetyLimiter::prepare(double sampleRate, float ceilingDb, float releaseMs) noexcept {
    setCeilingDb(ceilingDb);
    const double releaseSamples = std::max(1.0, sampleRate * releaseMs / 1000.0);
    releaseCoeff_ = static_cast<float>(std::exp(-1.0 / releaseSamples));
    reset();
}

void SafetyLimiter::setCeilingDb(float db) noexcept { ceiling_ = std::min(1.0f, dbToGain(db)); }

void SafetyLimiter::process(AudioBuffer& buffer, int numFrames) noexcept {
    const int channels = buffer.numChannels();
    float minGain = 1.0f;
    for (int i = 0; i < numFrames; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            float s = buffer.channel(ch)[i];
            if (!std::isfinite(s)) { s = 0.0f; buffer.channel(ch)[i] = 0.0f; } // NaN/Inf guard
            peak = std::max(peak, std::fabs(s));
        }
        const float required = peak > ceiling_ ? ceiling_ / peak : 1.0f;
        // Instant attack, exponential release toward 1.
        gain_ = required < gain_ ? required : std::min(required, 1.0f - releaseCoeff_ * (1.0f - gain_));
        minGain = std::min(minGain, gain_);
        for (int ch = 0; ch < channels; ++ch) {
            float& s = buffer.channel(ch)[i];
            s = std::clamp(s * gain_, -ceiling_, ceiling_);
        }
    }
    lastReduction_ = minGain;
}

} // namespace atmos
