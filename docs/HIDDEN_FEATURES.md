# Features taken out of the app (developer list)

Owner, 2026-11-05: things hidden from listeners are removed from the app's code, and
the removed code is kept here so it can come back, most likely in Viksen Studio (the
separate creator app, which folds in Reuvo). The full files as they were are also in
git history: commit `5b458e9` is the last one with all of this code in the app.

The shared C++ engine (`engine/`, `core/`) still contains the underlying parts (the
atmosphere bed, room memory, capture/recording, project packages, Studio stem
sessions). The desktop and Windows apps and the engine tests use them, so they stay
there for now; the iPhone app no longer reaches them. Songs still set their own room
colour through "roomBlend" in world.json (echo on the beat, feedback, reverb, tone,
frequency shift, ducking), applied by `LiveSession::applyRoomBlend`.

## What was removed, and what listeners have now

| Feature | Removed from | Listeners now have | Better home |
|---|---|---|---|
| Atmosphere bed (switch, Density, Colour; "Weather shapes the atmosphere"; "Intensity shapes the atmosphere") | Settings > Sound, Senses; `EngineController.bed*`, `applyBed`; bridge `setAtmosphereBed*` | Real rain and thunder when it rains | Studio, or authored per Dimension |
| "Hear the room" (separate switch under the room blend) | Settings > Sound; `EngineController.hearRoom` | "Blend the room" alone (turns the microphone on, blends the room, turns it off again) | none needed |
| Room effect sliders: Tone, Echo time, Echo feedback, Effect mix | Settings > Sound; `EngineController.micToneHz/micEchoSeconds/micFeedback/micWet`; bridge `setMic*` | Each song sets the room's colour | Studio |
| Frequency shift and Shift amount | Same; `micShiftHz/micShiftMix`; `Feature.frequencyShift` | Songs can shift the room themselves ("shiftHz", "shiftMix") | Per-Dimension room design |
| Room memory (replays recent surroundings, slowed) and its level and pitch (branch room-instrument: back as a per-song sound, not a listener control) | Same; `roomMemory*`; `Feature.roomMemory`; bridge `setRoomMemory*` | nothing | Studio or per-Dimension room design |
| "Echo follows your steps" (Activity) | Senses; `echoFollowsSteps`, `applyCadence` | The echo follows the song's beat | Studio |
| Level sliders with meters (Music, Atmosphere, Raw mic, Transformed mic, Master) | Settings > Sound; `BusSliders`, `BusSlider`, `LevelMeter`, `EngineController.busGains/masterGain/setGain`, `Theme.color(for:)` | One "Room level" slider while the room is blended | Studio |
| Arrange the instruments (moving stems in 3D, saved layouts) | `EngineController.moveStem/resetPositions/savedPositions/applySavedPositions`; bridge `moveStem`, `resetStemPositions`; `Feature.spatialCanvas` | Each Dimension's authored 3D layout (saved layouts from older builds are ignored) | Studio |
| Studio session (your own stems in sync, mixer) | `EngineController` Studio section; bridge `loadStudioStems`, `studioPlay/Pause/Seek`, `setStem` | nothing | Studio |
| Library and music import (your own songs) | `MediaLibrary`; `loadTrack/unloadTrack`, `trackName`; bridge `loadTrack`, `play/pause/seek`, `setLooping`; `Feature.musicImport` | nothing | Studio |
| Your own Dimensions: create from Studio, import, export, delete | `saveStudioAsDimension`, `importDimension`, `exportDimension`, `deleteDimension`; bridge `validateDimensionFolder` | nothing (any already on a device still load) | Studio |
| Projects and autosave (.viksen packages) | `saveProject/openProject`, autosave timer, `syncSettingsFromEngine`; bridge `saveProject`, `openProject`, `currentSettings` | After an unexpected close, Home offers to play the last Dimension again | Studio |
| Recording / capture modes | bridge `startRecording`, `stopRecording`, `AtmosRecordMode` (screens were removed earlier) | nothing (owner: no recording in Viksen Dimensions) | Studio (own music only) |
| Unused tier features: atmosphereBed, stemImport, spatialComposition, advancedEffects, automation, export | `Store/Entitlements.swift` | — | Studio's own tiers |

When something else is taken out, add it here with where it was, what replaced it,
and its code below.

## Backed-up code

The code removed from the iPhone app (2026-11-05), file by file, as it was in commit
`5b458e9`. Two files were deleted whole: `Engine/MediaLibrary.swift` and
`Shared/LevelMeter.swift`. They appear in full below.

### `apps/ios/Viksen/App/ViksenApp.swift`

```swift
    @State private var library = MediaLibrary()
```

```swift
                    .environment(library)
```

### `apps/ios/Viksen/Bridge/AtmosEngineBridge.h`

```objc
typedef NS_ENUM(NSInteger, AtmosRecordMode) {
    AtmosRecordModeFullMix = 1,        // what you hear
    AtmosRecordModeWithoutMic = 2,     // music + atmosphere
    AtmosRecordModeAtmosphereOnly = 3,
    AtmosRecordModeEnvironmentOnly = 4 // microphone paths (explicit consent in the UI)
};

```

```objc
@property (nonatomic) NSInteger roomMemoryVoices;
```

```objc
@property (nonatomic, readonly, nullable) NSString *loadedTrackName;
```

```objc
// Decodes off the main thread; completion on main thread.
- (void)loadTrackAtURL:(NSURL *)url completion:(void (^)(BOOL ok, NSString *_Nullable error))completion
    NS_SWIFT_NAME(loadTrack(at:completion:));
- (void)unloadTrack;
- (void)play;
- (void)pause;
- (void)seekToSeconds:(double)seconds NS_SWIFT_NAME(seek(toSeconds:));
- (void)setLooping:(BOOL)looping;

```

