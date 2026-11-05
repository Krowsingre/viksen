import AVFoundation
import Foundation
import Observation

/// Swift-side owner of the engine bridge. All UI state that mirrors the engine
/// lives here; views never talk to the bridge directly.
@MainActor
@Observable
final class EngineController {
    private let bridge = AtmosEngineBridge()
    let headTracker = HeadTracker()
    let activity = ActivityMonitor()
    let health = HealthService()
    let watch = WatchLink()
    let downloads = DimensionDownloads()
    let albums = AlbumLinks()
    let progress = ProgressStore()
    let nightClock = NightClock()
    private let nowPlaying = NowPlaying()
    let threshold = ThresholdSound()
    private let rainAmbience = RainAmbience()
    private let transitionCue = TransitionCue()
    private var appliedSpatial: Bool?
    private var headphonesForAudio: Bool {
        #if os(macOS)
        true   // a Mac cannot tell headphones from speakers
        #else
        onHeadphones
        #endif
    }
    /// Separate settings for headphones and for the device's speaker (owner, 2026-11-05):
    /// 3D audio and adaptive music, applied for whichever output is in use.
    private func applySpatial(force: Bool = false) {
        let phones = headphonesForAudio
        let adaptive = phones ? adaptiveOnHeadphones : adaptiveOnSpeaker
        if bridge.adaptive != adaptive { bridge.adaptive = adaptive }
        let spatial = phones ? dimension3D : spatialOnSpeaker
        guard force || spatial != appliedSpatial else { return }
        appliedSpatial = spatial
        bridge.setDimensionSpatialEnabled(spatial)
    }
    private var songTime: String?   // "day", "night" or "rain": what the Dimension plays now
    private var lastProgressTick = Date.distantPast
    private let delegateProxy = BridgeDelegateProxy()
    private var meterTimer: Timer?

    // Mirrors of engine state (source of truth for the UI).
    private(set) var audioRunning = false
    private(set) var micEnabled = false
    private(set) var meters = AtmosMeterSnapshot()
    private(set) var worldStatus = AtmosWorldStatus()
    // Slow-changing values, set only when they change, so views that need just these do
    // not redraw with every 30 Hz meter update.
    private(set) var activeDimensionID: String?   // the Dimension playing now (not Studio)
    private(set) var loadedDimensionID: String?   // the Dimension loaded (playing or not)
    private(set) var micLive = false
    private(set) var limiting = false
    private(set) var moonPhaseID = ""
    private(set) var lunarDay = 0
    private(set) var worlds: [AtmosWorldInfo] = []
    private(set) var route = AtmosRouteInfo()
    private(set) var statusMessage: String?
    var lastError: String? { didSet { if let e = lastError { AppLog.error(e) } } }
    private var loggedUnderruns: UInt64 = 0
    private var lastUnderrunLog = Date.distantPast

    var limiterCeilingDb: Float = -1 { didSet { bridge.setLimiterCeilingDb(limiterCeilingDb) } }
    /// How loud the room is under the music while "Blend the room" is on (0...1). Each
    /// song sets the room's colour (echo, reverb, tone) itself.
    var roomLevel: Float = UserDefaults.standard.object(forKey: "roomLevel") as? Float ?? 0.5 {
        didSet { UserDefaults.standard.set(roomLevel, forKey: "roomLevel"); applyRoomBlend() }
    }
    var motionSensing = false { didSet { bridge.setMotionSensingEnabled(motionSensing) } }
    var motionSensingAvailable: Bool { bridge.motionSensingAvailable }

    // Body & motion (each reaction can be switched off individually).
    var headTrackingEnabled = false {
        didSet { headTrackingEnabled ? headTracker.start() : headTracker.stop() }
    }
    var activityEnabled = false {
        didSet { activityEnabled ? activity.start() : activity.stop() }
    }

    var healthEnabled = false {
        didSet {
            healthEnabled ? health.start() : health.stop()
            if !healthEnabled { bridge.setExerciseIntensity(-1) }
        }
    }

