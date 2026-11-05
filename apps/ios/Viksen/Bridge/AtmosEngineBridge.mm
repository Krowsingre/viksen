#import "AtmosEngineBridge.h"

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#include <TargetConditionals.h>

#include <algorithm>
#include <memory>
#include <string>

#include "core/session/LiveSession.h"
#if TARGET_OS_IOS
#include "platform/ios/IOSAudioIO.h"
#import "platform/ios/IOSAudioSession.h"
#import "platform/ios/IOSMotion.h"
#else
#include "platform/desktop/MiniaudioHost.h"
#endif
#include "platform/ios/IOSStemDecoder.h"
#include "core/worlds/WorldManifest.h"
#include "core/worlds/WorldConditions.h"

using atmos::Bus;
using atmos::core::LiveSession;

#if TARGET_OS_IOS
constexpr int kMaxBlock = atmos::ios::IOSAudioIO::kMaxFramesPerSlice;
// iPhone: AVAudioSession reports interruptions and route changes to the bridge.
@interface AtmosEngineBridge () <AtmosAudioSessionDelegate>
@end
#else
constexpr int kMaxBlock = 8192;   // MiniaudioHost's largest period
#endif

@implementation AtmosWorldInfo
- (instancetype)init {
    if ((self = [super init])) { _worldID = @""; _name = @""; _artist = @""; _musicalKey = @""; _tier = @"free"; _songCount = 1; _songTitles = @[]; _album = @""; _albumLink = @""; }
    return self;
}
@end
@implementation AtmosWorldStemStatus
@end
@implementation AtmosWorldStatus
- (instancetype)init {
    if ((self = [super init])) { _worldID = @""; _name = @""; _stems = @[]; _sectionTitle = @""; _sectionLabel = @""; _songTitle = @""; _songCount = 1; _songLunarPhaseId = @""; _composed = YES; }
    return self;
}
@end

namespace {
NSURL* worldsFolder() { return [[NSBundle mainBundle] URLForResource:@"Worlds" withExtension:nil]; }
NSURL* userDimensionsFolder() {
    NSURL *docs = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    return [docs URLByAppendingPathComponent:@"Dimensions" isDirectory:YES];
}
// Folder of a World by id: built-in first (built-ins can never be shadowed), then user-made.
NSURL* folderForWorld(NSString *worldID) {
    NSURL *builtIn = [worldsFolder() URLByAppendingPathComponent:worldID];
    if ([[NSFileManager defaultManager] fileExistsAtPath:[builtIn URLByAppendingPathComponent:@"world.json"].path]) return builtIn;
    return [userDimensionsFolder() URLByAppendingPathComponent:worldID];
}

std::optional<atmos::core::DimensionManifest> readWorld(NSURL* folder, std::string* error) {
    NSData* data = [NSData dataWithContentsOfURL:[folder URLByAppendingPathComponent:@"world.json"]];
    if (!data) { if (error) *error = "world.json missing"; return std::nullopt; }
    const auto j = atmos::core::Json::parse(static_cast<const char*>(data.bytes), static_cast<const char*>(data.bytes) + data.length,
                                            nullptr, false);
    if (j.is_discarded()) { if (error) *error = "world.json is not valid JSON"; return std::nullopt; }
    return atmos::core::parseDimension(j, error);
}

// Every stem of every song is in the folder.
bool stemsPresent(NSURL* folder, const atmos::core::DimensionManifest& d) {
    for (const auto& w : d.songs)
        for (const auto& s : w.stems)
            if (![[NSFileManager defaultManager] fileExistsAtPath:[folder URLByAppendingPathComponent:@(s.file.c_str())].path]) return false;
    return true;
}
} // namespace

@implementation AtmosMeterSnapshot
- (instancetype)init {
    if ((self = [super init])) {
        _busPeaks = @[@0, @0, @0, @0];
        _limiterGain = 1.0f;
        _environmentLevelDb = -144.0f;
        _environmentSilent = YES;
        _moonPhaseId = @"new";
        _lunarDay = 1;
    }
    return self;
}
@end

namespace {
NSError* makeError(const std::string& msg) {
    return [NSError errorWithDomain:@"AtmosEngine" code:1 userInfo:@{NSLocalizedDescriptionKey: @(msg.c_str())}];
}
} // namespace

