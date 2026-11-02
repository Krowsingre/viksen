import Foundation
import Observation

/// Imported media lives in the app's own Documents/Library/Audio folder. Imports
/// are copies: the user's original file is never modified or moved.
@MainActor
@Observable
final class MediaLibrary {
    struct Item: Identifiable, Hashable {
        let url: URL
        var id: URL { url }
        var name: String { url.lastPathComponent }
        let byteSize: Int64
        let added: Date
    }

    private(set) var items: [Item] = []
    var lastError: String?

    let audioDirectory: URL = {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return docs.appendingPathComponent("Library/Audio", isDirectory: true)
    }()

    init() {
        try? FileManager.default.createDirectory(at: audioDirectory, withIntermediateDirectories: true)
        refresh()
    }

    func refresh() {
        let keys: [URLResourceKey] = [.fileSizeKey, .creationDateKey]
        let urls = (try? FileManager.default.contentsOfDirectory(at: audioDirectory, includingPropertiesForKeys: keys)) ?? []
        items = urls.compactMap { url in
            let v = try? url.resourceValues(forKeys: Set(keys))
            return Item(url: url, byteSize: Int64(v?.fileSize ?? 0), added: v?.creationDate ?? .distantPast)
        }
        .sorted { $0.added > $1.added }
    }

    /// Copies a user-picked file (security-scoped) into the library.
    func importCopy(of source: URL) -> URL? {
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        var dest = audioDirectory.appendingPathComponent(source.lastPathComponent)
        var n = 1
        while FileManager.default.fileExists(atPath: dest.path) {
            let base = source.deletingPathExtension().lastPathComponent
            dest = audioDirectory.appendingPathComponent("\(base)-\(n).\(source.pathExtension)")
            n += 1
        }
        do {
            try FileManager.default.copyItem(at: source, to: dest)
            refresh()
            return dest
        } catch {
            lastError = error.localizedDescription
            return nil
        }
    }

    func delete(_ item: Item) {
        do { try FileManager.default.removeItem(at: item.url) } catch { lastError = error.localizedDescription }
        refresh()
    }
}
