#import "AtmosEngineBridge.h"

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#include <TargetConditionals.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "core/assets/ClipPreparation.h"
#include "core/session/LiveSession.h"
#if TARGET_OS_IOS
#include "platform/ios/IOSAudioIO.h"
#import "platform/ios/IOSAudioSession.h"
#import "platform/ios/IOSMotion.h"
#else
#include "platform/desktop/MiniaudioHost.h"
#endif
#include "platform/ios/IOSMediaDecoder.h"
#include "platform/ios/IOSStemDecoder.h"
#include "core/worlds/WorldManifest.h"
#include "core/projects/ProjectPackage.h"
#include "core/session/SessionDocument.h"
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
    if ((self = [super init])) { _worldID = @""; _name = @""; _stems = @[]; _sectionTitle = @""; _sectionLabel = @""; _songTitle = @""; _songCount = 1; _composed = YES; }
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
#else
    // Mac: Core Audio through miniaudio. The host rebuilds the engine for the device rate.
    std::unique_ptr<atmos::desktop::MiniaudioHost> _host;
#endif
    NSString *_loadedTrackName;
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
#else
        _host = std::make_unique<atmos::desktop::MiniaudioHost>(*_session);
#endif
        _intensity = -1;
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
- (NSString *)loadedTrackName { return _loadedTrackName; }
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
    const double hwRate = [AVAudioSession sharedInstance].sampleRate;
    if (hwRate > 0 && hwRate != _session->sampleRate()) [self rebuildAtRate:hwRate];
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
    NSError *err = nil;
    if (![self startAudioWithError:&err])
        [self.delegate engineBridgeStateDidChange:[NSString stringWithFormat:@"Audio restart failed: %@", err.localizedDescription]];
}

#else
- (BOOL)audioRunning { return _host && _host->running(); }
- (BOOL)micEnabled { return _session->settings().micEnabled; }
- (double)sampleRate { return _session->sampleRate(); }
- (NSString *)loadedTrackName { return _loadedTrackName; }

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

// ------------------------------------------------------------------ track

- (void)loadTrackAtURL:(NSURL *)url completion:(void (^)(BOOL, NSString *_Nullable))completion {
    const double rate = _session->sampleRate();
    NSString *name = url.lastPathComponent;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const BOOL scoped = [url startAccessingSecurityScopedResource];
        auto decoded = std::make_shared<atmos::AudioClip>();
        atmos::core::AudioFileInfo info;
        std::string error;
        const bool ok = atmos::ios::decodeAudioFile(url, *decoded, info, error);
        if (scoped) [url stopAccessingSecurityScopedResource];
        // Resample off the main thread; LiveSession redoes it if the rate changed meanwhile.
        atmos::ClipPtr prepared = ok ? atmos::core::prepareClipForEngine(*decoded, rate) : nullptr;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!ok) { completion(NO, @(error.c_str())); return; }
            if (!self->_session->setTrack(decoded, prepared, rate)) { completion(NO, @"The engine rejected this file."); return; }
            self->_loadedTrackName = name;
            completion(YES, nil);
        });
    });
}

- (void)unloadTrack {
    _session->clearTrack();
    _loadedTrackName = nil;
}

- (void)play { _session->engine().play(); }
- (void)pause { _session->engine().pause(); }
- (void)seekToSeconds:(double)s { _session->engine().seek(static_cast<int64_t>(s * _session->sampleRate())); }
- (void)setLooping:(BOOL)looping { _session->setLooping(looping); }

// ------------------------------------------------------------------ mix

static bool validBus(AtmosBusId bus) { return bus >= 0 && bus < atmos::kNumBuses; }

- (void)setGain:(float)gain forBus:(AtmosBusId)bus { if (validBus(bus)) _session->setBusGain(static_cast<Bus>(bus), gain); }
- (float)gainForBus:(AtmosBusId)bus { return validBus(bus) ? _session->settings().busGain[bus] : 0.0f; }
- (void)setMuted:(BOOL)muted forBus:(AtmosBusId)bus { if (validBus(bus)) _session->setBusMuted(static_cast<Bus>(bus), muted); }
- (void)setMasterGain:(float)g { _session->setMasterGain(g); }
- (void)setLimiterCeilingDb:(float)db { _session->setLimiterCeilingDb(db); }

