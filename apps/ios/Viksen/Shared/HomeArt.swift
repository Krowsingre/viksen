import SwiftUI

/// The owner's night sky behind Home (HomeSky), falling back to the app background.
struct HomeSkyBackground: View {
    var body: some View {
        GeometryReader { geo in
            Image("HomeSky").resizable().scaledToFill()
                .frame(width: geo.size.width, height: geo.size.height)
                .clipped()
                .overlay {
                    // Keeps the card and lunar line readable over the brightest swirls.
                    LinearGradient(stops: [.init(color: .clear, location: 0.55),
                                           .init(color: .black.opacity(0.45), location: 1)],
                                   startPoint: .top, endPoint: .bottom)
                }
        }
        .background(Theme.background)
        .accessibilityHidden(true)
    }
}

/// A soft glow over tonight's moon in the owner's moon ring artwork. The eight moons sit
/// clockwise from New Moon at the top; positions are measured from MoonRing (1312 × 1199).
struct MoonRingPulse: View {
    var phaseID: String
    var ringWidth: CGFloat
    var ringHeight: CGFloat
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var bright = false

    private static let centres: [String: (x: CGFloat, y: CGFloat, r: CGFloat)] = [
        "new": (660, 184, 96), "waxingCrescent": (970, 300, 96), "firstQuarter": (1080, 600, 96),
        "waxingGibbous": (962, 874, 97), "full": (660, 1000, 108), "waningGibbous": (342, 878, 97),
        "lastQuarter": (230, 596, 96), "waningCrescent": (340, 300, 96),
    ]

    var body: some View {
        if let c = Self.centres[phaseID] {
            let sx = ringWidth / 1312, sy = ringHeight / 1199
            let d = c.r * 2 * sx
            ZStack {
                Circle().fill(RadialGradient(colors: [Color.white.opacity(0.45), Color(red: 0.8, green: 0.65, blue: 1).opacity(0.35), .clear],
                                             center: .center, startRadius: 0, endRadius: d * 0.75))
                    .frame(width: d * 1.5, height: d * 1.5)
                Circle().stroke(Color(red: 0.95, green: 0.88, blue: 1).opacity(0.9), lineWidth: 1.5)
                    .frame(width: d * 1.04, height: d * 1.04)
                    .shadow(color: Color(red: 0.8, green: 0.6, blue: 1), radius: 8)
            }
            .blendMode(.plusLighter)
            .opacity(reduceMotion ? 0.6 : (bright ? 0.95 : 0.3))
            .scaleEffect(reduceMotion ? 1 : (bright ? 1.06 : 0.98))
            .position(x: c.x * sx, y: c.y * sy)
            .allowsHitTesting(false)
            .accessibilityHidden(true)
            .onAppear {
                guard !reduceMotion else { return }
                withAnimation(.easeInOut(duration: 2.2).repeatForever(autoreverses: true)) { bright = true }
            }
        }
    }
}

/// The waveform in the fox's button while a Dimension plays: seven bars that move with the
/// music's level.
struct LiveWaveform: View {
    var level: Double   // output peak 0…1
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    private static let shape: [Double] = [0.30, 0.55, 0.75, 1.0, 0.75, 0.55, 0.30]

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30, paused: reduceMotion)) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            let energy = min(1, sqrt(max(0, level)) * 1.3)
            GeometryReader { g in
                let barW = g.size.width / 13
                HStack(alignment: .center, spacing: barW) {
                    ForEach(0..<7, id: \.self) { i in
                        let wobble = reduceMotion ? 1 : 0.75 + 0.25 * sin(t * (5 + Double(i) * 1.3) + Double(i))
                        let h = g.size.height * Self.shape[i] * (0.25 + 0.75 * energy) * wobble
                        Capsule()
                            .fill(LinearGradient(colors: [.white, Color(red: 1, green: 0.78, blue: 0.92)],
                                                 startPoint: .top, endPoint: .bottom))
                            .frame(width: barW, height: max(barW, h))
                    }
                }
                .frame(width: g.size.width, height: g.size.height)
                .shadow(color: Color(red: 1, green: 0.7, blue: 0.95).opacity(0.9), radius: 4)
            }
        }
        .accessibilityHidden(true)
    }
}

/// The play triangle in the fox's button, drawn to match the art's glow.
struct PlayGlyph: View {
    var body: some View {
        Triangle()
            .fill(LinearGradient(colors: [.white, Color(red: 1, green: 0.80, blue: 0.93)], startPoint: .top, endPoint: .bottom))
            .shadow(color: Color(red: 1, green: 0.7, blue: 0.95).opacity(0.9), radius: 5)
            .accessibilityHidden(true)
    }

    private struct Triangle: Shape {
        func path(in r: CGRect) -> Path {
            var p = Path()
            let k = r.width * 0.12   // rounded corners
            p.move(to: CGPoint(x: r.minX + k * 0.3, y: r.minY + k))
            p.addQuadCurve(to: CGPoint(x: r.minX + k, y: r.minY + k * 0.35), control: CGPoint(x: r.minX, y: r.minY))
            p.addLine(to: CGPoint(x: r.maxX - k * 0.6, y: r.midY - k * 0.5))
            p.addQuadCurve(to: CGPoint(x: r.maxX - k * 0.6, y: r.midY + k * 0.5), control: CGPoint(x: r.maxX + k * 0.2, y: r.midY))
            p.addLine(to: CGPoint(x: r.minX + k, y: r.maxY - k * 0.35))
            p.addQuadCurve(to: CGPoint(x: r.minX + k * 0.3, y: r.maxY - k), control: CGPoint(x: r.minX, y: r.maxY))
            p.closeSubpath()
            return p
        }
    }
}
