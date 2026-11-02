#import "AtmosEngineBridge.h"

#import <AVFoundation/AVFoundation.h>

#include <memory>
#include <string>

#include "core/assets/ClipPreparation.h"
#include "engine/audio/AudioEngine.h"
#include "platform/ios/IOSAudioIO.h"
#include "platform/ios/IOSMediaDecoder.h"

using atmos::AudioEngine;
using atmos::Bus;

@implementation AtmosMeterSnapshot
@end

namespace {
// Settings mirrored on the control side so the engine can be rebuilt (e.g. when
// the hardware sample rate changes after a route change) without losing state.
struct CachedSettings {
    float busGain[atmos::kNumBuses] = {atmos::defaultBusGain(Bus::Music), atmos::defaultBusGain(Bus::Atmosphere),
                                       atmos::defaultBusGain(Bus::MicRaw), atmos::defaultBusGain(Bus::MicTransformed)};
    bool busMuted[atmos::kNumBuses] = {};
    float master = 0.7f;
    float ceilingDb = -1.0f;
    bool bedEnabled = false;
    float bedLevel = 0.25f, bedColour = 900.0f;
    float micTone = 1800.0f, micEcho = 0.38f, micFeedback = 0.45f, micWet = 0.6f;
    bool looping = false;
};
NSError* makeError(const std::string& msg) {
    return [NSError errorWithDomain:@"AtmosEngine" code:1 userInfo:@{NSLocalizedDescriptionKey: @(msg.c_str())}];
}
} // namespace

@implementation AtmosEngineBridge {
    std::unique_ptr<AudioEngine> _engine;
    std::unique_ptr<atmos::ios::IOSAudioIO> _io;
    AtmosAudioSession *_session;
    CachedSettings _settings;
    std::shared_ptr<const atmos::AudioClip> _originalTrack; // native-rate decode, kept for rebuilds
    BOOL _wantsMic;
    BOOL _wasRunningBeforeInterruption;
    NSString *_loadedTrackName;
}

- (instancetype)init {
    if ((self = [super init])) {
        _session = [AtmosAudioSession new];
        _session.delegate = self;
        [self rebuildEngineAtRate:48000.0];
    }
    return self;
}

- (void)dealloc {
    _io.reset();
}

// ------------------------------------------------------------------ properties

- (BOOL)audioRunning { return _io && _io->isRunning(); }
- (BOOL)micEnabled { return _wantsMic; }
- (double)sampleRate { return _engine->config().sampleRate; }
- (NSString *)loadedTrackName { return _loadedTrackName; }
- (AtmosRouteInfo *)route { return [_session currentRoute]; }
- (AtmosMicPermission)micPermission { return [_session micPermission]; }

// ------------------------------------------------------------------ engine lifecycle

- (void)rebuildEngineAtRate:(double)rate {
    if (_io) _io->stop();
    _io.reset();
    _engine = std::make_unique<AudioEngine>();
    [self applyCachedSettings];
    _engine->prepare({rate, atmos::ios::IOSAudioIO::kMaxFramesPerSlice});
    _io = std::make_unique<atmos::ios::IOSAudioIO>(*_engine);
    if (_originalTrack) {
        auto clip = atmos::core::prepareClipForEngine(*_originalTrack, rate);
        _engine->setLayer(0, clip, Bus::Music, 0);
    }
}

- (void)applyCachedSettings {
    for (int b = 0; b < atmos::kNumBuses; ++b) {
        _engine->setBusGain(static_cast<Bus>(b), _settings.busGain[b]);
        _engine->setBusMuted(static_cast<Bus>(b), _settings.busMuted[b]);
    }
    _engine->setMasterGain(_settings.master);
    _engine->setLimiterCeilingDb(_settings.ceilingDb);
    _engine->setAtmosphereBedEnabled(_settings.bedEnabled);
    _engine->atmosphereBed().level.set(_settings.bedLevel);
    _engine->atmosphereBed().colourHz.set(_settings.bedColour);
    _engine->micTransform().cutoffHz.set(_settings.micTone);
    _engine->micTransform().echoSeconds.set(_settings.micEcho);
    _engine->micTransform().feedback.set(_settings.micFeedback);
    _engine->micTransform().wet.set(_settings.micWet);
    _engine->setLooping(_settings.looping);
    _engine->setMicInputEnabled(_wantsMic);
}