```objc
- (float)gainForBus:(AtmosBusId)bus NS_SWIFT_NAME(gain(for:));
- (void)setMuted:(BOOL)muted forBus:(AtmosBusId)bus NS_SWIFT_NAME(setMuted(_:for:));
- (void)setMasterGain:(float)gain;
```

```objc
- (void)setAtmosphereBedEnabled:(BOOL)enabled;
- (void)setAtmosphereBedLevel:(float)level;
- (void)setAtmosphereBedColourHz:(float)hz;

```

```objc
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

```

```objc
// Validates a Dimension folder (world.json + every stem present, at most 16 stems, ≤ 2 GB).
// Returns nil if valid, otherwise a user-facing reason.
- (nullable NSString *)validateDimensionFolder:(NSURL *)folder NS_SWIFT_NAME(validateDimensionFolder(_:));
```

```objc
// 3D (binaural) rendering of Dimension stems and the spatial canvas.
```

```objc
- (void)moveStem:(NSInteger)index azimuth:(float)azimuth elevation:(float)elevation distance:(float)distance
    NS_SWIFT_NAME(moveStem(_:azimuth:elevation:distance:));
- (void)resetStemPositions;   // back to the Dimension's authored layout
```

```objc
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

```

### `apps/ios/Viksen/Bridge/AtmosEngineBridge.mm`

```objc
#include <set>
```

```objc
#include "core/assets/ClipPreparation.h"
```

```objc
#include "platform/ios/IOSMediaDecoder.h"
```

```objc
#include "core/projects/ProjectPackage.h"
#include "core/session/SessionDocument.h"
```

```objc
    NSString *_loadedTrackName;
```

```objc
- (NSString *)loadedTrackName { return _loadedTrackName; }
```

```objc
- (NSString *)loadedTrackName { return _loadedTrackName; }
```

```objc
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

```

```objc
- (float)gainForBus:(AtmosBusId)bus { return validBus(bus) ? _session->settings().busGain[bus] : 0.0f; }
- (void)setMuted:(BOOL)muted forBus:(AtmosBusId)bus { if (validBus(bus)) _session->setBusMuted(static_cast<Bus>(bus), muted); }
- (void)setMasterGain:(float)g { _session->setMasterGain(g); }
```

```objc
- (void)setAtmosphereBedEnabled:(BOOL)e { _session->setBedEnabled(e); }
- (void)setAtmosphereBedLevel:(float)l { _session->setBedLevel(l); }
- (void)setAtmosphereBedColourHz:(float)hz { _session->setBedColourHz(hz); }

```

```objc
- (void)setMicToneHz:(float)v { _session->setMicToneHz(v); }
- (void)setMicEchoSeconds:(float)v { _session->setMicEchoSeconds(v); }
- (void)setMicFeedback:(float)v { _session->setMicFeedback(v); }
- (void)setMicWet:(float)v { _session->setMicWet(v); }
- (void)setMicShiftHz:(float)v { _session->setMicShiftHz(v); }
- (void)setMicShiftMix:(float)v { _session->setMicShiftMix(v); }
```

```objc
- (void)setRoomMemoryEnabled:(BOOL)e { _session->setRoomMemoryEnabled(e); }
- (void)setRoomMemoryLevel:(float)v { _session->setRoomMemoryLevel(v); }
- (void)setRoomMemoryPitch:(float)v { _session->setRoomMemoryPitch(v); }
```

```objc
    s.roomMemoryVoices = engine.roomMemory().activeVoices();
```

```objc
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

```

```objc
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
```

### `apps/ios/Viksen/Engine/EngineController.swift`

```swift
    enum BusLevel: Int, CaseIterable, Identifiable {
        case music = 0, atmosphere, micRaw, micTransformed
        var id: Int { rawValue }
        var title: String {
            switch self {
            case .music: "Music"
            case .atmosphere: "Atmosphere"
            case .micRaw: "Raw mic"
            case .micTransformed: "Transformed mic"
            }
        }
    }

```

```swift
    private(set) var trackName: String?
    private(set) var trackURL: URL?
    private(set) var isLoadingTrack = false
```

```swift
    var busGains: [Float] = [1.0, 0.5, 0.0, 0.5]
    /// The microphone normally only lets the music sense the room. With this on, the room
    /// itself (transformed) is also heard under the music, at the mic bus levels.
    var hearRoom: Bool = UserDefaults.standard.bool(forKey: "hearRoom") {
        didSet {
            UserDefaults.standard.set(hearRoom, forKey: "hearRoom")
            applyRoomBlend()
            AppLog.info("Hear the room \(hearRoom ? "on" : "off")")
        }
    }
    var masterGain: Float = 0.7 { didSet { bridge.setMasterGain(masterGain) } }
    var looping = false { didSet { bridge.setLooping(looping) } }
    var bedEnabled = false { didSet { bridge.setAtmosphereBedEnabled(bedEnabled) } }
    /// The user's own bed settings; reactions (intensity, weather) are applied on top.
    var bedLevel: Float = 0.25 { didSet { applyBed() } }
    var bedColourHz: Float = 900 { didSet { applyBed() } }
    var micToneHz: Float = 1800 { didSet { bridge.setMicToneHz(micToneHz) } }
    var micEchoSeconds: Float = 0.38 { didSet { bridge.setMicEchoSeconds(micEchoSeconds) } }
    var micFeedback: Float = 0.45 { didSet { bridge.setMicFeedback(micFeedback) } }
    var micWet: Float = 0.6 { didSet { bridge.setMicWet(micWet) } }
```

