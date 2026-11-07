import AVFoundation
import Foundation

/// Rain and thunder under the music while it rains where the listener is (owner,
/// 2026-11-05). Light rain: the light-moderate loop, with rare distant thunder. A storm:
/// the moderate loop, with thunder every 40 s to 2 min, mostly distant, sometimes very
/// close, rarely extremely close, each placed somewhere to the left or right. Plays only
/// while a Dimension plays.
///
/// Changes are always gentle (owner, 2026-11-07): rain fades in and out, light rain and a
/// storm crossfade into each other, and each rain loop's end is blended into its start so
/// the loop itself has no seam. Thunder is separate and simply overlaps the rain.
@MainActor
final class RainAmbience {
    enum Weather: Equatable { case none, rain, storm }

    private var engine: AVAudioEngine?
    private var rainVoice: AVAudioPlayerNode?            // the rain heard now (or fading in)
    private var fadingVoices: [AVAudioPlayerNode] = []    // earlier rain, fading out
    private var thunderNode: AVAudioPlayerNode?
    private var thunderMixer: AVAudioMixerNode?
    private var current: Weather = .none
    private var thunderTimer: Timer?
    private var fades: [ObjectIdentifier: Timer] = [:]
    private var buffers: [String: AVAudioPCMBuffer] = [:]

    private static let rainLevel: Float = 0.32
    private static let fadeIn: Double = 5
    private static let crossfade: Double = 6      // light rain <-> storm
    private static let fadeOut: Double = 5
    private static let folder = "Worlds/_ambience"

    func update(_ weather: Weather) {
        guard weather != current else { return }
        current = weather
        thunderTimer?.invalidate()
        if weather == .none { fadeAllOut(); return }
        crossfade(to: weather == .storm ? "rain-moderate" : "rain-light")
        scheduleThunder(storm: weather == .storm, first: true)
        AppLog.info("Rain ambience: \(weather == .storm ? "storm" : "rain")")
    }

    // MARK: Rain

    /// Brings in the new rain while the rain playing now fades out at the same time.
    private func crossfade(to name: String) {
        guard let rain = loopBuffer(name), let e = startEngine() else { return }
        if let old = rainVoice {
            rainVoice = nil
            fadingVoices.append(old)
            ramp(old, to: 0, seconds: Self.crossfade) { [weak self] in self?.retire(old) }
        }
        let v = AVAudioPlayerNode()
        e.attach(v)
        e.connect(v, to: e.mainMixerNode, format: rain.format)
        v.volume = 0
        v.scheduleBuffer(rain, at: nil, options: .loops)
        v.play()
        rainVoice = v
        ramp(v, to: Self.rainLevel, seconds: fadingVoices.isEmpty ? Self.fadeIn : Self.crossfade)
    }

    private func fadeAllOut() {
        guard engine != nil else { return }
        let voices = fadingVoices + [rainVoice].compactMap { $0 }
        rainVoice = nil
        fadingVoices = voices
        for v in voices {
            ramp(v, to: 0, seconds: Self.fadeOut) { [weak self] in
                self?.retire(v)
                if let self, self.current == .none, self.rainVoice == nil, self.fadingVoices.isEmpty { self.stopEngine() }
            }
        }
    }

    private func retire(_ v: AVAudioPlayerNode) {
        fades[ObjectIdentifier(v)]?.invalidate()
        fades[ObjectIdentifier(v)] = nil
        v.stop()
        engine?.detach(v)
        fadingVoices.removeAll { $0 === v }
    }

    // MARK: Thunder (overlaps the rain; not crossfaded)

    private func scheduleThunder(storm: Bool, first: Bool = false) {
        thunderTimer?.invalidate()
        // Storm: every 40-120 s. Light rain: a distant rumble now and then (2-5 min, 1 in 3).
        let wait = storm ? Double.random(in: first ? 15...40 : 40...120) : Double.random(in: 120...300)
        thunderTimer = Timer.scheduledTimer(withTimeInterval: wait, repeats: false) { [weak self] _ in
            MainActor.assumeIsolated {
                guard let self, self.engine != nil, self.current != .none else { return }
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

    // MARK: Engine

    private func startEngine() -> AVAudioEngine? {
        if let engine { return engine }
        let e = AVAudioEngine()
        let t = AVAudioPlayerNode(), tm = AVAudioMixerNode()
        [t, tm].forEach(e.attach)
        e.connect(t, to: tm, format: nil)
        e.connect(tm, to: e.mainMixerNode, format: nil)
        do { try e.start() } catch { AppLog.warning("Rain ambience could not start: \(error.localizedDescription)"); return nil }
        t.play()
        engine = e; thunderNode = t; thunderMixer = tm
        return e
    }

    private func stopEngine() {
        thunderTimer?.invalidate()
        fades.values.forEach { $0.invalidate() }
        fades = [:]
        thunderNode?.stop()
        engine?.stop()
        engine = nil; thunderNode = nil; thunderMixer = nil; rainVoice = nil; fadingVoices = []
    }

    /// Equal-power-ish volume ramp for one rain voice (each voice has its own).
    private func ramp(_ v: AVAudioPlayerNode, to target: Float, seconds: Double, then done: (() -> Void)? = nil) {
        let key = ObjectIdentifier(v)
        fades[key]?.invalidate()
        let from = v.volume, start = Date()
        fades[key] = Timer.scheduledTimer(withTimeInterval: 1.0 / 20, repeats: true) { [weak self] timer in
            MainActor.assumeIsolated {
                let p = min(1, Float(Date().timeIntervalSince(start) / max(0.01, seconds)))
                let shaped = target > from ? sin(p * .pi / 2) : cos((1 - p) * .pi / 2)   // smooth at both ends
                v.volume = from + (target - from) * shaped
                if p >= 1 { timer.invalidate(); self?.fades[key] = nil; done?() }
            }
        }
    }

    // MARK: Audio files

    /// A rain loop whose last few seconds are blended into its start, so looping it never
    /// jumps: the loop plays [start, end - n) with its first n frames crossfaded from the end.
    private func loopBuffer(_ name: String) -> AVAudioPCMBuffer? {
        let key = name + "#loop"
        if let b = buffers[key] { return b }
        guard let src = buffer(name, ext: "wav") else { return nil }
        let length = Int(src.frameLength)
        let n = min(Int(src.format.sampleRate * 3), length / 4)
        guard n > 0, let out = AVAudioPCMBuffer(pcmFormat: src.format, frameCapacity: AVAudioFrameCount(length - n)) else { return src }
        out.frameLength = AVAudioFrameCount(length - n)
        for ch in 0..<Int(src.format.channelCount) {
            guard let s = src.floatChannelData?[ch], let o = out.floatChannelData?[ch] else { return src }
            o.update(from: s, count: length - n)
            for i in 0..<n {
                let x = Float(i) / Float(n) * .pi / 2
                o[i] = s[i] * sin(x) + s[length - n + i] * cos(x)
            }
        }
        buffers[key] = out
        return out
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
