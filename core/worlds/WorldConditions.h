#pragma once
// Context conditions a World can require (all local, no network): time of day,
// quiet/loud, still/moving, total listening time, consecutive Worlds visited, and
// the LUNAR CALENDAR (moon phase and lunar day, computed on-device). The app uses
// the lunar calendar wherever dates matter; there are no solar-calendar conditions.
// Stored in World manifests as JSON, e.g.
//   {"all": [{"type":"moonPhase","phase":"full"}, {"type":"timeWindow","from":23,"to":5}, {"type":"quiet"}]}

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/serialization/Json.h"
#include "engine/reactive/EnvironmentStates.h"

namespace atmos::core {

enum class MoonPhaseName { New = 0, WaxingCrescent, FirstQuarter, WaxingGibbous, Full, WaningGibbous, LastQuarter, WaningCrescent };

// ---- The lunar calendar the app lives by (owner, 2026-11-18), as calendars print it:
// - New Moon, First Quarter, Full Moon and Last Quarter are moments; the local calendar
//   day that holds the moment carries that name (one day each).
// - The days between are Waxing Crescent, Waxing Gibbous, Waning Gibbous and Waning
//   Crescent (usually 5-7 days each).
// - Lunar day: the local date of the new moon is day 1, counting up at each local
//   midnight (29 or 30 days a month, none skipped or repeated). The number is only a
//   label; the app acts on the phases and events (the listener can show it as the
//   Moon's age, day 0 at new moon).
// - A night belongs to the date it began on: after midnight, until morning, it is still
//   the evening's date (so a phase's songs play its whole night).
// Dates are the listener's local calendar (time zone and daylight saving): the app gives
// LocalDateFn; without one, UTC dates are used.
using LocalDateFn = std::function<long(double daysUtc)>;   // a moment -> its local date (days since 1970-01-01)
LocalDateFn fixedOffsetDates(int utcOffsetMinutes);
long daysFromCivil(int year, int month, int day);         // a calendar date -> days since 1970-01-01

enum class MainPhase { New = 0, FirstQuarter, Full, LastQuarter };
double mainPhaseNear(double daysUtc, MainPhase p);         // the exact moment of that phase closest to daysUtc

struct LunarDate {
    long date = 0;                       // local date (days since 1970-01-01)
    MoonPhaseName phase = MoonPhaseName::New;
    std::optional<MainPhase> main;       // the main phase whose moment falls on this date
    double mainAtDaysUtc = 0;            // ...and that moment
    int day = 1;                         // lunar day, traditional (new moon's date = 1), 1..30
    long lunation = 0;                   // the local date of this lunar month's new moon
};
LunarDate lunarDateOn(long localDate, const LocalDateFn& dateOf = {});
// The date the app lives on at this moment (the night rule above).
long livingDate(double daysUtc, bool night, int localHour, const LocalDateFn& dateOf = {});

struct ConditionContext {
    int localHour = 12;                // 0..23
    bool micActive = false;            // mic-derived states are meaningful
    std::optional<double> intensity;   // exercise intensity 0..1 when Health/Watch is on
    double daysSinceUnixEpochUtc = 0;  // current time, for the moon phase
    LocalDateFn localDateOf;           // the listener's calendar (empty: UTC)
    std::optional<LunarDate> lunar;    // today's lunar date, when the app has worked it out already
    bool night = false;
    // Headphones in. The music adapts on every output (owner, 2026-11-05); headphones add
    // the 3D presentation and the room blend. Microphone moods (quiet, loud) need the mic,
    // which only runs with headphones, so on speakers they are simply unavailable.
    bool headphones = true;
    // The listener's "Adaptive music" setting for the current output (separate for
    // headphones and speakers). Off: songs play their parts and mix as written (day,
    // night and rain songs still change).
    bool adaptive = true;
    bool raining = false;              // it is raining where the listener is (weather, when on; thunderstorms too)
    bool storming = false;             // a thunderstorm where the listener is (weather, when on)
    bool windy = false;                // windy where the listener is (weather, when on)
    bool cloudy = false;               // cloudy where the listener is (weather, when on)
    bool snowing = false;              // snowing (weather, when on)
    bool foggy = false;                // fog or mist (weather, when on)
    bool dawn = false;                 // just after night ends (the listener's night rule)
    bool dusk = false;                 // just before night begins
    std::vector<std::string> moonEvents;   // moon events on now where the listener is, rarest first (MoonEvents.h)                // after dark, by the listener's chosen rule (fixed hours or sunset–sunrise)
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

// The lunar date the context lives on (ctx.lunar if given, else worked out from the
// context's time, night and calendar).
LunarDate lunarDateOf(const ConditionContext& c);

// ---- The Moon itself (astronomical: Meeus; phase moments within a few minutes).
constexpr double kSynodicMonthDays = 29.530588853;

double moonElongationDegrees(double daysSinceUnixEpochUtc); // Moon minus Sun longitude, [0,360)
double moonPhase(double daysSinceUnixEpochUtc);          // [0,1): 0 new, 0.5 full (the Moon's shape now)
// The calendar on that moment's UTC date (no time zone, no night rule): for tools and
// tests; the app uses lunarDateOf(context).
int lunarDay(double daysSinceUnixEpochUtc);              // 1..30
MoonPhaseName moonPhaseName(double daysSinceUnixEpochUtc);
const char* moonPhaseId(MoonPhaseName p);                // "new", "waxingCrescent", ...
std::optional<MoonPhaseName> moonPhaseFromId(const std::string& id);

} // namespace atmos::core
