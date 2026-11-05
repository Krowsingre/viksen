import SwiftUI

/// Each Dimension's look on Home (owner, 2026-11-05): its sky, and the colour the fox, its
/// button and the moon ring take (a hue shift of the shared purple art), the glow behind
/// tonight's moon and the drifting petals. Follows the Dimension that is playing; Viksen's
/// purple when nothing plays.
struct DimensionTheme: Equatable {
    let sky: String
    let hueShift: Double          // degrees, applied to the fox, its button and the moon ring
    let glow: Color
    let petal: Color
    /// The Home card's see-through tint and edge (owner, 2026-11-05: each card takes its
    /// own Dimension's badge colour; it stays transparent).
    let card: Color
    let edge: Color

    static let viksen = DimensionTheme(sky: "HomeSky", hueShift: 0,
                                       glow: Color(red: 0.78, green: 0.58, blue: 1),
                                       petal: Color(red: 0.93, green: 0.72, blue: 0.95),
                                       card: Color(red: 0.12, green: 0.07, blue: 0.20),
                                       edge: Color(red: 0.90, green: 0.86, blue: 1))

    static func `for`(_ dimensionID: String?) -> DimensionTheme {
        switch dimensionID {
        case "light-of-a-distant-sun":   // The Depths Have Spoken: deep blue and teal
            return DimensionTheme(sky: "Sky-light-of-a-distant-sun", hueShift: -75,
                                  glow: Color(red: 0.40, green: 0.85, blue: 1),
                                  petal: Color(red: 0.72, green: 0.84, blue: 1),
                                  card: Color(red: 0.03, green: 0.11, blue: 0.22),
                                  edge: Color(red: 0.55, green: 0.86, blue: 1))
        case "steel-steam-and-borrowed-dreams":   // The Shatter Gardens: crimson and smoke
            return DimensionTheme(sky: "Sky-steel-steam-and-borrowed-dreams", hueShift: 80,
                                  glow: Color(red: 1, green: 0.32, blue: 0.42),
                                  petal: Color(red: 1, green: 0.38, blue: 0.46),
                                  card: Color(red: 0.22, green: 0.04, blue: 0.08),
                                  edge: Color(red: 1, green: 0.55, blue: 0.60))
        default:
            return .viksen
        }
    }
}
