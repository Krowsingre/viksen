import SwiftUI

/// The Moon Chart (P-3, approved layout 2026-11-04): the eight lunar phases of one
/// Dimension in a ring. Discovered phases glow; tonight's phase is ringed and shows how
/// much of its 3 minutes has been heard. Progress is per Dimension. Global Secrets show
/// a count only, never a total.
struct MoonChartView: View {
    var initialDimensionID: String?
    @Environment(EngineController.self) private var engine
    @Environment(\.dismiss) private var dismiss
    @State private var dimensionID: String?
    @State private var selected: String?

    private var progress: ProgressStore { engine.progress }
    private var dimensions: [AtmosWorldInfo] { engine.worlds.filter { $0.worldID != "studio" } }

    var body: some View {
        let dim = dimensionID ?? initialDimensionID ?? engine.lastDimensionID ?? dimensions.first?.worldID ?? ""
        let found = progress.discoveryDates(dim)
        let tonight = progress.currentPhaseID
        let phase = selected ?? tonight
        NavigationStack {
            ScrollView {
                VStack(spacing: 16) {
                    dimensionPicker(dim)
                    ring(found: found, tonight: tonight, selected: phase, dim: dim)
                        .frame(height: 344)
                    VStack(spacing: 12) {
                        if let done = progress.journeyCompletedAt(dim) {
                            completeCard(done)
                        }
                        detailCard(phase: phase, found: found, tonight: tonight, dim: dim)
                        HStack(spacing: 12) {
                            journeyCard(found: found)
                            secretsCard(dim: dim)
                        }
                        if progress.globalSecretsFound > 0 { globalCard }
                    }
                    .padding(.horizontal, 20)
                }
                .padding(.bottom, 24)
            }
            .background(Theme.background.ignoresSafeArea())
            .navigationTitle("Moon Chart")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button { dismiss() } label: { Image(systemName: "xmark") }
                        .accessibilityLabel("Close")
                }
            }
        }
        .preferredColorScheme(.dark)
    }

    // MARK: Parts

    private func dimensionPicker(_ current: String) -> some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                ForEach(dimensions, id: \.worldID) { d in
                    let on = d.worldID == current
                    Button(d.name) { dimensionID = d.worldID; selected = nil }
                        .font(.subheadline.weight(on ? .semibold : .regular))
                        .padding(.horizontal, 14).frame(minHeight: 36)
                        .foregroundStyle(on ? .white : Color.white.opacity(0.8))
                        .background(on ? Theme.accent : Color.white.opacity(0.06), in: Capsule())
                        .overlay(Capsule().stroke(Theme.accent.opacity(on ? 1 : 0.35), lineWidth: 1))
                        .accessibilityAddTraits(on ? .isSelected : [])
                }
            }
            .padding(.horizontal, 20)
        }
    }

    private func ring(found: [String: Date], tonight: String, selected: String, dim: String) -> some View {
        GeometryReader { geo in
            let c = CGPoint(x: geo.size.width / 2, y: geo.size.height / 2)
            ZStack {
                if progress.journeyCompletedAt(dim) != nil {
                    Circle().stroke(Color(red: 0.75, green: 0.67, blue: 1).opacity(0.3), lineWidth: 1)
                        .frame(width: 256, height: 256).position(c)
                }
                ForEach(Array(Lunar.order.enumerated()), id: \.offset) { i, id in
                    let a = (-90.0 + Double(i) * 45.0) * .pi / 180
                    Button { self.selected = id } label: {
                        MoonDisc(fraction: Double(i) / 8, lit: found[id] != nil)
                            .frame(width: 48, height: 48)
                            .frame(width: 56, height: 56)
                            .overlay(Circle().stroke(Theme.accent.opacity(id == selected ? 1 : (id == tonight ? 0.55 : 0)),
                                                     lineWidth: id == selected ? 2 : 1))
                    }
                    .buttonStyle(.plain)
                    .position(x: c.x + 128 * cos(a), y: c.y + 128 * sin(a))
                    .accessibilityLabel("\(Lunar.phaseName(id)), \(found[id] != nil ? "discovered" : "not yet discovered")\(id == tonight ? ", tonight" : "")")
                }
                centre(found: found, tonight: tonight, dim: dim).frame(width: 200).position(c)
            }
        }
    }

    @ViewBuilder
    private func centre(found: [String: Date], tonight: String, dim: String) -> some View {
        if let done = progress.journeyCompletedAt(dim) {
            VStack(spacing: 4) {
                Text("LUNAR JOURNEY").font(.caption).tracking(1.6).foregroundStyle(Theme.gold)
                Text("Complete").font(.title2.weight(.semibold))
                Text("All eight phases, \(done.formatted(date: .abbreviated, time: .omitted))")
                    .font(.footnote).foregroundStyle(Theme.secondaryText)
            }
            .multilineTextAlignment(.center)
        } else {
            VStack(spacing: 4) {
                Text("TONIGHT").font(.caption).tracking(1.6).foregroundStyle(Theme.secondaryText)
                Text(Lunar.phaseName(tonight)).font(.title3.weight(.semibold))
                if let w = progress.nextWindow(tonight) {
                    Text("until \(w.end.formatted(.dateTime.month(.abbreviated).day()))")
                        .font(.footnote).foregroundStyle(Theme.secondaryText)
                }
                if found[tonight] == nil {
                    TimelineView(.periodic(from: .now, by: 1)) { _ in
                        let p = progress.phaseProgress(dim)
                        VStack(spacing: 4) {
                            ProgressView(value: p).tint(Theme.accent).frame(width: 150)
                            Text("\(Self.clock(p * 180)) of 3:00 heard").font(.footnote).foregroundStyle(.white.opacity(0.8))
                        }
                        .padding(.top, 8)
                    }
                } else {
                    Text("Discovered in this Dimension").font(.footnote).foregroundStyle(.white.opacity(0.8)).padding(.top, 8)
                }
            }
            .multilineTextAlignment(.center)
        }
    }

    private func detailCard(phase: String, found: [String: Date], tonight: String, dim: String) -> some View {
        let name = Lunar.phaseName(phase)
        let tag: String, tagColor: Color, line1: String, line2: String
        if let at = found[phase] {
            tag = "Discovered \(at.formatted(.dateTime.month(.abbreviated).day()))"
            tagColor = Color(red: 0.72, green: 0.65, blue: 1)
            line1 = "Heard in \(dimensions.first { $0.worldID == dim }?.name ?? "this Dimension")."
            line2 = "Its sound returns here with every \(name)."
        } else if phase == tonight {
            let left = max(0, Int(180 - progress.phaseProgress(dim) * 180))
            tag = "Tonight"; tagColor = Theme.gold
            line1 = left < 180 ? "Keep listening to this Dimension: \(left / 60) min \(left % 60) s to go."
                               : "Listen to this Dimension for 3 minutes while it lasts to discover it."
            line2 = "The minutes do not need to be continuous."
        } else {
            tag = "Not yet discovered"; tagColor = Theme.secondaryText
            line1 = "Listen to this Dimension for 3 minutes during the next \(name)."
            if let w = progress.nextWindow(phase) {
                let f = Date.FormatStyle.dateTime.month(.abbreviated).day()
                line2 = "Next: \(w.start.formatted(f)) – \(w.end.formatted(f))"
            } else { line2 = "" }
        }
        return VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text(name).font(.headline)
                Spacer()
                Text(tag).font(.footnote).foregroundStyle(tagColor)
            }
            Text(line1).font(.subheadline).foregroundStyle(.white.opacity(0.85))
            if !line2.isEmpty { Text(line2).font(.footnote).foregroundStyle(Theme.secondaryText) }
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 20))
    }

    private func completeCard(_ at: Date) -> some View {
        HStack(spacing: 14) {
            Image(systemName: "moon.stars.fill").font(.title2).foregroundStyle(Theme.gold)
                .frame(width: 52, height: 52).overlay(Circle().stroke(Theme.gold, lineWidth: 1.5))
            VStack(alignment: .leading, spacing: 2) {
                Text("Lunar Journey complete").font(.headline)
                Text("Every phase heard in this Dimension").font(.footnote).foregroundStyle(Theme.secondaryText)
            }
            Spacer()
        }
        .padding(16)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 20))
    }

    private func journeyCard(found: [String: Date]) -> some View {
        statCard(title: "LUNAR JOURNEY", value: "\(found.count)", suffix: "of 8 phases") {
            HStack(spacing: 5) {
                ForEach(Lunar.order, id: \.self) { id in
                    Circle().fill(found[id] != nil ? Color(red: 0.93, green: 0.91, blue: 1) : .clear)
                        .overlay(Circle().stroke(Theme.accent.opacity(0.6), lineWidth: 1))
                        .frame(width: 10, height: 10)
                }
            }
        }
    }

    private func secretsCard(dim: String) -> some View {
        let n = progress.secretsFound(dim)
        return statCard(title: "SECRETS", value: "\(n)", suffix: "of 5 found") {
            HStack(spacing: 6) {
                ForEach(0..<5, id: \.self) { k in
                    Rectangle().fill(k < n ? Theme.gold : .clear)
                        .overlay(Rectangle().stroke(Theme.accent.opacity(0.6), lineWidth: 1))
                        .frame(width: 9, height: 9).rotationEffect(.degrees(45))
                }
            }
        }
    }

    private var globalCard: some View {
        statCard(title: "GLOBAL SECRETS", value: "\(progress.globalSecretsFound)", suffix: "found") {
            Text("Across every Dimension").font(.caption).foregroundStyle(Theme.secondaryText)
        }
    }

    private func statCard<Extra: View>(title: String, value: String, suffix: String,
                                       @ViewBuilder extra: () -> Extra) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.caption).tracking(1.2).foregroundStyle(Theme.secondaryText)
            (Text(value).font(.title2.weight(.semibold)) + Text(" \(suffix)").font(.subheadline).foregroundColor(Theme.secondaryText))
            extra()
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 20))
        .accessibilityElement(children: .combine)
    }

    static func clock(_ seconds: Double) -> String {
        let s = Int(seconds)
        return "\(s / 60):" + String(format: "%02d", s % 60)
    }
}

