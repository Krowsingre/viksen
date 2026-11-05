import Foundation
import Observation
#if os(iOS)
import BackgroundAssets
import System
#endif

/// Dimension tracks are downloaded after install, one by one or all at once, and can be
/// removed the same way to save space. Each track (song) is its own Apple-hosted,
/// on-demand asset pack holding "Dimensions/<dimension id>/<its stem files>".
/// Pack ids: a single-song Dimension uses the Dimension id; songs of multi-song
/// Dimensions use "<dimension id>--<song id>". A build with the audio inside the app
/// (development) treats those tracks as installed and never removes them.
@MainActor
@Observable
final class DimensionDownloads {
    enum State: Equatable {
        case notInstalled
        case downloading(Double)   // 0...1
        case installed
        case failed(String)
    }

    struct Track: Identifiable, Equatable {
        let id: String            // song id ("dimension" or "dimension/song")
        let dimensionID: String
        let title: String
        let files: [String]
        var packID: String { id.replacingOccurrences(of: "/", with: "--") }   // letters, digits, hyphens only
    }

    private(set) var states: [String: State] = [:]   // by track id
    private(set) var sizes: [String: Int64] = [:]    // bytes on this device, by track id
    private var tasks: [String: Task<Void, Never>] = [:]
    private var fileMaps: [String: [String: URL]] = [:]
    private var tracksByDimension: [String: [Track]] = [:]
    /// Tracks the listener removed. iOS may finish deleting an asset pack later (for
    /// example while its files were in use), so these count as removed until downloaded
    /// again, and the removal is retried at the next launch.
    private var removed: Set<String> = Set(UserDefaults.standard.stringArray(forKey: "removedTracks") ?? []) {
        didSet { UserDefaults.standard.set(Array(removed), forKey: "removedTracks") }
    }

    static func path(_ dimensionID: String, _ file: String) -> String { "Dimensions/\(dimensionID)/\(file)" }

    // MARK: Tracks

    func setTracks(_ tracks: [Track], for dimensionID: String) { tracksByDimension[dimensionID] = tracks }
    func tracks(_ dimensionID: String) -> [Track] { tracksByDimension[dimensionID] ?? [] }
    func state(_ trackID: String) -> State { states[trackID] ?? .notInstalled }

    func installedCount(_ dimensionID: String) -> Int { tracks(dimensionID).filter { state($0.id) == .installed }.count }
    func isAnyInstalled(_ dimensionID: String) -> Bool { installedCount(dimensionID) > 0 }
    func isDownloading(_ dimensionID: String) -> Bool {
        tracks(dimensionID).contains { if case .downloading = state($0.id) { return true }; return false }
    }
    func bytes(_ dimensionID: String) -> Int64 { tracks(dimensionID).compactMap { sizes[$0.id] }.reduce(0, +) }

    /// Local URLs of every downloaded track's audio in a Dimension (file name -> URL).
    func audioFiles(_ dimensionID: String) -> [String: URL]? {
        var map: [String: URL] = [:]
        for t in tracks(dimensionID) { if let m = fileMaps[t.id] { map.merge(m) { a, _ in a } } }
        return map.isEmpty ? nil : map
    }

    // MARK: Refresh

    /// Refreshes what is on the device. `bundled`: Dimensions whose audio ships in the app.
    func refresh(bundled: Set<String>) {
        for (dimension, tracks) in tracksByDimension {
            for t in tracks {
                if bundled.contains(dimension) { states[t.id] = .installed; continue }
                if case .downloading = states[t.id] { continue }
                if removed.contains(t.id) {
                    states[t.id] = .notInstalled
                    #if os(iOS)
                    Task { try? await AssetPackManager.shared.remove(assetPackWithID: t.packID) }   // finish an earlier removal
                    #endif
                    continue
                }
                #if os(iOS)
                Task {
                    let map = await self.resolve(t)
                    if case .downloading = self.states[t.id] { return }
                    if let map {
                        self.fileMaps[t.id] = map
                        self.states[t.id] = .installed
                        self.sizes[t.id] = Self.measure(map)
                    } else {
                        self.fileMaps[t.id] = nil
                        self.sizes[t.id] = nil
                        if case .failed = self.states[t.id] {} else { self.states[t.id] = .notInstalled }
                    }
                }
                #else
                states[t.id] = .notInstalled
                #endif
            }
        }
    }

    // MARK: Download / remove

    func download(_ track: Track) {
        #if os(iOS)
        guard tasks[track.id] == nil, state(track.id) != .installed else { return }
        removed.remove(track.id)
        states[track.id] = .downloading(0)
        AppLog.info("Download track \(track.id)")
        let packID = track.packID
        let progressTask = Task {
            for await update in AssetPackManager.shared.statusUpdates(forAssetPackWithID: packID) {
                if case .downloading(_, let progress) = update {
                    self.states[track.id] = .downloading(progress.fractionCompleted)
                }
            }
        }
        tasks[track.id] = Task {
            defer { progressTask.cancel(); self.tasks[track.id] = nil }
            do {
                let pack = try await AssetPackManager.shared.assetPack(withID: packID)
                try await AssetPackManager.shared.ensureLocalAvailability(of: pack)
                let map = await self.resolve(track)
                self.fileMaps[track.id] = map
                self.states[track.id] = map == nil ? .failed("the download is incomplete") : .installed
                self.sizes[track.id] = map.map(Self.measure)
                AppLog.info("Track \(track.id) downloaded")
            } catch is CancellationError {
                self.states[track.id] = .notInstalled
            } catch {
                self.states[track.id] = .failed(error.localizedDescription)
                AppLog.error("Track \(track.id) download failed: \(error.localizedDescription)")
            }
        }
        #endif
    }

    func downloadAll(_ dimensionID: String) {
        for t in tracks(dimensionID) where state(t.id) != .installed { download(t) }
    }

    func cancel(_ track: Track) {
        tasks[track.id]?.cancel()
        tasks[track.id] = nil
        states[track.id] = .notInstalled
    }

    /// Removes a track: it is gone from the app at once; iOS deletes the files (now, or
    /// at the next launch if they were still in use).
    func remove(_ track: Track) {
        removed.insert(track.id)
        states[track.id] = .notInstalled
        sizes[track.id] = nil
        fileMaps[track.id] = nil
        #if os(iOS)
        Task {
            do {
                try await AssetPackManager.shared.remove(assetPackWithID: track.packID)
                AppLog.info("Track \(track.id) removed from this device")
            } catch {
                AppLog.warning("Track \(track.id): iOS will finish removing it later (\(error.localizedDescription))")
            }
        }
        #endif
    }

    func removeAll(_ dimensionID: String) {
        for t in tracks(dimensionID) where state(t.id) == .installed || state(t.id) != .notInstalled { remove(t) }
    }

    // MARK: Files

    private func resolve(_ track: Track) async -> [String: URL]? {
        #if os(iOS)
        guard !track.files.isEmpty else { return nil }
        var map: [String: URL] = [:]
        for f in track.files {
            guard let url = try? await AssetPackManager.shared.url(for: FilePath(Self.path(track.dimensionID, f))),
                  FileManager.default.fileExists(atPath: url.path) else { return nil }
            map[f] = url
        }
        return map
        #else
        return nil
        #endif
    }

    private static func measure(_ map: [String: URL]) -> Int64 {
        map.values.reduce(Int64(0)) { total, url in
            total + Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
        }
    }
}
