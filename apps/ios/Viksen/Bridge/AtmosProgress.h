#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// The listener's progression (P-2): the engine's append-only discovery log plus the
/// 3-minute Phase discovery rule. Swift persists the JSON locally and in iCloud and
/// merges copies from other devices (union, earliest time wins). Main thread only.
@interface AtmosProgress : NSObject

- (instancetype)initWithJSON:(nullable NSString *)json;
/// The whole log, for saving.
- (NSString *)jsonString;
/// Merges another device's log. Returns how many events were new here.
- (NSInteger)mergeJSON:(NSString *)json;

/// Call about once a second. `songPhaseId` is the playing song's lunar phase ("" if
/// none); `audible` means the Dimension is playing and can be heard. Returns the phase id
/// discovered by this tick, or nil.
- (nullable NSString *)tickDimension:(NSString *)dimensionID
                         songPhaseId:(NSString *)songPhaseId
                             audible:(BOOL)audible
                           dtSeconds:(double)dt;

- (NSArray<NSString *> *)discoveredPhaseIds:(NSString *)dimensionID;
- (BOOL)lunarJourneyComplete:(NSString *)dimensionID;
- (NSInteger)dimensionSecretsFound:(NSString *)dimensionID;
- (NSInteger)globalSecretsFound;
/// Qualifying seconds so far toward the current phase in this Dimension (of 180).
- (double)phaseProgressSeconds:(NSString *)dimensionID;
/// When each discovered phase was found in this Dimension (phase id → date).
- (NSDictionary<NSString *, NSDate *> *)discoveryDates:(NSString *)dimensionID;
/// When the 8th phase was found, or nil.
- (nullable NSDate *)lunarJourneyCompletedAt:(NSString *)dimensionID;
/// The next (or current) window in which the named phase is in the sky, from now.
+ (nullable NSDateInterval *)nextWindowForPhase:(NSString *)phaseId;
/// The current named lunar phase ("full", "waxingCrescent", ...).
+ (NSString *)currentPhaseId;

@end

NS_ASSUME_NONNULL_END