/// One moon in the chart: the phase's lit shape over a dark disc. Undiscovered phases
/// are drawn dim.
struct MoonDisc: View {
    var fraction: Double   // 0 new … 0.5 full … 1
    var lit: Bool

    var body: some View {
        ZStack {
            Circle().fill(Color(red: 0.10, green: 0.09, blue: 0.19))
                .overlay(Circle().stroke(lit ? Color(red: 0.93, green: 0.91, blue: 1).opacity(0.35)
                                             : Color(red: 0.18, green: 0.16, blue: 0.31), lineWidth: 1))
            MoonPhaseShape(fraction: fraction)
                .fill(lit ? Color(red: 0.93, green: 0.91, blue: 1) : Color(red: 0.23, green: 0.20, blue: 0.38))
        }
        .shadow(color: lit ? Color(red: 0.75, green: 0.67, blue: 1).opacity(0.55) : .clear, radius: 6)
        .accessibilityHidden(true)
    }
}

struct MoonPhaseShape: Shape {
    var fraction: Double

    func path(in rect: CGRect) -> Path {
        var p = Path()
        let r = min(rect.width, rect.height) / 2
        let c = CGPoint(x: rect.midX, y: rect.midY)
        let f = fraction.truncatingRemainder(dividingBy: 1)
        if f == 0 { return p }
        if f == 0.5 { p.addEllipse(in: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r)); return p }
        // The lit limb is a half circle (right while waxing, left while waning); the
        // terminator is half an ellipse whose width follows the phase.
        let waxing = f < 0.5
        let rx = r * abs(cos(2 * .pi * f))
        let bulgesWithLimb = waxing ? f < 0.25 : f > 0.75   // crescent: terminator on the lit side
        p.move(to: CGPoint(x: c.x, y: c.y - r))
        p.addArc(center: c, radius: r, startAngle: .degrees(-90), endAngle: .degrees(90), clockwise: !waxing)
        let side: CGFloat = (waxing == bulgesWithLimb) ? 1 : -1   // +1: terminator on the right
        let steps = 24
        for k in 1...steps {
            let t = Double.pi / 2 - Double(k) / Double(steps) * .pi   // from bottom (+90°) to top (−90°)
            p.addLine(to: CGPoint(x: c.x + side * rx * cos(t), y: c.y + r * sin(t)))
        }
        p.closeSubpath()
        return p
    }
}
