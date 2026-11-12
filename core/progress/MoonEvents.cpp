#include "core/progress/MoonEvents.h"

#include <cmath>
#include <map>
#include <mutex>

namespace atmos::core {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;
constexpr double kSynodic = 29.530588853;
constexpr double kEarthRadiusKm = 6378.14;
constexpr double kMoonRadiusKm = 1737.4;
constexpr double kAuKm = 149597870.7;

double norm360(double a) { a = std::fmod(a, 360.0); return a < 0 ? a + 360.0 : a; }
double wrap180(double a) { a = norm360(a); return a >= 180.0 ? a - 360.0 : a; }
double centuries(double days) { return (days + 2440587.5 - 2451545.0) / 36525.0; }

struct Ecliptic { double lon, lat, distKm; };   // degrees, degrees, km

// The Moon's geocentric ecliptic position (Meeus ch. 47, the main periodic terms:
// a few arc seconds and some tens of km).
Ecliptic moonPosition(double days) {
    const double T = centuries(days);
    const double Lp = norm360(218.3164477 + 481267.88123421 * T);
    const double D = norm360(297.8501921 + 445267.1114034 * T) * kRad;
    const double M = norm360(357.5291092 + 35999.0502909 * T) * kRad;
    const double Mp = norm360(134.9633964 + 477198.8675055 * T) * kRad;
    const double F = norm360(93.2720950 + 483202.0175233 * T) * kRad;
    const double A1 = norm360(119.75 + 131.849 * T) * kRad;
    const double A2 = norm360(53.09 + 479264.290 * T) * kRad;
    const double A3 = norm360(313.45 + 481266.484 * T) * kRad;
    const double E = 1.0 - 0.002516 * T - 0.0000074 * T * T;
    const double L = Lp * kRad;
    using std::sin; using std::cos;
    double sl = 6288774 * sin(Mp) + 1274027 * sin(2 * D - Mp) + 658314 * sin(2 * D) + 213618 * sin(2 * Mp) -
                185116 * E * sin(M) - 114332 * sin(2 * F) + 58793 * sin(2 * D - 2 * Mp) + 57066 * E * sin(2 * D - M - Mp) +
                53322 * sin(2 * D + Mp) + 45758 * E * sin(2 * D - M) - 40923 * E * sin(M - Mp) - 34720 * sin(D) -
                30383 * E * sin(M + Mp) + 15327 * sin(2 * D - 2 * F) - 12528 * sin(Mp + 2 * F) + 10980 * sin(Mp - 2 * F) +
                10675 * sin(4 * D - Mp) + 10034 * sin(3 * Mp) + 8548 * sin(4 * D - 2 * Mp) - 7888 * E * sin(2 * D + M - Mp) -
                6766 * E * sin(2 * D + M) - 5163 * sin(D - Mp) + 4987 * E * sin(D + M) + 4036 * E * sin(2 * D - M + Mp) +
                3994 * sin(2 * D + 2 * Mp) + 3861 * sin(4 * D) + 3665 * sin(2 * D - 3 * Mp) - 2689 * E * sin(M - 2 * Mp) -
                2602 * sin(2 * D - Mp + 2 * F) + 2390 * E * sin(2 * D - M - 2 * Mp) - 2348 * sin(D + Mp) +
                2236 * E * E * sin(2 * D - 2 * M) - 2120 * E * sin(M + 2 * Mp) - 2069 * E * E * sin(2 * M);
    sl += 3958 * sin(A1) + 1962 * sin(L - F) + 318 * sin(A2);
    const double sr =
        -20905355 * cos(Mp) - 3699111 * cos(2 * D - Mp) - 2955968 * cos(2 * D) - 569925 * cos(2 * Mp) +
        48888 * E * cos(M) - 3149 * cos(2 * F) + 246158 * cos(2 * D - 2 * Mp) - 152138 * E * cos(2 * D - M - Mp) -
        170733 * cos(2 * D + Mp) - 204586 * E * cos(2 * D - M) - 129620 * E * cos(M - Mp) + 108743 * cos(D) +
        104755 * E * cos(M + Mp) + 10321 * cos(2 * D - 2 * F) + 79661 * cos(Mp - 2 * F) - 34782 * cos(4 * D - Mp) -
        23210 * cos(3 * Mp) - 21636 * cos(4 * D - 2 * Mp) + 24208 * E * cos(2 * D + M - Mp) + 30824 * E * cos(2 * D + M) -
        8379 * cos(D - Mp) - 16675 * E * cos(D + M) - 12831 * E * cos(2 * D - M + Mp) - 10445 * cos(2 * D + 2 * Mp) -
        11650 * cos(4 * D) + 14403 * cos(2 * D - 3 * Mp) - 7003 * E * cos(M - 2 * Mp) + 10056 * E * cos(2 * D - M - 2 * Mp) +
        6322 * cos(D + Mp) - 9884 * E * E * cos(2 * D - 2 * M);
    double sb = 5128122 * sin(F) + 280602 * sin(Mp + F) + 277693 * sin(Mp - F) + 173237 * sin(2 * D - F) +
                55413 * sin(2 * D - Mp + F) + 46271 * sin(2 * D - Mp - F) + 32573 * sin(2 * D + F) + 17198 * sin(2 * Mp + F) +
                9266 * sin(2 * D + Mp - F) + 8822 * sin(2 * Mp - F) + 8216 * E * sin(2 * D - M - F) +
                4324 * sin(2 * D - 2 * Mp - F) + 4200 * sin(2 * D + Mp + F) - 3359 * E * sin(2 * D + M - F) +
                2463 * E * sin(2 * D - M - Mp + F) + 2211 * E * sin(2 * D - M + F) + 2065 * E * sin(2 * D - M - Mp - F) -
                1870 * E * sin(M - Mp - F) + 1828 * sin(4 * D - Mp - F) - 1794 * E * sin(M + F) - 1749 * sin(3 * F) -
                1565 * E * sin(M - Mp + F) - 1491 * sin(D + F) - 1475 * E * sin(M + Mp + F) - 1410 * E * sin(M + Mp - F) -
                1344 * E * sin(M - F) - 1335 * sin(D - F) + 1107 * sin(3 * Mp + F) + 1021 * sin(4 * D - F) +
                833 * sin(4 * D - Mp + F);
    sb += -2235 * sin(L) + 382 * sin(A3) + 175 * sin(A1 - F) + 175 * sin(A1 + F) + 127 * sin(L - Mp) - 115 * sin(L + Mp);
    return {norm360(Lp + sl / 1e6), sb / 1e6, 385000.56 + sr / 1000.0};
}

// The Sun's apparent ecliptic longitude and distance (Meeus ch. 25, low precision: ~0.01°).
Ecliptic sunPosition(double days) {
    const double T = centuries(days);
    const double L0 = 280.46646 + 36000.76983 * T + 0.0003032 * T * T;
    const double M = (357.52911 + 35999.05029 * T - 0.0001537 * T * T) * kRad;
    const double e = 0.016708634 - 0.000042037 * T;
    const double C = (1.914602 - 0.004817 * T) * std::sin(M) + (0.019993 - 0.000101 * T) * std::sin(2 * M) +
                     0.000289 * std::sin(3 * M);
    const double nu = M + C * kRad;
    const double R = 1.000001018 * (1 - e * e) / (1 + e * std::cos(nu));
    const double omega = (125.04 - 1934.136 * T) * kRad;
    return {norm360(L0 + C - 0.00569 - 0.00478 * std::sin(omega)), 0.0, R * kAuKm};
}

double obliquity(double days) {
    const double T = centuries(days);
    return (23.439291 - 0.0130042 * T + 0.00256 * std::cos((125.04 - 1934.136 * T) * kRad)) * kRad;
}

struct Equatorial { double ra, dec; };   // radians

Equatorial toEquatorial(const Ecliptic& p, double eps) {
    const double l = p.lon * kRad, b = p.lat * kRad;
    return {std::atan2(std::sin(l) * std::cos(eps) - std::tan(b) * std::sin(eps), std::cos(l)),
            std::asin(std::sin(b) * std::cos(eps) + std::cos(b) * std::sin(eps) * std::sin(l))};
}

double siderealDegrees(double days, double longitude) {
    const double T = centuries(days);
    const double jd = days + 2440587.5;
    return norm360(280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * T * T + longitude);
}

double altitudeOf(const Equatorial& q, double days, double lat, double lon) {
    const double H = siderealDegrees(days, lon) * kRad - q.ra;
    const double phi = lat * kRad;
    return std::asin(std::sin(phi) * std::sin(q.dec) + std::cos(phi) * std::cos(q.dec) * std::cos(H)) / kRad;
}

// The Moon seen from the listener's place (parallax applied; Meeus ch. 40, sea level).
Equatorial topocentricMoon(double days, double lat, double lon, double* distKm) {
    const Ecliptic m = moonPosition(days);
    const Equatorial g = toEquatorial(m, obliquity(days));
    const double sinPi = kEarthRadiusKm / m.distKm;
    const double phi = lat * kRad;
    const double H = siderealDegrees(days, lon) * kRad - g.ra;
    const double rc = std::cos(phi), rs = std::sin(phi);
    const double dra = std::atan2(-rc * sinPi * std::sin(H), std::cos(g.dec) - rc * sinPi * std::cos(H));
    const double dec = std::atan2((std::sin(g.dec) - rs * sinPi) * std::cos(dra), std::cos(g.dec) - rc * sinPi * std::cos(H));
    if (distKm) *distKm = m.distKm * (1 - sinPi * (rs * std::sin(g.dec) + rc * std::cos(g.dec) * std::cos(H)));
    return {g.ra + dra, dec};
}

double separation(const Equatorial& a, const Equatorial& b) {
    const double c = std::sin(a.dec) * std::sin(b.dec) + std::cos(a.dec) * std::cos(b.dec) * std::cos(a.ra - b.ra);
    return std::acos(std::max(-1.0, std::min(1.0, c))) / kRad;
}

double elongation(double days) { return norm360(moonPosition(days).lon - sunPosition(days).lon); }

// Local calendar month (year * 12 + month) of a moment.
long localMonth(double days, int utcOffsetMinutes) {
    const long z = static_cast<long>(std::floor(days + utcOffsetMinutes / 1440.0)) + 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const long doe = z - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    const long m = mp < 10 ? mp + 3 : mp - 9;
    const long y = yoe + era * 400 + (m <= 2 ? 1 : 0);
    return y * 12 + m;
}

// Lunar eclipse geometry (Danjon's enlargement of Earth's shadow, Meeus ch. 54): angles in degrees.
struct ShadowGeometry { double sep, umbra, penumbra, moonRadius; };
ShadowGeometry shadowAt(double days) {
    const Ecliptic m = moonPosition(days), s = sunPosition(days);
    const double dl = (m.lon - (s.lon + 180.0)) * kRad, b = m.lat * kRad;
    const double sep = std::acos(std::max(-1.0, std::min(1.0, std::cos(b) * std::cos(dl)))) / kRad;
    const double piM = std::asin(kEarthRadiusKm / m.distKm) / kRad;
    const double sM = std::asin(kMoonRadiusKm / m.distKm) / kRad;
    const double rAu = s.distKm / kAuKm;
    const double piS = 8.794 / 3600.0 / rAu, sS = 959.63 / 3600.0 / rAu;
    return {sep, 1.02 * (0.998340 * piM - sS + piS), 1.02 * (0.998340 * piM + sS + piS), sM};
}

// Per full moon: the kind of lunar eclipse (or none), from the closest approach of the
// Moon to the shadow's centre. Cached: the search runs once per full moon.
std::optional<MoonEvent> lunarEclipseKind(double fullDays) {
    static std::mutex mu;
    static std::map<long, std::optional<MoonEvent>> cache;
    const long key = std::lround(fullDays * 24);
    {
        std::lock_guard<std::mutex> lock(mu);
        if (auto it = cache.find(key); it != cache.end()) return it->second;
    }
    double best = 1e9; ShadowGeometry g{};
    for (double x = fullDays - 0.3; x <= fullDays + 0.3; x += 1.0 / 720.0) {   // every 2 minutes
        const ShadowGeometry s = shadowAt(x);
        if (s.sep < best) { best = s.sep; g = s; }
    }
    std::optional<MoonEvent> kind;
    if (g.sep < g.umbra - g.moonRadius) kind = MoonEvent::TotalLunarEclipse;
    else if (g.sep < g.umbra + g.moonRadius) kind = MoonEvent::PartialLunarEclipse;
    else if (g.sep < g.penumbra + g.moonRadius) kind = MoonEvent::PenumbralLunarEclipse;
    std::lock_guard<std::mutex> lock(mu);
    cache[key] = kind;
    return kind;
}

// Per new moon: whether the eclipse is central (total, annular or hybrid) somewhere on
// Earth: the shadow axis passes within Earth's radius (|gamma| < 0.9972).
bool centralSolarEclipse(double newDays) {
    static std::mutex mu;
    static std::map<long, bool> cache;
    const long key = std::lround(newDays * 24);
    {
        std::lock_guard<std::mutex> lock(mu);
        if (auto it = cache.find(key); it != cache.end()) return it->second;
    }
    double gamma = 1e9;
    for (double x = newDays - 0.3; x <= newDays + 0.3; x += 1.0 / 720.0) {
        const Ecliptic m = moonPosition(x), s = sunPosition(x);
        const double dl = (m.lon - s.lon) * kRad, b = m.lat * kRad;
        const double sep = std::acos(std::max(-1.0, std::min(1.0, std::cos(b) * std::cos(dl)))) / kRad;
        const double piM = std::asin(kEarthRadiusKm / m.distKm) / kRad;
        const double piS = 8.794 / 3600.0 / (s.distKm / kAuKm);
        gamma = std::min(gamma, sep / (piM - piS));
    }
    const bool central = gamma < 0.9972;
    std::lock_guard<std::mutex> lock(mu);
    cache[key] = central;
    return central;
}

constexpr double kMoonRiseAltitude = -0.3;   // centre, topocentric, refraction ignored (~ upper limb on the horizon)
constexpr double kNightWindow = 0.75;         // days either side of the exact moment: "the closest night"

} // namespace

