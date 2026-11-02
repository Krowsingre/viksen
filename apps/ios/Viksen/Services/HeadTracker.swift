import CoreMotion
import Foundation
import Observation

/// Head tracking from supported headphones (AirPods Pro / Max / 3rd gen and later,
/// some Beats) via CMHeadphoneMotionManager. Publishes head yaw relative to a
/// recentre point. Needs the Motion & Fitness permission; data stays on device.
@MainActor
@Observable
final class HeadTracker {
    private let manager = CMHeadphoneMotionManager()
    private let delegateProxy = HeadphoneDelegateProxy()
    private var referenceYaw: Double?

    private(set) var isRunning = false
    private(set) var headphonesConnected = false
    private(set) var yawDegrees: Double = 0     // + = turned right
    private(set) var lastError: String?

    /// Called on every update with the yaw in degrees.
    var onYaw: ((Double) -> Void)?

    var isSupported: Bool { manager.isDeviceMotionAvailable }

    init() {
        delegateProxy.onConnect = { [weak self] in self?.headphonesConnected = true; self?.referenceYaw = nil }
        delegateProxy.onDisconnect = { [weak self] in
            guard let self else { return }
            self.headphonesConnected = false
            self.yawDegrees = 0
            self.onYaw?(0)
        }
        manager.delegate = delegateProxy
    }

    func start() {
        guard isSupported, !isRunning else { return }
        referenceYaw = nil
        lastError = nil
        manager.startDeviceMotionUpdates(to: .main) { [weak self] motion, error in
            MainActor.assumeIsolated {
                guard let self else { return }
                if let error {
                    self.lastError = error.localizedDescription
                    return
                }
                guard let motion else { return }
                self.headphonesConnected = true
                let yaw = motion.attitude.yaw // radians, + = counter-clockwise (to the left)
                if self.referenceYaw == nil { self.referenceYaw = yaw }
                var delta = yaw - (self.referenceYaw ?? yaw)
                delta = atan2(sin(delta), cos(delta))   // wrap to -pi...pi
                self.yawDegrees = -delta * 180 / .pi    // + = right
                self.onYaw?(self.yawDegrees)
            }
        }
        isRunning = true
    }

    func stop() {
        manager.stopDeviceMotionUpdates()
        isRunning = false
        yawDegrees = 0
        onYaw?(0)
    }

    /// Makes the current head direction the new "front".
    func recenter() { referenceYaw = nil }
}

/// The delegate must be an NSObject; this keeps HeadTracker a plain Swift class.
private final class HeadphoneDelegateProxy: NSObject, CMHeadphoneMotionManagerDelegate {
    var onConnect: (@MainActor () -> Void)?
    var onDisconnect: (@MainActor () -> Void)?

    func headphoneMotionManagerDidConnect(_ manager: CMHeadphoneMotionManager) {
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onConnect?() } }
    }

    func headphoneMotionManagerDidDisconnect(_ manager: CMHeadphoneMotionManager) {
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onDisconnect?() } }
    }
}
