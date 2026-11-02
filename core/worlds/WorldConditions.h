#pragma once
// Context conditions a World can require (all local, no network): time of day,
// quiet/loud, still/moving, total listening time, consecutive Worlds visited, and
// the LUNAR CALENDAR (moon phase and lunar day, computed on-device). The app uses
// the lunar calendar wherever dates matter; there are no solar-calendar conditions.
// Stored in World manifests as JSON, e.g.
//   {"all": [{"type":"moonPhase","phase":"full"}, {"type":"timeWindow","from":23,"to":5}, {"type":"quiet"}]}

#include <optional>
#include <string>
#include <vector>

#include "core/serialization/Json.h"
#include "engine/reactive/EnvironmentStates.h"

namespace atmos::core {

struct ConditionContext {
    int localHour = 12;                // 0..23
    double daysSinceUnixEpochUtc = 0;  // current time, for the moon phase
    EnvironmentStateSnapshot env;
    double totalListeningSeconds = 0;
    int consecutiveWorlds = 0;
};

struct Condition {
    enum class Type { TimeWindow, Quiet, Loud, Still, Moving, MoonPhase, LunarDay, ListeningAtLeast, ConsecutiveWorldsAtLeast };
    Type type;
    // TimeWindow: from/to hour (wraps midnight). MoonPhase: a = phase index 0..7.
    // LunarDay: from/to lunar day 1..30 (wraps). Others: a = threshold.
    double a = 0, b = 0;
};

struct ConditionSet {
    std::vector<Condition> all;  // every condition must hold (empty = always)
    Json unknown = Json::array(); // conditions this build does not understand (preserved)

    bool evaluate(const ConditionContext& c) const;
    // Unknown conditions make the set unsatisfiable on this build (never "free unlock").
    bool hasUnknown() const { return !unknown.empty(); }
};

std::optional<ConditionSet> parseConditions(const Json& j, std::string* error = nullptr);
Json conditionsToJson(const ConditionSet& s);

// ---- Lunar calendar (mean synodic month; the true phase can differ by up to
// about half a day, which is ample for phase-of-the-night experiences).
constexpr double kSynodicMonthDays = 29.530588853;

enum class MoonPhaseName { New = 0, WaxingCrescent, FirstQuarter, WaxingGibbous, Full, WaningGibbous, LastQuarter, WaningCrescent };

double moonPhase(double daysSinceUnixEpochUtc);          // [0,1): 0 new, 0.5 full
double lunarAgeDays(double daysSinceUnixEpochUtc);       // [0, 29.53) days since new moon
int lunarDay(double daysSinceUnixEpochUtc);              // 1..30 (day 1 starts at new moon)
MoonPhaseName moonPhaseName(double daysSinceUnixEpochUtc); // eight named phases
const char* moonPhaseId(MoonPhaseName p);                // "new", "waxingCrescent", ...
std::optional<MoonPhaseName> moonPhaseFromId(const std::string& id);
bool isFullMoon(double daysSinceUnixEpochUtc, double toleranceDays = 1.0);

} // namespace atmos::core
