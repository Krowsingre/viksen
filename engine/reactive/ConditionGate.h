#pragma once
// Turns a noisy control value into a stable on/off state: separate on/off
// thresholds (hysteresis) plus minimum hold times before switching (debounce).
// Control-rate; not for the audio thread's per-sample loop.

namespace atmos {

class ConditionGate {
public:
    enum class Direction { Above, Below }; // condition is true when value is above/below

    ConditionGate(Direction d, float onThreshold, float offThreshold, double holdOnSec, double holdOffSec)
        : dir_(d), on_(onThreshold), off_(offThreshold), holdOn_(holdOnSec), holdOff_(holdOffSec) {}

    // Returns true if the state changed.
    bool update(float value, double dtSeconds) noexcept {
        const bool wants = state_ ? !crosses(value, off_, true) : crosses(value, on_, false);
        if (wants == state_) { pending_ = 0.0; timeInState_ += dtSeconds; return false; }
        pending_ += dtSeconds;
        if (pending_ < (state_ ? holdOff_ : holdOn_)) { timeInState_ += dtSeconds; return false; }
        state_ = wants;
        pending_ = 0.0;
        timeInState_ = 0.0;
        return true;
    }

    bool state() const noexcept { return state_; }
    double timeInState() const noexcept { return timeInState_; }
    void reset(bool state = false) noexcept { state_ = state; pending_ = 0.0; timeInState_ = 0.0; }
    void setThresholds(float on, float off) noexcept { on_ = on; off_ = off; }

private:
    // For "Above": on when value >= on; while on, stays on until value < off.
    bool crosses(float v, float t, bool releasing) const noexcept {
        if (dir_ == Direction::Above) return releasing ? v < t : v >= t;
        return releasing ? v > t : v <= t;
    }

    Direction dir_;
    float on_, off_;
    double holdOn_, holdOff_;
    bool state_ = false;
    double pending_ = 0.0, timeInState_ = 0.0;
};

} // namespace atmos
