#import "AtmosProgress.h"

#include "core/progress/DiscoveryTracker.h"
#include "core/progress/ProgressLog.h"

using namespace atmos::core;

static double nowDaysUtc() { return [[NSDate date] timeIntervalSince1970] / 86400.0; }

@implementation AtmosProgress {
    ProgressLog _log;
    DiscoveryTracker _tracker;
}

- (instancetype)initWithJSON:(NSString *)json {
    if ((self = [super init])) {
        if (json.length) [self mergeJSON:json];
    }
    return self;
}

- (void)syncTracker {
    for (const auto &e : _log.events())
        if (e.kind == ProgressKind::PhaseDiscovered)
            if (auto p = moonPhaseFromId(e.subject)) _tracker.markDiscovered(e.dimensionId, *p);
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
    const auto found = _tracker.update(now, dt, std::string(dimensionID.UTF8String ?: ""), phase, audible);
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
    return _tracker.progressSeconds(std::string(dimensionID.UTF8String ?: ""), nowDaysUtc());
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
    const auto want = moonPhaseFromId(std::string(phaseId.UTF8String ?: ""));
    if (!want) return nil;
    const double step = 1.0 / 96.0;   // 15 minutes
    double t = nowDaysUtc();
    const double limit = t + 31;
    if (moonPhaseName(t) == *want) {   // in the sky now: find where this window began
        while (moonPhaseName(t - step) == *want && t > nowDaysUtc() - 5) t -= step;
    } else {
        while (moonPhaseName(t) != *want && t < limit) t += step;
        if (t >= limit) return nil;
    }
    double end = t;
    while (moonPhaseName(end) == *want && end < t + 6) end += step;
    return [[NSDateInterval alloc] initWithStartDate:[NSDate dateWithTimeIntervalSince1970:t * 86400.0]
                                             endDate:[NSDate dateWithTimeIntervalSince1970:end * 86400.0]];
}

+ (NSString *)currentPhaseId { return @(moonPhaseId(moonPhaseName(nowDaysUtc()))); }

@end
