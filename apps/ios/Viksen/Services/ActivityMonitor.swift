import CoreMotion
import Foundation
import Observation

/// What the listener is doing (still, walking, running, cycling, in a vehicle) and
/// their step cadence, from Core Motion. Needs the Motion & Fitness permission.
@MainActor
@Observable
final class ActivityMonitor {
    enum Activity: String {
        case unknown, still, walking, running, cycling, vehicle
        var title: String {
            switch self {
            case .unknown: "Unknown"
            case .still: "Still"
            case .walking: "Walking"
            case .running: "Running"
            case .cycling: "Cycling"
            case .vehicle: "In a vehicle"
            }
        }
    }

    private let activityManager = CMMotionActivityManager()
    private let pedometer = CMPedometer()

    private(set) var isRunning = false
    private(set) var activity: Activity = .unknown
    private(set) var cadenceStepsPerMinute: Double?
    private(set) var lastError: String?

    var isSupported: Bool { CMMotionActivityManager.isActivityAvailable() }
    var permissionDenied: Bool {
        let s = CMMotionActivityManager.authorizationStatus()
        return s == .denied || s == .restricted
    }

    /// Called when cadence changes (steps per minute, nil when unknown).
    var onCadence: ((Double?) -> Void)?

    func start() {
        guard isSupported, !isRunning else { return }
        lastError = nil
        activityManager.startActivityUpdates(to: .main) { [weak self] a in
            MainActor.assumeIsolated {
                guard let self, let a, a.confidence != .low else { return }
                self.activity = a.running ? .running : a.walking ? .walking : a.cycling ? .cycling
                    : a.automotive ? .vehicle : a.stationary ? .still : .unknown
                if self.activity == .still || self.activity == .vehicle { self.setCadence(nil) }
            }
        }
        if CMPedometer.isCadenceAvailable() {
            pedometer.startUpdates(from: Date()) { [weak self] data, error in
                // Pedometer callbacks arrive on a background queue.
                let cadence = data?.currentCadence?.doubleValue // steps per second
                let message = error?.localizedDescription
                Task { @MainActor in
                    guard let self else { return }
                    if let message { self.lastError = message; return }
                    self.setCadence(cadence.map { $0 * 60 })
                }
            }
        }
        isRunning = true
    }

    func stop() {
        activityManager.stopActivityUpdates()
        pedometer.stopUpdates()
        isRunning = false
        activity = .unknown
        setCadence(nil)
    }

    private func setCadence(_ spm: Double?) {
        let valid = spm.flatMap { $0 > 30 ? $0 : nil }
        cadenceStepsPerMinute = valid
        onCadence?(valid)
    }
}