```swift
    var micShiftHz: Float = -180 { didSet { bridge.setMicShiftHz(micShiftHz) } }
    var micShiftMix: Float = 0 { didSet { bridge.setMicShiftMix(micShiftMix) } }
    var roomMemoryEnabled = false { didSet { bridge.setRoomMemoryEnabled(roomMemoryEnabled) } }
    var roomMemoryLevel: Float = 0.6 { didSet { bridge.setRoomMemoryLevel(roomMemoryLevel) } }
    var roomMemoryPitch: Float = -12 { didSet { bridge.setRoomMemoryPitch(roomMemoryPitch) } }
```

```swift
    /// Echo time follows the listener's step rhythm (one echo per step).
    var echoFollowsSteps = false { didSet { applyCadence(activity.cadenceStepsPerMinute) } }
    private var echoBeforeStepSync: Float?
```

```swift
            if !healthEnabled { currentIntensity = nil; bridge.setExerciseIntensity(-1); applyBed() }
```

```swift
    /// Exercise intensity brightens and thickens the atmosphere bed.
    var intensityShapesAtmosphere = false { didSet { applyBed() } }
    private var currentIntensity: Double?
```

```swift
    /// Weather colours the atmosphere bed (rain darker and denser, clear days brighter).
    var weatherShapesAtmosphere = false { didSet { applyBed() } }
```

```swift
        for bus in BusLevel.allCases { busGains[bus.rawValue] = bridge.gain(for: AtmosBusId(rawValue: bus.rawValue)!) }
```

```swift
        activity.onCadence = { [weak self] spm in self?.applyCadence(spm) }
```

```swift
        health.onIntensity = { [weak self] i in
            self?.currentIntensity = i
            self?.bridge.setExerciseIntensity(i)
            self?.applyBed()
        }
```

```swift
        weather.onMood = { [weak self] _, _ in self?.applyBed() }
```

```swift
    private var augmentedResume: (dimension: String?, mic: Bool)?
```

```swift
    func togglePlay() {
        if !audioRunning { startAudio() }
        if meters.playing { bridge.pause() } else { bridge.clearPanic(); bridge.play() }
    }

    func seek(to seconds: Double) { bridge.seek(toSeconds: seconds) }

    /// Immediate silence: stops playback and every output path.
    func panic() {
        AppLog.warning("Stop all sound pressed")
        bridge.panic()
        statusMessage = "Output stopped. Press Start to resume."
    }

```

```swift
    func setGain(_ gain: Float, for bus: BusLevel) {
        busGains[bus.rawValue] = gain
        if bus == .micRaw || bus == .micTransformed { applyRoomBlend(); return }
        bridge.setGain(gain, for: AtmosBusId(rawValue: bus.rawValue)!)
    }

    /// Mic buses reach the ears only while "Hear the room" is on.
```

```swift
        for bus in [BusLevel.micRaw, .micTransformed] {
            bridge.setGain(hearRoom ? busGains[bus.rawValue] : 0, for: AtmosBusId(rawValue: bus.rawValue)!)
        }
    }

    // MARK: Media

    func loadTrack(url: URL) {
        isLoadingTrack = true
        bridge.loadTrack(at: url) { [weak self] ok, error in
            MainActor.assumeIsolated {
                guard let self else { return }
                self.isLoadingTrack = false
                if ok {
                    self.trackName = self.bridge.loadedTrackName
                    self.trackURL = url
                    AppLog.info("Song loaded (\(url.pathExtension.lowercased()))")
                } else { self.lastError = error }
            }
        }
    }

    func unloadTrack() {
        bridge.unloadTrack()
        trackName = nil
        trackURL = nil
```

```swift
    var roomBlendOn: Bool { hearRoom && micEnabled }
```

```swift
            hearRoom = true
            // The room needs a level to be heard; give it a gentle default the first time.
            if busGains[BusLevel.micTransformed.rawValue] < 0.05 { setGain(0.5, for: .micTransformed) }
```

```swift
            hearRoom = false
```

```swift
        if tier < Feature.roomMemory.requiredTier { roomMemoryEnabled = false }
        if tier < Feature.frequencyShift.requiredTier { micShiftMix = 0 }
```

```swift
        if tier < Feature.activity.requiredTier { echoFollowsSteps = false; activityEnabled = false }
        if tier < Feature.health.requiredTier { intensityShapesAtmosphere = false; healthEnabled = false }
        if tier < Feature.weather.requiredTier { weatherShapesAtmosphere = false; weatherEnabled = false }
        if tier < Feature.musicImport.requiredTier, trackName != nil { unloadTrack() }
    }

    // MARK: Body & motion

    private func applyCadence(_ spm: Double?) {
        guard echoFollowsSteps, let spm else {
            if let saved = echoBeforeStepSync { micEchoSeconds = saved; echoBeforeStepSync = nil }
            return
        }
        if echoBeforeStepSync == nil { echoBeforeStepSync = micEchoSeconds }
        let seconds = Float(min(1.5, max(0.2, 60.0 / spm)))
        // Ignore tiny cadence wobbles so the echo does not constantly re-tune.
        if abs(seconds - micEchoSeconds) / micEchoSeconds > 0.03 { micEchoSeconds = seconds }
    }

    /// Effective bed = user settings + enabled reactions. The sliders keep showing
    /// the user's own values; only what is sent to the engine changes.
    private func applyBed() {
        var level = bedLevel
        var colour = bedColourHz
        if intensityShapesAtmosphere, let i = currentIntensity {
            level += 0.15 * Float(i)
            colour += 3000 * Float(i)
        }
        if weatherShapesAtmosphere, let mood = weather.mood {
            switch mood {
            case .storm: level += 0.12; colour *= 0.45
            case .rain: level += 0.08; colour *= 0.6
            case .snow: level += 0.04; colour *= 0.5
            case .fog: colour *= 0.7
            case .wind: level += 0.06; colour *= 1.2
            case .cloudy: colour *= 0.9
            case .clear: colour *= weather.isDaylight ? 1.4 : 1.0
            }
            if !weather.isDaylight { colour *= 0.8 }
        }
        bridge.setAtmosphereBedLevel(min(0.6, max(0, level)))
        bridge.setAtmosphereBedColourHz(min(8000, max(120, colour)))
```

