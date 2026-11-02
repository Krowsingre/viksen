#pragma once
// Discrete, debounced states of the listener's surroundings, derived from the mic
// features and (when the platform provides it) device motion. These are what
// Worlds react to and what World conditions test. Control thread.

#include "engine/environment/FeatureExtractor.h"
#include "engine/reactive/ConditionGate.h"

namespace atmos {

struct EnvironmentStateSnapshot {
    bool quiet = false;
    bool loud = false;
    bool still = false;        // only meaningful when motionAvailable
    bool moving = false;
    bool motionAvailable = false;
    int loudness = 0;          // 0 (silent) .. 4 (very loud), coarse level for design
    int activity = 0;          // 0 (still) .. 4 (vigorous), from motion
};

struct EnvironmentThresholds {
    // dBFS of the smoothed mic level. Defaults suit an iPhone built-in mic in
    // measurement mode; calibrate per input route (headset mics differ).
    float quietOnDb = -58.0f, quietOffDb = -52.0f;
    float loudOnDb = -28.0f, loudOffDb = -34.0f;
    // Motion energy (see MotionAnalyzer: smoothed deviation from 1 g, in g).
    float stillOn = 0.015f, stillOff = 0.035f;
    float movingOn = 0.08f, movingOff = 0.05f;
};

class EnvironmentStates {
public:
    explicit EnvironmentStates(EnvironmentThresholds t = {});

    // micActive=false resets mic-derived states (no mic, no claims about the room).
    void update(const EnvironmentFeatures& f, bool micActive, double dtSeconds);
    void updateMotion(float motionEnergy, double dtSeconds);
    void setMotionAvailable(bool available) noexcept;
    void setThresholds(const EnvironmentThresholds& t);

    EnvironmentStateSnapshot snapshot() const noexcept;

private:
    EnvironmentThresholds t_;
    ConditionGate quiet_, loud_, still_, moving_;
    bool motionAvailable_ = false;
    int loudness_ = 0, activity_ = 0;
};

} // namespace atmos