const char* moonEventId(MoonEvent e) {
    static const char* ids[] = {"supermoon", "risingSupermoon", "micromoon", "blueMoon", "blackMoon", "harvestMoon",
                                "huntersMoon", "totalLunarEclipse", "partialLunarEclipse", "penumbralLunarEclipse",
                                "solarEclipse"};
    return ids[static_cast<int>(e)];
}

const char* moonEventTitle(MoonEvent e) {
    static const char* titles[] = {"Supermoon", "Rising Supermoon", "Micromoon", "Blue Moon", "Black Moon",
                                   "Harvest Moon", "Hunter's Moon", "Total Lunar Eclipse", "Partial Lunar Eclipse",
                                   "Penumbral Lunar Eclipse", "Solar Eclipse"};
    return titles[static_cast<int>(e)];
}

std::optional<MoonEvent> moonEventFromId(const std::string& id) {
    for (int i = 0; i < kMoonEventCount; ++i)
        if (id == moonEventId(static_cast<MoonEvent>(i))) return static_cast<MoonEvent>(i);
    return std::nullopt;
}

double moonDistanceKm(double daysUtc) { return moonPosition(daysUtc).distKm; }
std::optional<MoonEvent> lunarEclipseAt(double fullMoonDaysUtc) { return lunarEclipseKind(fullMoonDaysUtc); }
bool centralSolarEclipseAt(double newMoonDaysUtc) { return centralSolarEclipse(newMoonDaysUtc); }

