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

+ (NSString *)currentPhaseId { return @(moonPhaseId(moonPhaseName(nowDaysUtc()))); }

@end
