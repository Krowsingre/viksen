#include "core/worlds/MusicReactor.h"

#include <algorithm>
#include <cmath>

namespace atmos::core {

MusicReactor::Role MusicReactor::defaultRole(const std::string& id) {
    for (const char* e : {"drums", "percussion", "bass", "guitar", "brass"})
        if (id == e) return Role::Energy;
    for (const char* c : {"strings", "synth", "keyboard", "woodwinds"})
        if (id == c) return Role::Calm;
    return Role::Neutral;
}

void MusicReactor::configure(const std::vector<std::string>& stemIds, const Json& react) {
    roles_.clear();
    const Json stems = react.is_object() && react.contains("stems") && react["stems"].is_object() ? react["stems"] : Json::object();
    for (const auto& id : stemIds) {
        Role r = defaultRole(id);
        if (stems.contains(id) && stems[id].is_string()) {
            const std::string s = stems[id].get<std::string>();
            r = s == "energy" ? Role::Energy : s == "calm" ? Role::Calm : Role::Neutral;
        }
        roles_.push_back(r);
    }
    depthDb_ = react.is_object() ? std::clamp(react.value("depthDb", 10.0), 0.0, 24.0) : 10.0;
    closedHz_ = react.is_object() ? std::clamp(react.value("filterClosedHz", 5000.0), 300.0, 20000.0) : 5000.0;
    energy_ = 0.5;
    neutralBlend_ = 1.0;
    engaged_ = false;
}

void MusicReactor::update(double dt, const ReactorInputs& in) {
    // Every available cue, each 0..1; the strongest wins.
    double target = 0.0;
    bool any = false;
    if (in.micActive) {
        const double room = std::clamp((in.roomLevelDb + 60.0) / 40.0, 0.0, 1.0);   // -60 dB calm .. -20 dB loud
        const double busy = std::clamp(in.onsetsPerSecond / 3.0, 0.0, 1.0);
        target = std::max(target, 0.6 * room + 0.4 * busy);
        any = true;
    }
    if (in.motionAvailable) { target = std::max(target, in.activity / 4.0); any = true; }
    if (in.intensity) { target = std::max(target, std::clamp(*in.intensity, 0.0, 1.0)); any = true; }
    engaged_ = any && in.adaptive;
    // Neutral when nothing is engaged; the change in and out of neutral takes about a second.
    neutralBlend_ += (1.0 - std::exp(-dt / 0.8)) * ((engaged_ ? 0.0 : 1.0) - neutralBlend_);
    if (engaged_) {
        const double tau = target > energy_ ? 0.3 : 2.0;   // rise fast, settle slowly
        energy_ += (1.0 - std::exp(-dt / tau)) * (target - energy_);
    }
}

float MusicReactor::cutoffHz() const {
    const double open = 20000.0;
    const double e = std::clamp(energy_ / 0.5, 0.0, 1.0);
    const double hz = std::exp(std::log(closedHz_) + e * (std::log(open) - std::log(closedHz_)));
    return static_cast<float>(hz + neutralBlend_ * (open - hz));
}

float MusicReactor::stemGain(int stem) const {
    double db = 0.0;
    switch (role(stem)) {
        case Role::Energy: db = -depthDb_ * (1.0 - energy_); break;
        case Role::Calm: {
            const double x = std::clamp((energy_ - 0.5) / 0.5, 0.0, 1.0);
            db = -0.5 * depthDb_ * x * x * (3 - 2 * x);   // smoothstep: pads recede only near full energy
            break;
        }
        case Role::Neutral: break;
    }
    db *= 1.0 - neutralBlend_;
    return static_cast<float>(std::pow(10.0, db / 20.0));
}

} // namespace atmos::core
