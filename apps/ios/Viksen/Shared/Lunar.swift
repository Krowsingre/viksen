import Foundation

/// Display names for the engine's lunar-calendar phase ids.
enum Lunar {
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
