#include "engine/environment/MicConditioner.h"

#include <algorithm>
#include <cmath>

namespace atmos {

void MicConditioner::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    auto coeff = [&](double seconds) { return static_cast<float>(1.0 - std::exp(-1.0 / (seconds * sampleRate))); };
    attack_ = coeff(0.05);
    release_ = coeff(1.5);
    limRelease_ = coeff(0.08);
    upPerSample_ = static_cast<float>(6.0 / sampleRate);     // dB per sample: 6 dB/s upward
    downPerSample_ = static_cast<float>(40.0 / sampleRate);  // and 40 dB/s downward (loud arrives fast)
    reset();
}

void MicConditioner::reset() noexcept {
    rms2_ = 0.0f;
    gainDb_ = 0.0f;
    limitGain_ = 1.0f;
}

void MicConditioner::process(float* x, int n) noexcept {
    const float ceiling = std::pow(10.0f, kCeilingDb / 20.0f);
    for (int i = 0; i < n; ++i) {
        const float p = x[i] * x[i];
        rms2_ += (p > rms2_ ? attack_ : release_) * (p - rms2_);
        const float levelDb = 10.0f * std::log10(rms2_ + 1e-12f);
        if (levelDb > kGateDb) {
            const float want = std::clamp(kTargetDb - levelDb, kMinGainDb, kMaxGainDb);
            if (want > gainDb_) gainDb_ = std::min(want, gainDb_ + upPerSample_);
            else gainDb_ = std::max(want, gainDb_ - downPerSample_);
        }
        float y = x[i] * std::pow(10.0f, gainDb_ / 20.0f);
        // Peak limiter: recover toward unity, but never let this sample past the ceiling.
        limitGain_ += limRelease_ * (1.0f - limitGain_);
        const float mag = std::fabs(y);
        if (mag * limitGain_ > ceiling) limitGain_ = ceiling / mag;
        x[i] = y * limitGain_;
    }
}

} // namespace atmos
