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
    private let delegateProxy = BridgeDelegateProxy()
    private var meterTimer: Timer?

    // Mirrors of engine state (source of truth for the UI).
    private(set) var audioRunning = false
    private(set) var micEnabled = false
    private(set) var trackName: String?
    private(set) var isLoadingTrack = false
    private(set) var meters = AtmosMeterSnapshot()
    private(set) var route = AtmosRouteInfo()
    private(set) var statusMessage: String?
    var lastError: String?

    var busGains: [Float] = [1.0, 0.5, 0.0, 0.5]
    var masterGain: Float = 0.7 { didSet { bridge.setMasterGain(masterGain) } }
    var looping = false { didSet { bridge.setLooping(looping) } }
    var bedEnabled = false { didSet { bridge.setAtmosphereBedEnabled(bedEnabled) } }
    var bedLevel: Float = 0.25 { didSet { bridge.setAtmosphereBedLevel(bedLevel) } }
    var bedColourHz: Float = 900 { didSet { bridge.setAtmosphereBedColourHz(bedColourHz) } }
    var micToneHz: Float = 1800 { didSet { bridge.setMicToneHz(micToneHz) } }
    var micEchoSeconds: Float = 0.38 { didSet { bridge.setMicEchoSeconds(micEchoSeconds) } }
    var micFeedback: Float = 0.45 { didSet { bridge.setMicFeedback(micFeedback) } }
    var micWet: Float = 0.6 { didSet { bridge.setMicWet(micWet) } }
    var limiterCeilingDb: Float = -1 { didSet { bridge.setLimiterCeilingDb(limiterCeilingDb) } }

    init() {
        delegateProxy.onChange = { [weak self] reason in self?.bridgeStateDidChange(reason) }
        bridge.delegate = delegateProxy
        for bus in BusLevel.allCases { busGains[bus.rawValue] = bridge.gain(for: AtmosBusId(rawValue: bus.rawValue)!) }
        route = bridge.route
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
        } catch {
            lastError = error.localizedDescription
        }
        route = bridge.route
    }

    func stopAudio() {
        bridge.stopAudio()
        audioRunning = false
    }

    func togglePlay() {
        if !audioRunning { startAudio() }
        if meters.playing { bridge.pause() } else { bridge.clearPanic(); bridge.play() }
    }

    func seek(to seconds: Double) { bridge.seek(toSeconds: seconds) }

    /// Immediate silence: stops playback and every output path.
    func panic() {
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
                if ok { self.trackName = self.bridge.loadedTrackName } else { self.lastError = error }
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
                if !ok { self.lastError = error }
                self.audioRunning = self.bridge.audioRunning
                self.route = self.bridge.route
            }
        }
    }

    // MARK: Meters / delegate

    private func refreshMeters() {
        meters = bridge.meters()
    }

    private func bridgeStateDidChange(_ reason: String) {
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
