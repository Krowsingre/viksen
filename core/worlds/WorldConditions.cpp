#include "core/worlds/WorldConditions.h"

#include <algorithm>
#include <cmath>

namespace atmos::core {

namespace {
constexpr double kSynodicMonth = kSynodicMonthDays;
constexpr const char* kPhaseIds[8] = {"new", "waxingCrescent", "firstQuarter", "waxingGibbous",
                                      "full", "waningGibbous", "lastQuarter", "waningCrescent"};
// A reference new moon: 2000-01-06 18:14 UTC, in days since 1970-01-01.
constexpr double kReferenceNewMoon = 10957.0 + 5.0 + (18.0 + 14.0 / 60.0) / 24.0;

struct TypeName { Condition::Type t; const char* name; };
constexpr TypeName kNames[] = {
    {Condition::Type::TimeWindow, "timeWindow"}, {Condition::Type::Quiet, "quiet"},
    {Condition::Type::Loud, "loud"},             {Condition::Type::Still, "still"},
    {Condition::Type::Moving, "moving"},         {Condition::Type::MoonPhase, "moonPhase"},
    {Condition::Type::LunarDay, "lunarDay"},
    {Condition::Type::ListeningAtLeast, "listeningAtLeast"},
    {Condition::Type::ConsecutiveWorldsAtLeast, "consecutiveWorldsAtLeast"},
};
} // namespace

double moonPhase(double days) {
    const double p = std::fmod((days - kReferenceNewMoon) / kSynodicMonth, 1.0);
    return p < 0 ? p + 1.0 : p;
}

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
        }
        if (!ok) return false;
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
        all.push_back(o);
    }
    for (const auto& u : s.unknown) all.push_back(u);
    return Json{{"all", all}};
}

} // namespace atmos::core
