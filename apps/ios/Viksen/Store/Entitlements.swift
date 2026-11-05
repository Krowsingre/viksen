import Foundation

/// Purchase tiers. Higher tiers include everything below them.
enum Tier: Int, Comparable, CaseIterable {
    case free = 0, experience, studio

    static func < (a: Tier, b: Tier) -> Bool { a.rawValue < b.rawValue }

    var title: String {
        switch self {
        case .free: "Viksen Free"
        case .experience: "Viksen Experience"
        case .studio: "Viksen Studio"
        }
    }
}

/// Every gated capability and the tier it needs. This table is the single place
/// to change what each tier includes (owner's tier plan, docs/DECISIONS.md #19).
enum Feature: CaseIterable {
    // Free: basic augmented audio
    case atmosphereBed, liveMicBasic, motionSensing
    // Experience: full listening experience, environmental transformations
    case frequencyShift, roomMemory, headTracking, activity, health, weather, expandedWorlds, spatialCanvas
    // Studio: music and stem imports, spatial composition, advanced effects, automation, export
    case musicImport, stemImport, spatialComposition, advancedEffects, automation, export

    var requiredTier: Tier {
        switch self {
        case .atmosphereBed, .liveMicBasic, .motionSensing, .weather: .free   // rain plays storm songs on every tier
        // Free keeps 3D audio itself ("limited spatial controls"); arranging it is Experience.
        case .frequencyShift, .roomMemory, .headTracking, .activity, .health, .expandedWorlds, .spatialCanvas: .experience
        case .musicImport, .stemImport, .spatialComposition, .advancedEffects, .automation, .export: .studio
        }
    }
}

/// App Store product identifiers (create these as non-consumable in-app purchases
/// in App Store Connect).
enum ProductID {
    static let experience = "com.krowsingre.viksen.experience"        // $9.99
    static let studio = "com.krowsingre.viksen.studio"                // $24.99, includes Experience
    static let studioUpgrade = "com.krowsingre.viksen.studio.upgrade" // $14.99, Experience owners only
    static let all = [experience, studio, studioUpgrade]

    static func tier(for id: String) -> Tier {
        switch id {
        case studio, studioUpgrade: .studio
        case experience: .experience
        default: .free
        }
    }
}
