import Foundation
import HealthKit
import Observation
import WatchConnectivity

/// Runs a workout session on Apple Watch and streams live heart rate to the
/// iPhone app, where it drives the exercise-intensity reaction. The workout is
/// saved to Apple Health when it ends.
@MainActor
@Observable
final class WorkoutManager {
    enum Kind: String, CaseIterable, Identifiable {
        case walk, run, cycle, other
        var id: String { rawValue }
        var title: String {
            switch self {
            case .walk: "Walk"
            case .run: "Run"
            case .cycle: "Cycle"
            case .other: "Other"
            }
        }
        var activityType: HKWorkoutActivityType {
            switch self {
            case .walk: .walking
            case .run: .running
            case .cycle: .cycling
            case .other: .other
            }
        }
    }

    private let store = HKHealthStore()
    private var session: HKWorkoutSession?
    private var builder: HKLiveWorkoutBuilder?
    private let link = PhoneLink()

    private(set) var isRunning = false
    private(set) var heartRate: Double?
    private(set) var phoneReachable = false
    private(set) var lastError: String?
    var kind: Kind = .walk

    init() {
        link.onReachability = { [weak self] reachable in self?.phoneReachable = reachable }
        link.activate()
    }

    func start() {
        lastError = nil
        let share: Set<HKSampleType> = [HKObjectType.workoutType()]
        let read: Set<HKObjectType> = [HKQuantityType(.heartRate)]
        store.requestAuthorization(toShare: share, read: read) { [weak self] ok, error in
            Task { @MainActor in
                guard let self else { return }
                guard ok else { self.lastError = error?.localizedDescription ?? "Health access was not granted."; return }
                self.beginSession()
            }
        }
    }

    private func beginSession() {
        let config = HKWorkoutConfiguration()
        config.activityType = kind.activityType
        config.locationType = .unknown
        do {
            let session = try HKWorkoutSession(healthStore: store, configuration: config)
            let builder = session.associatedWorkoutBuilder()
            builder.dataSource = HKLiveWorkoutDataSource(healthStore: store, workoutConfiguration: config)
            let delegate = BuilderDelegate { [weak self] bpm, date in
                Task { @MainActor in
                    self?.heartRate = bpm
                    self?.link.send(heartRate: bpm, at: date)
                }
            }
            builder.delegate = delegate
            builderDelegate = delegate
            self.session = session
            self.builder = builder
            let start = Date()
            session.startActivity(with: start)
            builder.beginCollection(withStart: start) { _, _ in }
            isRunning = true
        } catch {
            lastError = error.localizedDescription
        }
    }

    private var builderDelegate: BuilderDelegate?

    func stop() {
        guard let session, let builder else { return }
        session.end()
        builder.endCollection(withEnd: Date()) { _, _ in
            builder.finishWorkout { _, _ in }
        }
        self.session = nil
        self.builder = nil
        builderDelegate = nil
        isRunning = false
        heartRate = nil
        link.sendStopped()
    }
}

/// HKLiveWorkoutBuilderDelegate must be an NSObject.
private final class BuilderDelegate: NSObject, HKLiveWorkoutBuilderDelegate {
    private let onHeartRate: (Double, Date) -> Void
    init(onHeartRate: @escaping (Double, Date) -> Void) { self.onHeartRate = onHeartRate }

    func workoutBuilder(_ builder: HKLiveWorkoutBuilder, didCollectDataOf types: Set<HKSampleType>) {
        let hrType = HKQuantityType(.heartRate)
        guard types.contains(hrType),
              let q = builder.statistics(for: hrType)?.mostRecentQuantity() else { return }
        onHeartRate(q.doubleValue(for: .count().unitDivided(by: .minute())), Date())
    }

    func workoutBuilderDidCollectEvent(_ builder: HKLiveWorkoutBuilder) {}
}

/// Watch side of WatchConnectivity: sends live heart rate to the iPhone.
private final class PhoneLink: NSObject, WCSessionDelegate {
    var onReachability: (@MainActor (Bool) -> Void)?

    func activate() {
        guard WCSession.isSupported() else { return }
        WCSession.default.delegate = self
        WCSession.default.activate()
    }

    func send(heartRate: Double, at date: Date) {
        let s = WCSession.default
        guard s.activationState == .activated else { return }
        let payload: [String: Any] = ["hr": heartRate, "t": date.timeIntervalSince1970]
        if s.isReachable {
            s.sendMessage(payload, replyHandler: nil, errorHandler: nil)
        } else {
            try? s.updateApplicationContext(payload)
        }
    }

    func sendStopped() {
        let s = WCSession.default
        guard s.activationState == .activated else { return }
        let payload: [String: Any] = ["stopped": true]
        if s.isReachable { s.sendMessage(payload, replyHandler: nil, errorHandler: nil) }
        else { try? s.updateApplicationContext(payload) }
    }

    func session(_ session: WCSession, activationDidCompleteWith state: WCSessionActivationState, error: Error?) {
        let reachable = session.isReachable
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onReachability?(reachable) } }
    }

    func sessionReachabilityDidChange(_ session: WCSession) {
        let reachable = session.isReachable
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onReachability?(reachable) } }
    }
}
