import AVFoundation
import AVKit
import SwiftUI
#if os(iOS)
import UIKit
#endif

/// Picture in Picture (owner, 2026-11-06): a small floating window with the shown
/// Dimension's badge. iOS only offers PiP to video, so the badge is shown as a still
/// "video" frame (no movie, almost no battery).
///
/// - Play/pause: if the badge shown is the Dimension playing, it pauses (stops) it;
///   otherwise it starts the Dimension shown.
/// - Back/forward (iOS draws them as skip buttons): step through the unlocked,
///   downloaded Dimensions, looping at the ends, without starting them. Hidden when
///   there is only one. Home's card follows (they share the chosen Dimension).
/// - Opens by itself when the app is left while a Dimension plays, if
///   "Start automatically" is on in Settings > Sound.
@MainActor
final class PictureInPicture: NSObject {
    static let autoKey = "pip.auto"
    static var autoStart: Bool { UserDefaults.standard.object(forKey: autoKey) as? Bool ?? true }

    private weak var engine: EngineController?
    /// The Dimensions PiP can step through (unlocked and on this device), in Home's order.
    var choices: () -> [AtmosWorldInfo] = { [] }

    #if os(iOS)
    let displayLayer = AVSampleBufferDisplayLayer()
    private var controller: AVPictureInPictureController?
    private var shownID: String?
    private var images: [String: CGImage] = [:]
    #endif

    init(engine: EngineController) {
        self.engine = engine
        super.init()
        #if os(iOS)
        guard AVPictureInPictureController.isPictureInPictureSupported() else { return }
        displayLayer.videoGravity = .resizeAspect
        var timebase: CMTimebase?
        CMTimebaseCreateWithSourceClock(allocator: kCFAllocatorDefault, sourceClock: CMClockGetHostTimeClock(), timebaseOut: &timebase)
        if let timebase {
            CMTimebaseSetTime(timebase, time: .zero)
            CMTimebaseSetRate(timebase, rate: 1)
            displayLayer.controlTimebase = timebase
        }
        let source = AVPictureInPictureController.ContentSource(sampleBufferDisplayLayer: displayLayer, playbackDelegate: self)
        let c = AVPictureInPictureController(contentSource: source)
        c.delegate = self
        controller = c
        #endif
        // The Settings switch posts this so the automatic start follows it at once.
        NotificationCenter.default.addObserver(forName: Self.settingChanged, object: nil, queue: .main) { [weak self] _ in
            MainActor.assumeIsolated { self?.refresh() }
        }
    }
    static let settingChanged = Notification.Name("pip.settingChanged")

    /// The Dimension the window shows: the one chosen on Home's card (playing or not).
    private var shown: AtmosWorldInfo? {
        let list = choices()
        return list.first { $0.worldID == engine?.lastDimensionID }
            ?? list.first { $0.worldID == EngineController.coreDimensionID } ?? list.first
    }

    /// Read by iOS from any thread: whether the shown Dimension is not playing.
    nonisolated(unsafe) private var pausedCache = true

    /// Call when the playing or chosen Dimension, the choices or the setting change.
    func refresh() {
        #if os(iOS)
        guard let controller, let engine else { return }
        let world = shown
        if world?.worldID != shownID {
            shownID = world?.worldID
            show(world?.worldID)
        }
        pausedCache = engine.activeDimensionID != world?.worldID
        controller.requiresLinearPlayback = choices().count < 2   // hides back/forward
        controller.canStartPictureInPictureAutomaticallyFromInline = Self.autoStart && engine.activeDimensionID != nil
        controller.invalidatePlaybackState()
        #endif
    }

    // MARK: Actions

    fileprivate func playPause(_ play: Bool) {
        guard let engine, let world = shown else { return }
        let playing = engine.activeDimensionID == world.worldID
        if play && !playing { engine.enterWorld(world) }
        else if !play && playing { engine.leaveWorld() }
        refresh()
    }

    fileprivate func step(_ forward: Bool) {
        guard let engine else { return }
        let list = choices()
        guard list.count > 1 else { return }
        let i = list.firstIndex { $0.worldID == shown?.worldID } ?? 0
        let next = list[(i + (forward ? 1 : -1) + list.count) % list.count]
        // A playing Dimension keeps playing; the window (and Home's card) move to the next.
        engine.selectDimension(next.worldID)
        refresh()
    }

    #if os(iOS)
    // MARK: The still frame

    private func show(_ id: String?) {
        guard let image = image(for: id), let buffer = Self.sampleBuffer(from: image) else { return }
        displayLayer.flush()
        displayLayer.enqueue(buffer)
    }

    private func image(for id: String?) -> CGImage? {
        let key = id ?? "-"
        if let hit = images[key] { return hit }
        var ui: UIImage?
        if let id {
            if id == EngineController.coreDimensionID { ui = UIImage(named: "ViksenBadge") }
            ui = ui ?? UIImage(named: "Badge-\(id)")
            if ui == nil, let worlds = Bundle.main.url(forResource: "Worlds", withExtension: nil) {
                for name in ["art.jpg", "art.png"] where ui == nil {
                    ui = UIImage(contentsOfFile: worlds.appendingPathComponent(id).appendingPathComponent(name).path)
                }
            }
        }
        ui = ui ?? UIImage(named: "FoxEmpty")
        // A square frame at a modest size: sharp in the PiP window, light on memory.
        let side: CGFloat = 600
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        let rendered = UIGraphicsImageRenderer(size: CGSize(width: side, height: side), format: format).image { ctx in
            UIColor.black.setFill()
            ctx.fill(CGRect(x: 0, y: 0, width: side, height: side))
            ui?.draw(in: CGRect(x: 0, y: 0, width: side, height: side))
        }
        images[key] = rendered.cgImage
        return rendered.cgImage
    }