@implementation AtmosEngineBridge {
    // Settings, track and engine rebuilds are handled by the shared LiveSession.
    std::unique_ptr<LiveSession> _session;
#if TARGET_OS_IOS
    std::unique_ptr<atmos::ios::IOSAudioIO> _io;
    AtmosAudioSession *_audioSession;
    BOOL _wasRunningBeforeInterruption;
    AtmosMotionSource *_motion;
    dispatch_queue_t _sessionQueue;   // slow AVAudioSession changes, off the main thread
    NSUInteger _restartGeneration;
#else
    // Mac: Core Audio through miniaudio. The host rebuilds the engine for the device rate.
    std::unique_ptr<atmos::desktop::MiniaudioHost> _host;
#endif
    CFTimeInterval _lastMeterTime;
    double _intensity;
}

- (instancetype)init {
    if ((self = [super init])) {
        _session = std::make_unique<LiveSession>(48000.0, kMaxBlock);
#if TARGET_OS_IOS
        _audioSession = [AtmosAudioSession new];
        _audioSession.delegate = self;
        _io = std::make_unique<atmos::ios::IOSAudioIO>(_session->engine());
        _motion = [AtmosMotionSource new];
        _sessionQueue = dispatch_queue_create("viksen.audiosession", DISPATCH_QUEUE_SERIAL);
#else
        _host = std::make_unique<atmos::desktop::MiniaudioHost>(*_session);
#endif
        _intensity = -1;
        _adaptive = YES;
        _headphones = YES;
    }
    return self;
}

- (void)dealloc {
#if TARGET_OS_IOS
    _io.reset();
#else
    _host.reset();   // stops the device before the session goes away
#endif
}

// ------------------------------------------------------------------ properties

#if TARGET_OS_IOS
- (BOOL)audioRunning { return _io && _io->isRunning(); }
- (BOOL)micEnabled { return _session->settings().micEnabled; }
- (double)sampleRate { return _session->sampleRate(); }
- (AtmosRouteInfo *)route { return [_audioSession currentRoute]; }
- (AtmosMicPermission)micPermission { return [_audioSession micPermission]; }

// ------------------------------------------------------------------ engine lifecycle

// The IO host holds a reference to the engine, so it is recreated with it.
- (void)rebuildAtRate:(double)rate {
    if (_io) _io->stop();
    _io.reset();
    _session->rebuild(rate, kMaxBlock);
    _io = std::make_unique<atmos::ios::IOSAudioIO>(_session->engine());
}

- (BOOL)startAudioWithError:(NSError **)error {
    NSError *err = nil;
    const BOOL withMic = _session->settings().micEnabled;
    if (![_audioSession configureForMicrophone:withMic error:&err] || ![_audioSession setActive:YES error:&err]) {
        if (error) *error = err;
        return NO;
    }
    // The engine stays at its own rate (48 kHz): RemoteIO converts to whatever the
    // headphones run at (44.1 or 48 kHz), so switching headphones never rebuilds the engine
    // and its 3D setup (that rebuild froze the app for up to a second).
    std::string msg;
    if (!_io->start(_session->sampleRate(), withMic, msg)) {
        if (error) *error = makeError(msg);
        return NO;
    }
    return YES;
}

- (void)stopAudio {
    if (_io) _io->stop();
    _session->engine().pause();
    [_audioSession setActive:NO error:nil];
}

- (void)restartIOIfRunning {
    if (!self.audioRunning) return;
    _io->stop();
    // Changing the session (category for the microphone, activation) can take most of a
    // second, especially with Bluetooth: do it on a background queue, then restart the
    // audio unit (fast) on the main thread. A newer restart supersedes an older one.
    const BOOL withMic = _session->settings().micEnabled;
    const NSUInteger generation = ++_restartGeneration;
    AtmosAudioSession *audioSession = _audioSession;
    __weak AtmosEngineBridge *weakSelf = self;
    dispatch_async(_sessionQueue, ^{
        NSError *err = nil;
        const BOOL ok = [audioSession configureForMicrophone:withMic error:&err] && [audioSession setActive:YES error:&err];
        dispatch_async(dispatch_get_main_queue(), ^{
            AtmosEngineBridge *s = weakSelf;
            if (!s || generation != s->_restartGeneration) return;
            std::string msg;
            if (!ok || !s->_io->start(s->_session->sampleRate(), s->_session->settings().micEnabled, msg)) {
                NSString *why = ok ? @(msg.c_str()) : err.localizedDescription;
                [s.delegate engineBridgeStateDidChange:[NSString stringWithFormat:@"Audio restart failed: %@", why]];
            }
        });
    });
}

