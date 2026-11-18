#import "AtmosProgress.h"
#import "AtmosLunar.h"

#include "core/progress/DiscoveryTracker.h"
#include "core/progress/MoonEvents.h"
#include "core/progress/ProgressLog.h"
#include "core/progress/SecretTracker.h"

using namespace atmos::core;

static double nowDaysUtc() { return AtmosNowDaysUtc(); }
static BOOL sNight = NO;   // after dark (the listener's Night rule), set by the app every second

@implementation AtmosProgress {
    ProgressLog _log;
    DiscoveryTracker _tracker;
    SecretTracker _secrets;
}

+ (BOOL)night { return sNight; }
+ (void)setNight:(BOOL)night { sNight = night; }

- (instancetype)initWithJSON:(NSString *)json {
    if ((self = [super init])) {
        if (json.length) [self mergeJSON:json];
    }
    return self;
}

- (void)syncTracker {
    for (const auto &e : _log.events())
        if (e.kind == ProgressKind::PhaseDiscovered) {
            if (auto p = moonPhaseFromId(e.subject)) _tracker.markDiscovered(e.dimensionId, *p);
        } else if (e.kind == ProgressKind::DimensionSecret) {
            if (auto m = moonEventFromId(e.subject)) _secrets.markFound(e.dimensionId, *m);
        }
}

- (NSString *)jsonString { return @(_log.toJson().dump().c_str()); }

- (NSInteger)mergeJSON:(NSString *)json {
    try {
        const Json j = Json::parse(std::string(json.UTF8String ?: ""));
        const int added = _log.merge(ProgressLog::fromJson(j));
        [self syncTracker];
        return added;
    } catch (...) {
        return 0;   // a damaged copy never touches what we have
    }
}

- (NSString *)tickDimension:(NSString *)dimensionID songPhaseId:(NSString *)songPhaseId
                    audible:(BOOL)audible dtSeconds:(double)dt {
    const double now = nowDaysUtc();
    const auto phase = moonPhaseFromId(std::string(songPhaseId.UTF8String ?: ""));
    const auto found = _tracker.update(now, dt, AtmosLunarNow(sNight), std::string(dimensionID.UTF8String ?: ""), phase, audible);
    if (!found) return nil;
    _log.add({ProgressKind::PhaseDiscovered, found->dimensionId, moonPhaseId(found->phase), found->discoveredAtDaysUtc});
    return @(moonPhaseId(found->phase));
}

- (NSArray<NSString *> *)discoveredPhaseIds:(NSString *)dimensionID {
    NSMutableArray *out = [NSMutableArray array];
    for (auto p : _log.discoveredPhases(std::string(dimensionID.UTF8String ?: ""))) [out addObject:@(moonPhaseId(p))];
    return out;
}

- (BOOL)lunarJourneyComplete:(NSString *)dimensionID {
    return _log.lunarJourneyComplete(std::string(dimensionID.UTF8String ?: ""));
}

- (NSInteger)dimensionSecretsFound:(NSString *)dimensionID {
    return _log.dimensionSecretsFound(std::string(dimensionID.UTF8String ?: ""));
}

- (NSInteger)globalSecretsFound { return _log.globalSecretsFound(); }

- (double)phaseProgressSeconds:(NSString *)dimensionID {
    return _tracker.progressSeconds(std::string(dimensionID.UTF8String ?: ""), AtmosLunarNow(sNight));
}

- (NSDictionary<NSString *, NSDate *> *)discoveryDates:(NSString *)dimensionID {
    const std::string dim(dimensionID.UTF8String ?: "");
    NSMutableDictionary *out = [NSMutableDictionary dictionary];
    for (const auto &e : _log.events())
        if (e.kind == ProgressKind::PhaseDiscovered && e.dimensionId == dim)
            out[@(e.subject.c_str())] = [NSDate dateWithTimeIntervalSince1970:e.atDaysUtc * 86400.0];
    return out;
}

