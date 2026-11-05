import AVFoundation
import Foundation

/// Rain and thunder under the music while it rains where the listener is (owner,
/// 2026-11-05). Light rain: the light-moderate loop, with rare distant thunder. A storm:
/// the moderate loop, with thunder every 40 s to 2 min, mostly distant, sometimes very
/// close, rarely extremely close, each placed somewhere to the left or right. Fades in
/// and out over a few seconds. Plays only while a Dimension plays.
@MainActor
final class RainAmbience {
    enum Weather: Equatable { case none, rain, storm }

    private var engine: AVAudioEngine?
    private var rainNode: AVAudioPlayerNode?
    private var thunderNode: AVAudioPlayerNode?
    private var thunderMixer: AVAudioMixerNode?
    private var current: Weather = .none
    private var thunderTimer: Timer?
    private var fadeTimer: Timer?
    private var buffers: [String: AVAudioPCMBuffer] = [:]

    private static let rainLevel: Float = 0.32
    private static let folder = "Worlds/_ambience"

    func update(_ weather: Weather) {
        guard weather != current else { return }
        current = weather
        if weather == .none { fadeOut(); return }
        start(storm: weather == .storm)
    }

    // MARK: Playback

    private func start(storm: Bool) {
        guard let rain = buffer(storm ? "rain-moderate" : "rain-light", ext: "wav") else { return }
        stopNow()
        let e = AVAudioEngine()
        let r = AVAudioPlayerNode(), t = AVAudioPlayerNode(), tm = AVAudioMixerNode()
        [r, t, tm].forEach(e.attach)
        e.connect(r, to: e.mainMixerNode, format: rain.format)
        e.connect(t, to: tm, format: nil)
        e.connect(tm, to: e.mainMixerNode, format: nil)
        r.volume = 0
        do { try e.start() } catch { AppLog.warning("Rain ambience could not start: \(error.localizedDescription)"); return }
        r.scheduleBuffer(rain, at: nil, options: .loops)
        r.play()
        t.play()
        engine = e; rainNode = r; thunderNode = t; thunderMixer = tm
        ramp(to: Self.rainLevel, seconds: 4)
        scheduleThunder(storm: storm, first: true)
        AppLog.info("Rain ambience: \(storm ? "storm" : "rain")")
    }

    private func scheduleThunder(storm: Bool, first: Bool = false) {
        thunderTimer?.invalidate()
        // Storm: every 40-120 s. Light rain: a distant rumble now and then (2-5 min, 1 in 3).
        let wait = storm ? Double.random(in: first ? 15...40 : 40...120) : Double.random(in: 120...300)
        thunderTimer = Timer.scheduledTimer(withTimeInterval: wait, repeats: false) { [weak self] _ in
            MainActor.assumeIsolated {
                guard let self, self.engine != nil else { return }
                if storm || Int.random(in: 0..<3) == 0 { self.playThunder(storm: storm) }
                self.scheduleThunder(storm: storm)
            }
        }
    }

    private func playThunder(storm: Bool) {
        let r = Double.random(in: 0..<1)
        let name: String, level: Float
        if !storm || r < 0.6 { name = "thunder-distant"; level = 0.55 }
        else if r < 0.9 { name = "thunder-very-close"; level = 0.6 }
        else { name = "thunder-extremely-close"; level = 0.6 }
        guard let b = buffer(name, ext: "caf"), let node = thunderNode, let mixer = thunderMixer, let e = engine else { return }
        if node.outputFormat(forBus: 0) != b.format {
            e.disconnectNodeOutput(node)
            e.connect(node, to: mixer, format: b.format)
        }
        mixer.pan = Float.random(in: -0.8...0.8)
        node.volume = level
        node.scheduleBuffer(b, at: nil, options: [], completionHandler: nil)
        if !node.isPlaying { node.play() }
    }

    private func fadeOut() {
        thunderTimer?.invalidate()
        guard engine != nil else { return }
        ramp(to: 0, seconds: 4) { [weak self] in self?.stopNow() }
    }

    private func stopNow() {
        fadeTimer?.invalidate()
        thunderTimer?.invalidate()
        rainNode?.stop(); thunderNode?.stop(); engine?.stop()
        engine = nil; rainNode = nil; thunderNode = nil; thunderMixer = nil
    }

    private func ramp(to target: Float, seconds: Double, then done: (() -> Void)? = nil) {
        fadeTimer?.invalidate()
        guard let node = rainNode else { done?(); return }
        let from = node.volume, start = Date()
        fadeTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 20, repeats: true) { [weak self] timer in
            MainActor.assumeIsolated {
                let p = min(1, Float(Date().timeIntervalSince(start) / max(0.01, seconds)))
                self?.rainNode?.volume = from + (target - from) * p
                if p >= 1 { timer.invalidate(); done?() }
            }
        }
    }

    private func buffer(_ name: String, ext: String) -> AVAudioPCMBuffer? {
        if let b = buffers[name] { return b }
        guard let url = Bundle.main.url(forResource: name, withExtension: ext, subdirectory: Self.folder),
              let file = try? AVAudioFile(forReading: url),
              let b = AVAudioPCMBuffer(pcmFormat: file.processingFormat, frameCapacity: AVAudioFrameCount(file.length)),
              (try? file.read(into: b)) != nil else { return nil }
        buffers[name] = b
        return b
    }
}
