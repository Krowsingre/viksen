// The lunar calendar on this device (core/worlds/WorldConditions.h), shared by the bridge
// and the progress tracker: the listener's local calendar (time zone with daylight
// saving) and the night rule (a night keeps its evening's date until morning).
#pragma once
#import <Foundation/Foundation.h>

#include <cmath>

#include "core/worlds/WorldConditions.h"

// A moment (days since 1970-01-01 UTC) -> its local calendar date on this device.
inline long AtmosLocalDateOf(double daysUtc) {
    NSDate *d = [NSDate dateWithTimeIntervalSince1970:daysUtc * 86400.0];
    const NSInteger offset = [[NSTimeZone localTimeZone] secondsFromGMTForDate:d];
    return static_cast<long>(std::floor(daysUtc + offset / 86400.0));
}

inline double AtmosNowDaysUtc() { return [[NSDate date] timeIntervalSince1970] / 86400.0; }

// The lunar date the app lives on now.
inline atmos::core::LunarDate AtmosLunarNow(bool night) {
    const int hour = static_cast<int>([[NSCalendar currentCalendar] component:NSCalendarUnitHour fromDate:[NSDate date]]);
    const long date = atmos::core::livingDate(AtmosNowDaysUtc(), night, hour, AtmosLocalDateOf);
    return atmos::core::lunarDateOn(date, AtmosLocalDateOf);
}

// Local midnight at the start of a local calendar date.
inline NSDate *AtmosStartOfLocalDate(long date) {
    // Days since 1970-01-01 -> year, month, day (Howard Hinnant's algorithm).
    const long z = date + 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const long doe = z - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    NSDateComponents *c = [NSDateComponents new];
    c.day = doy - (153 * mp + 2) / 5 + 1;
    c.month = mp < 10 ? mp + 3 : mp - 9;
    c.year = yoe + era * 400 + (c.month <= 2 ? 1 : 0);
    return [[NSCalendar currentCalendar] dateFromComponents:c];
}
