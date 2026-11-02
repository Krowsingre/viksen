#pragma once
// Converts raw accelerometer samples (in g, any orientation) into a smoothed
// "motion energy" (deviation of |a| from 1 g) and detects free fall. Pure math;
// the platform layer (e.g. Core Motion on iPhone) feeds it. Control thread.

#include <cmath>

namespace atmos {

class MotionAnalyzer {
public:
    // smoothingSeconds: time constant of the energy average.
    explicit MotionAnalyzer(double smoothingSeconds = 0.6) : tau_(smoothingSeconds) {}

    void addSample(double ax, double ay, double az, double dtSeconds) noexcept {
        const double mag = std::sqrt(ax * ax + ay * ay + az * az);
        const double dev = std::fabs(mag - 1.0);
        const double a = 1.0 - std::exp(-dtSeconds / tau_);
        energy_ += a * (dev - energy_);
        // Free fall: |a| well below 1 g for a sustained moment (not a single jolt).
        freeFallTime_ = mag < 0.25 ? freeFallTime_ + dtSeconds : 0.0;
        if (freeFallTime_ >= 0.12 && !inFreeFall_) { inFreeFall_ = true; ++freeFallEvents_; }
        if (freeFallTime_ == 0.0) inFreeFall_ = false;
    }

    float energy() const noexcept { return static_cast<float>(energy_); }
    unsigned freeFallEvents() const noexcept { return freeFallEvents_; }

private:
    double tau_;
    double energy_ = 0.0;
    double freeFallTime_ = 0.0;
    bool inFreeFall_ = false;
    unsigned freeFallEvents_ = 0;
};

} // namespace atmos