```swift
        let active = worldStatus.loaded && worldStatus.playing && worldStatus.worldID != "studio" ? worldStatus.worldID : nil
```

```swift
        let loadedID = worldStatus.loaded && worldStatus.worldID != "studio" ? worldStatus.worldID : nil
```

```swift
            applySavedPositions(worldStatus.worldID)   // the new song has its own stems
```

```swift
            let audible = worldStatus.playing && masterGain > 0.001 && busGains[BusLevel.music.rawValue] > 0.001
```

```swift
    // MARK: Studio (the user's own stems)

    struct StudioStem: Identifiable, Equatable {
        let id = UUID()
        let url: URL
        var title: String { url.deletingPathExtension().lastPathComponent }
        var gainDb: Float = 0
        var muted = false
        var solo = false
        var lengthSeconds: Double = 0
    }

    private(set) var studioStems: [StudioStem] = []
    var studioActive: Bool { worldStatus.loaded && worldStatus.worldID == "studio" }
    var studioPlaying: Bool { studioActive && worldStatus.playing }
    /// Stems differ in length by more than half a second (they stay aligned at their start).
    var studioLengthsDiffer: Bool {
        guard let a = studioStems.map(\.lengthSeconds).min(), let b = studioStems.map(\.lengthSeconds).max() else { return false }
        return b - a > 0.5
    }

    static var studioFolder: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Studio", isDirectory: true)
    }

    /// Copies the picked files (originals untouched) and loads them as one synchronised session.
    func importStudioStems(_ picked: [URL]) {
        let folder = Self.studioFolder.appendingPathComponent(
            ISO8601DateFormatter().string(from: Date()).replacingOccurrences(of: ":", with: "-"), isDirectory: true)
        var copies: [URL] = []
        do {
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
            for src in picked.prefix(8) {
                let scoped = src.startAccessingSecurityScopedResource()
                defer { if scoped { src.stopAccessingSecurityScopedResource() } }
                let dest = folder.appendingPathComponent(src.lastPathComponent)
                try FileManager.default.copyItem(at: src, to: dest)
                copies.append(dest)
            }
        } catch {
            lastError = error.localizedDescription
            return
        }
        loadStudio(copies.map { StudioStem(url: $0) })
        if picked.count > 8 { statusMessage = "Studio uses the first 8 stems." }
    }

    func loadStudio(_ stems: [StudioStem]) {
        do {
            try bridge.loadStudioStems(stems.map(\.url))
            if trackName != nil { bridge.pause() }
            applySpatial(force: true)
            studioStems = stems.map { st in
                var s = st
                if let f = try? AVAudioFile(forReading: st.url) { s.lengthSeconds = Double(f.length) / f.fileFormat.sampleRate }
                return s
            }
            applyStudioMix()
            AppLog.info("Studio: \(stems.count) stems loaded")
        } catch {
            lastError = error.localizedDescription
        }
    }

    func studioTogglePlay() {
        guard studioActive else { return }
        if studioPlaying { bridge.studioPause() } else {
            if !audioRunning { startAudio() }
            bridge.clearPanic()
            bridge.studioPlay()
        }
    }

    func studioSeek(_ seconds: Double) { bridge.studioSeek(toSeconds: seconds) }

    func setStudioStem(_ id: UUID, gainDb: Float? = nil, muted: Bool? = nil, solo: Bool? = nil) {
        guard let i = studioStems.firstIndex(where: { $0.id == id }) else { return }
        if let gainDb { studioStems[i].gainDb = gainDb }
        if let muted { studioStems[i].muted = muted }
        if let solo { studioStems[i].solo = solo }
        applyStudioMix()
    }

    /// Solo: when any stem is soloed, every non-soloed stem is silent.
    private func applyStudioMix() {
        let anySolo = studioStems.contains { $0.solo }
        for (i, st) in studioStems.enumerated() {
            bridge.setStem(i, gainDb: st.gainDb, muted: st.muted || (anySolo && !st.solo))
        }
    }

    private var studioStemDictionaries: [[String: Any]] {
        studioActive ? studioStems.map { ["url": $0.url, "gainDb": $0.gainDb, "muted": $0.muted] } : []
    }

    // MARK: User-made Dimensions (create, import, export, delete)

    static var userDimensionsFolder: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Dimensions", isDirectory: true)
    }
```

```swift
    /// Removes one downloaded track (it can be downloaded again). If its Dimension is
    /// playing, the Dimension stops first so the audio can go.
    func removeTrack(_ track: DimensionDownloads.Track) {
        stopIfPlaying(track.dimensionID)
        downloads.remove(track)
    }

```

```swift
    /// Song titles of a Dimension (for Dimensions whose audio ships with the app).
    func bridgeSongTitles(_ world: AtmosWorldInfo) -> [String] {
        bridge.songs(forWorld: world.worldID).compactMap { $0["title"] as? String }
    }

```

