import SwiftUI

/// The Moon Chart (P-3), laid out like Settings and Dimensions (owner, 2026-11-05):
/// a standard page with the ring of moons at the top as its picture, then the Dimension,
/// the eight phases and the journey as grouped rows on a plain dark background. Tapping a
/// moon or a phase opens that phase's page. Progress is per Dimension; Global Secrets show
/// a count only, never a total.
struct MoonChartView: View {
    var initialDimensionID: String?
    var onClose: (() -> Void)? = nil
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @Environment(\.dismiss) private var dismiss
    @State private var dimensionID: String?
    @State private var path: [String] = []
    @Environment(\.colorScheme) private var scheme

    static let lavender = Color(red: 0.90, green: 0.86, blue: 1)
    static let violet = Color(red: 0.62, green: 0.48, blue: 1)
    private var progress: ProgressStore { engine.progress }

    /// Only the Dimensions this listener has access to.
    private var dimensions: [AtmosWorldInfo] {
        engine.worlds.filter { w in
            return store.hasAccess(w)
        }
    }

    var body: some View {
        let ids = Set(dimensions.map(\.worldID))
        let dim = [dimensionID, initialDimensionID, engine.lastDimensionID].compactMap { $0 }.first { ids.contains($0) }
            ?? dimensions.first?.worldID ?? ""
        let found = progress.discoveryDates(dim)
        let tonight = progress.currentPhaseID
        NavigationStack(path: $path) {
            List {
                Section {
                    // Square, as wide as the list: the whole ring always fits.
                    ring(found: found, tonight: tonight, dim: dim)
                        .aspectRatio(1, contentMode: .fit)
                        .frame(maxWidth: 440)
                        .frame(maxWidth: .infinity)
                        .listRowBackground(Color.clear)
                        .listRowInsets(EdgeInsets())
                }
                Section {
                    if dimensions.count > 1 {
                        Picker("Dimension", selection: Binding(get: { dim }, set: { dimensionID = $0 })) {
                            ForEach(dimensions, id: \.worldID) { d in Text(d.name).tag(d.worldID) }
                        }
                        .pickerStyle(.menu)
                    } else {
                        LabeledContent("Dimension", value: dimensions.first?.name ?? "")
                    }
                }
                Section {
                    ForEach(Lunar.order, id: \.self) { id in
                        NavigationLink(value: id) { PhaseRow(phase: id, found: found, tonight: tonight, dim: dim) }
                    }
                } header: { Text("Phases") } footer: {
                    Text("Listen to a Dimension for 3 minutes after dark during a phase to discover it. The minutes do not need to be continuous.")
                }
                Section {
                    if let done = progress.journeyCompletedAt(dim) {
                        Label("Complete, \(done.formatted(date: .abbreviated, time: .omitted))", systemImage: "moon.stars.fill")
                            .foregroundStyle(Theme.gold)
                    }
                    LabeledContent("Lunar Journey", value: "\(found.count) of 8 phases")
                    LabeledContent("Secrets", value: "\(progress.secretsFound(dim)) of 11 found")
                    if progress.globalSecretsFound > 0 {
                        LabeledContent("Global Secrets", value: "\(progress.globalSecretsFound) found")
                    }
                } header: { Text("Journey") } footer: {
                    if progress.globalSecretsFound > 0 { Text("Global Secrets are found across every Dimension.") }
                }
                // Badges for the moon-event secrets found in this Dimension (owner, 2026-11-11);
                // the ones still hidden are not shown.
                let secrets = progress.secretDates(dim).sorted { $0.value < $1.value }
                if !secrets.isEmpty {
                    Section {
                        ForEach(secrets, id: \.key) { entry in
                            HStack(spacing: 14) {
                                SecretBadge(eventID: entry.key)
                                Text(ProgressStore.secretTitle(entry.key))
                                Spacer()
                                Text(entry.value.formatted(date: .abbreviated, time: .omitted)).font(.subheadline).foregroundStyle(.secondary)
                            }
                        }
                    } header: { Text("Secrets found") } footer: {
                        Text("Rare moments in the sky, found by listening while they happen where you are.")
                    }
                }
            }
            .navigationTitle("Moon Chart")
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    CloseButton(compact: true) { if let onClose { onClose() } else { dismiss() } }
                }
            }
            .navigationDestination(for: String.self) { id in
                PhaseDetailView(phase: id, dimensionID: dim,
                                dimensionName: dimensions.first { $0.worldID == dim }?.name ?? "this Dimension")
            }
        }
    }

    // Colours that read in both light and dark (the page follows the Appearance setting).
    private var dark: Bool { scheme == .dark }
    /// Lines, sparkles and quiet text: lavender on dark, a deeper violet on light.
    private var soft: Color { dark ? Self.lavender : Color(red: 0.40, green: 0.30, blue: 0.72) }
    /// The glow of discovered moons and tonight's ring.
    private var glow: Color { dark ? Self.lavender : Self.violet }

    // MARK: Ring

    private func ring(found: [String: Date], tonight: String, dim: String) -> some View {
        GeometryReader { geo in
            let c = CGPoint(x: geo.size.width / 2, y: geo.size.height / 2)
            let radius: CGFloat = min(geo.size.width / 2 - 50, 160)
            let moon: CGFloat = 62
            ZStack {
                Circle().stroke(soft.opacity(dark ? 0.22 : 0.3), lineWidth: 1)
                    .frame(width: radius * 2 + 40, height: radius * 2 + 40).position(c)
                ForEach(0..<4, id: \.self) { k in
                    let a = Double(k) * Double.pi / 2 - Double.pi / 2
                    let ca: Double = cos(a), sa: Double = sin(a)
                    Image(systemName: "sparkle").font(.system(size: 9)).foregroundStyle(soft)
                        .position(x: c.x + (radius + 20) * CGFloat(ca), y: c.y + (radius + 20) * CGFloat(sa))
                }
                .accessibilityHidden(true)
                ForEach(Array(Lunar.order.enumerated()), id: \.offset) { i, id in
                    let a = (-90.0 + Double(i) * 45.0) * Double.pi / 180
                    let ca: Double = cos(a), sa: Double = sin(a)
                    let lit = found[id] != nil
                    Button { path.append(id) } label: {
                        Image("Moon-\(id)").resizable().scaledToFit()
                            .frame(width: moon, height: moon)
                            .saturation(lit ? 1 : 0.6)
                            .opacity(lit || id == tonight ? 1 : (dark ? 0.7 : 0.85))
                            // On light, every moon gets a soft shadow so it stands off the page.
                            .shadow(color: lit ? glow.opacity(0.6) : (dark ? .clear : .black.opacity(0.18)), radius: lit ? 8 : 4)
                            .overlay(Circle().stroke(Self.violet, lineWidth: id == tonight ? 2 : 0).padding(-6))
                            .shadow(color: id == tonight ? Self.violet : .clear, radius: 10)
                            .frame(width: moon + 16, height: moon + 16)
                            .contentShape(Circle())
                    }
                    .buttonStyle(.plain)
                    .position(x: c.x + radius * CGFloat(ca), y: c.y + radius * CGFloat(sa))
                    .accessibilityLabel("\(Lunar.phaseName(id)), \(lit ? "discovered" : "not yet discovered")\(id == tonight ? ", tonight" : "")")
                }
                centre(found: found, tonight: tonight, dim: dim).frame(width: 210).position(c)
            }
        }
    }

    @ViewBuilder
    private func centre(found: [String: Date], tonight: String, dim: String) -> some View {
        VStack(spacing: 6) {
            if let done = progress.journeyCompletedAt(dim) {
                Text("LUNAR JOURNEY").font(.system(size: 12, design: .serif)).tracking(3).foregroundStyle(Theme.gold)
                Text("Complete").font(.system(size: 28, design: .serif)).foregroundStyle(.primary)
                Text("All eight phases, \(done.formatted(date: .abbreviated, time: .omitted))")
                    .font(.system(size: 14, design: .serif)).foregroundStyle(soft.opacity(0.85))
            } else {
                Text("TONIGHT").font(.system(size: 13, design: .serif)).tracking(4).foregroundStyle(Self.violet)
                HStack(spacing: 6) {
                    Rectangle().fill(soft.opacity(0.4)).frame(width: 30, height: 0.6)
                    Image(systemName: "sparkle").font(.system(size: 8))
                    Rectangle().fill(soft.opacity(0.4)).frame(width: 30, height: 0.6)
                }
                .foregroundStyle(soft).accessibilityHidden(true)
                Text(Lunar.phaseName(tonight)).font(.system(size: 28, design: .serif)).foregroundStyle(.primary)
                    .lineLimit(1).minimumScaleFactor(0.7)
                if let w = progress.nextWindow(tonight) {
                    Text("until \(w.end.formatted(.dateTime.month(.abbreviated).day()))")
                        .font(.system(size: 15, design: .serif)).foregroundStyle(.secondary)
                }
                if found[tonight] == nil {
                    TimelineView(.periodic(from: .now, by: 1)) { _ in
                        let p = progress.phaseProgress(dim)
                        VStack(spacing: 8) {
                            GeometryReader { g in
                                ZStack(alignment: .leading) {
                                    Capsule().fill(Color.primary.opacity(0.15))
                                    Capsule().fill(LinearGradient(colors: [dark ? .white : Self.violet.opacity(0.5), Self.violet],
                                                                  startPoint: .leading, endPoint: .trailing))
                                        .frame(width: max(6, g.size.width * p))
                                }
                            }
                            .frame(width: 150, height: 4)
                            Text("\(Self.clock(p * 180)) of 3:00 heard")
                                .font(.system(size: 14).monospacedDigit()).foregroundStyle(.secondary)
                        }
                        .padding(.top, 8)
                    }
                } else {
                    Text("Discovered in this Dimension").font(.system(size: 14, design: .serif))
                        .foregroundStyle(.secondary).padding(.top, 8)
                }
            }
        }
        .multilineTextAlignment(.center)
    }

    static func clock(_ seconds: Double) -> String {
        let s = Int(seconds)
        return "\(s / 60):" + String(format: "%02d", s % 60)
    }
}

