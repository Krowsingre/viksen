#include "core/worlds/WorldLifecycle.h"

#include <algorithm>

namespace atmos::core {

void WorldLifecycle::rampTowards(double target, double seconds, double dt) {
    // Linear ramp covering the full 0..1 range in `seconds`.
    const double step = seconds > 0 ? dt / seconds : 1.0;
    gain_ = gain_ < target ? std::min(target, gain_ + step) : std::max(target, gain_ - step);
}

void WorldLifecycle::update(bool met, double dt) {
    const bool lost = !met && !stayForever_;
    switch (state_) {
        case State::Starting:
            rampTowards(1.0, t_.fadeInSeconds, dt);
            if (gain_ >= 1.0) state_ = State::Active;
            break;
        case State::Active:
            rampTowards(1.0, t_.duckSeconds, dt);
            if (lost) { state_ = State::Fading; fadingTime_ = 0.0; }
            break;
        case State::Fading:
            if (!lost) { state_ = State::Active; break; }
            fadingTime_ += dt;
            rampTowards(t_.duckGain, t_.duckSeconds, dt);
            if (fadingTime_ >= t_.countdownSeconds) state_ = State::Ending;
            break;
        case State::Ending:
            rampTowards(0.0, t_.fadeOutSeconds, dt);
            if (gain_ <= 0.0) state_ = State::Ended;
            break;
        case State::Ended:
            gain_ = 0.0;
            break;
    }
}

void WorldLifecycle::requestExit() {
    if (state_ != State::Ended) state_ = State::Ending;
}

double WorldLifecycle::countdownRemaining() const {
    return state_ == State::Fading ? std::max(0.0, t_.countdownSeconds - fadingTime_) : 0.0;
}

const char* toString(WorldLifecycle::State s) {
    switch (s) {
        case WorldLifecycle::State::Starting: return "starting";
        case WorldLifecycle::State::Active: return "active";
        case WorldLifecycle::State::Fading: return "fading";
        case WorldLifecycle::State::Ending: return "ending";
        case WorldLifecycle::State::Ended: return "ended";
    }
    return "?";
}

} // namespace atmos::core