```swift
    private static func slug(_ name: String) -> String {
        let allowed = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-"))
        let base = name.lowercased().replacingOccurrences(of: " ", with: "-")
            .unicodeScalars.filter { allowed.contains($0) }.map(String.init).joined()
        return "user-" + (base.isEmpty ? "dimension" : String(base.prefix(40)))
    }

    /// Turns the current Studio session into a Dimension of your own (stems copied,
    /// current levels and 3D layout kept).
    func saveStudioAsDimension(named name: String) {
        guard studioActive, !studioStems.isEmpty else { return }
        let fm = FileManager.default
        var id = Self.slug(name), n = 2
        while fm.fileExists(atPath: Self.userDimensionsFolder.appendingPathComponent(id).path) { id = Self.slug(name) + "-\(n)"; n += 1 }
        let folder = Self.userDimensionsFolder.appendingPathComponent(id, isDirectory: true)
        do {
            try fm.createDirectory(at: folder, withIntermediateDirectories: true)
            var stems: [[String: Any]] = []
            for (i, st) in studioStems.enumerated() {
                let file = String(format: "%02d-", i) + st.url.lastPathComponent
                try fm.copyItem(at: st.url, to: folder.appendingPathComponent(file))
                let pos = worldStatus.stems.indices.contains(i) ? worldStatus.stems[i] : nil
                stems.append(["id": "stem\(i)", "file": file, "title": st.title,
                              "baseGainDb": st.muted ? -60 : Double(st.gainDb),
                              "spatial": ["azimuth": Double(pos?.azimuth ?? 0), "elevation": Double(pos?.elevation ?? 0),
                                          "distance": Double(pos?.distance ?? 1.6), "width": Double(pos?.width ?? 40)]])
            }
            let json: [String: Any] = ["format": "atmos.world", "schemaVersion": 1, "id": id, "name": name,
                                       "artist": "", "tier": "free", "fadeSeconds": 4,
                                       "music": ["tempo": 0, "key": "", "loop": ["fromSeconds": 0]],   // unknown tempo
                                       "stems": stems, "rules": []]
            let data = try JSONSerialization.data(withJSONObject: json, options: [.prettyPrinted, .sortedKeys])
            try data.write(to: folder.appendingPathComponent("world.json"))
            refreshWorlds()
            statusMessage = "Saved Dimension \u{201C}\(name)\u{201D}"
            AppLog.info("User Dimension created")
        } catch {
            try? fm.removeItem(at: folder)
            lastError = error.localizedDescription
        }
    }

    /// Imports a Dimension folder (world.json + audio) chosen in Files.
    func importDimension(from picked: URL) {
        let scoped = picked.startAccessingSecurityScopedResource()
        defer { if scoped { picked.stopAccessingSecurityScopedResource() } }
        if let problem = bridge.validateDimensionFolder(picked) { lastError = problem; return }
        guard let data = try? Data(contentsOf: picked.appendingPathComponent("world.json")),
              var json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
            lastError = "Not a Dimension."; return
        }
        let fm = FileManager.default
        var id = Self.slug((json["name"] as? String) ?? "dimension"), n = 2
        while fm.fileExists(atPath: Self.userDimensionsFolder.appendingPathComponent(id).path) { id = Self.slug((json["name"] as? String) ?? "dimension") + "-\(n)"; n += 1 }
        json["id"] = id          // user Dimensions get their own id (never a built-in's)
        let dest = Self.userDimensionsFolder.appendingPathComponent(id, isDirectory: true)
        do {
            try fm.createDirectory(at: Self.userDimensionsFolder, withIntermediateDirectories: true)
            try fm.copyItem(at: picked, to: dest)
            let out = try JSONSerialization.data(withJSONObject: json, options: [.prettyPrinted, .sortedKeys])
            try out.write(to: dest.appendingPathComponent("world.json"))
            refreshWorlds()
            statusMessage = "Imported \u{201C}\((json["name"] as? String) ?? id)\u{201D}"
            AppLog.info("Dimension imported")
        } catch {
            try? fm.removeItem(at: dest)
            lastError = error.localizedDescription
        }
    }

    func deleteDimension(_ info: AtmosWorldInfo) {
        guard info.userMade, let folder = info.folderURL else { return }   // built-ins can never be deleted
        if worldStatus.loaded && worldStatus.worldID == info.worldID { bridge.unloadWorld() }
        try? FileManager.default.removeItem(at: folder)
        refreshWorlds()
        AppLog.info("User Dimension deleted")
    }

    /// Zips a user-made Dimension for sharing. Built-in Dimensions are never exportable.
    func exportDimension(_ info: AtmosWorldInfo) -> URL? {
        guard info.userMade, let folder = info.folderURL else { return nil }
        var result: URL?
        var coordError: NSError?
        NSFileCoordinator().coordinate(readingItemAt: folder, options: .forUploading, error: &coordError) { zipped in
            let dest = FileManager.default.temporaryDirectory.appendingPathComponent(info.name + ".zip")
            try? FileManager.default.removeItem(at: dest)
            if (try? FileManager.default.copyItem(at: zipped, to: dest)) != nil { result = dest }
        }
        if let coordError { lastError = coordError.localizedDescription }
        if result != nil { AppLog.info("Dimension exported") }
        return result
    }

    // MARK: Projects, autosave, crash recovery
```

```swift
    static var projectsFolder: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Projects", isDirectory: true)
    }
```

```swift
    static var autosaveURL: URL { supportFolder.appendingPathComponent("Autosave.viksen", isDirectory: true) }
```

```swift
    /// True when the previous run ended without going to the background first (crash or kill).
```

```swift
    var hasAutosave: Bool { FileManager.default.fileExists(atPath: Self.autosaveURL.appendingPathComponent("manifest.json").path) }
    private var autosaveTimer: Timer?
    private var lastAutosaveSignature = ""
```

