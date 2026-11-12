#include "core/progress/SecretTracker.h"

#include <algorithm>

namespace atmos::core {

std::optional<SecretDiscovery> SecretTracker::update(double nowDaysUtc, double dtSeconds, const std::string& dimensionId,
                                                     const std::vector<MoonEvent>& active, bool audible) {
    if (dimensionId.empty() || active.empty()) return std::nullopt;
    const double dt = std::clamp(dtSeconds, 0.0, 5.0);   // a long gap (sleep, clock change) adds at most a moment
    std::optional<SecretDiscovery> result;
    for (MoonEvent e : active) {
        const auto key = std::make_pair(dimensionId, static_cast<int>(e));
        if (found_.count(key)) continue;
        Progress& p = progress_[key];
        if (nowDaysUtc - p.lastOnDays > 2.0) p.seconds = 0;   // a new occurrence starts from nothing
        p.lastOnDays = nowDaysUtc;
        if (!audible) continue;
        p.seconds += dt;
        if (p.seconds >= kQualifyingSeconds && !result) {
            found_.insert(key);
            result = SecretDiscovery{dimensionId, e};
        }
    }
    return result;
}

} // namespace atmos::core
