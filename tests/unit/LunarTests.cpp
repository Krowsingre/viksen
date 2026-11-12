// The lunar calendar against published phase instants (UTC).

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "core/progress/DiscoveryTracker.h"
#include "core/progress/ProgressLog.h"
#include "core/worlds/WorldConditions.h"
#include "core/worlds/WorldManifest.h"
#include "tests/TestHarness.h"

using namespace atmos::core;

namespace {
// Days since 1970-01-01 for a UTC civil date and time (H. Hinnant's days_from_civil).
double daysUtc(int y, int m, int d, int hh, int mm) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    const double days = era * 146097.0 + static_cast<double>(doe) - 719468.0;
    return days + (hh + mm / 60.0) / 24.0;
}
double angleDiff(double a, double b) { return std::remainder(a - b, 360.0); }
} // namespace

TEST_CASE("worlds/lunar: phase instants match published times within minutes") {
    struct Instant { int y, m, d, hh, mm; double elongation; const char* what; };
    const Instant cases[] = {
        {2024, 1, 11, 11, 57, 0, "new moon"},
        {2024, 1, 25, 17, 54, 180, "full moon"},
        {2024, 4, 8, 18, 21, 0, "new moon (total solar eclipse)"},
        {2025, 1, 29, 12, 36, 0, "new moon"},
        {2025, 2, 5, 8, 2, 90, "first quarter"},
        {2025, 2, 20, 17, 32, 270, "last quarter"},
        {2025, 3, 14, 6, 55, 180, "full moon (total lunar eclipse)"},
    };
    for (const auto& c : cases) {
        const double e = moonElongationDegrees(daysUtc(c.y, c.m, c.d, c.hh, c.mm));
        const double err = angleDiff(e, c.elongation);
        // The Moon gains ~12.2 degrees a day on the Sun: 0.2 degrees is about 24 minutes.
        if (std::fabs(err) >= 0.2) std::printf("%s %04d-%02d-%02d: off by %.3f degrees\n", c.what, c.y, c.m, c.d, err);
        CHECK(std::fabs(err) < 0.2);
    }
}

TEST_CASE("worlds/lunar: named phases are octants centred on the principal phases") {
    const double full = daysUtc(2025, 3, 14, 6, 55);
    CHECK(moonPhaseName(full) == MoonPhaseName::Full);
    CHECK(moonPhaseName(full + 1.5) == MoonPhaseName::Full);        // inside the ~3.7-day window
    CHECK(moonPhaseName(full + 2.5) == MoonPhaseName::WaningGibbous);
    CHECK(moonPhaseName(full - 2.5) == MoonPhaseName::WaxingGibbous);
    const double newMoon = daysUtc(2025, 1, 29, 12, 36);
    CHECK(moonPhaseName(newMoon) == MoonPhaseName::New);
    CHECK(moonPhaseName(newMoon + 3.0) == MoonPhaseName::WaxingCrescent);
    CHECK(lunarDay(newMoon + 0.2) == 1);
    CHECK(lunarDay(newMoon + 14.9) >= 15);          // the Moon's speed varies: day 15 or 16
    CHECK(lunarDay(newMoon + 14.9) <= 16);
}


TEST_CASE("worlds/discovery: 3 audible minutes in the right phase, per Dimension, once") {
    DiscoveryTracker t;
    const double full = daysUtc(2025, 3, 14, 6, 55);
    const auto F = MoonPhaseName::Full;
    double now = full;
    auto tick = [&](const std::string& dim, std::optional<MoonPhaseName> song, bool audible, int seconds) {
        std::optional<PhaseDiscovery> found;
        for (int i = 0; i < seconds; ++i) {
            now += 1.0 / 86400.0;
            if (auto d = t.update(now, 1.0, dim, song, audible)) found = d;
        }
        return found;
    };
    CHECK(!tick("core", F, false, 600));                      // muted or paused: nothing
    CHECK(!tick("core", MoonPhaseName::New, true, 600));      // a song of another phase: nothing
    CHECK(!tick("core", F, true, 120));                       // 2 minutes: not yet
    CHECK(!tick("albumA", F, true, 120));                     // another Dimension keeps its own count
    auto d = tick("core", F, true, 61);                       // 3 minutes in total (not continuous)
    REQUIRE(d.has_value());
    CHECK(d->dimensionId == "core");
    CHECK(d->phase == F);
    CHECK(t.isDiscovered("core", F));
    CHECK(!t.isDiscovered("albumA", F));                      // never transferred to another Dimension
    CHECK(!tick("core", F, true, 600));                       // never reported twice
    CHECK(tick("albumA", F, true, 61).has_value());           // albumA reaches its own 3 minutes
}

