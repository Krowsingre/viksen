import AVFoundation
import Foundation

/// The optional opening ("threshold") sound (owner, 2026-11-05): a soft loop cut from
/// Lunar Promise that plays when Viksen opens. Off by default. It never interrupts the
/// listener's own audio: it only starts if nothing else is playing, and it mixes with
/// other sound. It fades away when a Dimension starts or the app leaves the screen.
@MainActor
final class ThresholdSound {
    static let settingKey = "thresholdSound"
    private var player: AVAudioPlayer?

    var enabled: Bool {
        get { UserDefaults.standard.bool(forKey: Self.settingKey) }
        set {
            UserDefaults.standard.set(newValue, forKey: Self.settingKey)
            if !newValue { fadeOut(seconds: 0.5) }
        }
    }

    /// Call when the app comes to the screen with nothing playing.
    func start() {
        guard enabled, player == nil,
              let url = Bundle.main.url(forResource: "threshold", withExtension: "m4a", subdirectory: "Worlds/viksen") else { return }
        #if os(iOS)
        let session = AVAudioSession.sharedInstance()
        guard !session.isOtherAudioPlaying else { return }   // never interrupt the listener's audio
        if session.category != .playAndRecord {               // the engine's own session, if running, already mixes us in
            try? session.setCategory(.ambient, options: [.mixWithOthers])
            try? session.setActive(true)
        }
        #endif
        guard let p = try? AVAudioPlayer(contentsOf: url) else { return }
        p.numberOfLoops = -1
        p.volume = 0
        p.play()
        p.setVolume(0.55, fadeDuration: 2.5)
        player = p
    }

    func fadeOut(seconds: Double = 2.0) {
        guard let p = player else { return }
        player = nil
        p.setVolume(0, fadeDuration: seconds)
        DispatchQueue.main.asyncAfter(deadline: .now() + seconds + 0.1) { p.stop() }
    }
}
