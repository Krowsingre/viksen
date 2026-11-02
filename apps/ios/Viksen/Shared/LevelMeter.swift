import SwiftUI

/// Horizontal peak meter, dB-scaled (-60 dBFS .. 0 dBFS).
struct LevelMeter: View {
    var peak: Float
    var color: Color

    private var fraction: CGFloat {
        guard peak > 0 else { return 0 }
        let db = 20 * log10(Double(peak))
        return CGFloat(min(1, max(0, (db + 60) / 60)))
    }

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                Capsule().fill(Color.secondary.opacity(0.18))
                Capsule().fill(color.opacity(0.85)).frame(width: geo.size.width * fraction)
            }
        }
        .frame(height: 4)
        .accessibilityHidden(true)
    }
}

/// Labelled level slider used for every bus. Shows the live bus peak beneath.
struct BusSlider: View {
    var title: String
    var color: Color
    @Binding var value: Float
    var peak: Float
    var range: ClosedRange<Float> = 0...1

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Circle().fill(color).frame(width: 8, height: 8)
                Text(title).font(.subheadline)
                Spacer()
                Text(value <= 0 ? "off" : String(format: "%.0f dB", 20 * log10(Double(value))))
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
            }
            Slider(value: $value, in: range)
                .tint(color)
                .accessibilityLabel(title)
            LevelMeter(peak: peak, color: color)
        }
    }
}

/// Honest status for workspaces whose functionality has not been built yet.
/// Intentionally contains no interactive controls (spec §12: no fake controls).
struct NotYetAvailableView: View {
    var title: String
    var systemImage: String
    var phase: String
    var summary: String
    var planned: [String]

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Label(phase, systemImage: "hammer").font(.caption).foregroundStyle(.secondary)
                        Text(summary).font(.body)
                    }
                    .padding(.vertical, 4)
                }
                Section("Planned") {
                    ForEach(planned, id: \.self) { item in
                        Text(item).foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle(title)
        }
    }
}