#else
- (BOOL)audioRunning { return _host && _host->running(); }
- (BOOL)micEnabled { return _session->settings().micEnabled; }
- (double)sampleRate { return _session->sampleRate(); }

- (AtmosRouteInfo *)route {
    AtmosRouteInfo *info = [AtmosRouteInfo new];
    if (_host && _host->running()) {
        info.outputName = @(_host->deviceName().c_str());
        info.inputName = _session->settings().micEnabled ? @"Default input" : @"None";
        info.sampleRate = _host->sampleRate();
        info.ioBufferDuration = _host->sampleRate() > 0 ? _host->periodFrames() / _host->sampleRate() : 0;
    }
    // Core Audio does not say whether the output is headphones; treat it as unknown.
    info.kind = AtmosRouteKindOther;
    return info;
}

- (AtmosMicPermission)micPermission {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
        case AVAuthorizationStatusAuthorized: return AtmosMicPermissionGranted;
        case AVAuthorizationStatusNotDetermined: return AtmosMicPermissionUndetermined;
        default: return AtmosMicPermissionDenied;
    }
}

// ------------------------------------------------------------------ engine lifecycle

- (BOOL)startAudioWithError:(NSError **)error {
    if (_host->running()) _host->stop();
    atmos::desktop::HostOptions options;
    options.withInput = _session->settings().micEnabled;
    std::string msg;
    if (!_host->start(options, msg)) {
        if (error) *error = makeError(msg);
        return NO;
    }
    return YES;
}

- (void)stopAudio {
    if (_host) _host->stop();
    _session->engine().pause();
}

- (void)restartIOIfRunning {
    if (!self.audioRunning) return;
    NSError *err = nil;
    if (![self startAudioWithError:&err])
        [self.delegate engineBridgeStateDidChange:[NSString stringWithFormat:@"Audio restart failed: %@", err.localizedDescription]];
}

#endif

// ------------------------------------------------------------------ mix

static bool validBus(AtmosBusId bus) { return bus >= 0 && bus < atmos::kNumBuses; }

- (void)setGain:(float)gain forBus:(AtmosBusId)bus { if (validBus(bus)) _session->setBusGain(static_cast<Bus>(bus), gain); }
- (void)setLimiterCeilingDb:(float)db { _session->setLimiterCeilingDb(db); }

// ------------------------------------------------------------------ microphone

- (void)setMicEnabled:(BOOL)enabled completion:(void (^)(BOOL, NSString *_Nullable))completion {
    if (!enabled) {
        _session->setMicEnabled(false);
        [self restartIOIfRunning]; // drops the input bus so the OS mic indicator turns off
        completion(YES, nil);
        return;
    }
    __weak AtmosEngineBridge *weakSelf = self;
    void (^enable)(void) = ^{
        AtmosEngineBridge *s = weakSelf;
        if (!s) return;
        s->_session->setMicEnabled(true);
        if (s.audioRunning) {
            [s restartIOIfRunning];
        } else {
            NSError *err = nil;
            if (![s startAudioWithError:&err]) {
                s->_session->setMicEnabled(false);
                completion(NO, err.localizedDescription);
                return;
            }
        }
        completion(YES, nil);
    };
    switch (self.micPermission) {
        case AtmosMicPermissionGranted: enable(); break;
        case AtmosMicPermissionDenied:
#if TARGET_OS_IOS
            completion(NO, @"Microphone access is off. Enable it in Settings > Privacy & Security > Microphone.");
#else
            completion(NO, @"Microphone access is off. Enable it in System Settings > Privacy & Security > Microphone.");
#endif
            break;
        case AtmosMicPermissionUndetermined: {
            void (^answer)(BOOL) = ^(BOOL granted) {
                if (granted) enable();
                else completion(NO, @"Microphone permission was not granted.");
            };
#if TARGET_OS_IOS
            [_audioSession requestMicPermission:answer];
#else
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
                dispatch_async(dispatch_get_main_queue(), ^{ answer(granted); });
            }];
#endif
            break;
        }
    }
}



// ------------------------------------------------------------------ motion

- (void)setListenerYawDegrees:(float)degrees { _session->setListenerYaw(degrees); }

