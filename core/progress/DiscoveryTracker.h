#pragma once
// Lunar-phase discovery (owner decisions #59, #62): a Dimension's Phase is discovered
// after 3 minutes of audible listening to that Dimension's song for the real current
// phase, during that phase. Time is credited only to the Dimension being heard, counts
// within one lunation (minutes from an earlier moon cycle do not carry over), need not
// be continuous, and pauses if the device clock jumps backwards. Persistence and sync
// are the caller's job (an event log, P-2). Control thread.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include "core/worlds/WorldConditions.h"

namespace atmos::core {

struct PhaseDiscovery {
    std::string dimensionId;
    MoonPhaseName phase;
    double discoveredAtDaysUtc;   // days since 1970-01-01 (UTC)
};

class DiscoveryTracker {
public:
    static constexpr double kQualifyingSeconds = 180.0;

    // Phases already discovered (from storage), so they are never reported twice.
    void markDiscovered(const std::string& dimensionId, MoonPhaseName phase);
    bool isDiscovered(const std::string& dimensionId, MoonPhaseName phase) const;

    // Call regularly (e.g. once a second). `songPhase`: the lunar phase the playing song
    // belongs to (nullopt for songs that are not lunar-phase songs). `audible`: the
    // Dimension is playing and not muted or paused. Returns a discovery when one happens.
    std::optional<PhaseDiscovery> update(double nowDaysUtc, double dtSeconds, const std::string& dimensionId,
                                         std::optional<MoonPhaseName> songPhase, bool audible);

    // Qualifying seconds so far for this Dimension in the current phase (for UI hints).
    double progressSeconds(const std::string& dimensionId, double nowDaysUtc) const;

private:
    static long lunation(double daysUtc);   // index of the lunar cycle containing this moment
    std::set<std::pair<std::string, int>> discovered_;
    std::map<std::tuple<std::string, int, long>, double> seconds_;   // (dimension, phase, lunation)
    double lastNow_ = -1e18;
    double clockFloor_ = -1e18;   // after a backwards clock jump, nothing counts until time passes this
};

} // namespace atmos::core
