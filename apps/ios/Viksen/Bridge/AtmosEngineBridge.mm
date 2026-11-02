#import "AtmosEngineBridge.h"

#import <AVFoundation/AVFoundation.h>

#include <memory>
#include <string>

#include "core/assets/ClipPreparation.h"
#include "core/session/LiveSession.h"
#include "platform/ios/IOSAudioIO.h"
#include "platform/ios/IOSMediaDecoder.h"

using atmos::Bus;
using atmos::core::LiveSession;

@implementation AtmosMeterSnapshot
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
}

- (instancetype)init {
    if ((self = [super init])) {
        _audioSession = [AtmosAudioSession new];
        _audioSession.delegate = self;
        _session = std::make_unique<LiveSession>(48000.0, atmos::ios::IOSAudioIO::kMaxFramesPerSlice);
        _io = std::make_unique<atmos::ios::IOSAudioIO>(_session->engine());
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
