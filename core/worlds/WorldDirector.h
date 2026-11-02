#pragma once
// Turns a World's rules and the current context into a smoothly moving gain per
// stem. Rules whose inputs are unavailable (sensor off) are ignored, so with no
// sensors the listener hears the authored mix. Control thread, e.g. 30 Hz.

#include <string>
#include <vector>

#include "core/worlds/WorldManifest.h"

namespace atmos::core {

class WorldDirector {
public:
    static constexpr float kSilenceDb = -60.0f;

    explicit WorldDirector(const WorldManifest& w);

    void update(const ConditionContext& c, double dtSeconds);
    void snapToTargets(const ConditionContext& c);       // no fade (e.g. on start)

    int stemCount() const { return static_cast<int>(current_.size()); }
    float currentDb(int stem) const { return current_[static_cast<size_t>(stem)]; }
    float targetDb(int stem) const { return target_[static_cast<size_t>(stem)]; }
    float linearGain(int stem) const;                    // 0 at/below kSilenceDb
    void setBaseGainDb(int stem, float db);              // the authored/user level when no rule applies
    // Labels of the rules currently driving this stem (for the UI).
    const std::vector<std::string>& reasons(int stem) const { return reasons_[static_cast<size_t>(stem)]; }

private:
    void computeTargets(const ConditionContext& c);

    WorldManifest w_;
    std::vector<float> current_, target_;
    std::vector<std::vector<std::string>> reasons_;
};

} // namespace atmos::core
