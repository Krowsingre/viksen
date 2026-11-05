import AVFoundation
import Foundation

/// Transition effects under a Dimension's change of song (owner, 2026-11-05): the gentle
/// drone as it turns to night or rain, the bright bells as day returns. Fades in, then out
/// on its own; never louder than the music.
@MainActor
final class TransitionCue {
    enum Kind: String { case drone = "transition-drone", bells = "transition-bells" }

    private var player: AVAudioPlayer?
    private var fadeWork: DispatchWorkItem?

    func play(_ kind: Kind) {
        guard let url = Bundle.main.url(forResource: kind.rawValue, withExtension: "caf", subdirectory: "Worlds/_ambience"),
              let p = try? AVAudioPlayer(contentsOf: url) else { return }
        player?.setVolume(0, fadeDuration: 1)
        p.volume = 0
        p.play()
        p.setVolume(0.45, fadeDuration: 1.5)
        player = p
        // The drone plays out (about 20 s); the long bells fade after about 14 s.
        let hold: Double = kind == .bells ? 14 : max(2, p.duration - 4)
        fadeWork?.cancel()
        let work = DispatchWorkItem { [weak p] in p?.setVolume(0, fadeDuration: 4) }
        fadeWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + hold, execute: work)
        AppLog.info("Transition cue: \(kind == .bells ? "bells" : "drone")")
    }
}
