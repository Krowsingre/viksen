#include "engine/dsp/FrequencyShifter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atmos {

void FrequencyShifter::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    const double pi = std::numbers::pi;
    // Ideal Hilbert impulse response: 2/(pi*n) for odd n, 0 for even n; Blackman window.
    for (int i = 0; i < kTaps; ++i) {
        const int n = i - kHalfTaps;
        const double w = 0.42 - 0.5 * std::cos(2 * pi * i / (kTaps - 1)) + 0.08 * std::cos(4 * pi * i / (kTaps - 1));
        coeffs_[i] = (n % 2 == 0) ? 0.0f : static_cast<float>(2.0 / (pi * n) * w);
    }
    history_.assign(2 * kTaps, 0.0f);
    reset();
}

void FrequencyShifter::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0f);
    pos_ = 0;
    phase_ = 0.0;
    lastIn_ = 0.0f;
}

float FrequencyShifter::process(float x) noexcept {
    // Write twice so history_[pos_ .. pos_+kTaps) is always the last kTaps samples.
    history_[pos_] = x;
    history_[pos_ + kTaps] = x;
    pos_ = (pos_ + 1) % kTaps;
    const float* h = history_.data() + pos_; // h[0] oldest ... h[kTaps-1] newest

    float q = 0.0f;
    // Non-zero taps are at odd n = i - kHalfTaps, i.e. even i (kHalfTaps is odd).
    for (int i = 0; i < kTaps; i += 2) q += coeffs_[i] * h[kTaps - 1 - i];
    const float in = h[kTaps - 1 - kHalfTaps]; // delayed to match the filter's group delay
    lastIn_ = in;

    const double twoPi = 2.0 * std::numbers::pi;
    phase_ += shiftHz_ / sampleRate_;
    phase_ -= std::floor(phase_);
    const float c = static_cast<float>(std::cos(twoPi * phase_));
    const float s = static_cast<float>(std::sin(twoPi * phase_));
    return in * c - q * s; // upper sideband: f -> f + shift (negative shift moves down)
}

} // namespace atmos
