// Objective-C facade over the shared C++ engine for Swift. Pure Objective-C header
// (no C++), imported through the bridging header. All methods: main thread only.

#import <Foundation/Foundation.h>

#import "platform/ios/IOSAudioSession.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AtmosBusId) {
    AtmosBusIdMusic = 0,
    AtmosBusIdAtmosphere = 1,
    AtmosBusIdMicRaw = 2,
    AtmosBusIdMicTransformed = 3,
};

@interface AtmosMeterSnapshot : NSObject
@property (nonatomic) float outputPeakLeft;
@property (nonatomic) float outputPeakRight;
@property (nonatomic, copy) NSArray<NSNumber *> *busPeaks;   // indexed by AtmosBusId
@property (nonatomic) float limiterGain;                     // 1 = no reduction
@property (nonatomic) double positionSeconds;
@property (nonatomic) double durationSeconds;
@property (nonatomic) BOOL playing;
@property (nonatomic) BOOL micActive;                        // engine is consuming mic input right now
@property (nonatomic) BOOL panicked;
@property (nonatomic) uint64_t underruns;
@property (nonatomic) float environmentLevelDb;
@property (nonatomic) BOOL environmentSilent;
@property (nonatomic) uint32_t transientCount;
// Debounced environment states (see engine/reactive/EnvironmentStates).
@property (nonatomic) BOOL quiet;
@property (nonatomic) BOOL loud;
@property (nonatomic) BOOL still;
@property (nonatomic) BOOL moving;
@property (nonatomic) BOOL motionAvailable;
@property (nonatomic) NSInteger loudness;   // 0..4
@property (nonatomic) NSInteger activity;   // 0..4
@property (nonatomic) NSInteger roomMemoryVoices;
// Lunar calendar (computed on-device).
@property (nonatomic, copy) NSString *moonPhaseId;   // "new", "waxingCrescent", ... "full" ...
@property (nonatomic) NSInteger lunarDay;            // 1..30
@end

@protocol AtmosEngineBridgeDelegate <NSObject>
// Something outside the user's direct control changed (interruption, route, reset).
- (void)engineBridgeStateDidChange:(NSString *)reason;
@end

@interface AtmosEngineBridge : NSObject <AtmosAudioSessionDelegate>

@property (nonatomic, weak, nullable) id<AtmosEngineBridgeDelegate> delegate;
@property (nonatomic, readonly) BOOL audioRunning;
@property (nonatomic, readonly) BOOL micEnabled;
@property (nonatomic, readonly) double sampleRate;
@property (nonatomic, readonly, nullable) NSString *loadedTrackName;
@property (nonatomic, readonly) AtmosRouteInfo *route;
@property (nonatomic, readonly) AtmosMicPermission micPermission;

- (BOOL)startAudioWithError:(NSError **)error NS_SWIFT_NAME(startAudio());
- (void)stopAudio;

// Decodes off the main thread; completion on main thread.
- (void)loadTrackAtURL:(NSURL *)url completion:(void (^)(BOOL ok, NSString *_Nullable error))completion
    NS_SWIFT_NAME(loadTrack(at:completion:));
- (void)unloadTrack;
- (void)play;
- (void)pause;
- (void)seekToSeconds:(double)seconds NS_SWIFT_NAME(seek(toSeconds:));
- (void)setLooping:(BOOL)looping;

- (void)setGain:(float)gain forBus:(AtmosBusId)bus NS_SWIFT_NAME(setGain(_:for:));
- (float)gainForBus:(AtmosBusId)bus NS_SWIFT_NAME(gain(for:));
- (void)setMuted:(BOOL)muted forBus:(AtmosBusId)bus NS_SWIFT_NAME(setMuted(_:for:));
- (void)setMasterGain:(float)gain;
- (void)setLimiterCeilingDb:(float)db;

- (void)setAtmosphereBedEnabled:(BOOL)enabled;
- (void)setAtmosphereBedLevel:(float)level;
- (void)setAtmosphereBedColourHz:(float)hz;

// Requests permission if needed, reconfigures the session and restarts audio I/O.
- (void)setMicEnabled:(BOOL)enabled completion:(void (^)(BOOL ok, NSString *_Nullable error))completion
    NS_SWIFT_NAME(setMicEnabled(_:completion:));
- (void)setMicToneHz:(float)hz;
- (void)setMicEchoSeconds:(float)seconds;
- (void)setMicFeedback:(float)feedback;
- (void)setMicWet:(float)wet;
- (void)setMicShiftHz:(float)hz;
- (void)setMicShiftMix:(float)mix;

// Room memory: replays recent seconds of the room (RAM only; wiped when the mic stops).
- (void)setRoomMemoryEnabled:(BOOL)enabled;
- (void)setRoomMemoryLevel:(float)level;
- (void)setRoomMemoryPitch:(float)semitones;

// Head direction from head-tracking headphones, degrees (+ = turned right).
- (void)setListenerYawDegrees:(float)degrees;

// Motion sensing from the accelerometer (drives the still/moving states).
@property (nonatomic, readonly) BOOL motionSensingAvailable;
@property (nonatomic, readonly) BOOL motionSensingEnabled;
- (void)setMotionSensingEnabled:(BOOL)enabled;

- (void)panic;        // immediate silence
- (void)clearPanic;

- (AtmosMeterSnapshot *)meters NS_SWIFT_NAME(meters()); // also frees retired audio buffers
@end

NS_ASSUME_NONNULL_END
