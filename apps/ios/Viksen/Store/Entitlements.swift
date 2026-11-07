import Foundation

/// The purchase model (owner, 2026-11-07):
/// - Free: the Viksen Dimension (and every sense and feature of the app).
/// - Each other Dimension is a one-time purchase, kept forever: standard $4.99, special $6.99
///   (prices are set in App Store Connect; the app shows the App Store's price).
/// - "Viksen All Dimensions" subscription (monthly or yearly, one-week free trial) unlocks
///   every Dimension while it is active. Dimensions bought stay yours either way.
/// No Family Sharing for now. Everything is tied to the listener's Apple Account.
enum ProductID {
    static let prefix = "com.krowsingre.viksen"
    /// One product per Dimension, e.g. com.krowsingre.viksen.dimension.the-shatter-gardens
    static func dimension(_ dimensionID: String) -> String { "\(prefix).dimension.\(dimensionID)" }
    static func dimensionID(for productID: String) -> String? {
        let p = "\(prefix).dimension."
        return productID.hasPrefix(p) ? String(productID.dropFirst(p.count)) : nil
    }
    static let allMonthly = "\(prefix).all.monthly"   // $2.99 a month
    static let allYearly = "\(prefix).all.yearly"     // $24.99 a year
    static let subscriptions = [allMonthly, allYearly]
    /// The App Store Connect subscription group's name (for reference).
    static let subscriptionGroup = "Viksen All Dimensions"
}

/// How a Dimension is sold, from its world.json "tier": "free", "standard" or "special".
enum Edition: String {
    case free, standard, special
    init(tier: String) { self = Edition(rawValue: tier) ?? (tier == "free" ? .free : .standard) }
}
