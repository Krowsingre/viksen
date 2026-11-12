import Foundation
import Observation

/// The listener's discoveries (P-2, DECISIONS #62): saved on the device and in the user's
/// iCloud (key-value storage, tied to their Apple ID), merged as a union so nothing is ever
/// lost or counted twice. Progress is per Dimension; Global Secrets are separate.
@MainActor
@Observable
final class ProgressStore {
    struct Discovery: Equatable {
        let dimensionID: String
        let phaseID: String
        let at: Date
    }

    private let progress: AtmosProgress
    private let cloud = NSUbiquitousKeyValueStore.default
    private static let cloudKey = "progress.v1"
    private var lastTick = Date()

    /// The most recent discovery made on this device (for a moment of celebration in the UI).
    private(set) var lastDiscovery: Discovery?

    /// A moon-event secret just found here (the notice on screen shows it).
    struct SecretFound: Equatable {
        let dimensionID: String
        let eventID: String
        let title: String
        let at: Date
    }
    private(set) var lastSecret: SecretFound?
    private var lastSecretTick = Date()
    /// Bumped whenever progress changes, so views reading it refresh.
    private(set) var revision = 0

    private static var fileURL: URL {
        let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("progress.json")
    }

    init() {
        progress = AtmosProgress(json: try? String(contentsOf: Self.fileURL, encoding: .utf8))
        NotificationCenter.default.addObserver(forName: NSUbiquitousKeyValueStore.didChangeExternallyNotification,
                                               object: cloud, queue: .main) { [weak self] note in
            let reason = note.userInfo?[NSUbiquitousKeyValueStoreChangeReasonKey] as? Int
            MainActor.assumeIsolated {
                if reason == NSUbiquitousKeyValueStoreQuotaViolationChange {
                    AppLog.warning("iCloud progress: storage quota reached")
                }
                self?.pullFromCloud()
            }
        }
        cloud.synchronize()
        pullFromCloud()
    }

    /// Call about once a second with what is playing.
    func tick(dimensionID: String?, songPhaseID: String, audible: Bool) {
        let now = Date()
        let dt = now.timeIntervalSince(lastTick)
        lastTick = now
        guard let dimensionID, !dimensionID.isEmpty else { return }
        if let phase = progress.tickDimension(dimensionID, songPhaseId: songPhaseID, audible: audible, dtSeconds: dt) {
            AppLog.info("Discovered \(phase) in \(dimensionID)")
            lastDiscovery = Discovery(dimensionID: dimensionID, phaseID: phase, at: now)
            save()
        }
    }

    /// Call about once a second with the moon events on now.
    func tickSecrets(dimensionID: String?, events: [String], audible: Bool) {
        let now = Date()
        let dt = now.timeIntervalSince(lastSecretTick)
        lastSecretTick = now
        guard let dimensionID, !dimensionID.isEmpty, !events.isEmpty else { return }
        if let found = progress.tickSecrets(dimensionID, events: events, audible: audible, dtSeconds: dt) {
            let title = AtmosProgress.moonEventTitle(found)
            AppLog.info("Secret found: \(title) in \(dimensionID)")
            lastSecret = SecretFound(dimensionID: dimensionID, eventID: found, title: title, at: now)
            save()
        }
    }
    func secretDates(_ dimensionID: String) -> [String: Date] { _ = revision; return progress.secretDates(dimensionID) }
    static func secretTitle(_ eventID: String) -> String { AtmosProgress.moonEventTitle(eventID) }
    func dismissSecret() { lastSecret = nil }

    func discoveredPhases(_ dimensionID: String) -> Set<String> { _ = revision; return Set(progress.discoveredPhaseIds(dimensionID)) }
    func lunarJourneyComplete(_ dimensionID: String) -> Bool { _ = revision; return progress.lunarJourneyComplete(dimensionID) }
    func secretsFound(_ dimensionID: String) -> Int { _ = revision; return progress.dimensionSecretsFound(dimensionID) }
    var globalSecretsFound: Int { _ = revision; return progress.globalSecretsFound() }
    func phaseProgress(_ dimensionID: String) -> Double { min(1, progress.phaseProgressSeconds(dimensionID) / 180) }
    var currentPhaseID: String { AtmosProgress.currentPhaseId() }
    func discoveryDates(_ dimensionID: String) -> [String: Date] { _ = revision; return progress.discoveryDates(dimensionID) }
    func journeyCompletedAt(_ dimensionID: String) -> Date? { _ = revision; return progress.journeyCompletedAt(dimensionID) }
    func nextWindow(_ phaseID: String) -> DateInterval? { AtmosProgress.nextWindow(forPhase: phaseID) }

    // MARK: Persistence

    private func pullFromCloud() {
        guard let remote = cloud.string(forKey: Self.cloudKey) else {
            if FileManager.default.fileExists(atPath: Self.fileURL.path) { save() }   // first sync from this device
            return
        }
        let added = progress.mergeJSON(remote)
        if added > 0 { AppLog.info("iCloud progress: \(added) discoveries from another device") }
        save()   // writes the union back, so both copies hold everything
    }

    private func save() {
        let json = progress.jsonString()
        do { try json.write(to: Self.fileURL, atomically: true, encoding: .utf8) }
        catch { AppLog.error("Could not save progress: \(error.localizedDescription)") }
        if cloud.string(forKey: Self.cloudKey) != json { cloud.set(json, forKey: Self.cloudKey) }
        revision += 1
    }
}
