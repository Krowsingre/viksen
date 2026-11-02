import SwiftUI

/// Restrained accents distinguish audio systems (spec §12).
enum Theme {
    // Deep violet palette (owner-selected concept direction).
    static let accent = Color(red: 0.55, green: 0.45, blue: 0.98)
    static let accentGlow = Color(red: 0.62, green: 0.40, blue: 1.0)
    static let background = Color(red: 0.03, green: 0.03, blue: 0.07)
    static let card = Color(red: 0.09, green: 0.08, blue: 0.16)
    static let music = Color(red: 0.45, green: 0.62, blue: 1.0)
    static let atmosphere = Color(red: 0.35, green: 0.82, blue: 0.74)
    static let micRaw = Color(red: 1.0, green: 0.72, blue: 0.30)
    static let micTransformed = Color(red: 0.74, green: 0.55, blue: 1.0)
    static let danger = Color(red: 1.0, green: 0.30, blue: 0.30)

    static func color(for bus: EngineController.BusLevel) -> Color {
        switch bus {
        case .music: music
        case .atmosphere: atmosphere
        case .micRaw: micRaw
        case .micTransformed: micTransformed
        }
    }
}

enum Appearance: String, CaseIterable, Identifiable {
    case dark, light, system
    var id: String { rawValue }
    var title: String { rawValue.capitalized }
    var colorScheme: ColorScheme? {
        switch self {
        case .dark: .dark
        case .light: .light
        case .system: nil
        }
    }
}
