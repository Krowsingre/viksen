import Foundation
import Observation

/// Swift-side owner of the engine bridge. All UI state that mirrors the engine
/// lives here; views never talk to the bridge directly.
@MainActor
@Observable
final class EngineController {
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

    private let bridge = AtmosEngineBridge()
    let headTracker = HeadTracker()
    let activity = ActivityMonitor()
    let health = HealthService()
    let watch = WatchLink()
    private let delegateProxy = BridgeDelegateProxy()
    private var meterTimer: Timer?

    // Mirrors of engine state (source of truth for the UI).
    private(set) var audioRunning = false
    private(set) var micEnabled = false
    private(set) var trackName: String?
    private(set) var isLoadingTrack = false
    private(set) var meters = AtmosMeterSnapshot()
    private(set) var worldStatus = AtmosWorldStatus()
    private(set) var worlds: [AtmosWorldInfo] = []
    private(set) var route = AtmosRouteInfo()
    private(set) var statusMessage: String?
    var lastError: String? { didSet { if let e = lastError { AppLog.error(e) } } }
    private var loggedUnderruns: UInt64 = 0
    private var lastUnderrunLog = Date.distantPast

    var busGains: [Float] = [1.0, 0.5, 0.0, 0.5]
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
    var limiterCeilingDb: Float = -1 { didSet { bridge.setLimiterCeilingDb(limiterCeilingDb) } }
    var micShiftHz: Float = -180 { didSet { bridge.setMicShiftHz(micShiftHz) } }
    var micShiftMix: Float = 0 { didSet { bridge.setMicShiftMix(micShiftMix) } }
    var roomMemoryEnabled = false { didSet { bridge.setRoomMemoryEnabled(roomMemoryEnabled) } }
    var roomMemoryLevel: Float = 0.6 { didSet { bridge.setRoomMemoryLevel(roomMemoryLevel) } }
    var roomMemoryPitch: Float = -12 { didSet { bridge.setRoomMemoryPitch(roomMemoryPitch) } }
    var motionSensing = false { didSet { bridge.setMotionSensingEnabled(motionSensing) } }
    var motionSensingAvailable: Bool { bridge.motionSensingAvailable }

    // Body & motion (each reaction can be switched off individually).
    var headTrackingEnabled = false {
        didSet { headTrackingEnabled ? headTracker.start() : headTracker.stop() }
    }
    var activityEnabled = false {
        didSet { activityEnabled ? activity.start() : activity.stop() }
    }
    /// Echo time follows the listener's step rhythm (one echo per step).
    var echoFollowsSteps = false { didSet { applyCadence(activity.cadenceStepsPerMinute) } }
    private var echoBeforeStepSync: Float?

    var healthEnabled = false {
        didSet {
            healthEnabled ? health.start() : health.stop()
            if !healthEnabled { currentIntensity = nil; bridge.setExerciseIntensity(-1); applyBed() }
        }
    }
    /// Exercise intensity brightens and thickens the atmosphere bed.
    var intensityShapesAtmosphere = false { didSet { applyBed() } }
    private var currentIntensity: Double?

    let weather = WeatherMonitor()
    var weatherEnabled = false {
        didSet { weatherEnabled ? weather.start() : weather.stop() }
    }
    /// Weather colours the atmosphere bed (rain darker and denser, clear days brighter).
    var weatherShapesAtmosphere = false { didSet { applyBed() } }

