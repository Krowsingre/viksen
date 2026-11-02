import SwiftUI

/// The Home scene (owner's concept art), drawn in code so it is sharp at any size:
/// night sky and violet nebula, a glowing eclipse ring with a starburst, misty
/// mountains, optional crystal pillars (wide layouts), and a still lake reflecting it.
struct DimensionArt: View {
    var pillars = false
    /// Where the horizon sits, as a fraction of the height.
    var horizon: CGFloat = 0.60
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 20.0, paused: reduceMotion)) { context in
            let t = reduceMotion ? 0 : context.date.timeIntervalSinceReferenceDate
            Canvas { ctx, size in
                draw(&ctx, size, t)
            }
        }
        .accessibilityHidden(true)
    }

    private func draw(_ ctx: inout GraphicsContext, _ size: CGSize, _ t: Double) {
        let w = size.width, h = size.height
        let horizonY = h * horizon
        let violet = Color(red: 0.55, green: 0.40, blue: 0.98)
        let glow = Color(red: 0.75, green: 0.60, blue: 1.0)
        let warm = Color(red: 1.0, green: 0.78, blue: 0.62)

        // Sky.
        ctx.fill(Path(CGRect(origin: .zero, size: size)), with: .linearGradient(
            Gradient(colors: [Color(red: 0.03, green: 0.02, blue: 0.08), Color(red: 0.09, green: 0.05, blue: 0.20),
                              Color(red: 0.05, green: 0.03, blue: 0.11)]),
            startPoint: .zero, endPoint: CGPoint(x: 0, y: horizonY)))
        ctx.fill(Path(CGRect(x: 0, y: horizonY, width: w, height: h - horizonY)), with: .color(Color(red: 0.02, green: 0.015, blue: 0.05)))

        // Stars (fixed field, gently twinkling).
        var rng = SeededRandom(seed: 7)
        for _ in 0..<110 {
            let x = rng.next() * w, y = rng.next() * horizonY * 0.95
            let r = 0.4 + rng.next() * 1.1
            let a = 0.25 + 0.55 * (0.5 + 0.5 * sin(t * (0.6 + rng.next()) + rng.next() * 6))
            ctx.fill(Path(ellipseIn: CGRect(x: x, y: y, width: r, height: r)), with: .color(.white.opacity(a)))
        }

        // Nebula clouds.
        ctx.drawLayer { layer in
            layer.addFilter(.blur(radius: min(w, h) * 0.08))
            for (cx, cy, rx, ry, a) in [(0.15, 0.52, 0.35, 0.10, 0.55), (0.85, 0.50, 0.38, 0.11, 0.55),
                                         (0.50, 0.58, 0.55, 0.07, 0.45), (0.30, 0.35, 0.25, 0.08, 0.25)] {
                let drift = sin(t * 0.05 + cx * 7) * 0.02
                let rect = CGRect(x: (cx + drift - rx) * w, y: (cy - ry) * h * (horizon / 0.6), width: rx * 2 * w, height: ry * 2 * h)
                layer.fill(Path(ellipseIn: rect), with: .color(violet.opacity(a)))
            }
        }

        // Eclipse ring: a large circle whose lower part sinks behind the mountains.
        let ringR = min(w * 0.46, h * 0.36)
        let centre = CGPoint(x: w / 2, y: horizonY - ringR * 0.55)
        let ring = Path(ellipseIn: CGRect(x: centre.x - ringR, y: centre.y - ringR, width: ringR * 2, height: ringR * 2))
        ctx.drawLayer { layer in
            layer.addFilter(.blur(radius: 18))
            layer.stroke(ring, with: .color(glow.opacity(0.55)), lineWidth: 10)
        }
        ctx.stroke(ring, with: .linearGradient(Gradient(colors: [warm, glow, violet.opacity(0.6)]),
                                               startPoint: CGPoint(x: centre.x - ringR, y: centre.y - ringR),
                                               endPoint: CGPoint(x: centre.x + ringR, y: centre.y + ringR)),
                   lineWidth: 2.2)

        // Starburst at the top of the ring.
        let top = CGPoint(x: centre.x, y: centre.y - ringR)
        let pulse = 0.85 + 0.15 * sin(t * 0.8)
        ctx.drawLayer { layer in
            layer.addFilter(.blur(radius: 6))
            layer.fill(Path(ellipseIn: CGRect(x: top.x - 14, y: top.y - 14, width: 28, height: 28)), with: .color(.white.opacity(0.9 * pulse)))
        }
        for (dx, dy, len) in [(0.0, 1.0, 0.45), (1.0, 0.0, 0.18), (0.0, -1.0, 0.20), (-1.0, 0.0, 0.18)] {
            var p = Path()
            p.move(to: CGPoint(x: top.x - dx * 1.2, y: top.y - dy * 1.2))
            p.addLine(to: CGPoint(x: top.x + dx * ringR * len, y: top.y + dy * ringR * len))
            ctx.stroke(p, with: .linearGradient(Gradient(colors: [.white.opacity(0.9 * pulse), .clear]),
                                                startPoint: top, endPoint: CGPoint(x: top.x + dx * ringR * len, y: top.y + dy * ringR * len)),
                       lineWidth: 1.3)
        }

        // Crystal pillars (wide layouts).
        if pillars {
            var prng = SeededRandom(seed: 3)
            for i in 0..<22 {
                let u = Double(i) / 21.0
                let x = w * (0.22 + 0.56 * u) + (prng.next() - 0.5) * 12
                let ph = h * (0.10 + 0.16 * (1 - abs(u - 0.5) * 1.6) * (0.6 + 0.4 * prng.next()))
                let pw = 10 + prng.next() * 14
                let rect = CGRect(x: x - pw / 2, y: horizonY - ph, width: pw, height: ph)
                ctx.fill(Path(roundedRect: rect, cornerRadius: pw / 2.5), with: .linearGradient(
                    Gradient(colors: [glow.opacity(0.75), violet.opacity(0.25)]),
                    startPoint: CGPoint(x: x, y: rect.minY), endPoint: CGPoint(x: x, y: rect.maxY)))
            }
        }

        // Mountains (two ranges) with mist.
        for (range, shade, seed) in [(0.16, 0.10, 11), (0.09, 0.06, 23)] {
            var m = Path()
            var mr = SeededRandom(seed: UInt64(seed))
            m.move(to: CGPoint(x: 0, y: horizonY))
            var x = 0.0
            while x < w {
                let peak = horizonY - h * range * (0.3 + 0.7 * mr.next()) * (abs(x / w - 0.5) * 1.4 + 0.25)
                m.addLine(to: CGPoint(x: x + w * 0.035, y: peak))
                x += w * (0.05 + 0.05 * mr.next())
                m.addLine(to: CGPoint(x: x, y: horizonY - h * range * 0.25 * mr.next()))
            }
            m.addLine(to: CGPoint(x: w, y: horizonY))
            m.closeSubpath()
            ctx.fill(m, with: .linearGradient(Gradient(colors: [Color(red: shade * 1.6, green: shade, blue: shade * 2.6), Color.black.opacity(0.9)]),
                                              startPoint: CGPoint(x: 0, y: horizonY - h * range), endPoint: CGPoint(x: 0, y: horizonY)))
        }
        ctx.drawLayer { layer in
            layer.addFilter(.blur(radius: 14))
            layer.fill(Path(CGRect(x: 0, y: horizonY - h * 0.03, width: w, height: h * 0.05)), with: .color(violet.opacity(0.35)))
        }

        // Lake: reflected glow column and ripples.
        ctx.drawLayer { layer in
            layer.addFilter(.blur(radius: 10))
            layer.fill(Path(CGRect(x: w / 2 - w * 0.08, y: horizonY, width: w * 0.16, height: h - horizonY)),
                       with: .linearGradient(Gradient(colors: [glow.opacity(0.45), .clear]),
                                             startPoint: CGPoint(x: 0, y: horizonY), endPoint: CGPoint(x: 0, y: h * 0.98)))
        }
        for i in 0..<14 {
            let y = horizonY + (h - horizonY) * (0.05 + Double(i) * 0.06)
            let half = w * (0.03 + 0.012 * Double(i)) * (0.8 + 0.2 * sin(t * 0.7 + Double(i)))
            var r = Path()
            r.move(to: CGPoint(x: w / 2 - half, y: y))
            r.addLine(to: CGPoint(x: w / 2 + half, y: y))
            ctx.stroke(r, with: .color(warm.opacity(0.35 * (1 - Double(i) / 14))), lineWidth: 1)
        }

        // Light ribbons along the bottom.
        for layerIndex in 0..<3 {
            var p = Path()
            let base = h * (0.92 + 0.025 * Double(layerIndex))
            for s in 0...80 {
                let u = Double(s) / 80
                let y = base + h * 0.02 * sin(u * 5 + t * (0.2 + 0.05 * Double(layerIndex)) + Double(layerIndex))
                if s == 0 { p.move(to: CGPoint(x: 0, y: y)) } else { p.addLine(to: CGPoint(x: w * u, y: y)) }
            }
            ctx.stroke(p, with: .color(violet.opacity(0.35 - 0.08 * Double(layerIndex))), lineWidth: 1)
        }
    }
}

/// Small deterministic generator so the scenery is the same on every draw.
struct SeededRandom {
    private var state: UInt64
    init(seed: UInt64) { state = seed &* 0x9E3779B97F4A7C15 | 1 }
    mutating func next() -> Double {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17
        return Double(state % 1_000_000) / 1_000_000
    }
}
