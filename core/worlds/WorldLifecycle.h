#pragma once
// Lifecycle of a running World, driven by whether its conditions currently hold.
//
//   Starting --fade in--> Active --conditions lost--> Fading (ducked, countdown)
//      Fading --conditions back--> Active
//      Fading --countdown expires--> Ending (fade out) --> Ended
//   requestExit() from any state -> Ending.  stayForever disables the countdown.
//
// Produces a gain (0..1) the host applies to the World's output, so state changes
// are always smooth. Control thread; call update() regularly (e.g. 30 Hz).

namespace atmos::core {

struct LifecycleTiming {
    double fadeInSeconds = 3.0;
    double duckGain = 0.35;          // level while conditions are lost
    double duckSeconds = 1.5;        // ramp time into/out of the duck
    double countdownSeconds = 12.0;  // time conditions may stay lost before ending
    double fadeOutSeconds = 1.5;
};

class WorldLifecycle {
public:
    enum class State { Starting, Active, Fading, Ending, Ended };

    explicit WorldLifecycle(LifecycleTiming t = {}) : t_(t) {}

    void update(bool conditionsMet, double dtSeconds);
    void requestExit();
    void setStayForever(bool stay) { stayForever_ = stay; }
    void setTiming(const LifecycleTiming& t) { t_ = t; }   // applies from the current gain onwards
    bool stayForever() const { return stayForever_; }

    State state() const { return state_; }
    double gain() const { return gain_; }
    double countdownRemaining() const;   // seconds, only meaningful while Fading
    bool finished() const { return state_ == State::Ended; }

private:
    void rampTowards(double target, double seconds, double dt);

    LifecycleTiming t_;
    State state_ = State::Starting;
    double gain_ = 0.0;
    double fadingTime_ = 0.0;
    bool stayForever_ = false;
};

const char* toString(WorldLifecycle::State s);

} // namespace atmos::core