```swift
    private var currentDimensionID: String? { worldStatus.loaded && worldStatus.worldID != "studio" ? worldStatus.worldID : nil }

    func saveProject(named name: String) {
        let clean = name.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-")
        guard !clean.isEmpty else { return }
        let url = Self.projectsFolder.appendingPathComponent(clean + ".viksen", isDirectory: true)
        do {
            try FileManager.default.createDirectory(at: Self.projectsFolder, withIntermediateDirectories: true)
            try bridge.saveProject(at: url, name: clean, song: trackURL, dimension: currentDimensionID,
                                   stems: studioStemDictionaries)
            statusMessage = "Saved project \u{201C}\(clean)\u{201D}"
            AppLog.info("Project saved")
        } catch {
            lastError = error.localizedDescription
        }
    }

    func openProject(_ url: URL) {
        do {
            let info = try bridge.openProject(at: url)   // throws when the package cannot be read
            syncSettingsFromEngine()
            if let song = info["song"] as? URL { loadTrack(url: song) } else if trackName != nil { unloadTrack() }
            if let stems = info["stems"] as? [[String: Any]], !stems.isEmpty {
                loadStudio(stems.compactMap { d in
                    guard let u = d["url"] as? URL else { return nil }
                    var st = StudioStem(url: u)
                    st.gainDb = (d["gainDb"] as? NSNumber)?.floatValue ?? 0
                    st.muted = (d["muted"] as? NSNumber)?.boolValue ?? false
                    return st
                })
            } else if let dim = info["dimension"] as? String, let world = worlds.first(where: { $0.worldID == dim }) {
                try? bridge.loadWorld(world.worldID)       // selected, not auto-played
                applySpatial(force: true)
                applySavedPositions(world.worldID)
            }
            statusMessage = "Opened \u{201C}\((info["name"] as? String) ?? url.deletingPathExtension().lastPathComponent)\u{201D}"
            AppLog.info("Project opened")
        } catch {
            lastError = error.localizedDescription
        }
    }
```

```swift
        guard autosaveTimer == nil else { return }   // once per launch
```

```swift
        previousRunEndedUnexpectedly = fm.fileExists(atPath: Self.runningFlag.path) && hasAutosave
        if previousRunEndedUnexpectedly { AppLog.warning("Previous session ended unexpectedly; autosave available") }
```

```swift
        autosaveTimer = Timer.scheduledTimer(withTimeInterval: 30, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.autosave() }
        }
```

```swift
        autosave()   // music, and the opening sound, keep playing in the background
```

```swift
        let loaded = st.loaded && st.worldID != "studio"
```

```swift
        if activeDimensionID == nil && !studioActive { threshold.start(engineAudioRunning: audioRunning) }
```

```swift
    func restoreAutosave() {
        openProject(Self.autosaveURL)
```

```swift
    /// Saves only when something changed since the last autosave.
    private func autosave() {
        let signature = [String(describing: busGains), "\(masterGain)", "\(bedEnabled)\(bedLevel)\(bedColourHz)",
                         "\(micToneHz)\(micEchoSeconds)\(micFeedback)\(micWet)\(micShiftHz)\(micShiftMix)",
                         "\(roomMemoryEnabled)\(roomMemoryLevel)\(roomMemoryPitch)\(looping)\(limiterCeilingDb)",
                         trackURL?.path ?? "-", currentDimensionID ?? "-",
                         studioStems.map { "\($0.url.lastPathComponent)\($0.gainDb)\($0.muted)" }.joined()].joined(separator: "|")
        guard signature != lastAutosaveSignature else { return }
        do {
            try bridge.saveProject(at: Self.autosaveURL, name: "Autosave", song: trackURL, dimension: currentDimensionID,
                                   stems: studioStemDictionaries)
            lastAutosaveSignature = signature
        } catch {
            AppLog.error("Autosave failed: \(error.localizedDescription)")
        }
    }

    /// After a project is applied in the engine, mirror its values into the UI state.
    /// (didSet handlers re-send the same values, which is harmless.)
    private func syncSettingsFromEngine() {
        let s = bridge.currentSettings()
        let engineGains = (0..<4).map { Float(truncating: s["busGain\($0)"] ?? 0) }
        // While the room is not heard the engine's mic buses read 0: keep the user's levels.
        busGains = engineGains.indices.map { i in
            (!hearRoom && (i == BusLevel.micRaw.rawValue || i == BusLevel.micTransformed.rawValue)) ? busGains[i] : engineGains[i]
        }
        applyRoomBlend()
        masterGain = Float(truncating: s["masterGain"] ?? 0.7)
        limiterCeilingDb = Float(truncating: s["limiterCeilingDb"] ?? -1)
        bedEnabled = (s["bedEnabled"]?.boolValue) ?? false
        bedLevel = Float(truncating: s["bedLevel"] ?? 0.25)
        bedColourHz = Float(truncating: s["bedColourHz"] ?? 900)
        micToneHz = Float(truncating: s["micToneHz"] ?? 1800)
        micEchoSeconds = Float(truncating: s["micEchoSeconds"] ?? 0.38)
        micFeedback = Float(truncating: s["micFeedback"] ?? 0.45)
        micWet = Float(truncating: s["micWet"] ?? 0.6)
        micShiftHz = Float(truncating: s["micShiftHz"] ?? -180)
        micShiftMix = Float(truncating: s["micShiftMix"] ?? 0)
        roomMemoryEnabled = (s["roomMemoryEnabled"]?.boolValue) ?? false
        roomMemoryLevel = Float(truncating: s["roomMemoryLevel"] ?? 0.6)
        roomMemoryPitch = Float(truncating: s["roomMemoryPitch"] ?? -12)
        looping = (s["looping"]?.boolValue) ?? false
    }

```

```swift
        if studioActive { studioStems = [] }   // a Dimension replaces the Studio session
```

```swift
            applySavedPositions(world.worldID)
```

```swift
            if trackName != nil { bridge.pause() }   // one music source at a time
```

```swift
            guard let self, self.activeDimensionID == nil, !self.studioActive else { return }
```

```swift
    // MARK: 3D / spatial canvas
```

