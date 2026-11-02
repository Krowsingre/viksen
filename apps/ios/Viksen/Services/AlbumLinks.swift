import Foundation
import Observation

/// Apple Music pages for the owner's albums. A Dimension can carry a fixed link; for
/// albums marked live, the app asks Apple's public catalogue search once (artist +
/// album title, exact match only) and remembers the result.
@MainActor
@Observable
final class AlbumLinks {
    private(set) var links: [String: URL] = [:]          // album title -> Apple Music page
    // Bookkeeping only: not observed, so starting a lookup while a view draws is harmless.
    @ObservationIgnored private var inFlight: Set<String> = []
    @ObservationIgnored private var tried: Set<String> = []   // one attempt per launch
    private let cacheKey = "albumLinks"

    init() {
        if let saved = UserDefaults.standard.dictionary(forKey: cacheKey) as? [String: String] {
            links = saved.compactMapValues(URL.init(string:))
        }
    }

    func link(for world: AtmosWorldInfo) -> URL? {
        if !world.albumLink.isEmpty, let url = URL(string: world.albumLink) { return url }
        guard world.albumOnAppleMusic, !world.album.isEmpty else { return nil }
        if let url = links[world.album] { return url }
        lookUp(album: world.album, artist: world.artist)
        return nil
    }

    private func lookUp(album: String, artist: String) {
        guard !inFlight.contains(album), !tried.contains(album) else { return }
        inFlight.insert(album)
        tried.insert(album)
        var comps = URLComponents(string: "https://itunes.apple.com/search")!
        let country = Locale.current.region?.identifier ?? "US"
        comps.queryItems = [.init(name: "term", value: "\(artist) \(album)"), .init(name: "entity", value: "album"),
                            .init(name: "attribute", value: "albumTerm"), .init(name: "limit", value: "25"),
                            .init(name: "country", value: country)]
        guard let url = comps.url else { return }
        Task {
            defer { self.inFlight.remove(album) }
            struct Result: Decodable { let collectionName: String?; let artistName: String?; let collectionViewUrl: String? }
            struct Response: Decodable { let results: [Result] }
            guard let (data, _) = try? await URLSession.shared.data(from: url),
                  let response = try? JSONDecoder().decode(Response.self, from: data) else { return }
            let match = response.results.first {
                $0.collectionName?.caseInsensitiveCompare(album) == .orderedSame &&
                $0.artistName?.caseInsensitiveCompare(artist) == .orderedSame
            }
            guard let page = match?.collectionViewUrl, let link = URL(string: page) else {
                AppLog.info("Album not found on Apple Music: \(album)")
                return
            }
            self.links[album] = link
            UserDefaults.standard.set(self.links.mapValues(\.absoluteString), forKey: self.cacheKey)
            AppLog.info("Album link found: \(album)")
        }
    }
}