    private static func sampleBuffer(from image: CGImage) -> CMSampleBuffer? {
        let w = image.width, h = image.height
        var pixelBuffer: CVPixelBuffer?
        let attrs: [CFString: Any] = [kCVPixelBufferCGImageCompatibilityKey: true,
                                      kCVPixelBufferCGBitmapContextCompatibilityKey: true,
                                      kCVPixelBufferIOSurfacePropertiesKey: [:] as CFDictionary]
        guard CVPixelBufferCreate(kCFAllocatorDefault, w, h, kCVPixelFormatType_32BGRA, attrs as CFDictionary, &pixelBuffer) == kCVReturnSuccess,
              let pb = pixelBuffer else { return nil }
        CVPixelBufferLockBaseAddress(pb, [])
        if let ctx = CGContext(data: CVPixelBufferGetBaseAddress(pb), width: w, height: h, bitsPerComponent: 8,
                               bytesPerRow: CVPixelBufferGetBytesPerRow(pb), space: CGColorSpaceCreateDeviceRGB(),
                               bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue) {
            ctx.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
        }
        CVPixelBufferUnlockBaseAddress(pb, [])
        var format: CMVideoFormatDescription?
        guard CMVideoFormatDescriptionCreateForImageBuffer(allocator: kCFAllocatorDefault, imageBuffer: pb, formatDescriptionOut: &format) == noErr,
              let format else { return nil }
        // One frame that lasts "forever", shown at once.
        var timing = CMSampleTimingInfo(duration: .invalid, presentationTimeStamp: .zero, decodeTimeStamp: .invalid)
        var sample: CMSampleBuffer?
        guard CMSampleBufferCreateReadyWithImageBuffer(allocator: kCFAllocatorDefault, imageBuffer: pb, formatDescription: format,
                                                       sampleTiming: &timing, sampleBufferOut: &sample) == noErr,
              let sample else { return nil }
        if let attachments = CMSampleBufferGetSampleAttachmentsArray(sample, createIfNecessary: true),
           CFArrayGetCount(attachments) > 0 {
            let dict = unsafeBitCast(CFArrayGetValueAtIndex(attachments, 0), to: CFMutableDictionary.self)
            CFDictionarySetValue(dict, Unmanaged.passUnretained(kCMSampleAttachmentKey_DisplayImmediately).toOpaque(),
                                 Unmanaged.passUnretained(kCFBooleanTrue).toOpaque())
        }
        return sample
    }
    #endif
}

#if os(iOS)
extension PictureInPicture: AVPictureInPictureSampleBufferPlaybackDelegate, AVPictureInPictureControllerDelegate {
    nonisolated func pictureInPictureController(_ pictureInPictureController: AVPictureInPictureController, setPlaying playing: Bool) {
        Task { @MainActor in self.playPause(playing) }
    }

    /// Looping music has no timeline: an endless range shows the window as "live".
    nonisolated func pictureInPictureControllerTimeRangeForPlayback(_ pictureInPictureController: AVPictureInPictureController) -> CMTimeRange {
        CMTimeRange(start: .negativeInfinity, duration: .positiveInfinity)
    }

    nonisolated func pictureInPictureControllerIsPlaybackPaused(_ pictureInPictureController: AVPictureInPictureController) -> Bool {
        pausedCache
    }

    nonisolated func pictureInPictureController(_ pictureInPictureController: AVPictureInPictureController,
                                                didTransitionToRenderSize newRenderSize: CMVideoDimensions) {}

    /// iOS's skip buttons step to the next or previous Dimension.
    nonisolated func pictureInPictureController(_ pictureInPictureController: AVPictureInPictureController,
                                                skipByInterval skipInterval: CMTime,
                                                completion completionHandler: @escaping () -> Void) {
        let forward = skipInterval.seconds > 0
        Task { @MainActor in
            self.step(forward)
            completionHandler()
        }
    }

    /// The music is the point: PiP never silences it.
    nonisolated func pictureInPictureControllerShouldProhibitBackgroundAudioPlayback(_ pictureInPictureController: AVPictureInPictureController) -> Bool {
        false
    }

    nonisolated func pictureInPictureController(_ pictureInPictureController: AVPictureInPictureController,
                                                failedToStartPictureInPictureWithError error: Error) {
        let message = error.localizedDescription
        Task { @MainActor in AppLog.warning("Picture in Picture could not start: \(message)") }
    }
}

/// Hosts the PiP layer in the window. iOS needs it on screen to start PiP; it is a
/// couple of points, behind everything, and invisible.
struct PictureInPictureHost: UIViewRepresentable {
    let layer: AVSampleBufferDisplayLayer

    func makeUIView(context: Context) -> UIView {
        let v = HostView()
        v.isUserInteractionEnabled = false
        v.hosted = layer
        v.layer.addSublayer(layer)
        return v
    }

    func updateUIView(_ uiView: UIView, context: Context) {}

    final class HostView: UIView {
        var hosted: CALayer?
        override func layoutSubviews() {
            super.layoutSubviews()
            hosted?.frame = bounds
        }
    }
}
#endif