    init() {
        delegateProxy.onChange = { [weak self] reason in self?.bridgeStateDidChange(reason) }
        bridge.delegate = delegateProxy
        for bus in BusLevel.allCases { busGains[bus.rawValue] = bridge.gain(for: AtmosBusId(rawValue: bus.rawValue)!) }
        route = bridge.route
        headTracker.onYaw = { [weak self] yaw in self?.bridge.setListenerYawDegrees(Float(yaw)) }
        bridge.setDimensionSpatialEnabled(dimension3D)
        activity.onCadence = { [weak self] spm in self?.applyCadence(spm) }
        watch.onHeartRate = { [weak self] bpm, date in self?.health.ingestLiveHeartRate(bpm, at: date) }
        watch.activate()
        health.onIntensity = { [weak self] i in
            self?.currentIntensity = i
            self?.bridge.setExerciseIntensity(i)
            self?.applyBed()
        }
        worlds = bridge.availableWorlds()
        weather.onMood = { [weak self] _, _ in self?.applyBed() }
        meterTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.refreshMeters() }
        }
    }

    var sampleRate: Double { bridge.sampleRate }
    var micPermission: AtmosMicPermission { bridge.micPermission }
    var isOnSpeaker: Bool { route.kind == .speaker }
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

    // MARK: Mix

    func setGain(_ gain: Float, for bus: BusLevel) {
        busGains[bus.rawValue] = gain
        bridge.setGain(gain, for: AtmosBusId(rawValue: bus.rawValue)!)
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
                    AppLog.info("Song loaded (\(url.pathExtension.lowercased()))")
                } else { self.lastError = error }
            }
        }
    }

    func unloadTrack() {
        bridge.unloadTrack()
        trackName = nil
    }

    // MARK: Microphone

    func setMicEnabled(_ enabled: Bool) {
        bridge.setMicEnabled(enabled) { [weak self] ok, error in
            MainActor.assumeIsolated {
                guard let self else { return }
                self.micEnabled = ok ? enabled : false
                if !ok { self.lastError = error } else { AppLog.info("Microphone \(enabled ? "on" : "off")") }
                self.audioRunning = self.bridge.audioRunning
                self.route = self.bridge.route
            }
        }
    }

    // MARK: Tiers

    /// Switches off anything the current tier no longer includes (e.g. after a refund).
    func enforce(_ tier: Tier) {
        AppLog.info("Edition: \(tier.title)")
        if tier < Feature.roomMemory.requiredTier { roomMemoryEnabled = false }
        if tier < Feature.frequencyShift.requiredTier { micShiftMix = 0 }
        if tier < Feature.headTracking.requiredTier { headTrackingEnabled = false }
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
    }

    // MARK: Meters / delegate

    private func refreshMeters() {
        meters = bridge.meters()
        worldStatus = bridge.worldStatus()
        // Dropouts: log the increase at most once a minute.
        let total = meters.underruns + worldStatus.underruns
        if total > loggedUnderruns, Date().timeIntervalSince(lastUnderrunLog) > 60 {
            AppLog.warning("Audio dropouts: +\(total - loggedUnderruns) (total \(total))")
            loggedUnderruns = total
            lastUnderrunLog = Date()
        }
    }

    // MARK: Worlds

    /// Enters a World: loads its music, starts audio if needed and fades it in.
    func enterWorld(_ world: AtmosWorldInfo) {
        if worldStatus.loaded && worldStatus.worldID == world.worldID && worldStatus.playing { return }
        do {
            try bridge.loadWorld(world.worldID)
            bridge.setDimensionSpatialEnabled(dimension3D)
            applySavedPositions(world.worldID)
            AppLog.info("Enter Dimension \(world.worldID) (3D \(dimension3D), HRTF \(bridge.binauralHRTF))")
            if trackName != nil { bridge.pause() }   // one music source at a time
            if !audioRunning { startAudio() }
            bridge.clearPanic()
            bridge.startWorld()
        } catch {
            lastError = error.localizedDescription
            AppLog.error("Enter Dimension \(world.worldID) failed: \(error.localizedDescription)")
        }
    }

    /// Leaves the current World with a fade-out.
    func leaveWorld() {
        AppLog.info("Leave Dimension \(worldStatus.worldID)")
        bridge.stopWorld()
    }

    // MARK: 3D / spatial canvas

    /// Binaural 3D for Dimension stems (headphones). On by default.
    var dimension3D: Bool = UserDefaults.standard.object(forKey: "dimension3D") as? Bool ?? true {
        didSet {
            bridge.setDimensionSpatialEnabled(dimension3D)
            UserDefaults.standard.set(dimension3D, forKey: "dimension3D")
            AppLog.info("3D audio \(dimension3D ? "on" : "off")")
        }
    }
    var binauralHRTF: Bool { bridge.binauralHRTF }

    /// Moves a stem on the spatial canvas and remembers it for this Dimension.
    func moveStem(_ index: Int, azimuth: Float, distance: Float) {
        guard worldStatus.loaded, index < worldStatus.stems.count else { return }
        let elevation = worldStatus.stems[index].elevation
        bridge.moveStem(index, azimuth: azimuth, elevation: elevation, distance: distance)
        var saved = savedPositions(worldStatus.worldID)
        saved[String(index)] = [Double(azimuth), Double(elevation), Double(distance)]
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
        for (key, v) in savedPositions(worldID) {
            guard let i = Int(key), v.count == 3 else { continue }
            bridge.moveStem(i, azimuth: Float(v[0]), elevation: Float(v[1]), distance: Float(v[2]))
        }
    }

    private func bridgeStateDidChange(_ reason: String) {
        AppLog.info("\(reason) (output: \(bridge.route.outputName))")
        statusMessage = reason
        route = bridge.route
        audioRunning = bridge.audioRunning
        busGains[BusLevel.micRaw.rawValue] = bridge.gain(for: .micRaw)
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
