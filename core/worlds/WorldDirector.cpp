#include "core/worlds/WorldDirector.h"

#include <algorithm>
#include <cmath>

namespace atmos::core {

WorldDirector::WorldDirector(const WorldManifest& w) : w_(w) {
    current_.assign(w.stems.size(), kSilenceDb);
    target_.assign(w.stems.size(), kSilenceDb);
    reasons_.assign(w.stems.size(), {});
}

void WorldDirector::computeTargets(const ConditionContext& c) {
    for (size_t i = 0; i < w_.stems.size(); ++i) {
        float best = -1e9f;
        bool any = false;
        reasons_[i].clear();
        for (const auto& r : w_.rules) {
            if (r.stem != w_.stems[i].id || !r.when.inputsAvailable(c)) continue;
            const bool met = r.when.evaluate(c);
            const float v = met ? r.gainDb : r.otherwiseDb;
            if (met && !r.label.empty()) reasons_[i].push_back(r.label);
            best = any ? std::max(best, v) : v;
            any = true;
        }
        target_[i] = std::max(kSilenceDb, any ? best : w_.stems[i].baseGainDb);
    }
}

void WorldDirector::update(const ConditionContext& c, double dt) {
    computeTargets(c);
    // Constant-rate fade in dB: a full silence <-> 0 dB change takes fadeSeconds.
    const float step = static_cast<float>(-kSilenceDb / std::max(0.1f, w_.fadeSeconds) * dt);
    for (size_t i = 0; i < current_.size(); ++i) {
        const float d = target_[i] - current_[i];
        current_[i] += std::clamp(d, -step, step);
    }
}

void WorldDirector::snapToTargets(const ConditionContext& c) {
    computeTargets(c);
    current_ = target_;
}

float WorldDirector::linearGain(int stem) const {
    const float db = current_[static_cast<size_t>(stem)];
    return db <= kSilenceDb + 0.01f ? 0.0f : std::pow(10.0f, db / 20.0f);
}

} // namespace atmos::core