- (void)setAtmosphereBedEnabled:(BOOL)e { _session->setBedEnabled(e); }
- (void)setAtmosphereBedLevel:(float)l { _session->setBedLevel(l); }
- (void)setAtmosphereBedColourHz:(float)hz { _session->setBedColourHz(hz); }

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

- (void)setMicToneHz:(float)v { _session->setMicToneHz(v); }
- (void)setMicEchoSeconds:(float)v { _session->setMicEchoSeconds(v); }
- (void)setMicFeedback:(float)v { _session->setMicFeedback(v); }
- (void)setMicWet:(float)v { _session->setMicWet(v); }
- (void)setMicShiftHz:(float)v { _session->setMicShiftHz(v); }
- (void)setMicShiftMix:(float)v { _session->setMicShiftMix(v); }

- (void)setRoomMemoryEnabled:(BOOL)e { _session->setRoomMemoryEnabled(e); }
- (void)setRoomMemoryLevel:(float)v { _session->setRoomMemoryLevel(v); }
- (void)setRoomMemoryPitch:(float)v { _session->setRoomMemoryPitch(v); }

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
    s.roomMemoryVoices = engine.roomMemory().activeVoices();
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

- (NSString *)validateDimensionFolder:(NSURL *)folder {
    std::string err;
    auto w = readWorld(folder, &err);
    if (!w) return [NSString stringWithFormat:@"Not a Dimension: %s", err.c_str()];
    if (!stemsPresent(folder, *w)) return @"Some of this Dimension's audio files are missing.";
    unsigned long long total = 0;
    for (const auto &song : w->songs)
        for (const auto &s : song.stems) {
            NSDictionary *a = [[NSFileManager defaultManager] attributesOfItemAtPath:[folder URLByAppendingPathComponent:@(s.file.c_str())].path error:nil];
            total += [a fileSize];
        }
    if (total > 2ull * 1024 * 1024 * 1024) return @"This Dimension is larger than 2 GB.";
    return nil;
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
        [out addObject:@{@"id": @(song.id.c_str()), @"title": @(song.name.c_str()), @"files": files}];
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
- (void)moveStem:(NSInteger)index azimuth:(float)az elevation:(float)el distance:(float)d {
    _session->setWorldStemPosition(static_cast<int>(index), az, el, d);
}
- (BOOL)startRecordingMode:(AtmosRecordMode)mode toURL:(NSURL *)url error:(NSError **)error {
    std::string err;
    if (!_session->startRecording(url.fileSystemRepresentation, static_cast<atmos::CaptureMode>(mode), &err)) {
        if (error) *error = makeError(err.empty() ? "Could not start recording" : err);
        return NO;
    }
    return YES;
}
- (double)stopRecording { return _session->stopRecording(); }
- (BOOL)recording { return _session->recording(); }
- (double)recordingSeconds { return _session->recordingSeconds(); }
- (uint64_t)recordingDroppedBlocks { return _session->recordingDroppedBlocks(); }
- (BOOL)takeRecordingInterrupted { return _session->takeRecordingInterrupted(); }

- (BOOL)saveProjectAtURL:(NSURL *)packageURL name:(NSString *)name songURL:(NSURL *)songURL
             dimensionID:(NSString *)dimensionID stems:(NSArray<NSDictionary<NSString *, id> *> *)stemList
                   error:(NSError **)error {
    namespace fs = std::filesystem;
    using namespace atmos::core;
    const fs::path pkg(packageURL.fileSystemRepresentation);
    std::error_code ec;
    fs::create_directories(pkg / "assets" / "audio", ec);
    // Files already in the package (same name and size) are reused; everything else is copied in.
    ProjectManifest old;
    std::vector<std::string> keep;
    std::set<std::string> oldPaths;
    if (ProjectPackage::load(pkg, old).ok)
        for (const auto &src : old.sources) oldPaths.insert(src.path);
    auto importOrReuse = [&](NSURL *url, std::string &rel) -> bool {
        const fs::path src(url.fileSystemRepresentation);
        for (const auto &p : oldPaths) {
            if (fs::path(p).filename() == src.filename() && fs::exists(pkg / p, ec) &&
                fs::file_size(pkg / p, ec) == fs::file_size(src, ec)) { rel = p; return true; }
        }
        if (fs::equivalent(src.parent_path(), pkg / "assets" / "audio", ec)) {  // already inside this package
            rel = (fs::path("assets") / "audio" / src.filename()).generic_string();
            return true;
        }
        const auto r = ProjectPackage::importAsset(pkg, src, rel);
        if (!r.ok && error) *error = makeError(r.error);
        return r.ok;
    };
    auto mediaOf = [](NSURL *url) {
        MediaInfo media;
        if (AVAudioFile *f = [[AVAudioFile alloc] initForReading:url error:nil]) {
            media.sampleRate = f.fileFormat.sampleRate;
            media.channels = static_cast<int>(f.fileFormat.channelCount);
            media.frames = f.length;
        }
        return media;
    };
    std::optional<SessionTrack> track;
    if (songURL) {
        std::string rel;
        if (!importOrReuse(songURL, rel)) return NO;
        keep.push_back(rel);
        track = SessionTrack{rel, songURL.lastPathComponent.UTF8String, mediaOf(songURL)};
    }
    std::vector<SessionStem> stems;
    for (NSDictionary *d in stemList) {
        NSURL *u = d[@"url"];
        if (![u isKindOfClass:[NSURL class]]) continue;
        std::string rel;
        if (!importOrReuse(u, rel)) return NO;
        keep.push_back(rel);
        stems.push_back({SessionTrack{rel, u.lastPathComponent.UTF8String, mediaOf(u)},
                         [d[@"gainDb"] floatValue], static_cast<bool>([d[@"muted"] boolValue])});
    }
    // Drop audio no longer referenced (e.g. a replaced song).
    for (auto &e : fs::directory_iterator(pkg / "assets" / "audio", ec)) {
        const std::string rel = (fs::path("assets") / "audio" / e.path().filename()).generic_string();
        if (std::find(keep.begin(), keep.end(), rel) == keep.end()) fs::remove(e.path(), ec);
    }
    auto m = makeSessionProject(name.UTF8String, _session->settings(), track,
                                dimensionID ? dimensionID.UTF8String : "", _session->sampleRate(), stems);
    const auto r = ProjectPackage::save(pkg, m);
    if (!r.ok) { if (error) *error = makeError(r.error); return NO; }
    return YES;
}

- (NSDictionary<NSString *, id> *)openProjectAtURL:(NSURL *)packageURL error:(NSError **)error {
    using namespace atmos::core;
    const std::filesystem::path pkg(packageURL.fileSystemRepresentation);
    ProjectManifest m;
    const auto r = ProjectPackage::load(pkg, m);
    if (!r.ok) { if (error) *error = makeError(r.error); return nil; }
    const auto restore = readSessionProject(m, _session->settings());
    _session->applyAllSettings(restore.settings);
    NSMutableDictionary *out = [@{ @"name": @(m.name.c_str()) } mutableCopy];
    if (restore.track) {
        const auto file = pkg / restore.track->packagedPath;
        if (std::filesystem::exists(file)) out[@"song"] = [NSURL fileURLWithPath:@(file.c_str())];
    }
    if (!restore.dimensionId.empty()) out[@"dimension"] = @(restore.dimensionId.c_str());
    NSMutableArray *stems = [NSMutableArray array];
    for (const auto &st : restore.stems) {
        const auto file = pkg / st.file.packagedPath;
        if (!std::filesystem::exists(file)) continue;
        [stems addObject:@{ @"url": [NSURL fileURLWithPath:@(file.c_str())], @"gainDb": @(st.gainDb), @"muted": @(st.muted) }];
    }
    out[@"stems"] = stems;
    return out;
}

- (NSDictionary<NSString *, NSNumber *> *)currentSettings {
    const auto &s = _session->settings();
    return @{
        @"busGain0": @(s.busGain[0]), @"busGain1": @(s.busGain[1]), @"busGain2": @(s.busGain[2]), @"busGain3": @(s.busGain[3]),
        @"masterGain": @(s.masterGain), @"limiterCeilingDb": @(s.limiterCeilingDb),
        @"bedEnabled": @(s.bedEnabled), @"bedLevel": @(s.bedLevel), @"bedColourHz": @(s.bedColourHz),
        @"micToneHz": @(s.micToneHz), @"micEchoSeconds": @(s.micEchoSeconds), @"micFeedback": @(s.micFeedback),
        @"micWet": @(s.micWet), @"micShiftHz": @(s.micShiftHz), @"micShiftMix": @(s.micShiftMix),
        @"roomMemoryEnabled": @(s.roomMemoryEnabled), @"roomMemoryLevel": @(s.roomMemoryLevel),
        @"roomMemoryPitch": @(s.roomMemoryPitch), @"looping": @(s.looping),
    };
}

- (BOOL)loadStudioStems:(NSArray<NSURL *> *)urls error:(NSError **)error {
    using namespace atmos::core;
    if (urls.count == 0 || urls.count > 8) { if (error) *error = makeError("Choose between 1 and 8 stems."); return NO; }
    WorldManifest w;
    w.id = "studio";
    w.name = "Studio";
    w.tier = "studio";
    w.fadeSeconds = 0.25f;   // mixer moves are near-immediate
    std::vector<NSURL *> files;
    const float n = static_cast<float>(urls.count);
    for (NSUInteger i = 0; i < urls.count; ++i) {
        WorldStem st;
        st.id = "stem" + std::to_string(i);
        st.file = urls[i].lastPathComponent.UTF8String;
        st.title = [urls[i].lastPathComponent stringByDeletingPathExtension].UTF8String;
        // Spread across a frontal arc by default; the canvas can move them.
        st.azimuthDeg = n > 1 ? -70.0f + 140.0f * static_cast<float>(i) / (n - 1) : 0.0f;
        st.distanceM = 1.6f;
        st.widthDeg = 40.0f;
        w.stems.push_back(st);
        files.push_back(urls[i]);
    }
    StemDecoderFactory factory = [files](double rate, std::string &e) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        for (NSURL *u : files) {
            auto d = atmos::ios::makeStemDecoder(u, rate, e);
            if (!d) return std::vector<std::unique_ptr<StemDecoder>>{};
            v.push_back(std::move(d));
        }
        return v;
    };
    std::string err;
    if (!_session->loadWorld(w, factory, &err)) { if (error) *error = makeError(err); return NO; }
    return YES;
}
- (void)studioPlay { _session->startWorld(true); }
- (void)studioPause { _session->stopWorld(true); }
- (void)studioSeekToSeconds:(double)seconds { _session->seekWorld(seconds); }
- (void)setStem:(NSInteger)index gainDb:(float)gainDb muted:(BOOL)muted {
    _session->setWorldStemLevel(static_cast<int>(index), gainDb, muted);
}

- (void)resetStemPositions { _session->resetWorldStemPositions(); }
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
        // Headphones removed: pause playback and silence raw monitoring so audio does
        // not suddenly play from the speaker and the mic cannot feed back.
        _session->engine().pause();
        _session->setBusGain(Bus::MicRaw, 0.0f);
    }
    if (self.audioRunning && route.sampleRate > 0 && route.sampleRate != _session->sampleRate()) {
        [self rebuildAtRate:route.sampleRate];
        [self startAudioWithError:nil];
    }
    [self.delegate engineBridgeStateDidChange:oldDeviceUnavailable ? @"Headphones disconnected: paused" : @"Audio route changed"];
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
