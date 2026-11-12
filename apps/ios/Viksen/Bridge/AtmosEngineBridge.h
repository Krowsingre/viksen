// Objective-C facade over the shared C++ engine for Swift. Pure Objective-C header
// (no C++), imported through the bridging header. All methods: main thread only.

#import <Foundation/Foundation.h>

#import "platform/ios/AtmosTypes.h"

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
// Lunar calendar (computed on-device).
@property (nonatomic, copy) NSString *moonPhaseId;   // "new", "waxingCrescent", ... "full" ...
@property (nonatomic) NSInteger lunarDay;            // 1..30
@end

@interface AtmosWorldInfo : NSObject
@property (nonatomic, copy) NSString *worldID;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *artist;
@property (nonatomic, copy) NSString *musicalKey;
@property (nonatomic, copy) NSString *tier;          // "free", "standard" or "special" (how it is sold)
@property (nonatomic) double tempo;
@property (nonatomic) NSInteger stemCount;
@property (nonatomic) BOOL audioAvailable;           // all stem files are in this build
@property (nonatomic) NSInteger songCount;           // songs in this Dimension (1 or more)
@property (nonatomic, copy) NSString *album;          // album the songs come from ("" when unknown)
@property (nonatomic, copy) NSString *albumLink;      // fixed Apple Music URL ("" = look it up)
@property (nonatomic) BOOL albumOnAppleMusic;         // the album is live on Apple Music
@property (nonatomic, copy) NSArray<NSString *> *songTitles;
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
@property (nonatomic, copy) NSString *songTitle;      // song being heard (a Dimension can hold several)
@property (nonatomic) NSInteger songCount;
@property (nonatomic, copy) NSString *songLunarPhaseId;   // lunar phase the playing song belongs to ("" if none)
@property (nonatomic, copy) NSArray<AtmosWorldStemStatus *> *stems;
@end

@protocol AtmosEngineBridgeDelegate <NSObject>
// Something outside the user's direct control changed (interruption, route, reset).
- (void)engineBridgeStateDidChange:(NSString *)reason;
@end

@interface AtmosEngineBridge : NSObject

/// After dark by the listener's chosen rule: lunar Dimensions play tonight's lunar song.
@property (nonatomic) BOOL night;
/// Headphones are in: contextual mixing is on. Without them songs play as written.
@property (nonatomic) BOOL headphones;
/// It is raining (weather on): Dimensions with a rain song play it.
@property (nonatomic) BOOL raining;
/// Windy / cloudy (weather on): by day, Dimensions with a windy or cloudy song play it.
@property (nonatomic) BOOL windy;
@property (nonatomic) BOOL cloudy;
@property (nonatomic) BOOL snowing;
@property (nonatomic) BOOL foggy;
/// Just after night ends / just before it begins: Dimensions with a dawn or dusk song play it.
@property (nonatomic) BOOL dawn;
/// Moon events on now where the listener is, rarest first: a Dimension's hidden song for
/// one of them plays first.
@property (nonatomic, copy) NSArray<NSString *> *moonEvents;
@property (nonatomic) BOOL dusk;
/// "Adaptive music" for the current output: off plays songs as written.
@property (nonatomic) BOOL adaptive;
@property (nonatomic, weak, nullable) id<AtmosEngineBridgeDelegate> delegate;
@property (nonatomic, readonly) BOOL audioRunning;
@property (nonatomic, readonly) BOOL micEnabled;
@property (nonatomic, readonly) double sampleRate;
@property (nonatomic, readonly) AtmosRouteInfo *route;
@property (nonatomic, readonly) AtmosMicPermission micPermission;

- (BOOL)startAudioWithError:(NSError **)error NS_SWIFT_NAME(startAudio());
- (void)stopAudio;

- (void)setGain:(float)gain forBus:(AtmosBusId)bus NS_SWIFT_NAME(setGain(_:for:));
- (void)setLimiterCeilingDb:(float)db;
// The loop point (song end back to its start): YES = the jump transition (echo/reverb
// wash and sweep), NO = a clean 8 ms crossfade. Applies at once.
- (void)setLoopTransitionFx:(BOOL)on;

// Requests permission if needed, reconfigures the session and restarts audio I/O.
- (void)setMicEnabled:(BOOL)enabled completion:(void (^)(BOOL ok, NSString *_Nullable error))completion
    NS_SWIFT_NAME(setMicEnabled(_:completion:));
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
- (BOOL)loadWorld:(NSString *)worldID error:(NSError **)error NS_SWIFT_NAME(loadWorld(_:));
// Same, with the Dimension's audio somewhere else (downloaded tracks): stem file
// name (as in world.json) -> local URL. Files not in the map are read from the folder.
// Songs whose audio is not on the device are left out; at least one must be present.
- (BOOL)loadWorld:(NSString *)worldID audioFiles:(nullable NSDictionary<NSString *, NSURL *> *)files error:(NSError **)error
    NS_SWIFT_NAME(loadWorld(_:audioFiles:));
// Stem file names of every song in a Dimension (for locating downloaded audio).
- (NSArray<NSString *> *)stemFilesForWorld:(NSString *)worldID NS_SWIFT_NAME(stemFiles(forWorld:));
// The songs (tracks) of a Dimension: @{ @"id": song id, @"title": NSString, @"files": NSArray<NSString *> }.
// Single-song Dimensions have one song whose id is the Dimension id.
- (NSArray<NSDictionary<NSString *, id> *> *)songsForWorld:(NSString *)worldID NS_SWIFT_NAME(songs(forWorld:));
- (void)startWorld;
- (void)stopWorld;
- (void)unloadWorld;
- (AtmosWorldStatus *)worldStatus;
// 3D (binaural) rendering of Dimension stems.
- (void)setDimensionSpatialEnabled:(BOOL)enabled;
@property (nonatomic, readonly) BOOL binauralHRTF;   // Steam Audio active in this build/device
// Exercise intensity 0...1 from Health / Apple Watch; negative = unknown.
- (void)setExerciseIntensity:(double)intensity;
@end

NS_ASSUME_NONNULL_END
