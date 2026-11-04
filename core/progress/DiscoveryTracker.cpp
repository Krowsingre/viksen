#include "core/progress/DiscoveryTracker.h"

#include <cmath>

namespace atmos::core {

void DiscoveryTracker::markDiscovered(const std::string& dimensionId, MoonPhaseName phase) {
    discovered_.insert({dimensionId, static_cast<int>(phase)});
}

bool DiscoveryTracker::isDiscovered(const std::string& dimensionId, MoonPhaseName phase) const {
    return discovered_.count({dimensionId, static_cast<int>(phase)}) > 0;
}

long DiscoveryTracker::lunation(double daysUtc) {
    // The most recent new moon, rounded to whole days, numbered in synodic months.
    const double lastNewMoon = daysUtc - moonPhase(daysUtc) * kSynodicMonthDays;
    return static_cast<long>(std::lround(lastNewMoon / kSynodicMonthDays));
}

std::optional<PhaseDiscovery> DiscoveryTracker::update(double now, double dt, const std::string& dimensionId,
                                                       std::optional<MoonPhaseName> songPhase, bool audible) {
    // Clock moved backwards by more than a minute: hold credit until real time catches up.
    if (now + 60.0 / 86400.0 < lastNow_) clockFloor_ = std::max(clockFloor_, lastNow_);
    lastNow_ = now;
    if (now < clockFloor_) return std::nullopt;
    if (!audible || !songPhase || dimensionId.empty() || dt <= 0) return std::nullopt;
    const MoonPhaseName current = moonPhaseName(now);
    if (*songPhase != current) return std::nullopt;          // this song belongs to another phase
    if (isDiscovered(dimensionId, current)) return std::nullopt;
    double& s = seconds_[{dimensionId, static_cast<int>(current), lunation(now)}];
    s += std::min(dt, 5.0);                                  // a long gap between calls is not listening
    if (s < kQualifyingSeconds) return std::nullopt;
    markDiscovered(dimensionId, current);
    return PhaseDiscovery{dimensionId, current, now};
}

double DiscoveryTracker::progressSeconds(const std::string& dimensionId, double now) const {
    const auto it = seconds_.find({dimensionId, static_cast<int>(moonPhaseName(now)), lunation(now)});
    return it == seconds_.end() ? 0.0 : it->second;
}

} // namespace atmos::core