    let weather = WeatherMonitor()
    /// Weather (rain plays a Dimension's storm song). Remembered, and on by default
    /// (owner, 2026-11-05: rain must work without hunting for a setting); iOS asks once for
    /// approximate location.
    var weatherEnabled: Bool = UserDefaults.standard.object(forKey: "weatherEnabled") as? Bool ?? true {
        didSet {
            UserDefaults.standard.set(weatherEnabled, forKey: "weatherEnabled")
            weatherEnabled ? weather.start() : weather.stop()
        }
    }

    init() {
        delegateProxy.onChange = { [weak self] reason in self?.bridgeStateDidChange(reason) }
        bridge.delegate = delegateProxy
        applyRoomBlend()
        route = bridge.route
        headTracker.onYaw = { [weak self] yaw in self?.bridge.setListenerYawDegrees(Float(yaw)) }
        applySpatial(force: true)
        watch.onHeartRate = { [weak self] bpm, date in self?.health.ingestLiveHeartRate(bpm, at: date) }
        watch.activate()
        health.onIntensity = { [weak self] i in self?.bridge.setExerciseIntensity(i) }
        worlds = bridge.availableWorlds()
        nowPlaying.onPlay = { [weak self] in self?.resumeLastDimension() }
        nowPlaying.onPause = { [weak self] in self?.leaveWorld() }
        refreshWorlds()   // once at launch; again only after imports (not on every screen)
        if weatherEnabled { weather.start() }
        meterTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.refreshMeters() }
        }
    }

    var sampleRate: Double { bridge.sampleRate }
    var micPermission: AtmosMicPermission { bridge.micPermission }
    var isOnSpeaker: Bool { route.kind == .speaker }
    /// Headphones (wired, USB or Bluetooth) carry the augmented layer: the microphone and
    /// the room are only offered with them, since a speaker would feed back into the mic.
    var onHeadphones: Bool { route.kind == .wired || route.kind == .bluetooth }

    /// What was on when headphones were removed, so plugging them back in returns to it.
    private var augmentedResume: (dimension: String?, mic: Bool, blend: Bool)?
    var isOnBluetooth: Bool { route.kind == .bluetooth }

    // MARK: Transport

    func startAudio() {
        do {
            bridge.clearPanic()
            try bridge.startAudio()
            audioRunning = true
            lastError = nil
            AppLog.info(String(format: "Audio started: %@, %.0f Hz, buffer %.1f ms", bridge.route.outputName,
                               bridge.route.sampleRate, bridge.route.ioBufferDuration * 1000))
        } catch {
            lastError = error.localizedDescription
        }
        route = bridge.route
    }

    func stopAudio() {
        bridge.stopAudio()
        audioRunning = false
        AppLog.info("Audio stopped")
    }

    // MARK: Mix

    /// The room reaches the ears only while "Blend the room" is on, at the room level
    /// (transformed only: the raw microphone is never played).
    private func applyRoomBlend() {
        bridge.setGain(0, for: .micRaw)
        bridge.setGain(roomBlendOn ? roomLevel : 0, for: .micTransformed)
    }

    // MARK: Microphone

    func setMicEnabled(_ enabled: Bool) {
        if enabled && !onHeadphones {
            statusMessage = "Connect headphones to let the room in."
            return
        }
        bridge.setMicEnabled(enabled) { [weak self] ok, error in
            MainActor.assumeIsolated {
                guard let self else { return }
                self.micEnabled = ok ? enabled : false
                if !self.micEnabled && self.roomBlendOn { self.roomBlendOn = false; self.micOnForBlend = false }
                if !ok { self.lastError = error } else { AppLog.info("Microphone \(enabled ? "on" : "off")") }
                self.audioRunning = self.bridge.audioRunning
                self.route = self.bridge.route
            }
        }
    }

    /// The room blend on Home (owner, 2026-11-05): the room around you, echoed on the
    /// song's beat and reverberated, under the music. Needs headphones (a speaker would
    /// feed back into the microphone). Turning it off also turns the microphone off if
    /// the blend turned it on.
    private(set) var roomBlendOn = false { didSet { applyRoomBlend(); AppLog.info("Room blend \(roomBlendOn ? "on" : "off")") } }
    private var micOnForBlend = false

    func setRoomBlend(_ on: Bool) {
        if on {
            guard onHeadphones else { statusMessage = "Connect headphones to blend the room in."; return }
            if !micEnabled { micOnForBlend = true; setMicEnabled(true) }
            roomBlendOn = true
        } else {
            roomBlendOn = false
            if micOnForBlend { micOnForBlend = false; setMicEnabled(false) }
        }
    }

    // MARK: Tiers

    /// Switches off anything the current tier no longer includes (e.g. after a refund).
    func enforce(_ tier: Tier) {
        AppLog.info("Edition: \(tier.title)")
        if tier < Feature.headTracking.requiredTier { headTrackingEnabled = false }
        if tier < Feature.activity.requiredTier { activityEnabled = false }
        if tier < Feature.health.requiredTier { healthEnabled = false }
        if tier < Feature.weather.requiredTier { weatherEnabled = false }
    }

    // MARK: Meters / delegate

    private var statusTick = 0

    /// 30 Hz: the meters (only the live meters and the waveform read them). The world
    /// status, which many views read, refreshes 6 times a second.
    private func refreshMeters() {
        meters = bridge.meters()
        statusTick += 1
        guard statusTick % 5 == 0 else { return }
        let previousSong = worldStatus.songTitle
        worldStatus = bridge.worldStatus()
        let active = worldStatus.loaded && worldStatus.playing ? worldStatus.worldID : nil
        if active != activeDimensionID { activeDimensionID = active }
        let loadedID = worldStatus.loaded ? worldStatus.worldID : nil
        if loadedID != loadedDimensionID { loadedDimensionID = loadedID }
        if meters.micActive != micLive { micLive = meters.micActive }
        let limit = meters.limiterGain < 0.98
        if limit != limiting { limiting = limit }
        if meters.moonPhaseId != moonPhaseID { moonPhaseID = meters.moonPhaseId }
        if meters.lunarDay != lunarDay { lunarDay = meters.lunarDay }
        if worldStatus.loaded && worldStatus.songCount > 1 && worldStatus.songTitle != previousSong && !previousSong.isEmpty {
            AppLog.info("Dimension \(worldStatus.worldID): now \(worldStatus.songTitle)")
        }
        // Phase discovery: once a second, credit audible listening to the Dimension heard.
        if Date().timeIntervalSince(lastProgressTick) >= 1 {
            lastProgressTick = Date()
            nightClock.refresh()
            if bridge.night != nightClock.isNight { bridge.night = nightClock.isNight }
            // The music adapts on every output; the headphone 3D (HRTF) effect is only for
            // headphones, so speakers get plain stereo.
            if bridge.headphones != headphonesForAudio { bridge.headphones = headphonesForAudio }
            applySpatial()
            // Rain plays a Dimension's storm song, on every tier (needs Weather on).
            let raining = weatherEnabled && (weather.mood == .rain || weather.mood == .storm)
            if bridge.raining != raining { bridge.raining = raining }
            // Rain and thunder under the music while it rains and a Dimension plays.
            rainAmbience.update(!raining || activeDimensionID == nil ? .none : (weather.mood == .storm ? .storm : .rain))
            // Transition effects when a day-and-night Dimension changes song: the drone
            // into night or rain, the bells back into day.
            let time = raining ? "rain" : (nightClock.isNight ? "night" : "day")
            if let id = activeDimensionID, (worlds.first { $0.worldID == id }?.songCount ?? 1) > 1,
               let previous = songTime, previous != time {
                transitionCue.play(time == "day" ? .bells : .drone)
            }
            songTime = activeDimensionID == nil ? nil : time
            let audible = worldStatus.playing
            // Only tonight's lunar song, after dark, counts. TestFlight-only check of the
            // counter: any song counts, at any hour.
            let testing = AppBuild.isTestFlight && UserDefaults.standard.bool(forKey: "test.anySongIsLunar")
            let songPhase = testing ? progress.currentPhaseID
                : (nightClock.isNight ? worldStatus.songLunarPhaseId : "")
            progress.tick(dimensionID: currentDimensionID, songPhaseID: songPhase, audible: audible)
            updateNowPlaying()
        }
        // Dropouts: log the increase at most once a minute.
        let total = meters.underruns + worldStatus.underruns
        if total > loggedUnderruns, Date().timeIntervalSince(lastUnderrunLog) > 60 {
            AppLog.warning("Audio dropouts: +\(total - loggedUnderruns) (total \(total))")
            loggedUnderruns = total
            lastUnderrunLog = Date()
        }
    }

    // MARK: Dimensions on this device

    func refreshWorlds() {
        worlds = bridge.availableWorlds()
        // Whole Dimensions download as one pack (owner, 2026-11-05): its songs are found by
        // listening, not chosen. The pack id is the Dimension id.
        var phases: [String: Set<String>] = [:]
        for w in worlds where !w.userMade {
            let songs = bridge.songs(forWorld: w.worldID)
            phases[w.worldID] = Set(songs.compactMap { ($0["lunarPhase"] as? String).flatMap { $0.isEmpty ? nil : $0 } })
            let files = songs.flatMap { ($0["files"] as? [String]) ?? [] }
            downloads.setTracks([DimensionDownloads.Track(id: w.worldID, dimensionID: w.worldID, title: w.name, files: files)],
                                for: w.worldID)
        }
        phaseSongs = phases
        downloads.refresh(bundled: Set(worlds.filter(\.audioAvailable).map(\.worldID)))
    }

    /// Phases that have their own song in each Dimension (only those can be discovered).
    private(set) var phaseSongs: [String: Set<String>] = [:]
    /// Dimensions with phase songs show "still to come" for phases without one.
    func phaseHasSong(_ dimensionID: String, _ phase: String) -> Bool {
        guard let set = phaseSongs[dimensionID], !set.isEmpty else { return true }
        return set.contains(phase)
    }

    /// Removes every downloaded track of a Dimension.
    func removeAllTracks(_ world: AtmosWorldInfo) {
        stopIfPlaying(world.worldID)
        downloads.removeAll(world.worldID)
    }

    private func stopIfPlaying(_ dimensionID: String) {
        if worldStatus.loaded && worldStatus.worldID == dimensionID {
            bridge.unloadWorld()
            worldStatus = bridge.worldStatus()
        }
    }

    // MARK: Crash recovery

    private static var supportFolder: URL {
        let u = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }
    private static var runningFlag: URL { supportFolder.appendingPathComponent("session.running") }

    /// True when the previous run ended without going to the background first (crash or
    /// kill) while a Dimension had been chosen: Home offers to play it again.
    private(set) var previousRunEndedUnexpectedly = false
    private var runTracking = false

    private var currentDimensionID: String? { worldStatus.loaded ? worldStatus.worldID : nil }

    /// Called once at launch.
    func beginRunTracking() {
        guard !runTracking else { return }   // once per launch
        runTracking = true
        let fm = FileManager.default
        previousRunEndedUnexpectedly = fm.fileExists(atPath: Self.runningFlag.path) && lastDimensionID != nil
        if previousRunEndedUnexpectedly { AppLog.warning("Previous session ended unexpectedly") }
        fm.createFile(atPath: Self.runningFlag.path, contents: nil)
    }

    /// Scene phase hook: going to the background is a clean stop point.
    func appDidEnterBackground() {
        // Music, and the opening sound, keep playing in the background.
        try? FileManager.default.removeItem(at: Self.runningFlag)
    }
    /// The Viksen Dimension, built into the app (its day track is Lunar Promise).
    static let coreDimensionID = "viksen"

    /// Lock screen / Control Center: play starts the last Dimension again.
    func resumeLastDimension() {
        let playable = worlds.filter(isPlayable)
        if let w = playable.first(where: { $0.worldID == lastDimensionID })
            ?? playable.first(where: { $0.worldID == Self.coreDimensionID }) ?? playable.first { enterWorld(w) }
    }

    private func updateNowPlaying() {
        let st = worldStatus
        let loaded = st.loaded
        let id = loaded ? st.worldID : lastDimensionID
        guard let info = worlds.first(where: { $0.worldID == id }) else {
            nowPlaying.update(dimension: nil, song: "", artist: "", playing: false, position: 0, length: 0)
            return
        }
        nowPlaying.update(dimension: info.name, song: loaded ? st.songTitle : "", artist: info.artist,
                          playing: loaded && st.playing, position: loaded ? st.positionSeconds : 0,
                          length: loaded ? st.lengthSeconds : 0)
    }

    /// The app is being closed: stop the sound immediately (no fade), then save.
    func shutdownNow() {
        bridge.stopAudio()
        appDidEnterBackground()
    }
    func appWillEnterForeground() {
        if weatherEnabled { weather.refreshNow() }
        if activeDimensionID == nil { threshold.start(engineAudioRunning: audioRunning) }
        FileManager.default.createFile(atPath: Self.runningFlag.path, contents: nil)
    }

    /// After an unexpected close: play the last Dimension again.
    func restoreLastSession() {
        previousRunEndedUnexpectedly = false
        resumeLastDimension()
    }

    func dismissRestore() { previousRunEndedUnexpectedly = false }

    // MARK: Worlds

    /// Enters a World: loads its music, starts audio if needed and fades it in.
    /// The Dimension the listener entered last (Home's "Enter Dimension" returns to it).
    private(set) var lastDimensionID: String? = UserDefaults.standard.string(forKey: "lastDimension")

    /// The Dimension chosen on the Home card (what the play fox starts).
    func selectDimension(_ id: String) {
        guard id != lastDimensionID else { return }
        lastDimensionID = id
        UserDefaults.standard.set(id, forKey: "lastDimension")
    }

    /// True when a Dimension's audio is on this device.
    func isPlayable(_ world: AtmosWorldInfo) -> Bool {
        world.audioAvailable || world.userMade || downloads.isAnyInstalled(world.worldID)
    }

    func enterWorld(_ world: AtmosWorldInfo) {
        lastDimensionID = world.worldID
        UserDefaults.standard.set(world.worldID, forKey: "lastDimension")
        if worldStatus.loaded && worldStatus.worldID == world.worldID && worldStatus.playing { return }
        do {
            let started = Date()
            if world.audioAvailable || world.userMade {
                try bridge.loadWorld(world.worldID)            // audio inside the app or the user's folder
            } else {
                guard let files = downloads.audioFiles(world.worldID) else {
                    lastError = "Download a track of \(world.name) first."
                    return
                }
                try bridge.loadWorld(world.worldID, audioFiles: files)
            }
            applySpatial(force: true)
            AppLog.info("Enter Dimension \(world.worldID) (3D \(dimension3D), HRTF \(bridge.binauralHRTF))")
            if !audioRunning { startAudio() }
            bridge.clearPanic()
            threshold.fadeOut(seconds: 2.5)   // the opening sound gives way to the Dimension
            bridge.startWorld()
            AppLog.info(String(format: "Timing: enter Dimension %.0f ms", Date().timeIntervalSince(started) * 1000))
        } catch {
            lastError = error.localizedDescription
            AppLog.error("Enter Dimension \(world.worldID) failed: \(error.localizedDescription)")
        }
    }

    /// Leaves the current World with a fade-out.
    func leaveWorld() {
        AppLog.info("Leave Dimension \(worldStatus.worldID)")
        bridge.stopWorld()
        // Back to the opening loops once the Dimension has faded out (if they are on).
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { [weak self] in
            guard let self, self.activeDimensionID == nil else { return }
            self.threshold.start(engineAudioRunning: self.audioRunning)
        }
    }

    // MARK: 3D audio and adaptive music

    /// Binaural 3D for Dimension stems (headphones). On by default.
    var dimension3D: Bool = UserDefaults.standard.object(forKey: "dimension3D") as? Bool ?? true {
        didSet {
            applySpatial(force: true)
            UserDefaults.standard.set(dimension3D, forKey: "dimension3D")
            AppLog.info("3D audio \(dimension3D ? "on" : "off")")
        }
    }
    var binauralHRTF: Bool { bridge.binauralHRTF }
    /// 3D audio on the device's own speaker (off by default: the 3D effect is made for
    /// headphones; some people like it on speakers anyway).
    var spatialOnSpeaker: Bool = UserDefaults.standard.bool(forKey: "spatial.speaker") {
        didSet { UserDefaults.standard.set(spatialOnSpeaker, forKey: "spatial.speaker"); applySpatial(force: true) }
    }
    /// Adaptive music (parts and mix following the moment), with headphones and on speaker.
    var adaptiveOnHeadphones: Bool = UserDefaults.standard.object(forKey: "adaptive.headphones") as? Bool ?? true {
        didSet { UserDefaults.standard.set(adaptiveOnHeadphones, forKey: "adaptive.headphones"); applySpatial(force: true) }
    }
    var adaptiveOnSpeaker: Bool = UserDefaults.standard.object(forKey: "adaptive.speaker") as? Bool ?? true {
        didSet { UserDefaults.standard.set(adaptiveOnSpeaker, forKey: "adaptive.speaker"); applySpatial(force: true) }
    }

    private func bridgeStateDidChange(_ reason: String) {
        AppLog.info("\(reason) (output: \(bridge.route.outputName))")
        let wasOnHeadphones = onHeadphones
        route = bridge.route
        audioRunning = bridge.audioRunning
        statusMessage = reason
        if wasOnHeadphones && !onHeadphones { leaveAugmentedReality() }
        else if !wasOnHeadphones && onHeadphones { returnToAugmentedReality() }
    }

    /// Headphones out: a gentle step back to regular reality. The microphone paths were
    /// silenced at once by the bridge; the mic turns off and the Dimension fades out.
    private func leaveAugmentedReality() {
        let playingDimension = worldStatus.playing ? worldStatus.worldID : nil
        augmentedResume = (playingDimension, micEnabled, roomBlendOn)
        if micEnabled { setMicEnabled(false) }
        if playingDimension != nil { bridge.stopWorld() }   // fades out
        statusMessage = "Headphones disconnected: back to regular reality"
        AppLog.info("Left augmented reality (Dimension \(playingDimension ?? "none"), mic \(augmentedResume?.mic == true))")
    }

    /// Headphones in: fade back into where the listener was.
    private func returnToAugmentedReality() {
        guard let resume = augmentedResume else {
            statusMessage = "Headphones connected"
            return
        }
        augmentedResume = nil
        if let id = resume.dimension, let world = worlds.first(where: { $0.worldID == id }) {
            enterWorld(world)
        }
        if resume.blend && micPermission == .granted { setRoomBlend(true) }
        else if resume.mic && micPermission == .granted { setMicEnabled(true) }
        statusMessage = "Headphones connected: back in augmented reality"
        AppLog.info("Returned to augmented reality")
    }
}

/// The bridge delegate must be an NSObject; this keeps EngineController a plain
/// Swift class. Callbacks arrive on the main thread.
private final class BridgeDelegateProxy: NSObject, AtmosEngineBridgeDelegate {
    var onChange: (@MainActor (String) -> Void)?

    func engineBridgeStateDidChange(_ reason: String) {
        MainActor.assumeIsolated { onChange?(reason) }
    }
}
