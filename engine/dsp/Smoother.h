#pragma once
// Click-free parameter smoothing. Linear ramp over a fixed time; real-time safe.

#include <algorithm>
#include <cmath>

namespace atmos {

class LinearSmoother {
public:
    void prepare(double sampleRate, double rampSeconds) noexcept {
        rampSamples_ = static_cast<int>(std::max(1.0, sampleRate * rampSeconds));
    }

    void reset(float value) noexcept {
        current_ = target_ = value;
        remaining_ = 0;
        step_ = 0.0f;
    }

    void setTarget(float target) noexcept {
        if (!(target < target_ || target > target_)) return; // unchanged (avoids -Wfloat-equal)
        target_ = target;
        remaining_ = rampSamples_;
        step_ = (target_ - current_) / static_cast<float>(remaining_);
    }

    float next() noexcept {
        if (remaining_ > 0) {
            current_ += step_;
            if (--remaining_ == 0) current_ = target_;
        }
        return current_;
    }

    bool isSmoothing() const noexcept { return remaining_ > 0; }
    float current() const noexcept { return current_; }
    float target() const noexcept { return target_; }

private:
    float current_ = 0.0f;
    float target_ = 0.0f;
    float step_ = 0.0f;
    int remaining_ = 0;
    int rampSamples_ = 480;
};

inline float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
inline float gainToDb(float gain) noexcept { return gain > 0.0f ? 20.0f * std::log10(gain) : -144.0f; }

} // namespace atmos