#if TARGET_OS_IOS
- (BOOL)motionSensingAvailable { return _motion.available; }
- (BOOL)motionSensingEnabled { return _motion.running; }

- (void)setMotionSensingEnabled:(BOOL)enabled {
    if (!enabled || !_motion.available) {
        [_motion stop];
        _session->setMotionAvailable(false);
        return;
    }
    _session->setMotionAvailable(true);
    __weak AtmosEngineBridge *weakSelf = self;
    [_motion startWithRateHz:30.0 handler:^(double ax, double ay, double az, double dt) {
        AtmosEngineBridge *s = weakSelf;
        if (s) s->_session->addMotionSample(ax, ay, az, dt);
    }];
}
#else
// No accelerometer on Mac: the still/moving states stay unavailable.
- (BOOL)motionSensingAvailable { return NO; }
- (BOOL)motionSensingEnabled { return NO; }
- (void)setMotionSensingEnabled:(BOOL)enabled { (void)enabled; _session->setMotionAvailable(false); }
#endif

// ------------------------------------------------------------------ safety

- (void)panic {
    _session->engine().panic();
    _session->engine().pause();
}
- (void)clearPanic { _session->engine().clearPanic(); }

// ------------------------------------------------------------------ meters

- (AtmosMeterSnapshot *)meters {
    atmos::AudioEngine &engine = _session->engine();
    engine.collectGarbage();
    const CFTimeInterval now = CACurrentMediaTime();
    const double dt = _lastMeterTime > 0 ? std::min(0.5, now - _lastMeterTime) : 0.0;
    _lastMeterTime = now;
    _session->updateEnvironment(dt);
    const atmos::EnvironmentStateSnapshot st = _session->environmentStates();
    {
        atmos::core::ConditionContext ctx;
        ctx.env = st;
        ctx.micActive = engine.meters().micActive && self.audioRunning;
        if (_intensity >= 0) ctx.intensity = _intensity;
        ctx.localHour = static_cast<int>([[NSCalendar currentCalendar] component:NSCalendarUnitHour fromDate:[NSDate date]]);
        ctx.daysSinceUnixEpochUtc = [NSDate date].timeIntervalSince1970 / 86400.0;
        ctx.night = self.night;
        ctx.headphones = self.headphones;
        ctx.raining = self.raining;
        ctx.adaptive = self.adaptive;
        _session->updateWorld(dt, ctx);
    }
    const atmos::EngineMeters m = engine.meters();
    const atmos::EnvironmentFeatures f = engine.environmentFeatures();
    const double rate = _session->sampleRate();
    AtmosMeterSnapshot *s = [AtmosMeterSnapshot new];
    s.outputPeakLeft = m.outputPeak[0];
    s.outputPeakRight = m.outputPeak[1];
    s.busPeaks = @[@(m.busPeak[0]), @(m.busPeak[1]), @(m.busPeak[2]), @(m.busPeak[3])];
    s.limiterGain = m.limiterGain;
    s.positionSeconds = m.positionFrames / rate;
    s.durationSeconds = engine.timelineLengthFrames() / rate;
    s.playing = m.playing;
    s.micActive = m.micActive && self.audioRunning;
    s.panicked = m.panicked;
    s.underruns = m.underruns;
    s.environmentLevelDb = f.levelDb;
    s.environmentSilent = f.silent;
    s.transientCount = f.transientCount;
    s.quiet = st.quiet;
    s.loud = st.loud;
    s.still = st.still;
    s.moving = st.moving;
    s.motionAvailable = st.motionAvailable;
    s.loudness = st.loudness;
    s.activity = st.activity;
    const double days = [NSDate date].timeIntervalSince1970 / 86400.0;
    s.moonPhaseId = @(atmos::core::moonPhaseId(atmos::core::moonPhaseName(days)));
    s.lunarDay = atmos::core::lunarDay(days);
    return s;
}

// ------------------------------------------------------------------ Worlds

