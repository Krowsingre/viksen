import CoreLocation
import Foundation
import Observation

/// When is it night? (owner, 2026-11-04) After dark a Dimension plays tonight's lunar song
/// and lunar listening counts toward the Moon Chart. The listener chooses the rule:
/// fixed hours (7 pm to 4 am, the default) or local sunset to sunrise, which needs an
/// approximate location. The location is kept in memory only, never stored or sent.
@MainActor
@Observable
final class NightClock {
    enum Rule: String, CaseIterable, Identifiable {
        case fixedHours, sunsetSunrise
        var id: String { rawValue }
        var title: String {
            switch self {
            case .fixedHours: "7 pm to 4 am"
            case .sunsetSunrise: "Sunset to sunrise"
            }
        }
    }

    static let nightStartHour = 19, nightEndHour = 4

    var rule: Rule = Rule(rawValue: UserDefaults.standard.string(forKey: "nightRule") ?? "") ?? .fixedHours {
        didSet {
            UserDefaults.standard.set(rule.rawValue, forKey: "nightRule")
            if rule == .sunsetSunrise { locate() }
            refresh()
        }
    }
    private(set) var isNight = false
    /// Dawn and dusk (owner, 2026-11-11): the 45 minutes just after night ends and just
    /// before it begins, by whichever rule is chosen. They never overlap the night.
    private(set) var isDawn = false
    private(set) var isDusk = false
    static let twilight: TimeInterval = 45 * 60
    /// Sunset to sunrise needs a location; until there is one, the fixed hours apply.
    private(set) var usingFallback = false
    private(set) var coordinate: CLLocationCoordinate2D?
    private let manager = CLLocationManager()
    private let proxy = NightLocationProxy()

    init() {
        manager.desiredAccuracy = kCLLocationAccuracyReduced
        manager.delegate = proxy
        proxy.onLocation = { [weak self] c in
            MainActor.assumeIsolated { self?.coordinate = c; self?.refresh() }
        }
        proxy.onAuthorization = { [weak self] in
            MainActor.assumeIsolated { if self?.rule == .sunsetSunrise { self?.locate() } }
        }
        if rule == .sunsetSunrise { locate() }
        refresh()
    }

    var locationDenied: Bool {
        let s = manager.authorizationStatus
        return s == .denied || s == .restricted
    }

    func refresh(at date: Date = Date()) {
        // Assigned only on change: views reading these are not redrawn every second.
        let (night, fallback) = nightAt(date)
        let dawn = !night && nightAt(date.addingTimeInterval(-Self.twilight)).night
        let dusk = !night && !dawn && nightAt(date.addingTimeInterval(Self.twilight)).night
        if night != isNight { isNight = night }
        if dawn != isDawn { isDawn = dawn }
        if dusk != isDusk { isDusk = dusk }
        if fallback != usingFallback { usingFallback = fallback }
    }

    private func nightAt(_ date: Date) -> (night: Bool, fallback: Bool) {
        if rule == .sunsetSunrise, let c = coordinate, let dark = Self.isDark(at: date, coordinate: c) {
            return (dark, false)
        }
        return (Self.inFixedHours(date), rule == .sunsetSunrise)
    }

    private func locate() {
        switch manager.authorizationStatus {
        case .notDetermined: manager.requestWhenInUseAuthorization()
        case .denied, .restricted: break
        default: manager.requestLocation()
        }
    }

    static func inFixedHours(_ date: Date) -> Bool {
        let h = Calendar.current.component(.hour, from: date)
        return h >= nightStartHour || h < nightEndHour
    }

    /// True between local sunset and sunrise; nil in polar day (the sun never sets), where
    /// the fixed hours apply instead.
    static func isDark(at date: Date, coordinate c: CLLocationCoordinate2D) -> Bool? {
        guard let (rise, set) = sunTimes(on: date, coordinate: c) else {
            return sunAlwaysDown(on: date, latitude: c.latitude) ? true : nil
        }
        return date < rise || date >= set
    }

    // NOAA sunrise equation (accurate to a minute or two), for the UTC day containing `date`.
    private static func declinationAndEquation(_ date: Date) -> (decl: Double, eqMinutes: Double) {
        let jd = date.timeIntervalSince1970 / 86400 + 2440587.5
        let t = (jd - 2451545) / 36525
        let l0 = (280.46646 + t * (36000.76983 + t * 0.0003032)).truncatingRemainder(dividingBy: 360)
        let m = 357.52911 + t * (35999.05029 - 0.0001537 * t)
        let e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t)
        let rad = Double.pi / 180
        let c = sin(m * rad) * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin(2 * m * rad) * (0.019993 - 0.000101 * t)
            + sin(3 * m * rad) * 0.000289
        let lambda = l0 + c - 0.00569 - 0.00478 * sin((125.04 - 1934.136 * t) * rad)
        let eps = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60) / 60
            + 0.00256 * cos((125.04 - 1934.136 * t) * rad)
        let decl = asin(sin(eps * rad) * sin(lambda * rad)) / rad
        let y = pow(tan(eps * rad / 2), 2)
        let eq = 4 / rad * (y * sin(2 * l0 * rad) - 2 * e * sin(m * rad) + 4 * e * y * sin(m * rad) * cos(2 * l0 * rad)
            - 0.5 * y * y * sin(4 * l0 * rad) - 1.25 * e * e * sin(2 * m * rad))
        return (decl, eq)
    }

    private static func hourAngleCos(_ latitude: Double, _ decl: Double) -> Double {
        let rad = Double.pi / 180
        return (cos(90.833 * rad) - sin(latitude * rad) * sin(decl * rad)) / (cos(latitude * rad) * cos(decl * rad))
    }

    static func sunTimes(on date: Date, coordinate c: CLLocationCoordinate2D) -> (rise: Date, set: Date)? {
        var utc = Calendar(identifier: .gregorian)
        utc.timeZone = TimeZone(identifier: "UTC")!
        // Use the local calendar day, solved around local noon.
        let localNoon = Calendar.current.date(bySettingHour: 12, minute: 0, second: 0, of: date) ?? date
        let (decl, eq) = declinationAndEquation(localNoon)
        let cosH = hourAngleCos(c.latitude, decl)
        guard cosH >= -1, cosH <= 1 else { return nil }
        let h = acos(cosH) * 180 / Double.pi
        let noonMinutesUTC = 720 - 4 * c.longitude - eq
        let dayStartUTC = utc.startOfDay(for: localNoon)
        // The solar noon nearest the local noon.
        var noon = dayStartUTC.addingTimeInterval(noonMinutesUTC * 60)
        while noon.timeIntervalSince(localNoon) > 43200 { noon.addTimeInterval(-86400) }
        while localNoon.timeIntervalSince(noon) > 43200 { noon.addTimeInterval(86400) }
        return (noon.addingTimeInterval(-h * 4 * 60), noon.addingTimeInterval(h * 4 * 60))
    }

    private static func sunAlwaysDown(on date: Date, latitude: Double) -> Bool {
        let (decl, _) = declinationAndEquation(date)
        return hourAngleCos(latitude, decl) > 1
    }
}

private final class NightLocationProxy: NSObject, CLLocationManagerDelegate {
    var onLocation: ((CLLocationCoordinate2D) -> Void)?
    var onAuthorization: (() -> Void)?
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        if let c = locations.last?.coordinate { onLocation?(c) }
    }
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {}
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) { onAuthorization?() }
}
