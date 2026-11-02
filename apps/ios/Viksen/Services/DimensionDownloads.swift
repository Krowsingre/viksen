import Foundation
import Observation
#if os(iOS)
import BackgroundAssets
import System
#endif

/// Dimensions are downloaded after install (Apple-hosted asset packs, one per Dimension,
/// on demand) and can be removed and downloaded again to save space. Each pack holds
/// "Dimensions/<id>/<stem files>". A build with the audio inside the app (development)
/// treats those Dimensions as installed and never removes them.
@MainActor
@Observable
final class DimensionDownloads {
    enum State: Equatable {
        case notInstalled
        case downloading(Double)   // 0...1
        case installed
        case failed(String)
    }

    private(set) var states: [String: State] = [:]
    private(set) var sizes: [String: Int64] = [:]   // bytes on this device, when installed
    private var tasks: [String: Task<Void, Never>] = [:]

    static func packID(_ dimensionID: String) -> String { dimensionID }
    static func path(_ dimensionID: String, _ file: String) -> String { "Dimensions/\(dimensionID)/\(file)" }

    func state(_ id: String) -> State { states[id] ?? .notInstalled }

    /// Stem file names of a Dimension (set by the engine controller).
    var stemFiles: ((String) -> [String])?

    private func measure(_ id: String, _ files: [String]) -> Int64? {
        guard let map = audioFiles(id, files: files) else { return nil }
        return map.values.reduce(Int64(0)) { total, url in
            total + Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
        }
    }

    /// Refreshes what is on the device. `bundled`: Dimensions whose audio ships in the app.
    func refresh(_ ids: [String], bundled: Set<String>) {
        for id in ids {
            if bundled.contains(id) { states[id] = .installed; continue }
            if case .downloading = states[id] { continue }
            #if os(iOS)
            let local = AssetPackManager.shared.assetPackIsAvailableLocally(withID: Self.packID(id))
            states[id] = local ? .installed : (states[id].map { if case .failed = $0 { return $0 }; return .notInstalled } ?? .notInstalled)
            if local, let files = stemFiles?(id) { sizes[id] = measure(id, files) }
            #else
            states[id] = .notInstalled
            #endif
        }
    }

    func download(_ id: String) {
        #if os(iOS)
        guard tasks[id] == nil else { return }
        states[id] = .downloading(0)
        AppLog.info("Download Dimension \(id)")
        let packID = Self.packID(id)
        let progressTask = Task {
            for await update in AssetPackManager.shared.statusUpdates(forAssetPackWithID: packID) {
                if case .downloading(_, let progress) = update {
                    self.states[id] = .downloading(progress.fractionCompleted)
                }
            }
        }
        tasks[id] = Task {
            defer { progressTask.cancel(); self.tasks[id] = nil }
            do {
                let pack = try await AssetPackManager.shared.assetPack(withID: packID)
                try await AssetPackManager.shared.ensureLocalAvailability(of: pack)
                self.states[id] = .installed
                if let files = self.stemFiles?(id) { self.sizes[id] = self.measure(id, files) }
                AppLog.info("Dimension \(id) downloaded")
            } catch is CancellationError {
                self.states[id] = .notInstalled
            } catch {
                self.states[id] = .failed(error.localizedDescription)
                AppLog.error("Dimension \(id) download failed: \(error.localizedDescription)")
            }
        }
        #endif
    }

    func cancel(_ id: String) {
        tasks[id]?.cancel()
        tasks[id] = nil
        states[id] = .notInstalled
    }

    func remove(_ id: String) {
        #if os(iOS)
        Task {
            do {
                try await AssetPackManager.shared.remove(assetPackWithID: Self.packID(id))
                self.states[id] = .notInstalled
                self.sizes[id] = nil
                AppLog.info("Dimension \(id) removed from this device")
            } catch {
                self.states[id] = .failed(error.localizedDescription)
                AppLog.error("Remove Dimension \(id) failed: \(error.localizedDescription)")
            }
        }
        #endif
    }

    /// Local URLs of a downloaded Dimension's audio (file name -> URL), or nil.
    func audioFiles(_ id: String, files: [String]) -> [String: URL]? {
        #if os(iOS)
        var map: [String: URL] = [:]
        for f in files {
            guard let url = try? AssetPackManager.shared.url(for: FilePath(Self.path(id, f))) else { return nil }
            map[f] = url
        }
        return map
        #else
        return nil
        #endif
    }
}
