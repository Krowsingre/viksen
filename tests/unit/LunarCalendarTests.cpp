// The lunar calendar (WorldConditions.h) against an independent reference: PyEphem's phase
// moments and the IANA time zone rules (tests/data/lunar_reference.txt), every local date
// from 2026 to 2035 in four time zones, with daylight saving.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/worlds/WorldConditions.h"
#include "tests/TestHarness.h"

using namespace atmos::core;

namespace {
struct Reference {
    std::vector<std::pair<double, int>> moments;                        // (daysUtc, kind)
    std::map<std::string, std::vector<std::pair<double, int>>> zones;   // zone -> (from daysUtc, offset minutes)
};
const Reference& reference() {
    static Reference r = [] {
        Reference out;
        std::ifstream in(std::filesystem::path(ATMOS_SOURCE_DIR) / "tests/data/lunar_reference.txt");
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream s(line);
            std::string tag;
            s >> tag;
            if (tag == "P") { int k; double t; s >> k >> t; out.moments.push_back({t, k}); }
            if (tag == "Z") { std::string z; double t; int off; s >> z >> t >> off; out.zones[z].push_back({t, off}); }
        }
        return out;
    }();
    return r;
}
LocalDateFn zoneDates(const std::string& zone) {
    const auto& changes = reference().zones.at(zone);
    return [&changes](double t) {
        int off = changes.front().second;
        for (const auto& c : changes) { if (c.first <= t) off = c.second; else break; }
        return static_cast<long>(std::floor(t + off / 1440.0));
    };
}
const char* kZones[] = {"UTC", "America/Los_Angeles", "Asia/Tokyo", "Pacific/Auckland"};
} // namespace

TEST_CASE("lunar calendar: main phase moments within minutes of PyEphem, 2025-2036") {
    const auto& r = reference();
    REQUIRE(r.moments.size() > 500);
    double worst = 0;
    for (const auto& [t, k] : r.moments) worst = std::max(worst, std::fabs(mainPhaseNear(t, static_cast<MainPhase>(k)) - t) * 1440.0);
    std::printf("    main phase moments: worst %.1f minutes from PyEphem over %zu moments\n", worst, r.moments.size());
    CHECK(worst < 6.0);
}

TEST_CASE("lunar calendar: every date 2026-2035, four time zones, matches the reference") {
    const auto& r = reference();
    static const MoonPhaseName onDay[4] = {MoonPhaseName::New, MoonPhaseName::FirstQuarter, MoonPhaseName::Full, MoonPhaseName::LastQuarter};
    static const MoonPhaseName after[4] = {MoonPhaseName::WaxingCrescent, MoonPhaseName::WaxingGibbous, MoonPhaseName::WaningGibbous, MoonPhaseName::WaningCrescent};
    const long first = daysFromCivil(2026, 1, 1), last = daysFromCivil(2035, 12, 31);
    for (const char* zone : kZones) {
        const LocalDateFn dateOf = zoneDates(zone);
        int checked = 0, close = 0, wrong = 0;
        for (long date = first; date <= last; ++date) {
            // Expected, from the reference moments. A moment within 6 minutes of a local
            // midnight could honestly fall on either date (the models differ by minutes):
            // there the app's own moment decides the date, and it is counted as "close".
            int lastKind = -1; long lastNewDate = 0; bool onThisDate = false; bool usedClose = false;
            for (const auto& [t, k] : r.moments) {
                long d = dateOf(t);
                if (dateOf(t - 6.0 / 1440.0) != dateOf(t + 6.0 / 1440.0)) {
                    d = dateOf(mainPhaseNear(t, static_cast<MainPhase>(k)));
                    if (d >= date - 31 && d <= date) usedClose = true;
                }
                if (d > date) break;
                lastKind = k;
                onThisDate = d == date;
                if (k == 0) lastNewDate = d;
            }
            if (usedClose) ++close;
            const MoonPhaseName want = onThisDate ? onDay[lastKind] : after[lastKind];
            const int wantDay = static_cast<int>(date - lastNewDate) + 1;
            const LunarDate got = lunarDateOn(date, dateOf);
            ++checked;
            if (got.phase != want || got.day != wantDay) {
                if (++wrong <= 5) std::printf("    %s date %ld: got %s day %d, want %s day %d\n", zone, date,
                                              moonPhaseId(got.phase), got.day, moonPhaseId(want), wantDay);
            }
        }
        std::printf("    %s: %d dates checked (%d near a phase within 6 minutes of midnight), %d wrong\n", zone, checked, close, wrong);
        CHECK(wrong == 0);
        CHECK(checked > 3600);
    }
}

TEST_CASE("lunar calendar: main phases last one date; lunar days run 1..29 or 30 without gaps") {
    for (const char* zone : kZones) {
        const LocalDateFn dateOf = zoneDates(zone);
        int run = 0;
        MoonPhaseName prev = MoonPhaseName::WaningCrescent;
        int prevDay = 0;
        for (long date = daysFromCivil(2026, 1, 1); date <= daysFromCivil(2035, 12, 31); ++date) {
            const LunarDate d = lunarDateOn(date, dateOf);
            const bool main = d.phase == MoonPhaseName::New || d.phase == MoonPhaseName::FirstQuarter ||
                              d.phase == MoonPhaseName::Full || d.phase == MoonPhaseName::LastQuarter;
            run = d.phase == prev ? run + 1 : 1;
            if (main) CHECK(run == 1);                             // never two days running
            else CHECK(run <= 8);
            CHECK(d.main.has_value() == main);
            if (prevDay != 0) {
                if (d.day == 1) CHECK((prevDay == 29 || prevDay == 30));
                else CHECK(d.day == prevDay + 1);
            }
            CHECK((d.day == 1) == (d.phase == MoonPhaseName::New));
            prev = d.phase;
            prevDay = d.day;
        }
    }
}

TEST_CASE("lunar calendar: a night keeps its evening's date until morning") {
    // Full moon 24 Nov 2026 14:53 UTC. In Los Angeles (UTC-8) that is the 24th.
    const LocalDateFn la = zoneDates("America/Los_Angeles");
    auto at = [&](int y, int m, int d, int hour, bool night) {
        ConditionContext c;
        c.daysSinceUnixEpochUtc = static_cast<double>(daysFromCivil(y, m, d)) + (hour + 8) / 24.0;
        c.localHour = hour;
        c.night = night;
        c.localDateOf = la;
        return lunarDateOf(c);
    };
    CHECK(at(2026, 11, 24, 21, true).phase == MoonPhaseName::Full);           // the Full Moon night
    CHECK(at(2026, 11, 25, 2, true).phase == MoonPhaseName::Full);            // after midnight: still that night
    CHECK(at(2026, 11, 25, 9, false).phase == MoonPhaseName::WaningGibbous);  // morning: the next date
    CHECK(at(2026, 11, 24, 2, true).phase == MoonPhaseName::WaxingGibbous);   // the night before
    // The new moon of 9 Nov 07:02 UTC is 8 Nov 23:02 in Los Angeles: day 1 there is the 8th.
    CHECK(at(2026, 11, 8, 23, true).day == 1);
    CHECK(at(2026, 11, 24, 21, true).day == 17);
}
