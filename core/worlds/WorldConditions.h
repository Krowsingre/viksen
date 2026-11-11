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
    bool micActive = false;            // mic-derived states are meaningful
    std::optional<double> intensity;   // exercise intensity 0..1 when Health/Watch is on
    double daysSinceUnixEpochUtc = 0;  // current time, for the moon phase
    bool night = false;
    // Headphones in. The music adapts on every output (owner, 2026-11-05); headphones add
    // the 3D presentation and the room blend. Microphone moods (quiet, loud) need the mic,
    // which only runs with headphones, so on speakers they are simply unavailable.
    bool headphones = true;
    // The listener's "Adaptive music" setting for the current output (separate for
    // headphones and speakers). Off: songs play their parts and mix as written (day,
    // night and rain songs still change).
    bool adaptive = true;
    bool raining = false;              // it is raining where the listener is (weather, when on)
    bool windy = false;                // windy where the listener is (weather, when on)
    bool cloudy = false;               // cloudy where the listener is (weather, when on)                // after dark, by the listener's chosen rule (fixed hours or sunset–sunrise)
    EnvironmentStateSnapshot env;
    double totalListeningSeconds = 0;
    int consecutiveWorlds = 0;
};

struct Condition {
    enum class Type { TimeWindow, Quiet, Loud, Still, Moving, MoonPhase, LunarDay, ListeningAtLeast, ConsecutiveWorldsAtLeast,
                      IntensityAtLeast };
    Type type;
    // TimeWindow: from/to hour (wraps midnight). MoonPhase: a = phase index 0..7.
    // LunarDay: from/to lunar day 1..30 (wraps). Others: a = threshold.
    double a = 0, b = 0;
};

struct ConditionSet {
    std::vector<Condition> all;  // every condition must hold (empty = always)
    Json unknown = Json::array(); // conditions this build does not understand (preserved)

    bool evaluate(const ConditionContext& c) const;
    // True when every input the set depends on is currently available (e.g. "quiet"
    // needs the mic on, "still" needs motion sensing). Used so reactive rules fall
    // back to the authored mix instead of reacting to sensors that are off.
    bool inputsAvailable(const ConditionContext& c) const;
    // Unknown conditions make the set unsatisfiable on this build (never "free unlock").
    bool hasUnknown() const { return !unknown.empty(); }
};

std::optional<ConditionSet> parseConditions(const Json& j, std::string* error = nullptr);
Json conditionsToJson(const ConditionSet& s);

// ---- Lunar calendar (astronomical: Meeus; phase instants within a few minutes, UTC).
// Each named phase is one eighth of the Moon-Sun elongation, centred on the principal
// phases (New = 337.5..22.5 degrees, Full = 157.5..202.5 degrees).
constexpr double kSynodicMonthDays = 29.530588853;

enum class MoonPhaseName { New = 0, WaxingCrescent, FirstQuarter, WaxingGibbous, Full, WaningGibbous, LastQuarter, WaningCrescent };

double moonElongationDegrees(double daysSinceUnixEpochUtc); // Moon minus Sun longitude, [0,360)
double moonPhase(double daysSinceUnixEpochUtc);          // [0,1): 0 new, 0.5 full (astronomical, UTC)
double lunarAgeDays(double daysSinceUnixEpochUtc);       // [0, 29.53) days since new moon
int lunarDay(double daysSinceUnixEpochUtc);              // 1..30 (day 1 starts at new moon)
MoonPhaseName moonPhaseName(double daysSinceUnixEpochUtc); // eight named phases
const char* moonPhaseId(MoonPhaseName p);                // "new", "waxingCrescent", ...
std::optional<MoonPhaseName> moonPhaseFromId(const std::string& id);
bool isFullMoon(double daysSinceUnixEpochUtc, double toleranceDays = 1.0);

} // namespace atmos::core
