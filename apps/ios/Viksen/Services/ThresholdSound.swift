import AVFoundation
import Foundation

/// The optional opening ("threshold") sound (owner, 2026-11-05), cut from Lunar Promise.
/// Off by default. It never interrupts the listener's own audio: it only starts if nothing
/// else is playing, and it mixes with other sound. It fades away when a Dimension starts,
/// returns when the Dimension is stopped, and keeps playing when the app is minimised.
///
/// Three 8-bar loops at 80 BPM: B is home and always opens. At each loop boundary A may
/// come in (about 1 in 8) or, more rarely, C (about 1 in 25); either stays for 4–6 loops,
/// then B returns. Each loop file carries the bar that follows it, so every next loop
/// (the same or another) crossfades out of the one before over one bar, on the beat.
@MainActor
final class ThresholdSound {
    static let settingKey = "thresholdSound"
    enum Loop: String, CaseIterable { case a = "A", b = "B", c = "C" }

    private static let barFrames: AVAudioFrameCount = 144_000   // 3 s at 48 kHz (80 BPM, 4/4)
    private static let level: Float = 0.55

    private var engine: AVAudioEngine?
    private var node: AVAudioPlayerNode?
    private var sources: [Loop: AVAudioPCMBuffer] = [:]
    private var chooser = LoopChooser()
    private var previous: Loop?
    private var fadeTimer: Timer?

    var enabled: Bool { UserDefaults.standard.bool(forKey: Self.settingKey) }

    /// Call when the app comes to the screen with nothing playing.
    func start(engineAudioRunning: Bool) {
        guard enabled, engine == nil, loadSources() else { return }
        #if os(iOS)
        let session = AVAudioSession.sharedInstance()
        guard !session.isOtherAudioPlaying else { return }   // never interrupt the listener's audio
        // Never change the session while the music engine runs (its category keeps
        // Dimensions playing in the background). Otherwise: playback that mixes with
        // other apps and keeps going when Viksen is minimised.
        if !engineAudioRunning {
            try? session.setCategory(.playback, options: [.mixWithOthers])
            try? session.setActive(true)
        }
        #endif
        guard let format = sources[.b]?.format else { return }
        let e = AVAudioEngine()
        let n = AVAudioPlayerNode()
        e.attach(n)
        e.connect(n, to: e.mainMixerNode, format: format)
        e.mainMixerNode.outputVolume = 0
        do { try e.start() } catch { AppLog.warning("Opening sound could not start: \(error.localizedDescription)"); return }
        engine = e
        node = n
        chooser = LoopChooser()
        previous = nil
        scheduleNext()
        scheduleNext()   // always one loop queued ahead, so playback never waits
        n.play()
        ramp(to: Self.level, seconds: 2.5)
    }

    func fadeOut(seconds: Double = 2.0) {
        guard engine != nil else { return }
        ramp(to: 0, seconds: seconds) { [weak self] in self?.stopNow() }
    }

    private func stopNow() {
        fadeTimer?.invalidate()
        node?.stop()
        engine?.stop()
        node = nil
        engine = nil
    }

    // MARK: Loops

    private func loadSources() -> Bool {
        if sources.count == Loop.allCases.count { return true }
        for loop in Loop.allCases {
            guard let url = Bundle.main.url(forResource: "threshold-\(loop.rawValue)", withExtension: "wav",
                                            subdirectory: "Worlds/viksen"),
                  let file = try? AVAudioFile(forReading: url),
                  let buffer = AVAudioPCMBuffer(pcmFormat: file.processingFormat, frameCapacity: AVAudioFrameCount(file.length)),
                  (try? file.read(into: buffer)) != nil,
                  buffer.frameLength > Self.barFrames * 2 else { return false }
            sources[loop] = buffer
        }
        return true
    }

    /// Queues the next loop: its first bar crossfades (equal power) out of the bar that
    /// follows the previous loop, so changes land on the beat without a seam.
    private func scheduleNext() {
        guard let node, let engine, engine.isRunning else { return }
        let loop = chooser.next()
        guard let src = sources[loop], let out = AVAudioPCMBuffer(pcmFormat: src.format,
                                                                     frameCapacity: src.frameLength - Self.barFrames) else { return }
        let length = Int(src.frameLength - Self.barFrames)
        out.frameLength = AVAudioFrameCount(length)
        let tail = previous.flatMap { sources[$0] }
        let bar = Int(Self.barFrames)
        for ch in 0..<Int(src.format.channelCount) {
            guard let s = src.floatChannelData?[ch], let o = out.floatChannelData?[ch] else { continue }
            o.update(from: s, count: length)
            if let tail, let t = tail.floatChannelData?[ch] {
                for i in 0..<bar {
                    let x = Float(i) / Float(bar) * .pi / 2
                    o[i] = s[i] * sin(x) + t[length + i] * cos(x)
                }
            }
        }
        previous = loop
        node.scheduleBuffer(out, completionCallbackType: .dataConsumed) { [weak self] _ in
            Task { @MainActor in self?.scheduleNext() }
        }
    }

    private func ramp(to target: Float, seconds: Double, then done: (() -> Void)? = nil) {
        fadeTimer?.invalidate()
        guard let mixer = engine?.mainMixerNode else { done?(); return }
        let from = mixer.outputVolume
        let start = Date()
        fadeTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30, repeats: true) { [weak self] timer in
            MainActor.assumeIsolated {
                let p = min(1, Float(Date().timeIntervalSince(start) / max(0.01, seconds)))
                self?.engine?.mainMixerNode.outputVolume = from + (target - from) * p
                if p >= 1 { timer.invalidate(); done?() }
            }
        }
    }
}

/// Which loop plays next: B by default; A (about 1 in 8) or C (about 1 in 25) may take
/// over for 4–6 loops, then B returns. The opening loop is always B.
struct LoopChooser {
    private(set) var current: ThresholdSound.Loop = .b
    private var remaining = 0
    private var started = false

    mutating func next() -> ThresholdSound.Loop {
        guard started else { started = true; return .b }
        if current != .b {
            if remaining > 1 { remaining -= 1; return current }
            current = .b
            return .b
        }
        let r = Double.random(in: 0..<1)
        if r < 0.04 { current = .c } else if r < 0.16 { current = .a } else { return .b }
        remaining = Int.random(in: 4...6)
        return current
    }
}
