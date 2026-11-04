import Foundation
import MediaPlayer
#if canImport(UIKit)
import UIKit
#endif

/// Player controls on the lock screen and in Control Center (owner, 2026-11-04): the
/// Dimension and song, progress, and play/pause. Play resumes the last Dimension; pause
/// fades it out like the stop button.
@MainActor
final class NowPlaying {
    var onPlay: (() -> Void)?
    var onPause: (() -> Void)?
    private var last: [String: AnyHashable] = [:]
    private var artwork: MPMediaItemArtwork?

    init() {
        let c = MPRemoteCommandCenter.shared()
        c.playCommand.addTarget { [weak self] _ in
            Task { @MainActor in self?.onPlay?() }
            return .success
        }
        c.pauseCommand.addTarget { [weak self] _ in
            Task { @MainActor in self?.onPause?() }
            return .success
        }
        c.stopCommand.addTarget { [weak self] _ in
            Task { @MainActor in self?.onPause?() }
            return .success
        }
        c.togglePlayPauseCommand.addTarget { [weak self] _ in
            Task { @MainActor in
                guard let self else { return }
                if (self.last["playing"] as? Bool) == true { self.onPause?() } else { self.onPlay?() }
            }
            return .success
        }
        // A Dimension is not a playlist: no skipping or scrubbing.
        for cmd in [c.nextTrackCommand, c.previousTrackCommand, c.skipForwardCommand, c.skipBackwardCommand,
                    c.changePlaybackPositionCommand, c.seekForwardCommand, c.seekBackwardCommand] {
            cmd.isEnabled = false
        }
        #if canImport(UIKit)
        if let image = UIImage(named: "FoxEmpty") {
            artwork = MPMediaItemArtwork(boundsSize: image.size) { _ in image }
        }
        #endif
    }

    /// Call about once a second. `dimension` nil: nothing has been played yet.
    func update(dimension: String?, song: String, artist: String, playing: Bool, position: Double, length: Double) {
        guard let dimension else {
            if !last.isEmpty { MPNowPlayingInfoCenter.default().nowPlayingInfo = nil; last = [:] }
            return
        }
        let state: [String: AnyHashable] = ["d": dimension, "s": song, "a": artist, "playing": playing,
                                            "p": Int(position), "l": Int(length)]
        guard state != last else { return }
        last = state
        var info: [String: Any] = [
            MPMediaItemPropertyTitle: song.isEmpty ? dimension : song,
            MPMediaItemPropertyAlbumTitle: dimension,
            MPMediaItemPropertyArtist: artist.isEmpty ? "Viksen" : artist,
            MPNowPlayingInfoPropertyPlaybackRate: playing ? 1.0 : 0.0,
            MPNowPlayingInfoPropertyElapsedPlaybackTime: position,
        ]
        if length > 0 { info[MPMediaItemPropertyPlaybackDuration] = length }
        if let artwork { info[MPMediaItemPropertyArtwork] = artwork }
        MPNowPlayingInfoCenter.default().nowPlayingInfo = info
        #if os(macOS)
        MPNowPlayingInfoCenter.default().playbackState = playing ? .playing : .paused
        #endif
    }
}
