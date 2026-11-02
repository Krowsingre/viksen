#include "engine/reactive/EnvironmentStates.h"

#include <algorithm>

namespace atmos {

using D = ConditionGate::Direction;

EnvironmentStates::EnvironmentStates(EnvironmentThresholds t)
    : t_(t),
      quiet_(D::Below, t.quietOnDb, t.quietOffDb, 1.5, 0.4),
      loud_(D::Above, t.loudOnDb, t.loudOffDb, 1.0, 2.0),
      still_(D::Below, t.stillOn, t.stillOff, 3.0, 0.5),
      moving_(D::Above, t.movingOn, t.movingOff, 2.0, 3.0) {}

void EnvironmentStates::setThresholds(const EnvironmentThresholds& t) {
    t_ = t;
    quiet_.setThresholds(t.quietOnDb, t.quietOffDb);
    loud_.setThresholds(t.loudOnDb, t.loudOffDb);
    still_.setThresholds(t.stillOn, t.stillOff);
    moving_.setThresholds(t.movingOn, t.movingOff);
}

void EnvironmentStates::update(const EnvironmentFeatures& f, bool micActive, double dt) {
    if (!micActive) {
        quiet_.reset(false);
        loud_.reset(false);
        loudness_ = 0;
        return;
    }
    quiet_.update(f.levelDb, dt);
    loud_.update(f.levelDb, dt);
    // Coarse level: 0 below quiet threshold, 4 above loud threshold, 1-3 in between.
    const float span = std::max(1.0f, t_.loudOnDb - t_.quietOnDb);
    const float x = (f.levelDb - t_.quietOnDb) / span;
    loudness_ = f.levelDb <= t_.quietOnDb ? 0 : f.levelDb >= t_.loudOnDb ? 4 : 1 + std::clamp(static_cast<int>(x * 3.0f), 0, 2);
}

void EnvironmentStates::updateMotion(float energy, double dt) {
    if (!motionAvailable_) return;
    still_.update(energy, dt);
    moving_.update(energy, dt);
    activity_ = energy < t_.stillOn ? 0 : energy < t_.movingOff ? 1 : energy < t_.movingOn ? 2 : energy < 3 * t_.movingOn ? 3 : 4;
}

void EnvironmentStates::setMotionAvailable(bool available) noexcept {
    motionAvailable_ = available;
    if (!available) { still_.reset(false); moving_.reset(false); activity_ = 0; }
}

EnvironmentStateSnapshot EnvironmentStates::snapshot() const noexcept {
    EnvironmentStateSnapshot s;
    s.quiet = quiet_.state();
    s.loud = loud_.state();
    s.motionAvailable = motionAvailable_;
    s.still = motionAvailable_ && still_.state();
    s.moving = motionAvailable_ && moving_.state();
    s.loudness = loudness_;
    s.activity = activity_;
    return s;
}

} // namespace atmos
