#pragma once
// Moon events found as secrets (owner, 2026-11-11). Each Dimension has one secret per
// event, found once and kept forever by listening to that Dimension for 3 minutes while
// the event is on where the listener is. Everything is computed on the device from the
// Sun's and Moon's positions (Meeus, "Astronomical Algorithms"), no tables or services.
//
// When each event is "on":
// - Supermoon / Micromoon / Blue Moon / Harvest Moon / Hunter's Moon: after dark, on the
//   night closest to that exact full moon. Black Moon: after dark, on the night of that
//   new moon. Blue and Black Moons use the listener's local calendar month. The Harvest
//   Moon is the full moon nearest the autumn equinox (March in the Southern Hemisphere,
//   when the location is known); the Hunter's Moon is the next full moon.
// - Rising Supermoon: the first hour after the supermoon rises where the listener is.
// - Lunar eclipses: while the eclipse is under way and the Moon is above the listener's
//   horizon; total, partial or penumbral by the eclipse's greatest phase.
// - Solar eclipse (total, annular or hybrid; partial-only eclipses are left out): while
//   the Moon visibly covers part of the Sun where the listener is, the Sun above the horizon.
// Events that need the sky at the listener's place (eclipses, rising supermoon) are
// never on without a location.

#include <optional>
#include <string>
#include <vector>

namespace atmos::core {

enum class MoonEvent {
    Supermoon = 0, RisingSupermoon, Micromoon, BlueMoon, BlackMoon, HarvestMoon, HuntersMoon,
    TotalLunarEclipse, PartialLunarEclipse, PenumbralLunarEclipse, SolarEclipse,
};
constexpr int kMoonEventCount = 11;

const char* moonEventId(MoonEvent e);        // "supermoon", "risingSupermoon", ...
const char* moonEventTitle(MoonEvent e);     // "Supermoon", "Rising Supermoon", ...
std::optional<MoonEvent> moonEventFromId(const std::string& id);

struct MoonEventContext {
    double daysUtc = 0;                  // days since 1970-01-01 UTC
    int utcOffsetMinutes = 0;            // the listener's local time zone, for calendar months
    bool night = false;                  // after dark, by the listener's Night rule
    std::optional<double> latitude, longitude;   // approximate location (degrees), if allowed
};

// The events on now, rarest first.
std::vector<MoonEvent> activeMoonEvents(const MoonEventContext& c);

// Building blocks, exposed for tests and the app.
double moonDistanceKm(double daysUtc);                   // centre to centre
double nearestSyzygy(double daysUtc, bool full);         // the full (or new) moon closest to this moment
double equinoxNear(double daysUtc, bool march);          // the March or September equinox of that year
// Altitude (degrees, refraction not included) of the Moon's or Sun's centre seen from here.
double moonAltitude(double daysUtc, double latitude, double longitude);
double sunAltitude(double daysUtc, double latitude, double longitude);
// The kind of lunar eclipse at this full moon, if any; whether this new moon brings a
// central (total, annular or hybrid) solar eclipse somewhere on Earth.
std::optional<MoonEvent> lunarEclipseAt(double fullMoonDaysUtc);
bool centralSolarEclipseAt(double newMoonDaysUtc);

constexpr double kSupermoonKm = 361885;   // closer than this at full: supermoon
constexpr double kMicromoonKm = 405000;   // farther than this at full: micromoon

} // namespace atmos::core
