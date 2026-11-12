#include <cmath>

#include "core/progress/MoonEvents.h"
#include "core/progress/SecretTracker.h"
#include "tests/TestHarness.h"

using namespace atmos::core;

// Reference values are from an independent ephemeris (PyEphem); times are days since
// 1970-01-01 UTC.
namespace {
bool has(const std::vector<MoonEvent>& v, MoonEvent e) {
    for (auto x : v) if (x == e) return true;
    return false;
}
MoonEventContext at(double days, bool night, int offsetMinutes = 0) {
    MoonEventContext c;
    c.daysUtc = days;
    c.night = night;
    c.utcOffsetMinutes = offsetMinutes;
    return c;
}
MoonEventContext at(double days, bool night, int offsetMinutes, double lat, double lon) {
    MoonEventContext c = at(days, night, offsetMinutes);
    c.latitude = lat;
    c.longitude = lon;
    return c;
}
} // namespace

TEST_CASE("moon/full moons, distances and equinoxes match the ephemeris") {
    const double full = nearestSyzygy(20781.3, true);                 // 24 Nov 2026, 14:53 UTC
    CHECK(std::fabs(full - 20781.620139) * 1440 < 5);
    CHECK(moonDistanceKm(full) < kSupermoonKm);                        // 360,765 km: a supermoon
    CHECK(std::fabs(moonDistanceKm(full) - 360765) < 100);
    CHECK(moonDistanceKm(nearestSyzygy(20869.9, true)) > kSupermoonKm); // Feb 2027: not
    const double sept2027 = equinoxNear(21076, false);                 // 23 Sep 2027 ~06:02 UTC
    CHECK(std::fabs(sept2027 - 21084.25) < 0.05);
}

TEST_CASE("moon/eclipse kinds: lunar by greatest phase, solar when central") {
    CHECK(lunarEclipseAt(nearestSyzygy(21549.70, true)) == MoonEvent::TotalLunarEclipse);    // 31 Dec 2028
    CHECK(lunarEclipseAt(nearestSyzygy(20693.17, true)) == MoonEvent::PartialLunarEclipse);  // 28 Aug 2026
    CHECK(lunarEclipseAt(nearestSyzygy(20869.97, true)) == MoonEvent::PenumbralLunarEclipse); // 20 Feb 2027
    CHECK(!lunarEclipseAt(nearestSyzygy(20899.45, true)).has_value());                       // 22 Mar 2027: none
    CHECK(centralSolarEclipseAt(nearestSyzygy(21032.42, false)));                             // 2 Aug 2027 total
    CHECK(!centralSolarEclipseAt(nearestSyzygy(21563.72, false)));                            // 14 Jan 2029 partial only
}

TEST_CASE("moon/events are on only where and when they can be seen") {
    // Total lunar eclipse, 31 Dec 2028 16:52 UTC: the Moon is up in Tokyo, down in New York.
    CHECK(has(activeMoonEvents(at(21549.702778, true, 540, 35.7, 139.7)), MoonEvent::TotalLunarEclipse));
    CHECK(!has(activeMoonEvents(at(21549.702778, true, -300, 40.7, -74.0)), MoonEvent::TotalLunarEclipse));
    CHECK(!has(activeMoonEvents(at(21549.702778, true, 540)), MoonEvent::TotalLunarEclipse));   // no location
    // Total solar eclipse, 2 Aug 2027 ~10:07 UTC: seen from Luxor, not from Sydney.
    CHECK(has(activeMoonEvents(at(21032.421528, false, 120, 25.7, 32.6)), MoonEvent::SolarEclipse));
    CHECK(!has(activeMoonEvents(at(21032.421528, false, 600, -33.9, 151.2)), MoonEvent::SolarEclipse));
    // Rising Supermoon over New York: the Moon rises ~21:17 UTC on 24 Nov 2026.
    CHECK(has(activeMoonEvents(at(20781.900694, true, -300, 40.7, -74.0)), MoonEvent::RisingSupermoon));
    CHECK(!has(activeMoonEvents(at(20781.949306, true, -300, 40.7, -74.0)), MoonEvent::RisingSupermoon));
    // The supermoon itself: after dark only.
    CHECK(has(activeMoonEvents(at(20781.949306, true, -300)), MoonEvent::Supermoon));
    CHECK(!has(activeMoonEvents(at(20781.62, false)), MoonEvent::Supermoon));
}

TEST_CASE("moon/Blue and Black Moons follow the local month; Harvest follows the hemisphere") {
    // 31 Dec 2028 16:48 UTC is the second full moon of December in New York...
    CHECK(has(activeMoonEvents(at(21550.166667, true, -300)), MoonEvent::BlueMoon));
    // ...but already 1 January at UTC+14, where it is the month's first.
    CHECK(!has(activeMoonEvents(at(21549.5, true, 14 * 60)), MoonEvent::BlueMoon));
    CHECK(has(activeMoonEvents(at(21061.875, true, 60)), MoonEvent::BlackMoon));   // 31 Aug 2027
    CHECK(has(activeMoonEvents(at(21076.875, true, 60, 51.5, -0.1)), MoonEvent::HarvestMoon));    // 15 Sep 2027, London
    CHECK(!has(activeMoonEvents(at(21076.875, true, 600, -33.9, 151.2)), MoonEvent::HarvestMoon)); // not in Sydney
    CHECK(has(activeMoonEvents(at(20899.416667, true, 600, -33.9, 151.2)), MoonEvent::HarvestMoon)); // 22 Mar 2027 there
    CHECK(has(activeMoonEvents(at(21106.875, true, 60)), MoonEvent::HuntersMoon));  // 15 Oct 2027
}

TEST_CASE("moon/secrets: three minutes during the event, per Dimension, once") {
    SecretTracker t;
    const std::vector<MoonEvent> on = {MoonEvent::Supermoon};
    double now = 20781.9;
    std::optional<SecretDiscovery> found;
    for (int i = 0; i < 179 && !found; ++i) found = t.update(now += 1.0 / 86400, 1.0, "viksen", on, true);
    CHECK(!found);
    found = t.update(now += 1.0 / 86400, 1.0, "viksen", on, true);
    REQUIRE(found.has_value());
    CHECK(found->event == MoonEvent::Supermoon);
    CHECK(!t.update(now, 1.0, "viksen", on, true));                    // found once
    CHECK(!t.update(now, 1.0, "the-shatter-gardens", on, true));       // each Dimension on its own
    CHECK(!t.update(now, 1.0, "the-shatter-gardens", on, false));      // silence does not count
    // Time from an earlier occurrence does not carry over.
    for (int i = 0; i < 100; ++i) t.update(now += 1.0 / 86400, 1.0, "x", on, true);
    now += 30;
    for (int i = 0; i < 100; ++i) CHECK(!t.update(now += 1.0 / 86400, 1.0, "x", on, true));
}
