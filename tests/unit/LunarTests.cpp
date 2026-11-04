// The lunar calendar against published phase instants (UTC).

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "core/progress/DiscoveryTracker.h"
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