- (BOOL)startAudioWithError:(NSError **)error {
    NSError *err = nil;
    if (![_session configureForMicrophone:_wantsMic error:&err] || ![_session setActive:YES error:&err]) {
        if (error) *error = err;
        return NO;
    }
    const double hwRate = [AVAudioSession sharedInstance].sampleRate;
    if (hwRate > 0 && hwRate != _engine->config().sampleRate) [self rebuildEngineAtRate:hwRate];
    std::string msg;
    if (!_io->start(_engine->config().sampleRate, _wantsMic, msg)) {
        if (error) *error = makeError(msg);
        return NO;
    }
    return YES;
}

- (void)stopAudio {
    if (_io) _io->stop();
    _engine->pause();
    [_session setActive:NO error:nil];
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
    const double rate = _engine->config().sampleRate;
    NSString *name = url.lastPathComponent;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const BOOL scoped = [url startAccessingSecurityScopedResource];
        auto decoded = std::make_shared<atmos::AudioClip>();
        atmos::core::AudioFileInfo info;
        std::string error;
        const bool ok = atmos::ios::decodeAudioFile(url, *decoded, info, error);
        if (scoped) [url stopAccessingSecurityScopedResource];
        atmos::ClipPtr prepared = ok ? atmos::core::prepareClipForEngine(*decoded, rate) : nullptr;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!ok) { completion(NO, @(error.c_str())); return; }
            if (self->_engine->config().sampleRate != rate) {
                // Engine was rebuilt while decoding: resample for the new rate.
                const BOOL running = self.audioRunning;
                self->_originalTrack = decoded;
                [self rebuildEngineAtRate:self->_engine->config().sampleRate];
                if (running) [self startAudioWithError:nil];
            } else {
                self->_originalTrack = decoded;
                self->_engine->pause();
                self->_engine->seek(0);
                if (!self->_engine->setLayer(0, prepared, Bus::Music, 0)) { completion(NO, @"Engine rejected the track"); return; }
            }
            self->_loadedTrackName = name;
            completion(YES, nil);
        });
    });
}

- (void)unloadTrack {
    _engine->pause();
    _engine->clearLayer(0);
    _originalTrack.reset();
    _loadedTrackName = nil;
}

- (void)play { _engine->play(); }
- (void)pause { _engine->pause(); }
- (void)seekToSeconds:(double)s { _engine->seek(static_cast<int64_t>(s * _engine->config().sampleRate)); }
- (void)setLooping:(BOOL)looping { _settings.looping = looping; _engine->setLooping(looping); }

// ------------------------------------------------------------------ mix

- (void)setGain:(float)gain forBus:(AtmosBusId)bus {
    if (bus < 0 || bus >= atmos::kNumBuses) return;
    _settings.busGain[bus] = gain;
    _engine->setBusGain(static_cast<Bus>(bus), gain);
}
- (float)gainForBus:(AtmosBusId)bus {
    return (bus >= 0 && bus < atmos::kNumBuses) ? _settings.busGain[bus] : 0.0f;
}
- (void)setMuted:(BOOL)muted forBus:(AtmosBusId)bus {
    if (bus < 0 || bus >= atmos::kNumBuses) return;
    _settings.busMuted[bus] = muted;
    _engine->setBusMuted(static_cast<Bus>(bus), muted);
}
- (void)setMasterGain:(float)g { _settings.master = g; _engine->setMasterGain(g); }
- (void)setLimiterCeilingDb:(float)db { _settings.ceilingDb = db; _engine->setLimiterCeilingDb(db); }

- (void)setAtmosphereBedEnabled:(BOOL)e { _settings.bedEnabled = e; _engine->setAtmosphereBedEnabled(e); }
- (void)setAtmosphereBedLevel:(float)l { _settings.bedLevel = l; _engine->atmosphereBed().level.set(l); }
- (void)setAtmosphereBedColourHz:(float)hz { _settings.bedColour = hz; _engine->atmosphereBed().colourHz.set(hz); }

// ------------------------------------------------------------------ microphone