/// One phase in the list: its moon, its name, and where it stands.
private struct PhaseRow: View {
    let phase: String
    let found: [String: Date]
    let tonight: String
    let dim: String
    @Environment(EngineController.self) private var engine

    var body: some View {
        let lit = found[phase] != nil
        HStack(spacing: 14) {
            Image("Moon-\(phase)").resizable().scaledToFit()
                .frame(width: 36, height: 36)
                .saturation(lit ? 1 : 0.6)
                .opacity(lit || phase == tonight ? 1 : 0.75)
            Text(Lunar.phaseName(phase))
            Spacer()
            Text(status).font(.subheadline).foregroundStyle(phase == tonight && !lit ? Theme.gold : .secondary)
        }
    }

    private var status: String {
        if let at = found[phase] { return "Discovered \(at.formatted(.dateTime.month(.abbreviated).day()))" }
        if !engine.phaseHasSong(dim, phase) { return "Still to come" }
        if phase == tonight { return "Tonight" }
        return "Not yet"
    }
}

/// One phase's page: its moon, where it stands, and how to discover it.
struct PhaseDetailView: View {
    let phase: String
    let dimensionID: String
    let dimensionName: String
    @Environment(EngineController.self) private var engine
    private var progress: ProgressStore { engine.progress }

    var body: some View {
        let name = Lunar.phaseName(phase)
        let found = progress.discoveryDates(dimensionID)
        let tonight = progress.currentPhaseID
        let f = Date.FormatStyle.dateTime.month(.abbreviated).day()
        List {
            Section {
                VStack(spacing: 10) {
                    Image("Moon-\(phase)").resizable().scaledToFit()
                        .frame(width: 120, height: 120)
                        .saturation(found[phase] != nil ? 1 : 0.6)
                        .shadow(color: found[phase] != nil ? MoonChartView.violet.opacity(0.6) : .black.opacity(0.15), radius: 12)
                        .accessibilityHidden(true)
                    Text(name).font(.system(size: 26, design: .serif))
                    Text(dimensionName).font(.subheadline).foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .listRowBackground(Color.clear)
            }
            Section {
                if let at = found[phase] {
                    LabeledContent("Discovered", value: at.formatted(date: .abbreviated, time: .omitted))
                    Text("Its song returns here after dark with every \(name).")
                } else if !engine.phaseHasSong(dimensionID, phase) {
                    LabeledContent("Status", value: "Still to come")
                    Text("The \(name)'s song for this Dimension is still being written. It can be discovered once it arrives.")
                } else if phase == tonight {
                    LabeledContent("Status") { Text("Tonight").foregroundStyle(Theme.gold) }
                    if let w = progress.nextWindow(phase) { LabeledContent("Until", value: w.end.formatted(f)) }
                    TimelineView(.periodic(from: .now, by: 1)) { _ in
                        let p = progress.phaseProgress(dimensionID)
                        VStack(alignment: .leading, spacing: 8) {
                            ProgressView(value: p).tint(MoonChartView.violet)
                            Text("\(MoonChartView.clock(p * 180)) of 3:00 heard")
                                .font(.subheadline.monospacedDigit()).foregroundStyle(.secondary)
                        }
                        .padding(.vertical, 4)
                    }
                } else {
                    LabeledContent("Status", value: "Not yet discovered")
                    if let w = progress.nextWindow(phase) {
                        LabeledContent("Next", value: "\(w.start.formatted(f)) – \(w.end.formatted(f))")
                    }
                }
            } footer: {
                if found[phase] == nil && engine.phaseHasSong(dimensionID, phase) {
                    Text("Listen to \(dimensionName) for 3 minutes after dark during the \(name) to discover it. The minutes do not need to be continuous.")
                }
            }
        }
        .navigationTitle(name)
        .inlineNavigationTitle()
    }
}


/// A secret's badge: a symbol for its moon event in a gold ring.
struct SecretBadge: View {
    let eventID: String
    var size: CGFloat = 36

    static func symbol(_ id: String) -> String {
        switch id {
        case "supermoon": "moon.circle.fill"
        case "risingSupermoon": "moon.haze.fill"
        case "micromoon": "smallcircle.filled.circle"
        case "blueMoon": "moon.fill"
        case "blackMoon": "circle.fill"
        case "harvestMoon": "leaf.fill"
        case "huntersMoon": "moon.stars.fill"
        case "totalLunarEclipse": "circle.lefthalf.filled"
        case "partialLunarEclipse": "circle.righthalf.filled"
        case "penumbralLunarEclipse": "circle.dotted"
        case "solarEclipse": "sun.max.circle.fill"
        default: "sparkle"
        }
    }

    var body: some View {
        Image(systemName: Self.symbol(eventID))
            .font(.system(size: size * 0.5))
            .foregroundStyle(eventID == "blueMoon" ? Color(red: 0.45, green: 0.62, blue: 1) : Theme.gold)
            .frame(width: size, height: size)
            .overlay(Circle().stroke(Theme.gold.opacity(0.8), lineWidth: 1.5))
            .accessibilityHidden(true)
    }
}
