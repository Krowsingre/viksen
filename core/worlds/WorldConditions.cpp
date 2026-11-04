#include "core/worlds/WorldConditions.h"

#include <algorithm>
#include <cmath>

namespace atmos::core {

namespace {
constexpr double kSynodicMonth = kSynodicMonthDays;
constexpr const char* kPhaseIds[8] = {"new", "waxingCrescent", "firstQuarter", "waxingGibbous",
                                      "full", "waningGibbous", "lastQuarter", "waningCrescent"};

struct TypeName { Condition::Type t; const char* name; };
constexpr TypeName kNames[] = {
    {Condition::Type::TimeWindow, "timeWindow"}, {Condition::Type::Quiet, "quiet"},
    {Condition::Type::Loud, "loud"},             {Condition::Type::Still, "still"},
    {Condition::Type::Moving, "moving"},         {Condition::Type::MoonPhase, "moonPhase"},
    {Condition::Type::LunarDay, "lunarDay"},
    {Condition::Type::ListeningAtLeast, "listeningAtLeast"},
    {Condition::Type::ConsecutiveWorldsAtLeast, "consecutiveWorldsAtLeast"},
    {Condition::Type::IntensityAtLeast, "intensityAtLeast"},
};
} // namespace

// Moon-Sun elongation from Jean Meeus, "Astronomical Algorithms" (2nd ed.): the Moon's
// longitude from the main periodic terms of chapter 47 (accurate to a few arcminutes)
// and the Sun's from chapter 25. The Moon moves about 12 degrees a day relative to the
// Sun, so phase instants come out within a few minutes. Computed from UTC (the ~70 s
// TT-UTC difference is negligible here), so every device and time zone agrees.
double moonElongationDegrees(double daysSinceUnixEpochUtc) {
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    const double jd = daysSinceUnixEpochUtc + 2440587.5;
    const double T = (jd - 2451545.0) / 36525.0;
    auto norm = [](double a) { a = std::fmod(a, 360.0); return a < 0 ? a + 360.0 : a; };
    const double Lp = norm(218.3164477 + 481267.88123421 * T);
    const double D = norm(297.8501921 + 445267.1114034 * T) * kDeg;
    const double M = norm(357.5291092 + 35999.0502909 * T) * kDeg;
    const double Mp = norm(134.9633964 + 477198.8675055 * T) * kDeg;
    const double F = norm(93.2720950 + 483202.0175233 * T) * kDeg;
    const double E = 1.0 - 0.002516 * T;
    // Main terms of the Moon's longitude, in millionths of a degree.
    const double sl =
        6288774 * std::sin(Mp) + 1274027 * std::sin(2 * D - Mp) + 658314 * std::sin(2 * D) +
        213618 * std::sin(2 * Mp) - 185116 * E * std::sin(M) - 114332 * std::sin(2 * F) +
        58793 * std::sin(2 * D - 2 * Mp) + 57066 * E * std::sin(2 * D - M - Mp) + 53322 * std::sin(2 * D + Mp) +
        45758 * E * std::sin(2 * D - M) - 40923 * E * std::sin(M - Mp) - 34720 * std::sin(D) -
        30383 * E * std::sin(M + Mp) + 15327 * std::sin(2 * D - 2 * F) - 12528 * std::sin(Mp + 2 * F) +
        10980 * std::sin(Mp - 2 * F) + 10675 * std::sin(4 * D - Mp) + 10034 * std::sin(3 * Mp) +
        8548 * std::sin(4 * D - 2 * Mp) - 7888 * E * std::sin(2 * D + M - Mp) - 6766 * E * std::sin(2 * D + M) -
        5163 * std::sin(D - Mp) + 4987 * E * std::sin(D + M) + 4036 * E * std::sin(2 * D - M + Mp) +
        3994 * std::sin(2 * D + 2 * Mp) + 3861 * std::sin(4 * D) + 3665 * std::sin(2 * D - 3 * Mp) -
        2689 * E * std::sin(M - 2 * Mp) - 2602 * std::sin(2 * D - Mp + 2 * F) + 2390 * E * std::sin(2 * D - M - 2 * Mp) -
        2348 * std::sin(D + Mp) + 2236 * E * E * std::sin(2 * D - 2 * M) - 2120 * E * std::sin(M + 2 * Mp) -
        2069 * E * E * std::sin(2 * M);
    const double moon = Lp + sl / 1e6;
    // The Sun's true longitude (aberration and nutation largely cancel in the difference).
    const double L0 = 280.46646 + 36000.76983 * T;
    const double Ms = (357.52911 + 35999.05029 * T) * kDeg;
    const double C = (1.914602 - 0.004817 * T) * std::sin(Ms) + (0.019993 - 0.000101 * T) * std::sin(2 * Ms) +
                     0.000289 * std::sin(3 * Ms);
    const double sun = L0 + C;
    return norm(moon - sun);
}

double moonPhase(double days) { return moonElongationDegrees(days) / 360.0; }

// Days since the last new moon (the Moon's elongation advances at a slightly uneven rate;
// the mean month is accurate enough for the 1..30 lunar day).
double lunarAgeDays(double days) { return moonPhase(days) * kSynodicMonth; }

int lunarDay(double days) { return std::min(30, static_cast<int>(lunarAgeDays(days)) + 1); }

MoonPhaseName moonPhaseName(double days) {
    // Each named phase spans 1/8 of the cycle, centred on its exact point.
    return static_cast<MoonPhaseName>(static_cast<int>(std::floor(moonPhase(days) * 8.0 + 0.5)) % 8);
}

const char* moonPhaseId(MoonPhaseName p) { return kPhaseIds[static_cast<int>(p)]; }

std::optional<MoonPhaseName> moonPhaseFromId(const std::string& id) {
    for (int i = 0; i < 8; ++i)
        if (id == kPhaseIds[i]) return static_cast<MoonPhaseName>(i);
    return std::nullopt;
}

bool isFullMoon(double days, double toleranceDays) {
    return std::fabs(moonPhase(days) - 0.5) * kSynodicMonth <= toleranceDays;
}

bool ConditionSet::evaluate(const ConditionContext& c) const {
    if (hasUnknown()) return false;
    for (const auto& k : all) {
        bool ok = false;
        switch (k.type) {
            case Condition::Type::TimeWindow: {
                const int from = static_cast<int>(k.a), to = static_cast<int>(k.b);
                ok = from <= to ? (c.localHour >= from && c.localHour < to) : (c.localHour >= from || c.localHour < to);
                break;
            }
            case Condition::Type::Quiet: ok = c.env.quiet; break;
            case Condition::Type::Loud: ok = c.env.loud; break;
            case Condition::Type::Still: ok = c.env.still; break;
            case Condition::Type::Moving: ok = c.env.moving; break;
            case Condition::Type::MoonPhase:
                ok = static_cast<int>(moonPhaseName(c.daysSinceUnixEpochUtc)) == static_cast<int>(k.a);
                break;
            case Condition::Type::LunarDay: {
                const int d = lunarDay(c.daysSinceUnixEpochUtc), from = static_cast<int>(k.a), to = static_cast<int>(k.b);
                ok = from <= to ? (d >= from && d <= to) : (d >= from || d <= to);
                break;
            }
            case Condition::Type::ListeningAtLeast: ok = c.totalListeningSeconds >= k.a; break;
            case Condition::Type::ConsecutiveWorldsAtLeast: ok = c.consecutiveWorlds >= static_cast<int>(k.a); break;
            case Condition::Type::IntensityAtLeast: ok = c.intensity && *c.intensity >= k.a; break;
        }
        if (!ok) return false;
    }
    return true;
}

bool ConditionSet::inputsAvailable(const ConditionContext& c) const {
    for (const auto& k : all) {
        switch (k.type) {
            case Condition::Type::Quiet:
            case Condition::Type::Loud: if (!c.micActive) return false; break;
            case Condition::Type::Still:
            case Condition::Type::Moving: if (!c.env.motionAvailable) return false; break;
            case Condition::Type::IntensityAtLeast: if (!c.intensity) return false; break;
            default: break;
        }
    }
    return true;
}

std::optional<ConditionSet> parseConditions(const Json& j, std::string* error) {
    ConditionSet set;
    if (j.is_null()) return set;
    if (!j.is_object() || !j.contains("all") || !j["all"].is_array()) {
        if (error) *error = "conditions must be {\"all\": [...]}";
        return std::nullopt;
    }
    for (const auto& c : j["all"]) {
        const std::string type = c.is_object() && c.contains("type") && c["type"].is_string() ? c["type"].get<std::string>() : "";
        bool known = false;
        for (const auto& n : kNames) {
            if (type != n.name) continue;
            Condition cond{n.t};
            if (n.t == Condition::Type::TimeWindow) {
                cond.a = c.value("from", 0.0);
                cond.b = c.value("to", 24.0);
            } else if (n.t == Condition::Type::MoonPhase) {
                const auto phase = moonPhaseFromId(c.value("phase", std::string{}));
                if (!phase) break; // unknown phase name: treated as an unknown condition below
                cond.a = static_cast<int>(*phase);
            } else if (n.t == Condition::Type::LunarDay) {
                cond.a = c.value("from", 1.0);
                cond.b = c.value("to", 30.0);
            } else if (n.t == Condition::Type::ListeningAtLeast) {
                cond.a = c.value("seconds", 0.0);
            } else if (n.t == Condition::Type::ConsecutiveWorldsAtLeast) {
                cond.a = c.value("count", 0.0);
            } else if (n.t == Condition::Type::IntensityAtLeast) {
                cond.a = c.value("value", 0.5);
            }
            set.all.push_back(cond);
            known = true;
        }
        if (!known) set.unknown.push_back(c);
    }
    return set;
}

Json conditionsToJson(const ConditionSet& s) {
    Json all = Json::array();
    for (const auto& c : s.all) {
        const char* name = "";
        for (const auto& n : kNames) if (n.t == c.type) name = n.name;
        Json o = {{"type", name}};
        if (c.type == Condition::Type::TimeWindow || c.type == Condition::Type::LunarDay) { o["from"] = c.a; o["to"] = c.b; }
        if (c.type == Condition::Type::MoonPhase) o["phase"] = moonPhaseId(static_cast<MoonPhaseName>(static_cast<int>(c.a)));
        if (c.type == Condition::Type::ListeningAtLeast) o["seconds"] = c.a;
        if (c.type == Condition::Type::ConsecutiveWorldsAtLeast) o["count"] = c.a;
        if (c.type == Condition::Type::IntensityAtLeast) o["value"] = c.a;
        all.push_back(o);
    }
    for (const auto& u : s.unknown) all.push_back(u);
    return Json{{"all", all}};
}

} // namespace atmos::core