- (NSArray<AtmosWorldInfo *> *)availableWorlds {
    NSMutableArray *out = [NSMutableArray array];
    NSMutableSet *seen = [NSMutableSet set];
    for (int pass = 0; pass < 2; ++pass) {
        NSURL *root = pass == 0 ? worldsFolder() : userDimensionsFolder();
        if (!root) continue;
        NSArray<NSURL *> *dirs = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:root includingPropertiesForKeys:nil
                                                                                  options:NSDirectoryEnumerationSkipsHiddenFiles error:nil];
        for (NSURL *dir in dirs) {
            auto w = readWorld(dir, nullptr);
            if (!w) continue;
            NSString *wid = @(w->id.c_str());
            // The folder name must match the id (that is how loadWorld finds it); built-ins win on clashes.
            if ([seen containsObject:wid] || ![dir.lastPathComponent isEqualToString:wid]) continue;
            [seen addObject:wid];
            AtmosWorldInfo *info = [AtmosWorldInfo new];
            info.worldID = wid;
            info.name = @(w->name.c_str());
            info.artist = @(w->artist.c_str());
            const auto &first = w->songs.front();
            info.musicalKey = @(first.key.c_str());
            info.tier = pass == 0 ? @(w->tier.c_str()) : @"free";
            info.tempo = first.tempo;
            info.songCount = static_cast<NSInteger>(w->songs.size());
            info.album = @(w->album.c_str());
            info.albumLink = @(w->albumLink.c_str());
            info.albumOnAppleMusic = w->albumOnAppleMusic;
            NSMutableArray<NSString *> *titles = [NSMutableArray array];
            for (const auto &song : w->songs) [titles addObject:@(song.name.c_str())];
            info.songTitles = titles;
            info.stemCount = static_cast<NSInteger>(w->totalStems());
            info.audioAvailable = stemsPresent(dir, *w);
            info.userMade = pass == 1;
            info.folderURL = dir;
            [out addObject:info];
        }
    }
    [out sortUsingComparator:^NSComparisonResult(AtmosWorldInfo *a, AtmosWorldInfo *b) {
        if (a.userMade != b.userMade) return a.userMade ? NSOrderedDescending : NSOrderedAscending;
        return [a.name compare:b.name];
    }];
    return out;
}

- (BOOL)loadWorld:(NSString *)worldID error:(NSError **)error {
    return [self loadWorld:worldID audioFiles:nil error:error];
}

- (NSArray<NSString *> *)stemFilesForWorld:(NSString *)worldID {
    auto w = readWorld(folderForWorld(worldID), nullptr);
    NSMutableArray<NSString *> *out = [NSMutableArray array];
    if (w)
        for (const auto &song : w->songs)
            for (const auto &st : song.stems) [out addObject:@(st.file.c_str())];
    return out;
}

- (NSArray<NSDictionary<NSString *, id> *> *)songsForWorld:(NSString *)worldID {
    auto w = readWorld(folderForWorld(worldID), nullptr);
    NSMutableArray *out = [NSMutableArray array];
    if (!w) return out;
    for (const auto &song : w->songs) {
        NSMutableArray<NSString *> *files = [NSMutableArray array];
        for (const auto &st : song.stems) [files addObject:@(st.file.c_str())];
        [out addObject:@{@"id": @(song.id.c_str()), @"title": @(song.name.c_str()), @"files": files,
                          @"lunarPhase": song.lunarPhase ? @(atmos::core::moonPhaseId(*song.lunarPhase)) : @""}];
    }
    return out;
}

- (BOOL)loadWorld:(NSString *)worldID audioFiles:(NSDictionary<NSString *, NSURL *> *)files error:(NSError **)error {
    NSURL *folder = folderForWorld(worldID);
    std::string err;
    auto w = readWorld(folder, &err);
    if (!w) { if (error) *error = makeError(err); return NO; }
    NSDictionary<NSString *, NSURL *> *map = files ?: @{};
    auto urlFor = [folder, map](const std::string &f) -> NSURL * {
        NSURL *u = map[@(f.c_str())];
        return u ?: [folder URLByAppendingPathComponent:@(f.c_str())];
    };
    // Keep only the songs whose every stem is on this device (tracks download one by one).
    std::vector<atmos::core::WorldManifest> present;
    for (const auto &song : w->songs) {
        bool all = true;
        for (const auto &st : song.stems)
            if (![[NSFileManager defaultManager] fileExistsAtPath:urlFor(st.file).path]) { all = false; break; }
        if (all) present.push_back(song);
    }
    if (present.empty()) {
        if (error) *error = makeError("Download a track of this Dimension first.");
        return NO;
    }
    w->songs = std::move(present);
    atmos::core::LiveSession::StemFileDecoder decode = [urlFor](const std::string &f, double rate, std::string &e) {
        return atmos::ios::makeStemDecoder(urlFor(f), rate, e);
    };
    if (!_session->loadDimension(*w, decode, &err)) { if (error) *error = makeError(err); return NO; }
    return YES;
}

