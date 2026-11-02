import Foundation
import HealthKit
import Observation

/// Reads heart rate from Apple Health and turns it into an exercise intensity
/// (0…1, heart-rate reserve). Live, second-by-second heart rate comes from the
/// Apple Watch companion during a workout; without it, Health delivers the
/// Watch's readings with some delay. Nothing leaves the device.
@MainActor
@Observable
final class HealthService {
    private let store = HKHealthStore()
    private var query: HKAnchoredObjectQuery?

    private(set) var isRunning = false
    private(set) var heartRate: Double?          // beats per minute (latest)
    private(set) var heartRateDate: Date?
    private(set) var restingHeartRate: Double?
    private(set) var maxHeartRate: Double = 190  // refined from age when available
    private(set) var lastError: String?

    /// Called with intensity 0…1 whenever a new heart rate arrives.
    var onIntensity: ((Double) -> Void)?

    var isAvailable: Bool { HKHealthStore.isHealthDataAvailable() }

    /// Heart-rate reserve: 0 at rest, 1 at estimated maximum.
    var intensity: Double? {
        guard let hr = heartRate else { return nil }
        let rest = restingHeartRate ?? 65
        return min(1, max(0, (hr - rest) / max(1, maxHeartRate - rest)))
    }

    private let readTypes: Set<HKObjectType> = [
        HKQuantityType(.heartRate),
        HKQuantityType(.restingHeartRate),
        HKObjectType.workoutType(),
        HKCharacteristicType(.dateOfBirth),
    ]

    func start() {
        guard isAvailable, !isRunning else { return }
        lastError = nil
        store.requestAuthorization(toShare: [], read: readTypes) { [weak self] _, error in
            Task { @MainActor in
                guard let self else { return }
                if let error { self.lastError = error.localizedDescription; return }
                self.loadProfile()
                self.startHeartRateQuery()
                self.isRunning = true
            }
        }
    }

    func stop() {
        if let query { store.stop(query) }
        query = nil
        isRunning = false
        heartRate = nil
    }

    /// Live samples streamed from the Watch companion (see WatchLink).
    func ingestLiveHeartRate(_ bpm: Double, at date: Date) {
        heartRate = bpm
        heartRateDate = date
        if let i = intensity { onIntensity?(i) }
    }

    private func loadProfile() {
        if let dob = try? store.dateOfBirthComponents(), let year = dob.year {
            let age = Double(Calendar.current.component(.year, from: Date()) - year)
            maxHeartRate = 208 - 0.7 * age // Tanaka estimate
        }
        let type = HKQuantityType(.restingHeartRate)
        let sort = [NSSortDescriptor(key: HKSampleSortIdentifierEndDate, ascending: false)]
        let q = HKSampleQuery(sampleType: type, predicate: nil, limit: 1, sortDescriptors: sort) { [weak self] _, samples, _ in
            let bpm = (samples?.first as? HKQuantitySample)?.quantity.doubleValue(for: .count().unitDivided(by: .minute()))
            Task { @MainActor in self?.restingHeartRate = bpm }
        }
        store.execute(q)
    }

    private func startHeartRateQuery() {
        let type = HKQuantityType(.heartRate)
        // Only samples from the last 10 minutes onwards: older readings say nothing about now.
        let predicate = HKQuery.predicateForSamples(withStart: Date().addingTimeInterval(-600), end: nil)
        let handler: (HKAnchoredObjectQuery, [HKSample]?, [HKDeletedObject]?, HKQueryAnchor?, Error?) -> Void = { [weak self] _, samples, _, _, _ in
            guard let latest = (samples as? [HKQuantitySample])?.max(by: { $0.endDate < $1.endDate }) else { return }
            let bpm = latest.quantity.doubleValue(for: .count().unitDivided(by: .minute()))
            let date = latest.endDate
            Task { @MainActor in
                guard let self else { return }
                if let current = self.heartRateDate, current >= date { return }
                self.ingestLiveHeartRate(bpm, at: date)
            }
        }
        let q = HKAnchoredObjectQuery(type: type, predicate: predicate, anchor: nil, limit: HKObjectQueryNoLimit,
                                      resultsHandler: handler)
        q.updateHandler = handler
        store.execute(q)
        query = q
    }
}
