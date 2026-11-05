import SwiftUI

/// The night sky behind Home, alive (owner, 2026-11-05): the playing Dimension's sky (it
/// fades between Dimensions), a very slow drift, the swirls flowing (a gentle distortion
/// shader), their light breathing, and petals drifting down. Clock-driven (20 frames a
/// second). Settings > Appearance > Animated background turns the motion off, as does
/// Reduce Motion.
struct HomeSkyBackground: View {
    static let settingKey = "animatedBackground"
    var theme: DimensionTheme = .viksen
    @AppStorage(HomeSkyBackground.settingKey) private var enabled = true
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        let animate = enabled && !reduceMotion
        GeometryReader { geo in
            TimelineView(.animation(minimumInterval: 1.0 / 20, paused: !animate)) { context in
                let t = animate ? context.date.timeIntervalSinceReferenceDate.truncatingRemainder(dividingBy: 100_000) : 0
                let drift = 0.5 - 0.5 * cos(t * 2 * .pi / 70)     // 0…1…0 over 70 s
                let breath = 0.5 - 0.5 * cos(t * 2 * .pi / 7)     // 0…1…0 over 7 s
                ZStack {
                    ZStack {
                        sky(geo.size)
                        sky(geo.size)
                            .blendMode(.screen)
                            .saturation(1.2)
                            .opacity(animate ? 0.45 * breath : 0)
                    }
                    .distortionEffect(ShaderLibrary.skyFlow(.float(Float(t)), .float(animate ? 7 : 0)),
                                      maxSampleOffset: CGSize(width: 14, height: 14))
                    .scaleEffect(1.05 + 0.05 * drift)
                    .offset(x: geo.size.width * (0.015 - 0.035 * drift), y: geo.size.height * (-0.008 + 0.02 * drift))
                    if animate { Petals(time: t, color: theme.petal, size: geo.size) }
                }
                .frame(width: geo.size.width, height: geo.size.height)
                .clipped()
            }
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

    private func sky(_ size: CGSize) -> some View {
        Image(theme.sky).resizable().scaledToFill()
            .frame(width: size.width, height: size.height)
            .id(theme.sky)
            .transition(.opacity)
    }
}

/// A few petals drifting down and across, turning as they fall (drawn, so they take the
/// Dimension's colour).
private struct Petals: View {
    let time: Double
    let color: Color
    let size: CGSize
    private static let count = 12

    var body: some View {
        Canvas { ctx, sz in
            for i in 0..<Self.count {
                let seed = Double(i) * 12.9898
                let r1 = (sin(seed) * 43758.5453).truncatingRemainder(dividingBy: 1).magnitude
                let r2 = (sin(seed * 1.7) * 24634.6345).truncatingRemainder(dividingBy: 1).magnitude
                let speed = 18 + 22 * r2                                   // points per second
                let travel = sz.height + 60
                let y = (time * speed + r1 * travel).truncatingRemainder(dividingBy: travel) - 30
                let x = r1 * sz.width + sin(time * (0.35 + r2 * 0.4) + seed) * 28 + y * 0.12
                let angle = Angle.radians(time * (0.6 + r2) + seed)
                var c = ctx
                c.translateBy(x: x, y: y)
                c.rotate(by: angle)
                let w = 7 + 5 * r2, h = 4 + 3 * r1
                c.opacity = 0.55 + 0.3 * r2
                c.fill(Path(ellipseIn: CGRect(x: -w / 2, y: -h / 2, width: w, height: h)),
                       with: .color(color))
            }
        }
        .allowsHitTesting(false)
    }
}

/// A glow behind tonight's moon in the owner's moon ring artwork (drawn under the ring,
/// so it seems to come from behind the moon). The eight moons sit clockwise from New Moon
/// at the top; positions are measured from MoonRing (1312 × 1199).
struct MoonRingPulse: View {
    var phaseID: String
    var glow: Color = DimensionTheme.viksen.glow
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
            Circle()
                .fill(RadialGradient(stops: [.init(color: Color(red: 0.98, green: 0.92, blue: 1).opacity(0.95), location: 0.45),
                                            .init(color: glow.opacity(0.55), location: 0.62),
                                            .init(color: glow.opacity(0.18), location: 0.8),
                                            .init(color: .clear, location: 1)],
                                     center: .center, startRadius: 0, endRadius: d * 0.95))
                .frame(width: d * 1.9, height: d * 1.9)
                .opacity(reduceMotion ? 0.8 : (bright ? 1 : 0.35))
                .scaleEffect(reduceMotion ? 1 : (bright ? 1.08 : 0.94))
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
