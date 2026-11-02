import SwiftUI

/// Layered, slowly drifting sine ribbons with a soft glow.
struct LightWave: View {
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        // With Reduce Motion the wave is drawn once and stays still.
        TimelineView(.animation(minimumInterval: 1.0 / 30.0, paused: reduceMotion)) { context in
            let t = context.date.timeIntervalSinceReferenceDate
            Canvas { ctx, size in
                for layer in 0..<4 {
                    var path = Path()
                    let phase = t * (0.25 + Double(layer) * 0.07) + Double(layer) * 1.3
                    let amp = size.height * (0.16 + 0.05 * Double(layer))
                    let mid = size.height * (0.45 + 0.06 * Double(layer))
                    for step in 0...120 {
                        let x = size.width * Double(step) / 120.0
                        let u = Double(step) / 120.0
                        let y = mid + amp * sin(u * 2.4 * .pi + phase) * (0.6 + 0.4 * sin(u * .pi))
                        if step == 0 { path.move(to: CGPoint(x: x, y: y)) } else { path.addLine(to: CGPoint(x: x, y: y)) }
                    }
                    let alpha = 0.55 - Double(layer) * 0.11
                    ctx.stroke(path, with: .color(Theme.accentGlow.opacity(alpha)), lineWidth: layer == 0 ? 2.5 : 1.2)
                }
            }
            .blur(radius: 1.2)
            .background(
                RadialGradient(colors: [Theme.accentGlow.opacity(0.28), .clear],
                               center: .center, startRadius: 10, endRadius: 260)
            )
        }
    }
}
