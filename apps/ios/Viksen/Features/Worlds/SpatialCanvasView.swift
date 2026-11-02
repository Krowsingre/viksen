import SwiftUI

/// Top-down view of the listener (centre, nose pointing up) and the Dimension's
/// instruments. Drag an instrument to move it around you; the distance from the
/// centre is its distance in metres. Positions are what the 3D renderer uses.
struct SpatialCanvasView: View {
    @Environment(EngineController.self) private var engine
    /// Outer ring radius in metres.
    private let maxDistance: Float = 4.0
    @State private var dragging: Int?

    var body: some View {
        let stems = engine.worldStatus.stems
        GeometryReader { geo in
            let size = min(geo.size.width, geo.size.height)
            let centre = CGPoint(x: geo.size.width / 2, y: size / 2)
            let radius = size / 2 - 22
            ZStack {
                ForEach([1.0, 0.5], id: \.self) { f in
                    Circle()
                        .stroke(Color.secondary.opacity(0.25), lineWidth: 1)
                        .frame(width: radius * 2 * f, height: radius * 2 * f)
                        .position(centre)
                }
                Text("front").font(.caption2).foregroundStyle(.secondary)
                    .position(x: centre.x, y: centre.y - radius - 12)
                // Listener: rotates with head tracking.
                Image(systemName: "location.north.fill")
                    .font(.title3)
                    .foregroundStyle(Theme.accent)
                    .rotationEffect(.degrees(engine.headTracker.isRunning ? engine.headTracker.yawDegrees : 0))
                    .position(centre)
                    .accessibilityLabel("You")
                ForEach(Array(stems.enumerated()), id: \.offset) { index, stem in
                    let p = point(azimuth: stem.azimuth, distance: stem.distance, centre: centre, radius: radius)
                    VStack(spacing: 2) {
                        Circle()
                            .fill(Theme.music.opacity(0.35 + 0.65 * Double(min(1, stem.peak * 3))))
                            .overlay(Circle().stroke(dragging == index ? Theme.accent : Color.white.opacity(0.6), lineWidth: 1.5))
                            .frame(width: 26, height: 26)
                        Text(stem.title).font(.caption2).lineLimit(1).fixedSize()
                    }
                    .position(p)
                    .gesture(
                        DragGesture(minimumDistance: 0, coordinateSpace: .named(Self.space))
                            .onChanged { g in
                                dragging = index
                                let (az, dist) = polar(g.location, centre: centre, radius: radius)
                                engine.moveStem(index, azimuth: az, distance: dist)
                            }
                            .onEnded { _ in dragging = nil }
                    )
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel(stem.title)
                    .accessibilityValue(String(format: "%.0f degrees, %.1f metres", stem.azimuth, stem.distance))
                    .accessibilityAdjustableAction { direction in
                        let step: Float = direction == .increment ? 15 : -15
                        engine.moveStem(index, azimuth: stem.azimuth + step, distance: stem.distance)
                    }
                }
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .coordinateSpace(name: Self.space)   // drag locations relative to the canvas, not the dot
        }
        .frame(height: 300)
    }

    private static let space = "spatialCanvas"

    private func point(azimuth: Float, distance: Float, centre: CGPoint, radius: CGFloat) -> CGPoint {
        let r = CGFloat(min(distance, maxDistance) / maxDistance) * radius
        let a = CGFloat(azimuth) * .pi / 180
        return CGPoint(x: centre.x + sin(a) * r, y: centre.y - cos(a) * r)
    }

    private func polar(_ p: CGPoint, centre: CGPoint, radius: CGFloat) -> (Float, Float) {
        let dx = p.x - centre.x, dy = centre.y - p.y
        let az = Float(atan2(dx, dy) * 180 / .pi)
        let dist = Float(min(1, max(0.08, hypot(dx, dy) / radius))) * maxDistance
        return (az, dist)
    }
}