- (void)setMicEnabled:(BOOL)enabled completion:(void (^)(BOOL, NSString *_Nullable))completion {
    if (!enabled) {
        _wantsMic = NO;
        _engine->setMicInputEnabled(false);
        [self restartIOIfRunning]; // drops the input bus; the OS mic indicator turns off
        completion(YES, nil);
        return;
    }
    __weak AtmosEngineBridge *weakSelf = self;
    void (^enable)(void) = ^{
        AtmosEngineBridge *s = weakSelf;
        if (!s) return;
        s->_wantsMic = YES;
        s->_engine->setMicInputEnabled(true);
        if (s.audioRunning) {
            [s restartIOIfRunning];
        } else {
            NSError *err = nil;
            if (![s startAudioWithError:&err]) {
                s->_wantsMic = NO;
                s->_engine->setMicInputEnabled(false);
                completion(NO, err.localizedDescription);
                return;
            }
        }
        completion(YES, nil);
    };
    switch ([_session micPermission]) {
        case AtmosMicPermissionGranted: enable(); break;
        case AtmosMicPermissionDenied: completion(NO, @"Microphone access is off. Enable it in Settings > Privacy > Microphone."); break;
        case AtmosMicPermissionUndetermined:
            [_session requestMicPermission:^(BOOL granted) {
                if (granted) enable();
                else completion(NO, @"Microphone permission was not granted.");
            }];
            break;
    }
}

- (void)setMicToneHz:(float)v { _settings.micTone = v; _engine->micTransform().cutoffHz.set(v); }
- (void)setMicEchoSeconds:(float)v { _settings.micEcho = v; _engine->micTransform().echoSeconds.set(v); }
- (void)setMicFeedback:(float)v { _settings.micFeedback = v; _engine->micTransform().feedback.set(v); }
- (void)setMicWet:(float)v { _settings.micWet = v; _engine->micTransform().wet.set(v); }

// ------------------------------------------------------------------ safety

- (void)panic {
    _engine->panic();
    _engine->pause();
}
- (void)clearPanic { _engine->clearPanic(); }

// ------------------------------------------------------------------ meters

- (AtmosMeterSnapshot *)meters {
    _engine->collectGarbage();
    const atmos::EngineMeters m = _engine->meters();
    const atmos::EnvironmentFeatures f = _engine->environmentFeatures();
    const double rate = _engine->config().sampleRate;
    AtmosMeterSnapshot *s = [AtmosMeterSnapshot new];
    s.outputPeakLeft = m.outputPeak[0];
    s.outputPeakRight = m.outputPeak[1];
    s.busPeaks = @[@(m.busPeak[0]), @(m.busPeak[1]), @(m.busPeak[2]), @(m.busPeak[3])];
    s.limiterGain = m.limiterGain;
    s.positionSeconds = m.positionFrames / rate;
    s.durationSeconds = _engine->timelineLengthFrames() / rate;
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
    _engine->pause();
    [self.delegate engineBridgeStateDidChange:@"Audio interrupted by the system"];
}

- (void)audioSessionInterruptionEndedShouldResume:(BOOL)shouldResume {
    if (_wasRunningBeforeInterruption && shouldResume) {
        NSError *err = nil;
        [self startAudioWithError:&err];
    }
    [self.delegate engineBridgeStateDidChange:@"Audio interruption ended"];
}

- (void)audioSessionRouteChanged:(AtmosRouteInfo *)route oldDeviceUnavailable:(BOOL)oldDeviceUnavailable {
    if (oldDeviceUnavailable) {
        // Headphones removed: pause playback and silence live monitoring so audio does
        // not suddenly play from the speaker (and the mic cannot feed back).
        _engine->pause();
        [self setGain:0.0f forBus:AtmosBusIdMicRaw];
    }
    if (self.audioRunning && route.sampleRate > 0 && route.sampleRate != _engine->config().sampleRate) {
        [self rebuildEngineAtRate:route.sampleRate];
        NSError *err = nil;
        [self startAudioWithError:&err];
    }
    [self.delegate engineBridgeStateDidChange:oldDeviceUnavailable ? @"Headphones disconnected: paused" : @"Audio route changed"];
}

- (void)audioSessionMediaServicesWereReset {
    const BOOL wasRunning = self.audioRunning;
    _io.reset();
    _session = [AtmosAudioSession new];
    _session.delegate = self;
    [self rebuildEngineAtRate:_engine->config().sampleRate];
    if (wasRunning) [self startAudioWithError:nil];
    [self.delegate engineBridgeStateDidChange:@"Media services were reset"];
}

@end
