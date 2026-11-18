import Foundation

/// How the lunar day is numbered on screen (owner, 2026-11-18). Only a label: the app acts
/// on the moon's phases and events, never on this number.
enum LunarDayStyle: String, CaseIterable, Identifiable {
    case traditional   // the date of the new moon is day 1 (lunar calendars)
    case moonAge       // the date of the new moon is day 0 (the Moon's age)
    var id: String { rawValue }
    var title: String {
        switch self {
        case .traditional: "Traditional (new moon is day 1)"
        case .moonAge: "Moon's age (new moon is day 0)"
        }
    }
}

/// Display names for the engine's lunar-calendar phase ids.
enum Lunar {
    /// The eight named phases in order through a lunar cycle.
    static let order = ["new", "waxingCrescent", "firstQuarter", "waxingGibbous",
                        "full", "waningGibbous", "lastQuarter", "waningCrescent"]

    static func phaseName(_ id: String) -> String {
        switch id {
        case "new": "New Moon"
        case "waxingCrescent": "Waxing Crescent"
        case "firstQuarter": "First Quarter"
        case "waxingGibbous": "Waxing Gibbous"
        case "full": "Full Moon"
        case "waningGibbous": "Waning Gibbous"
        case "lastQuarter": "Last Quarter"
        case "waningCrescent": "Waning Crescent"
        default: "Moon"
        }
    }

    static func symbol(_ id: String) -> String {
        switch id {
        case "new": "moonphase.new.moon"
        case "waxingCrescent": "moonphase.waxing.crescent"
        case "firstQuarter": "moonphase.first.quarter"
        case "waxingGibbous": "moonphase.waxing.gibbous"
        case "full": "moonphase.full.moon"
        case "waningGibbous": "moonphase.waning.gibbous"
        case "lastQuarter": "moonphase.last.quarter"
        case "waningCrescent": "moonphase.waning.crescent"
        default: "moon"
        }
    }
}