```swift
    /// Moves a stem on the spatial canvas and remembers it for this Dimension.
    func moveStem(_ index: Int, azimuth: Float, distance: Float) {
        guard worldStatus.loaded, index < worldStatus.stems.count else { return }
        let elevation = worldStatus.stems[index].elevation
        bridge.moveStem(index, azimuth: azimuth, elevation: elevation, distance: distance)
        var saved = savedPositions(worldStatus.worldID)
        // Keyed by instrument, so a move carries across the songs of a Dimension.
        saved[worldStatus.stems[index].title] = [Double(azimuth), Double(elevation), Double(distance)]
        UserDefaults.standard.set(saved, forKey: "positions." + worldStatus.worldID)
    }

    /// Forgets canvas moves for a Dimension and restores its authored layout
    /// immediately, without interrupting the music.
    func resetPositions(_ worldID: String) {
        UserDefaults.standard.removeObject(forKey: "positions." + worldID)
        AppLog.info("Reset 3D layout for \(worldID)")
        if worldStatus.loaded && worldStatus.worldID == worldID { bridge.resetStemPositions() }
    }

    private func savedPositions(_ worldID: String) -> [String: [Double]] {
        UserDefaults.standard.dictionary(forKey: "positions." + worldID) as? [String: [Double]] ?? [:]
    }

    private func applySavedPositions(_ worldID: String) {
        let saved = savedPositions(worldID)
        guard !saved.isEmpty else { return }
        for (i, stem) in bridge.worldStatus().stems.enumerated() {
            guard let v = saved[stem.title], v.count == 3 else { continue }
            bridge.moveStem(i, azimuth: Float(v[0]), elevation: Float(v[1]), distance: Float(v[2]))
        }
    }

```

```swift
        let playingDimension = worldStatus.playing && worldStatus.worldID != "studio" ? worldStatus.worldID : nil
        augmentedResume = (playingDimension, micEnabled)
```

```swift
        applyRoomBlend()                                    // restore the user's mic levels for later
```

```swift
        if resume.mic && micPermission == .granted { setMicEnabled(true) }
```

### `apps/ios/Viksen/Engine/MediaLibrary.swift`

```swift
import Foundation
import Observation

/// Imported media lives in the app's own Documents/Library/Audio folder. Imports
/// are copies: the user's original file is never modified or moved.
@MainActor
@Observable
final class MediaLibrary {
    struct Item: Identifiable, Hashable {
        let url: URL
        var id: URL { url }
        var name: String { url.lastPathComponent }
        let byteSize: Int64
        let added: Date
    }

    private(set) var items: [Item] = []
    var lastError: String?

    let audioDirectory: URL = {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return docs.appendingPathComponent("Library/Audio", isDirectory: true)
    }()

    init() {
        try? FileManager.default.createDirectory(at: audioDirectory, withIntermediateDirectories: true)
        refresh()
    }

    func refresh() {
        let keys: [URLResourceKey] = [.fileSizeKey, .creationDateKey]
        let urls = (try? FileManager.default.contentsOfDirectory(at: audioDirectory, includingPropertiesForKeys: keys)) ?? []
        items = urls.compactMap { url in
            let v = try? url.resourceValues(forKeys: Set(keys))
            return Item(url: url, byteSize: Int64(v?.fileSize ?? 0), added: v?.creationDate ?? .distantPast)
        }
        .sorted { $0.added > $1.added }
    }

    /// Copies a user-picked file (security-scoped) into the library.
    func importCopy(of source: URL) -> URL? {
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        var dest = audioDirectory.appendingPathComponent(source.lastPathComponent)
        var n = 1
        while FileManager.default.fileExists(atPath: dest.path) {
            let base = source.deletingPathExtension().lastPathComponent
            dest = audioDirectory.appendingPathComponent("\(base)-\(n).\(source.pathExtension)")
            n += 1
        }
        do {
            try FileManager.default.copyItem(at: source, to: dest)
            refresh()
            return dest
        } catch {
            lastError = error.localizedDescription
            return nil
        }
    }

    func delete(_ item: Item) {
        do { try FileManager.default.removeItem(at: item.url) } catch { lastError = error.localizedDescription }
        refresh()
    }
}
```

### `apps/ios/Viksen/Features/Experience/ExperienceView.swift`

```swift
/// The level sliders with live meters: their own view, so only they redraw with the meters.
struct BusSliders: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
        ForEach(EngineController.BusLevel.allCases) { bus in
            BusSlider(title: bus.title,
                      color: Theme.color(for: bus),
                      value: Binding(get: { engine.busGains[bus.rawValue] },
                                     set: { engine.setGain($0, for: bus) }),
                      peak: engine.meters.busPeaks.indices.contains(bus.rawValue)
                          ? engine.meters.busPeaks[bus.rawValue].floatValue : 0)
        }
        BusSlider(title: "Master", color: .primary, value: $engine.masterGain,
                  peak: max(engine.meters.outputPeakLeft, engine.meters.outputPeakRight))
    }
}

```

```swift
/// Optional: hear the surroundings, transformed, under the music.
```

