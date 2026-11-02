#pragma once
// RBJ cookbook biquad (transposed direct form II). Coefficient updates are not
// smoothed here; callers changing cutoff continuously should do so per block.

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atmos {

class Biquad {
public:
    enum class Type { LowPass, HighPass, BandPass, Peaking };

    void setup(Type type, double sampleRate, double frequency, double q, double gainDb = 0.0) noexcept {
        const double f = std::clamp(frequency, 10.0, sampleRate * 0.49);
        const double w0 = 2.0 * std::numbers::pi * f / sampleRate;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
        double b0 = 0, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (type) {
            case Type::LowPass:
                b0 = (1 - cosw) / 2; b1 = 1 - cosw; b2 = (1 - cosw) / 2;
                a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
                break;
            case Type::HighPass:
                b0 = (1 + cosw) / 2; b1 = -(1 + cosw); b2 = (1 + cosw) / 2;
                a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
                break;
            case Type::BandPass:
                b0 = alpha; b1 = 0; b2 = -alpha;
                a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
                break;
            case Type::Peaking: {
                const double A = std::pow(10.0, gainDb / 40.0);
                b0 = 1 + alpha * A; b1 = -2 * cosw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cosw; a2 = 1 - alpha / A;
                break;
            }
        }
        b0_ = static_cast<float>(b0 / a0); b1_ = static_cast<float>(b1 / a0); b2_ = static_cast<float>(b2 / a0);
        a1_ = static_cast<float>(a1 / a0); a2_ = static_cast<float>(a2 / a0);
    }

    void reset() noexcept { z1_ = z2_ = 0.0f; }

    float process(float x) noexcept {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

private:
    float b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
    float z1_ = 0, z2_ = 0;
};

} // namespace atmos