double nearestSyzygy(double daysUtc, bool full) {
    const double target = full ? 180.0 : 0.0;
    double t = daysUtc - wrap180(elongation(daysUtc) - target) / (360.0 / kSynodic);
    for (int i = 0; i < 8; ++i) {
        const double d = wrap180(elongation(t) - target);
        t -= d / 12.19;
        if (std::fabs(d) < 1e-6) break;
    }
    return t;
}

double equinoxNear(double daysUtc, bool march) {
    const long ym = localMonth(daysUtc, 0);
    const long year = (ym - 1) / 12;
    // Days from 1970-01-01 to 20 March / 22 September of that year (civil calendar).
    auto daysFromCivil = [](long y, long m, long d) {
        y -= m <= 2;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return static_cast<double>(era * 146097 + doe - 719468);
    };
    double t = march ? daysFromCivil(year, 3, 20) : daysFromCivil(year, 9, 22);
    const double target = march ? 0.0 : 180.0;
    for (int i = 0; i < 6; ++i) t -= wrap180(sunPosition(t).lon - target) / 0.9856;
    return t;
}

double moonAltitude(double daysUtc, double latitude, double longitude) {
    return altitudeOf(topocentricMoon(daysUtc, latitude, longitude, nullptr), daysUtc, latitude, longitude);
}