```swift
        if !engine.onHeadphones {
            Label("Connect headphones to blend the room into the music.", systemImage: "headphones")
                .font(.footnote).foregroundStyle(.secondary)
        }
        Toggle("Hear the room", isOn: $engine.hearRoom)
            .disabled(!engine.onHeadphones)
        if engine.hearRoom && !engine.micEnabled {
            Text("Turn on \u{201C}Listen to the room\u{201D} above to hear it.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        if engine.hearRoom && engine.micEnabled && engine.onHeadphones {
            LabeledSlider(title: "Tone", value: $engine.micToneHz, range: 200...12000, format: "%.0f Hz")
            LabeledSlider(title: "Echo time", value: $engine.micEchoSeconds, range: 0.05...1.5, format: "%.2f s")
            LabeledSlider(title: "Echo feedback", value: $engine.micFeedback, range: 0...0.9, format: "%.2f")
            LabeledSlider(title: "Effect mix", value: $engine.micWet, range: 0...1, format: "%.2f")
            Gated(feature: .frequencyShift, title: "Frequency shift") {
                LabeledSlider(title: "Frequency shift", value: $engine.micShiftHz, range: -600...600, format: "%.0f Hz")
                LabeledSlider(title: "Shift amount", value: $engine.micShiftMix, range: 0...1, format: "%.2f")
            }
            Gated(feature: .roomMemory, title: "Room memory") {
                Toggle("Room memory", isOn: $engine.roomMemoryEnabled)
            }
            if engine.roomMemoryEnabled {
                Text("Replays the last few seconds of your surroundings, slowed and faded, as a drifting texture. Held in memory only and erased when the microphone turns off.")
                    .font(.footnote).foregroundStyle(.secondary)
                LabeledSlider(title: "Memory level", value: $engine.roomMemoryLevel, range: 0...1, format: "%.2f")
                LabeledSlider(title: "Memory pitch", value: $engine.roomMemoryPitch, range: -12...12, format: "%.0f st")
            }
```

```swift
                Text(String(format: format, value)).font(.caption.monospacedDigit()).foregroundStyle(.secondary)
```

```swift
                    Toggle("Echo follows your steps", isOn: $engine.echoFollowsSteps)
```

```swift
                Toggle("Intensity shapes the atmosphere", isOn: $engine.intensityShapesAtmosphere)
```

```swift
                Toggle("Weather shapes the atmosphere", isOn: $engine.weatherShapesAtmosphere)
```

### `apps/ios/Viksen/Features/Home/HomeView.swift`

```swift
            Text("Restore your last session? The microphone stays off until you turn it on.")
```

```swift
                Button("Restore") { engine.restoreAutosave(); nav.go(.experience) }
```

### `apps/ios/Viksen/Features/Settings/SettingsView.swift`

```swift
                     ? "Your surroundings, echoed on the song's beat and reverberated, under the music. Nothing is recorded, saved or uploaded."
```

```swift
            Section("Atmosphere") {
                Toggle("Atmosphere bed", isOn: $engine.bedEnabled)
                if engine.bedEnabled {
                    LabeledSlider(title: "Density", value: $engine.bedLevel, range: 0...0.6, format: "%.2f")
                    LabeledSlider(title: "Colour", value: $engine.bedColourHz, range: 150...6000, format: "%.0f Hz")
                }
                BusSliders()
            }
```

### `apps/ios/Viksen/Shared/LevelMeter.swift`

```swift
import SwiftUI

/// Horizontal peak meter, dB-scaled (-60 dBFS .. 0 dBFS).
struct LevelMeter: View {
    var peak: Float
    var color: Color

    private var fraction: CGFloat {
        guard peak > 0 else { return 0 }
        let db = 20 * log10(Double(peak))
        return CGFloat(min(1, max(0, (db + 60) / 60)))
    }

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                Capsule().fill(Color.secondary.opacity(0.18))
                Capsule().fill(color.opacity(0.85)).frame(width: geo.size.width * fraction)
            }
        }
        .frame(height: 4)
        .accessibilityHidden(true)
    }
}

/// Labelled level slider used for every bus. Shows the live bus peak beneath.
struct BusSlider: View {
    var title: String
    var color: Color
    @Binding var value: Float
    var peak: Float
    var range: ClosedRange<Float> = 0...1

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Circle().fill(color).frame(width: 8, height: 8)
                Text(title).font(.subheadline)
                Spacer()
                Text(value <= 0 ? "off" : String(format: "%.0f dB", 20 * log10(Double(value))))
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
            }
            Slider(value: $value, in: range)
                .tint(color)
                .accessibilityLabel(title)
            LevelMeter(peak: peak, color: color)
        }
    }
}

/// Honest status for workspaces whose functionality has not been built yet.
/// Intentionally contains no interactive controls (spec §12: no fake controls).
struct NotYetAvailableView: View {
    var title: String
    var systemImage: String
    var phase: String
    var summary: String
    var planned: [String]

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Label(phase, systemImage: "hammer").font(.caption).foregroundStyle(.secondary)
                        Text(summary).font(.body)
                    }
                    .padding(.vertical, 4)
                }
                Section("Planned") {
                    ForEach(planned, id: \.self) { item in
                        Text(item).foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle(title)
        }
    }
}
```

### `apps/ios/Viksen/Shared/Theme.swift`

```swift

    static func color(for bus: EngineController.BusLevel) -> Color {
        switch bus {
        case .music: music
        case .atmosphere: atmosphere
        case .micRaw: micRaw
        case .micTransformed: micTransformed
        }
    }
```

### `apps/ios/Viksen/Store/Entitlements.swift`

```swift
    case atmosphereBed, liveMicBasic, motionSensing
    // Experience: full listening experience, environmental transformations
    case frequencyShift, roomMemory, headTracking, activity, health, weather, expandedWorlds, spatialCanvas
    // Studio: music and stem imports, spatial composition, advanced effects, automation, export
    case musicImport, stemImport, spatialComposition, advancedEffects, automation, export
```

```swift
        case .atmosphereBed, .liveMicBasic, .motionSensing, .weather: .free   // rain plays storm songs on every tier
        // Free keeps 3D audio itself ("limited spatial controls"); arranging it is Experience.
        case .frequencyShift, .roomMemory, .headTracking, .activity, .health, .expandedWorlds, .spatialCanvas: .experience
        case .musicImport, .stemImport, .spatialComposition, .advancedEffects, .automation, .export: .studio
```