- (NSDate *)lunarJourneyCompletedAt:(NSString *)dimensionID {
    const auto at = _log.lunarJourneyCompletedAt(std::string(dimensionID.UTF8String ?: ""));
    return at ? [NSDate dateWithTimeIntervalSince1970:*at * 86400.0] : nil;
}

+ (NSDateInterval *)nextWindowForPhase:(NSString *)phaseId {
    // The lunar calendar's dates for this phase: the run of local dates now (if it is this
    // phase tonight) or the next run, from the first date's midnight to the morning after
    // the last (a night keeps its evening's date).
    const auto want = moonPhaseFromId(std::string(phaseId.UTF8String ?: ""));
    if (!want) return nil;
    const long today = AtmosLunarNow(sNight).date;
    long start = today;
    if (lunarDateOn(start, AtmosLocalDateOf).phase == *want) {
        while (start > today - 10 && lunarDateOn(start - 1, AtmosLocalDateOf).phase == *want) --start;
    } else {
        while (start < today + 32 && lunarDateOn(start, AtmosLocalDateOf).phase != *want) ++start;
        if (start >= today + 32) return nil;
    }
    long end = start;
    while (end < start + 10 && lunarDateOn(end + 1, AtmosLocalDateOf).phase == *want) ++end;
    return [[NSDateInterval alloc] initWithStartDate:AtmosStartOfLocalDate(start) endDate:AtmosStartOfLocalDate(end + 1)];
}

+ (NSString *)currentPhaseId { return @(moonPhaseId(AtmosLunarNow(sNight).phase)); }

+ (NSArray<NSString *> *)activeMoonEventsAtNight:(BOOL)night utcOffsetMinutes:(NSInteger)utcOffsetMinutes
                                        latitude:(NSNumber *)latitude longitude:(NSNumber *)longitude {
    MoonEventContext c;
    c.daysUtc = nowDaysUtc();
    c.night = night;
    c.utcOffsetMinutes = static_cast<int>(utcOffsetMinutes);
    c.localDateOf = AtmosLocalDateOf;
    c.localHour = static_cast<int>([[NSCalendar currentCalendar] component:NSCalendarUnitHour fromDate:[NSDate date]]);
    if (latitude && longitude) { c.latitude = latitude.doubleValue; c.longitude = longitude.doubleValue; }
    NSMutableArray *out = [NSMutableArray array];
    for (MoonEvent e : activeMoonEvents(c)) [out addObject:@(moonEventId(e))];
    return out;
}

+ (NSString *)moonEventTitle:(NSString *)eventId {
    const auto e = moonEventFromId(std::string(eventId.UTF8String ?: ""));
    return e ? @(moonEventTitle(*e)) : eventId;
}

- (NSString *)tickSecrets:(NSString *)dimensionID events:(NSArray<NSString *> *)events
                  audible:(BOOL)audible dtSeconds:(double)dt {
    std::vector<MoonEvent> on;
    for (NSString *ident in events)
        if (auto e = moonEventFromId(std::string(ident.UTF8String ?: ""))) on.push_back(*e);
    const double now = nowDaysUtc();
    const auto found = _secrets.update(now, dt, std::string(dimensionID.UTF8String ?: ""), on, audible);
    if (!found) return nil;
    _log.add({ProgressKind::DimensionSecret, found->dimensionId, moonEventId(found->event), now});
    return @(moonEventId(found->event));
}

- (NSDictionary<NSString *, NSDate *> *)secretDates:(NSString *)dimensionID {
    const std::string dim(dimensionID.UTF8String ?: "");
    NSMutableDictionary *out = [NSMutableDictionary dictionary];
    for (const auto &e : _log.events())
        if (e.kind == ProgressKind::DimensionSecret && e.dimensionId == dim)
            out[@(e.subject.c_str())] = [NSDate dateWithTimeIntervalSince1970:e.atDaysUtc * 86400.0];
    return out;
}

@end
