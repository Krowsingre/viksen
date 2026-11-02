#import "AtmosEngineBridge.h"

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>

#include <memory>
#include <string>

#include "core/assets/ClipPreparation.h"
#include "core/session/LiveSession.h"
#include "platform/ios/IOSAudioIO.h"
#include "platform/ios/IOSMediaDecoder.h"
#include "platform/ios/IOSStemDecoder.h"
#include "core/worlds/WorldManifest.h"
#include "core/projects/ProjectPackage.h"
#include "core/session/SessionDocument.h"
#include "core/worlds/WorldConditions.h"
#import "platform/ios/IOSMotion.h"

using atmos::Bus;
using atmos::core::LiveSession;

@implementation AtmosWorldInfo
@end
@implementation AtmosWorldStemStatus
@end
@implementation AtmosWorldStatus
- (instancetype)init {
    if ((self = [super init])) { _worldID = @""; _name = @""; _stems = @[]; }
    return self;
}
@end

namespace {
NSURL* worldsFolder() { return [[NSBundle mainBundle] URLForResource:@"Worlds" withExtension:nil]; }

std::optional<atmos::core::WorldManifest> readWorld(NSURL* folder, std::string* error) {
    NSData* data = [NSData dataWithContentsOfURL:[folder URLByAppendingPathComponent:@"world.json"]];
    if (!data) { if (error) *error = "world.json missing"; return std::nullopt; }
    const auto j = atmos::core::Json::parse(static_cast<const char*>(data.bytes), static_cast<const char*>(data.bytes) + data.length,
                                            nullptr, false);
    if (j.is_discarded()) { if (error) *error = "world.json is not valid JSON"; return std::nullopt; }
    return atmos::core::parseWorld(j, error);
}

bool stemsPresent(NSURL* folder, const atmos::core::WorldManifest& w) {
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
    std::unique_ptr<atmos::ios::IOSAudioIO> _io;
    AtmosAudioSession *_audioSession;
    BOOL _wasRunningBeforeInterruption;
    NSString *_loadedTrackName;
    AtmosMotionSource *_motion;
    CFTimeInterval _lastMeterTime;
    double _intensity;
}

- (instancetype)init {
    if ((self = [super init])) {
        _audioSession = [AtmosAudioSession new];
        _audioSession.delegate = self;
        _session = std::make_unique<LiveSession>(48000.0, atmos::ios::IOSAudioIO::kMaxFramesPerSlice);
        _io = std::make_unique<atmos::ios::IOSAudioIO>(_session->engine());
        _motion = [AtmosMotionSource new];
        _intensity = -1;
    }
    return self;
}

- (void)dealloc {
    _io.reset();
}

// ------------------------------------------------------------------ properties

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
    _session->rebuild(rate, atmos::ios::IOSAudioIO::kMaxFramesPerSlice);
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
    switch ([_audioSession micPermission]) {
        case AtmosMicPermissionGranted: enable(); break;
        case AtmosMicPermissionDenied:
            completion(NO, @"Microphone access is off. Enable it in Settings > Privacy & Security > Microphone.");
            break;
        case AtmosMicPermissionUndetermined:
            [_audioSession requestMicPermission:^(BOOL granted) {
                if (granted) enable();
                else completion(NO, @"Microphone permission was not granted.");
            }];
            break;
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
    NSURL *root = worldsFolder();
    if (!root) return out;
    NSArray<NSURL *> *dirs = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:root includingPropertiesForKeys:nil
                                                                              options:NSDirectoryEnumerationSkipsHiddenFiles error:nil];
    for (NSURL *dir in dirs) {
        auto w = readWorld(dir, nullptr);
        if (!w) continue;
        AtmosWorldInfo *info = [AtmosWorldInfo new];
        info.worldID = @(w->id.c_str());
        info.name = @(w->name.c_str());
        info.artist = @(w->artist.c_str());
        info.musicalKey = @(w->key.c_str());
        info.tier = @(w->tier.c_str());
        info.tempo = w->tempo;
        info.stemCount = static_cast<NSInteger>(w->stems.size());
        info.audioAvailable = stemsPresent(dir, *w);
        [out addObject:info];
    }
    [out sortUsingComparator:^NSComparisonResult(AtmosWorldInfo *a, AtmosWorldInfo *b) { return [a.name compare:b.name]; }];
    return out;
}

- (BOOL)loadWorld:(NSString *)worldID error:(NSError **)error {
    NSURL *folder = [worldsFolder() URLByAppendingPathComponent:worldID];
    std::string err;
    auto w = readWorld(folder, &err);
    if (!w) { if (error) *error = makeError(err); return NO; }
    if (!stemsPresent(folder, *w)) {
        if (error) *error = makeError("This build does not include this Dimension's music yet.");
        return NO;
    }
    std::vector<std::string> files;
    for (const auto &s : w->stems) files.push_back(s.file);
    atmos::core::StemDecoderFactory factory = [folder, files](double rate, std::string &e) {
        std::vector<std::unique_ptr<atmos::core::StemDecoder>> v;
        for (const auto &f : files) {
            auto d = atmos::ios::makeStemDecoder([folder URLByAppendingPathComponent:@(f.c_str())], rate, e);
            if (!d) return std::vector<std::unique_ptr<atmos::core::StemDecoder>>{};
            v.push_back(std::move(d));
        }
        return v;
    };
    if (!_session->loadWorld(*w, factory, &err)) { if (error) *error = makeError(err); return NO; }
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
             dimensionID:(NSString *)dimensionID error:(NSError **)error {
    namespace fs = std::filesystem;
    using namespace atmos::core;
    const fs::path pkg(packageURL.fileSystemRepresentation);
    std::optional<SessionTrack> track;
    if (songURL) {
        const fs::path src(songURL.fileSystemRepresentation);
        std::error_code ec;
        // Reuse the copy already in the package when it is the same file (autosave).
        std::string rel;
        ProjectManifest old;
        if (ProjectPackage::load(pkg, old).ok) {
            const auto prev = readSessionProject(old).track;
            if (prev && fs::exists(pkg / prev->packagedPath, ec) && fs::path(prev->packagedPath).filename() == src.filename() &&
                fs::file_size(pkg / prev->packagedPath, ec) == fs::file_size(src, ec))
                rel = prev->packagedPath;
        }
        if (rel.empty()) {
            // Remove stale song copies before importing the new one.
            for (auto &e : fs::directory_iterator(pkg / "assets" / "audio", ec)) fs::remove(e.path(), ec);
            const auto r = ProjectPackage::importAsset(pkg, src, rel);
            if (!r.ok) { if (error) *error = makeError(r.error); return NO; }
        }
        MediaInfo media;
        if (AVAudioFile *f = [[AVAudioFile alloc] initForReading:songURL error:nil]) {
            media.sampleRate = f.fileFormat.sampleRate;
            media.channels = static_cast<int>(f.fileFormat.channelCount);
            media.frames = f.length;
        }
        track = SessionTrack{rel, songURL.lastPathComponent.UTF8String, media};
    }
    auto m = makeSessionProject(name.UTF8String, _session->settings(), track,
                                dimensionID ? dimensionID.UTF8String : "", _session->sampleRate());
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

@end