double sunAltitude(double daysUtc, double latitude, double longitude) {
    return altitudeOf(toEquatorial(sunPosition(daysUtc), obliquity(daysUtc)), daysUtc, latitude, longitude);
}

std::vector<MoonEvent> activeMoonEvents(const MoonEventContext& c) {
    const double t = c.daysUtc;
    const bool located = c.latitude && c.longitude;
    bool on[kMoonEventCount] = {};

    // Full-moon events.
    const double F = nearestSyzygy(t, true);
    const double distF = moonDistanceKm(F);
    if (c.night && std::fabs(t - F) <= kNightWindow) {
        if (distF < kSupermoonKm) on[static_cast<int>(MoonEvent::Supermoon)] = true;
        if (distF > kMicromoonKm) on[static_cast<int>(MoonEvent::Micromoon)] = true;
        const double prevF = nearestSyzygy(F - kSynodic, true);
        if (localMonth(prevF, c.utcOffsetMinutes) == localMonth(F, c.utcOffsetMinutes))
            on[static_cast<int>(MoonEvent::BlueMoon)] = true;
        // Harvest: the full moon nearest the autumn equinox (March in the south); Hunter's: the next.
        const bool south = c.latitude && *c.latitude < 0;
        if (std::fabs(F - equinoxNear(F, south)) <= kSynodic / 2) on[static_cast<int>(MoonEvent::HarvestMoon)] = true;
        if (std::fabs(prevF - equinoxNear(prevF, south)) <= kSynodic / 2) on[static_cast<int>(MoonEvent::HuntersMoon)] = true;
    }
    // Rising Supermoon: the first hour after the supermoon rises here.
    if (located && distF < kSupermoonKm && std::fabs(t - F) <= 1.25) {
        const double now = moonAltitude(t, *c.latitude, *c.longitude);
        const double before = moonAltitude(t - 1.0 / 24.0, *c.latitude, *c.longitude);
        if (now > kMoonRiseAltitude && before <= kMoonRiseAltitude) on[static_cast<int>(MoonEvent::RisingSupermoon)] = true;
    }
    // Lunar eclipse, while under way and the Moon above the horizon here.
    if (located && std::fabs(t - F) <= 0.3) {
        if (const auto kind = lunarEclipseKind(F)) {
            const ShadowGeometry s = shadowAt(t);
            if (s.sep < s.penumbra + s.moonRadius && moonAltitude(t, *c.latitude, *c.longitude) > kMoonRiseAltitude)
                on[static_cast<int>(*kind)] = true;
        }
    }
    // New-moon events.
    const double N = nearestSyzygy(t, false);
    if (c.night && std::fabs(t - N) <= kNightWindow) {
        const double prevN = nearestSyzygy(N - kSynodic, false);
        if (localMonth(prevN, c.utcOffsetMinutes) == localMonth(N, c.utcOffsetMinutes))
            on[static_cast<int>(MoonEvent::BlackMoon)] = true;
    }
    // Solar eclipse (central somewhere), while the Moon covers part of the Sun here.
    if (located && std::fabs(t - N) <= 0.3 && centralSolarEclipse(N)) {
        double moonKm = 0;
        const Equatorial moon = topocentricMoon(t, *c.latitude, *c.longitude, &moonKm);
        const Ecliptic sunE = sunPosition(t);
        const Equatorial sun = toEquatorial(sunE, obliquity(t));
        const double sunRadius = 959.63 / 3600.0 / (sunE.distKm / kAuKm);
        const double moonRadius = std::asin(kMoonRadiusKm / moonKm) / kRad;
        if (separation(moon, sun) < sunRadius + moonRadius && sunAltitude(t, *c.latitude, *c.longitude) > kMoonRiseAltitude)
            on[static_cast<int>(MoonEvent::SolarEclipse)] = true;
    }

    // Rarest first.
    static const MoonEvent order[] = {
        MoonEvent::SolarEclipse, MoonEvent::TotalLunarEclipse, MoonEvent::PartialLunarEclipse,
        MoonEvent::PenumbralLunarEclipse, MoonEvent::BlueMoon, MoonEvent::BlackMoon, MoonEvent::RisingSupermoon,
        MoonEvent::HarvestMoon, MoonEvent::HuntersMoon, MoonEvent::Supermoon, MoonEvent::Micromoon};
    std::vector<MoonEvent> out;
    for (MoonEvent e : order)
        if (on[static_cast<int>(e)]) out.push_back(e);
    return out;
}

} // namespace atmos::core
