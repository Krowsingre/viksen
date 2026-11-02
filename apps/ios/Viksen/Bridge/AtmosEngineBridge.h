// Objective-C facade over the shared C++ engine for Swift. Pure Objective-C header
// (no C++), imported through the bridging header. All methods: main thread only.

#import <Foundation/Foundation.h>

#import "platform/ios/AtmosTypes.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AtmosRecordMode) {
    AtmosRecordModeFullMix = 1,        // what you hear
    AtmosRecordModeWithoutMic = 2,     // music + atmosphere
    AtmosRecordModeAtmosphereOnly = 3,
    AtmosRecordModeEnvironmentOnly = 4 // microphone paths (explicit consent in the UI)
};

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

@interface AtmosWorldInfo : NSObject
@property (nonatomic, copy) NSString *worldID;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *artist;
@property (nonatomic, copy) NSString *musicalKey;
@property (nonatomic, copy) NSString *tier;          // "free", "experience", "studio"
@property (nonatomic) double tempo;
@property (nonatomic) NSInteger stemCount;
@property (nonatomic) BOOL audioAvailable;           // all stem files are in this build
@property (nonatomic) BOOL userMade;                 // created/imported by the user (exportable); NO = built in
@property (nonatomic, copy, nullable) NSURL *folderURL;
@end

@interface AtmosWorldStemStatus : NSObject
@property (nonatomic, copy) NSString *title;
@property (nonatomic) float azimuth;      // degrees, 0 front, +90 right
@property (nonatomic) float elevation;    // degrees
@property (nonatomic) float distance;     // metres
@property (nonatomic) float width;        // degrees
@property (nonatomic) float gainDb;
@property (nonatomic) float peak;
@property (nonatomic, copy) NSArray<NSString *> *reasons;
@end

@interface AtmosWorldStatus : NSObject
@property (nonatomic) BOOL loaded;
@property (nonatomic) BOOL playing;
@property (nonatomic, copy) NSString *worldID;
@property (nonatomic, copy) NSString *name;
@property (nonatomic) double positionSeconds;
@property (nonatomic) double lengthSeconds;
@property (nonatomic) float master;
@property (nonatomic) uint64_t underruns;
@property (nonatomic) BOOL spatial;       // 3D headphone mode
@property (nonatomic) BOOL hrtf;          // Steam Audio HRTF (NO = simplified panner, e.g. Simulator)
@property (nonatomic) BOOL adaptive;      // the Dimension moves between parts of its song
@property (nonatomic) BOOL composed;      // no sense is steering it: playing as written
@property (nonatomic, copy) NSString *sectionTitle;   // part of the song being heard
@property (nonatomic, copy) NSString *sectionLabel;   // why ("You are moving")
@property (nonatomic, copy) NSArray<AtmosWorldStemStatus *> *stems;
@end

@protocol AtmosEngineBridgeDelegate <NSObject>
// Something outside the user's direct control changed (interruption, route, reset).
- (void)engineBridgeStateDidChange:(NSString *)reason;
@end

@interface AtmosEngineBridge : NSObject

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

- (AtmosMeterSnapshot *)meters NS_SWIFT_NAME(meters()); // also frees retired audio buffers; advances Worlds

// ---- Worlds (built-in music that reacts to the surroundings) ----
- (NSArray<AtmosWorldInfo *> *)availableWorlds;   // built-in + Documents/Dimensions
// Validates a Dimension folder (world.json + every stem present, at most 16 stems, ≤ 2 GB).
// Returns nil if valid, otherwise a user-facing reason.
- (nullable NSString *)validateDimensionFolder:(NSURL *)folder NS_SWIFT_NAME(validateDimensionFolder(_:));
- (BOOL)loadWorld:(NSString *)worldID error:(NSError **)error NS_SWIFT_NAME(loadWorld(_:));
- (void)startWorld;
- (void)stopWorld;
- (void)unloadWorld;
- (AtmosWorldStatus *)worldStatus;
// 3D (binaural) rendering of Dimension stems and the spatial canvas.
- (void)setDimensionSpatialEnabled:(BOOL)enabled;
- (void)moveStem:(NSInteger)index azimuth:(float)azimuth elevation:(float)elevation distance:(float)distance
    NS_SWIFT_NAME(moveStem(_:azimuth:elevation:distance:));
- (void)resetStemPositions;   // back to the Dimension's authored layout
@property (nonatomic, readonly) BOOL binauralHRTF;   // Steam Audio active in this build/device
// ---- Explicit session recording (24-bit WAV in Documents/Recordings) ----
- (BOOL)startRecordingMode:(AtmosRecordMode)mode toURL:(NSURL *)url error:(NSError **)error
    NS_SWIFT_NAME(startRecording(mode:to:));
- (double)stopRecording;                                // seconds recorded
@property (nonatomic, readonly) BOOL recording;
@property (nonatomic, readonly) double recordingSeconds;
@property (nonatomic, readonly) uint64_t recordingDroppedBlocks;
- (BOOL)takeRecordingInterrupted;                       // YES once after a device change ended a recording

// ---- Projects (.viksen packages: settings + copied song + current Dimension) ----
// Saves the current session into `packageURL` (created or replaced). The song file,
// if any, is copied into the package (reused when unchanged).
// `stems`: Studio stems as @{ @"url": NSURL, @"gainDb": NSNumber, @"muted": NSNumber } (may be empty).
- (BOOL)saveProjectAtURL:(NSURL *)packageURL name:(NSString *)name songURL:(nullable NSURL *)songURL
             dimensionID:(nullable NSString *)dimensionID stems:(NSArray<NSDictionary<NSString *, id> *> *)stems
                   error:(NSError **)error
    NS_SWIFT_NAME(saveProject(at:name:song:dimension:stems:));
// Opens a package: applies its settings now; returns @{ @"song": NSURL?, @"dimension": NSString?, @"name": NSString,
// @"stems": NSArray of @{ @"url", @"gainDb", @"muted" } }.
- (nullable NSDictionary<NSString *, id> *)openProjectAtURL:(NSURL *)packageURL error:(NSError **)error
    NS_SWIFT_NAME(openProject(at:));

// ---- Studio: the user's own stems, streamed in sync (uses the World slot) ----
- (BOOL)loadStudioStems:(NSArray<NSURL *> *)urls error:(NSError **)error NS_SWIFT_NAME(loadStudioStems(_:));
- (void)studioPlay;
- (void)studioPause;
- (void)studioSeekToSeconds:(double)seconds NS_SWIFT_NAME(studioSeek(toSeconds:));
- (void)setStem:(NSInteger)index gainDb:(float)gainDb muted:(BOOL)muted NS_SWIFT_NAME(setStem(_:gainDb:muted:));

// Snapshot of the live settings (keys: busGain0..3, masterGain, limiterCeilingDb, bedEnabled,
// bedLevel, bedColourHz, micToneHz, micEchoSeconds, micFeedback, micWet, micShiftHz, micShiftMix,
// roomMemoryEnabled, roomMemoryLevel, roomMemoryPitch, looping).
- (NSDictionary<NSString *, NSNumber *> *)currentSettings;

// Exercise intensity 0...1 from Health / Apple Watch; negative = unknown.
- (void)setExerciseIntensity:(double)intensity;
@end

NS_ASSUME_NONNULL_END