- (void)startWorld { _session->startWorld(); }
- (void)stopWorld { _session->stopWorld(); }
- (void)unloadWorld { _session->unloadWorld(); }
- (void)setExerciseIntensity:(double)intensity { _intensity = intensity; }
- (void)setDimensionSpatialEnabled:(BOOL)enabled { _session->setWorldSpatialEnabled(enabled); }
- (BOOL)binauralHRTF { return _session->engine().worldPlayer().hrtfActive(); }

- (AtmosWorldStatus *)worldStatus {
    const atmos::core::WorldStatus st = _session->worldStatus();
    AtmosWorldStatus *s = [AtmosWorldStatus new];
    s.loaded = st.loaded;
    s.playing = st.playing;
    s.worldID = @(st.id.c_str());
    s.name = @(st.name.c_str());
    s.positionSeconds = st.positionSeconds;
    s.lengthSeconds = st.lengthSeconds;
    s.master = st.master;
    s.underruns = st.underruns;
    s.spatial = st.spatial;
    s.hrtf = st.hrtf;
    s.adaptive = st.adaptive;
    s.composed = st.composed;
    s.sectionTitle = @(st.sectionTitle.c_str());
    s.sectionLabel = @(st.sectionLabel.c_str());
    s.songTitle = @(st.songTitle.c_str());
    s.songCount = st.songCount;
    s.songLunarPhaseId = @(st.songLunarPhase.c_str());
    NSMutableArray *stems = [NSMutableArray array];
    for (const auto &x : st.stems) {
        AtmosWorldStemStatus *t = [AtmosWorldStemStatus new];
        t.title = @(x.title.c_str());
        t.azimuth = x.azimuthDeg;
        t.elevation = x.elevationDeg;
        t.distance = x.distanceM;
        t.width = x.widthDeg;
        t.gainDb = x.gainDb;
        t.peak = x.peak;
        NSMutableArray *r = [NSMutableArray array];
        for (const auto &reason : x.reasons) [r addObject:@(reason.c_str())];
        t.reasons = r;
        [stems addObject:t];
    }
    s.stems = stems;
    return s;
}

#if TARGET_OS_IOS
// ------------------------------------------------------------------ AtmosAudioSessionDelegate

- (void)audioSessionInterruptionBegan {
    _wasRunningBeforeInterruption = self.audioRunning;
    if (_io) _io->stop();
    _session->engine().pause();
    [self.delegate engineBridgeStateDidChange:@"Audio interrupted by the system"];
}

- (void)audioSessionInterruptionEndedShouldResume:(BOOL)shouldResume {
    if (_wasRunningBeforeInterruption && shouldResume) [self startAudioWithError:nil];
    [self.delegate engineBridgeStateDidChange:@"Audio interruption ended"];
}

- (void)audioSessionRouteChanged:(AtmosRouteInfo *)route oldDeviceUnavailable:(BOOL)oldDeviceUnavailable {
    if (oldDeviceUnavailable) {
        // Headphones removed: silence the microphone paths at once (no feedback through
        // the speaker). The app then fades the Dimension out and turns the mic off, so the
        // step back to regular reality is gentle rather than a cut.
        _session->engine().setBusGain(Bus::MicRaw, 0.0f);
        _session->engine().setBusGain(Bus::MicTransformed, 0.0f);
        _session->engine().pause();   // a song track (Studio) still stops, per Apple's guidance
    }
    // No rebuild on a new hardware rate: RemoteIO converts (see startAudioWithError).
    [self.delegate engineBridgeStateDidChange:oldDeviceUnavailable ? @"Headphones disconnected" : @"Audio route changed"];
}

- (void)audioSessionMediaServicesWereReset {
    const BOOL wasRunning = self.audioRunning;
    _io.reset();
    _audioSession = [AtmosAudioSession new];
    _audioSession.delegate = self;
    [self rebuildAtRate:_session->sampleRate()];
    if (wasRunning) [self startAudioWithError:nil];
    [self.delegate engineBridgeStateDidChange:@"Media services were reset"];
}
#endif

@end