TEST_CASE("worlds/discovery: minutes from an earlier moon cycle do not carry over; clock tampering pauses") {
    DiscoveryTracker t;
    const auto F = MoonPhaseName::Full;
    double now = daysUtc(2025, 3, 14, 6, 55);
    for (int i = 0; i < 150; ++i) { now += 1.0 / 86400.0; t.update(now, 1.0, "core", F, true); }
    now = daysUtc(2025, 4, 13, 0, 22);                        // next full moon (2025-04-13 00:22 UTC)
    CHECK(t.progressSeconds("core", now) == 0.0);
    std::optional<PhaseDiscovery> d;
    for (int i = 0; i < 150; ++i) { now += 1.0 / 86400.0; if (auto x = t.update(now, 1.0, "core", F, true)) d = x; }
    CHECK(!d.has_value());                                    // 150 + 150 seconds across two cycles is not 3 minutes in one
    // Clock set back an hour: nothing counts until real time passes the latest moment seen.
    const double latest = now;
    now -= 1.0 / 24.0;
    for (int i = 0; i < 300; ++i) { now += 1.0 / 86400.0; if (auto x = t.update(now, 1.0, "core", F, true)) d = x; }
    CHECK(!d.has_value());
    CHECK(now < latest);
}

TEST_CASE("worlds/songs carry an optional lunar phase") {
    auto j = Json::parse(R"({"format":"atmos.world","schemaVersion":1,"id":"d","name":"D","lunarPhase":"full",
                             "stems":[{"id":"a","file":"a.wav"}]})");
    auto w = parseWorld(j);
    REQUIRE(w.has_value());
    REQUIRE(w->lunarPhase.has_value());
    CHECK(*w->lunarPhase == MoonPhaseName::Full);
    auto back = parseWorld(worldToJson(*w));
    REQUIRE(back.has_value());
    CHECK(back->lunarPhase == w->lunarPhase);
    j["lunarPhase"] = "blue";
    CHECK(!parseWorld(j).has_value());
}


TEST_CASE("worlds/progress log: idempotent, per Dimension, union merge keeps the earliest time") {
    ProgressLog a, b;
    auto phase = [](const char* dim, const char* p, double at) { return ProgressEvent{ProgressKind::PhaseDiscovered, dim, p, at}; };
    CHECK(a.add(phase("core", "full", 100)));
    CHECK(!a.add(phase("core", "full", 200)));                // repeat: not new, earliest time kept
    CHECK(a.add(phase("albumA", "full", 150)));               // same phase, other Dimension: separate
    CHECK(a.discoveredPhases("core").size() == 1);
    CHECK(a.discoveredPhases("albumA").size() == 1);
    CHECK(a.discoveredPhases("albumB").empty());
    // Another device discovered the same Full Moon earlier, plus a New Moon.
    CHECK(b.add(phase("core", "full", 90)));
    CHECK(b.add(phase("core", "new", 120)));
    CHECK(a.merge(b) == 1);                                   // only the New Moon is new
    CHECK(b.merge(a) == 1);                                   // and albumA reaches b
    CHECK(a.size() == b.size());
    for (const auto& e : a.events()) if (e.id() == "phase:core:full") CHECK(e.atDaysUtc == 90.0);
    // Merging again changes nothing (no duplicates).
    CHECK(a.merge(b) == 0);
    // Survives a round trip; malformed entries are skipped.
    Json j = a.toJson();
    j["events"].push_back({{"kind", "phase"}, {"dimension", "core"}, {"subject", "blue-moon"}, {"at", 1}});
    j["events"].push_back({{"kind", "unknown"}, {"dimension", "core"}, {"subject", "x"}, {"at", 1}});
    auto back = ProgressLog::fromJson(j);
    CHECK(back.size() == a.size());
}

TEST_CASE("worlds/progress log: lunar journey, five secrets, global secrets without totals") {
    ProgressLog log;
    const char* phases[] = {"new", "waxingCrescent", "firstQuarter", "waxingGibbous", "full", "waningGibbous", "lastQuarter"};
    double t = 10;
    for (auto p : phases) log.add({ProgressKind::PhaseDiscovered, "core", p, t++});
    CHECK(!log.lunarJourneyComplete("core"));
    CHECK(log.dimensionSecretsFound("core") == 0);
    log.add({ProgressKind::PhaseDiscovered, "core", "waningCrescent", 50});
    CHECK(log.lunarJourneyComplete("core"));
    CHECK(*log.lunarJourneyCompletedAt("core") == 50.0);   // when the 8th phase was found
    CHECK(!log.lunarJourneyComplete("albumA"));            // never transferred
    CHECK(!log.allDimensionSecretsFound("core"));          // journey complete does not mean all secrets
    for (int i = 0; i < 10; ++i) log.add({ProgressKind::DimensionSecret, "core", "s" + std::to_string(i), 60.0 + i});
    CHECK(log.dimensionSecretsFound("core") == 10);
    CHECK(!log.allDimensionSecretsFound("core"));
    log.add({ProgressKind::DimensionSecret, "core", "s10", 70.5});
    CHECK(log.dimensionSecretsFound("core") == 11);          // one per moon event
    CHECK(log.allDimensionSecretsFound("core"));
    CHECK(log.dimensionSecretsFound("albumA") == 0);
    // Global secrets have no Dimension and no total.
    log.add({ProgressKind::GlobalSecret, "core", "g-moonrise", 70});   // a dimension given by mistake is dropped
    log.add({ProgressKind::GlobalSecret, "", "g-moonrise", 80});
    CHECK(log.globalSecretsFound() == 1);
    // Invalid events are refused.
    CHECK(!log.add({ProgressKind::PhaseDiscovered, "", "full", 1}));
    CHECK(!log.add({ProgressKind::DimensionSecret, "core", "", 1}));
}
