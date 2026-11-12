import CoreLocation
import Foundation
import Observation
import WeatherKit

/// Optional, opt-in weather (Apple WeatherKit). Uses approximate location only,
/// refreshes at most every 30 minutes while enabled, and never stores location.
@MainActor
@Observable
final class WeatherMonitor {
    enum Mood: String {
        case clear, cloudy, rain, storm, snow, fog, wind
        var title: String { rawValue.capitalized }
    }

    private let locationManager = CLLocationManager()
    private let proxy = LocationProxy()
    private var refreshTask: Task<Void, Never>?
    private var lastFetch: Date?

    private(set) var isRunning = false
    private(set) var mood: Mood?
    private(set) var conditionText: String?
    private(set) var temperatureText: String?
    private(set) var isDaylight = true
    private(set) var attributionMarkURL: URL?
    private(set) var attributionLegalURL: URL?
    private(set) var lastError: String?

    var onMood: ((Mood?, Bool) -> Void)?

    var locationDenied: Bool {
        let s = locationManager.authorizationStatus
        return s == .denied || s == .restricted
    }

    init() {
        locationManager.desiredAccuracy = kCLLocationAccuracyReduced
        locationManager.delegate = proxy
        proxy.onLocation = { [weak self] loc in Task { await self?.fetch(for: loc) } }
        proxy.onError = { [weak self] message in self?.lastError = message }
        proxy.onAuthorization = { [weak self] in
            guard let self, self.isRunning else { return }
            if self.isAuthorized { self.locationManager.requestLocation() }
        }
    }

    private var isAuthorized: Bool {
        let s = locationManager.authorizationStatus
        #if os(iOS)
        return s == .authorizedWhenInUse || s == .authorizedAlways
        #else
        return s == .authorizedAlways   // macOS has a single "authorized" state
        #endif
    }

    func start() {
        guard !isRunning else { return }
        isRunning = true
        lastError = nil
        if locationManager.authorizationStatus == .notDetermined {
            locationManager.requestWhenInUseAuthorization()
        } else if isAuthorized {
            locationManager.requestLocation()
        }
        refreshTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(900))   // every 15 minutes
                guard let self, self.isRunning, self.isAuthorized else { continue }
                self.locationManager.requestLocation()
            }
        }
    }

    /// Checks again now (e.g. when the app comes back), unless checked in the last 5 minutes.
    func refreshNow() {
        guard isRunning, isAuthorized else { return }
        if let last = lastFetch, Date().timeIntervalSince(last) < 300 { return }
        lastFetch = nil
        locationManager.requestLocation()
    }

    func stop() {
        isRunning = false
        refreshTask?.cancel()
        refreshTask = nil
        mood = nil
        conditionText = nil
        temperatureText = nil
        onMood?(nil, true)
    }

    /// The approximate location last used (in memory only; also for the moon's local sky).
    private(set) var coordinate: CLLocationCoordinate2D?

    private func fetch(for location: CLLocation) async {
        coordinate = location.coordinate
        if let last = lastFetch, Date().timeIntervalSince(last) < 800, mood != nil { return }
        do {
            let current = try await WeatherService.shared.weather(for: location, including: .current)
            let attribution = try? await WeatherService.shared.attribution
            lastFetch = Date()
            attributionMarkURL = attribution?.combinedMarkDarkURL
            attributionLegalURL = attribution?.legalPageURL
            conditionText = current.condition.description
            temperatureText = current.temperature.formatted(.measurement(width: .abbreviated, usage: .weather))
            isDaylight = current.isDaylight
            mood = Self.mood(for: current)
            onMood?(mood, isDaylight)
        } catch {
            lastError = "Weather unavailable: \(error.localizedDescription)"
        }
    }

    private static func mood(for w: CurrentWeather) -> Mood {
        switch w.condition {
        case .thunderstorms, .isolatedThunderstorms, .scatteredThunderstorms, .strongStorms, .tropicalStorm, .hurricane:
            return .storm
        case .rain, .heavyRain, .drizzle, .sunShowers, .freezingRain, .freezingDrizzle, .sleet, .hail:
            return .rain
        case .snow, .heavySnow, .flurries, .sunFlurries, .blizzard, .blowingSnow, .wintryMix:
            return .snow
        case .foggy, .haze, .smoky, .blowingDust:
            return .fog
        case .breezy, .windy:
            return .wind
        case .cloudy, .mostlyCloudy, .partlyCloudy:
            return .cloudy
        default:
            return .clear
        }
    }
}

private final class LocationProxy: NSObject, CLLocationManagerDelegate {
    var onLocation: (@MainActor (CLLocation) -> Void)?
    var onError: (@MainActor (String) -> Void)?
    var onAuthorization: (@MainActor () -> Void)?

    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let loc = locations.last else { return }
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onLocation?(loc) } }
    }

    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        let message = error.localizedDescription
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onError?(message) } }
    }

    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onAuthorization?() } }
    }
}
