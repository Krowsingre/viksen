import SwiftUI

/// The Moon Chart (P-3), after the owner's concept (2026-11-04): the eight lunar phases of
/// one Dimension in a ring of real moons on the night sky. Discovered phases shine; tonight's
/// is ringed and shows how much of its 3 minutes has been heard. Progress is per Dimension.
/// Global Secrets show a count only, never a total.
struct MoonChartView: View {
    var initialDimensionID: String?
    var onClose: (() -> Void)? = nil
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @Environment(\.dismiss) private var dismiss
    @State private var dimensionID: String?
    @State private var selected: String?

    static let lavender = Color(red: 0.90, green: 0.86, blue: 1)
    static let violet = Color(red: 0.62, green: 0.48, blue: 1)
    private var progress: ProgressStore { engine.progress }

    /// Only the Dimensions this listener has access to.
    private var dimensions: [AtmosWorldInfo] {
        engine.worlds.filter { w in
            guard w.worldID != "studio" else { return false }
            let need: Tier = w.tier == "studio" ? .studio : w.tier == "experience" ? .experience : .free
            return need <= store.tier
        }
    }

    var body: some View {
        let ids = Set(dimensions.map(\.worldID))
        let dim = [dimensionID, initialDimensionID, engine.lastDimensionID].compactMap { $0 }.first { ids.contains($0) }
            ?? dimensions.first?.worldID ?? ""
        let found = progress.discoveryDates(dim)
        let tonight = progress.currentPhaseID
        let phase = selected ?? tonight
        ZStack(alignment: .top) {
            GeometryReader { geo in
                Image("MoonChartSky").resizable().scaledToFill()
                    .frame(width: geo.size.width, height: geo.size.height).clipped()
            }
            .ignoresSafeArea()
            .accessibilityHidden(true)
            ScrollView {
                VStack(spacing: 18) {
                    header
                    dimensionPicker(dim)
                    ring(found: found, tonight: tonight, selected: phase, dim: dim)
                        .frame(height: 330)
                    VStack(spacing: 14) {
                        if let done = progress.journeyCompletedAt(dim) { completeCard(done) }
                        detailCard(phase: phase, found: found, tonight: tonight, dim: dim)
                        HStack(spacing: 14) {
                            journeyCard(found: found)
                            secretsCard(dim: dim)
                        }
                        if progress.globalSecretsFound > 0 { globalCard }
                    }
                    .padding(.horizontal, 20)
                }
                .padding(.bottom, 32)
            }
            .scrollIndicators(.hidden)
        }
        .overlay(alignment: .topTrailing) {
            CloseButton { if let onClose { onClose() } else { dismiss() } }
                .padding(.trailing, 12).padding(.top, 0)
        }
        .preferredColorScheme(.dark)
    }

    // MARK: Header

    private var header: some View {
        VStack(spacing: 2) {
            ZStack {
                Circle().trim(from: 0.62, to: 0.88)
                    .stroke(Self.lavender.opacity(0.7), style: StrokeStyle(lineWidth: 1.2, lineCap: .round))
                    .frame(width: 130, height: 130)
                    .offset(y: 46)
                    .shadow(color: Self.violet, radius: 4)
                Image(systemName: "sparkle").font(.system(size: 9)).foregroundStyle(Self.lavender)
                    .offset(y: 4)
            }
            .frame(height: 40)
            .clipped()
            .accessibilityHidden(true)
            Text("Moon Chart")
                .font(.system(size: 44, weight: .regular, design: .serif))
                .foregroundStyle(Self.lavender)
                .shadow(color: Self.violet.opacity(0.5), radius: 8)
                .accessibilityAddTraits(.isHeader)
        }
        .padding(.top, 10)
    }

    private func dimensionPicker(_ current: String) -> some View {
        Menu {
            Picker("Dimension", selection: Binding(get: { current },
                                                   set: { dimensionID = $0; selected = nil })) {
                ForEach(dimensions, id: \.worldID) { d in Text(d.name).tag(d.worldID) }
            }
        } label: {
            HStack(spacing: 12) {
                Image(systemName: "moon").font(.title3).foregroundStyle(Self.lavender)
                Text(dimensions.first { $0.worldID == current }?.name ?? "Dimension")
                    .font(.system(size: 18, design: .serif))
                    .lineLimit(1).minimumScaleFactor(0.8)
                Image(systemName: "chevron.down").font(.footnote.weight(.semibold))
            }
            .foregroundStyle(.white)
            .padding(.horizontal, 22).frame(minHeight: 50)
            .background(Color(red: 0.14, green: 0.09, blue: 0.24).opacity(0.6), in: Capsule())
            .overlay(Capsule().stroke(Self.violet.opacity(0.55), lineWidth: 1))
        }
        .accessibilityLabel("Dimension")
        .padding(.horizontal, 20)
    }

    // MARK: Ring

    private func ring(found: [String: Date], tonight: String, selected: String, dim: String) -> some View {
        GeometryReader { geo in
            let c = CGPoint(x: geo.size.width / 2, y: geo.size.height / 2)
            let radius: CGFloat = min(geo.size.width / 2 - 52, 150)
            let moon: CGFloat = 62
            ZStack {
                Circle().stroke(Self.lavender.opacity(0.22), lineWidth: 1)
                    .frame(width: radius * 2 + 40, height: radius * 2 + 40).position(c)
                ForEach(0..<4, id: \.self) { k in
                    let a = Double(k) * Double.pi / 2 - Double.pi / 2
                    let ca: Double = cos(a), sa: Double = sin(a)
                    Image(systemName: "sparkle").font(.system(size: 9)).foregroundStyle(Self.lavender)
                        .position(x: c.x + (radius + 20) * CGFloat(ca), y: c.y + (radius + 20) * CGFloat(sa))
                }
                .accessibilityHidden(true)
                ForEach(Array(Lunar.order.enumerated()), id: \.offset) { i, id in
                    let a = (-90.0 + Double(i) * 45.0) * Double.pi / 180
                    let ca: Double = cos(a), sa: Double = sin(a)
                    let lit = found[id] != nil
                    Button { self.selected = id } label: {
                        Image("Moon-\(id)").resizable().scaledToFit()
                            .frame(width: moon, height: moon)
                            .saturation(lit ? 1 : 0.6)
                            .opacity(lit ? 1 : (id == tonight ? 0.85 : 0.5))
                            .shadow(color: lit ? Self.lavender.opacity(0.6) : .clear, radius: 8)
                            .overlay(Circle().stroke(Self.violet, lineWidth: id == selected ? 2.5 : 0).padding(-6))
                            .shadow(color: id == selected ? Self.violet : .clear, radius: 10)
                            .overlay(Circle().stroke(Self.lavender.opacity(id == tonight && id != selected ? 0.6 : 0), lineWidth: 1).padding(-6))
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
                Text("Complete").font(.system(size: 28, design: .serif)).foregroundStyle(.white)
                Text("All eight phases, \(done.formatted(date: .abbreviated, time: .omitted))")
                    .font(.system(size: 14, design: .serif)).foregroundStyle(Self.lavender.opacity(0.8))
            } else {
                Text("TONIGHT").font(.system(size: 13, design: .serif)).tracking(4).foregroundStyle(Self.violet)
                HStack(spacing: 6) {
                    Rectangle().fill(Self.lavender.opacity(0.4)).frame(width: 30, height: 0.6)
                    Image(systemName: "sparkle").font(.system(size: 8))
                    Rectangle().fill(Self.lavender.opacity(0.4)).frame(width: 30, height: 0.6)
                }
                .foregroundStyle(Self.lavender).accessibilityHidden(true)
                Text(Lunar.phaseName(tonight)).font(.system(size: 28, design: .serif)).foregroundStyle(.white)
                    .lineLimit(1).minimumScaleFactor(0.7)
                if let w = progress.nextWindow(tonight) {
                    Text("until \(w.end.formatted(.dateTime.month(.abbreviated).day()))")
                        .font(.system(size: 15, design: .serif)).foregroundStyle(Self.lavender.opacity(0.85))
                }
                if found[tonight] == nil {
                    TimelineView(.periodic(from: .now, by: 1)) { _ in
                        let p = progress.phaseProgress(dim)
                        VStack(spacing: 8) {
                            GeometryReader { g in
                                ZStack(alignment: .leading) {
                                    Capsule().fill(Color.white.opacity(0.15))
                                    Capsule().fill(LinearGradient(colors: [.white, Self.violet], startPoint: .leading, endPoint: .trailing))
                                        .frame(width: max(6, g.size.width * p))
                                }
                            }
                            .frame(width: 150, height: 4)
                            Text("\(Self.clock(p * 180)) of 3:00 heard")
                                .font(.system(size: 14).monospacedDigit()).foregroundStyle(Self.lavender.opacity(0.9))
                        }
                        .padding(.top, 8)
                    }
                } else {
                    Text("Discovered in this Dimension").font(.system(size: 14, design: .serif))
                        .foregroundStyle(Self.lavender.opacity(0.9)).padding(.top, 8)
                }
            }
        }
        .multilineTextAlignment(.center)
    }

    // MARK: Cards

    private func glass<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        content()
            .padding(18)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color(red: 0.13, green: 0.08, blue: 0.22).opacity(0.55), in: RoundedRectangle(cornerRadius: 22))
            .background(.ultraThinMaterial.opacity(0.3), in: RoundedRectangle(cornerRadius: 22))
            .overlay(RoundedRectangle(cornerRadius: 22).stroke(Self.violet.opacity(0.35), lineWidth: 1))
    }

    private func detailCard(phase: String, found: [String: Date], tonight: String, dim: String) -> some View {
        let name = Lunar.phaseName(phase)
        let tag: String, tagColor: Color, line1: String, line2: String
        if let at = found[phase] {
            tag = "Discovered \(at.formatted(.dateTime.month(.abbreviated).day()))"
            tagColor = Self.lavender
            line1 = "Heard in \(dimensions.first { $0.worldID == dim }?.name ?? "this Dimension")."
            line2 = "Its song returns here after dark with every \(name)."
        } else if !engine.phaseHasSong(dim, phase) {
            tag = "Still to come"; tagColor = Self.lavender.opacity(0.7)
            line1 = "The \(name)'s song for this Dimension is still being written."
            line2 = "It will be discoverable once it arrives."
        } else if phase == tonight {
            let left = max(0, Int(180 - progress.phaseProgress(dim) * 180))
            tag = "Tonight"; tagColor = Theme.gold
            line1 = left < 180 ? "Keep listening after dark: \(left / 60) min \(left % 60) s to go."
                               : "Listen to this Dimension for 3 minutes after dark while it lasts to discover it."
            line2 = "The minutes do not need to be continuous."
        } else {
            tag = "Not yet discovered"; tagColor = Self.lavender.opacity(0.7)
            line1 = "Listen to this Dimension for 3 minutes after dark during the next \(name)."
            if let w = progress.nextWindow(phase) {
                let f = Date.FormatStyle.dateTime.month(.abbreviated).day()
                line2 = "Next: \(w.start.formatted(f)) – \(w.end.formatted(f))"
            } else { line2 = "" }
        }
        return glass {
            VStack(alignment: .leading, spacing: 8) {
                HStack(alignment: .firstTextBaseline) {
                    Text(name).font(.system(size: 24, design: .serif)).foregroundStyle(.white)
                    Spacer()
                    Text(tag).font(.system(size: 15, design: .serif)).foregroundStyle(tagColor)
                }
                Text(line1).font(.system(size: 16, design: .serif)).foregroundStyle(Self.lavender.opacity(0.95))
                if !line2.isEmpty {
                    Text(line2).font(.system(size: 14, design: .serif)).italic().foregroundStyle(Self.lavender.opacity(0.7))
                }
            }
        }
    }

    private func completeCard(_ at: Date) -> some View {
        glass {
            HStack(spacing: 14) {
                Image(systemName: "moon.stars.fill").font(.title2).foregroundStyle(Theme.gold)
                    .frame(width: 52, height: 52).overlay(Circle().stroke(Theme.gold, lineWidth: 1.5))
                VStack(alignment: .leading, spacing: 2) {
                    Text("Lunar Journey complete").font(.system(size: 18, design: .serif)).foregroundStyle(.white)
                    Text("Every phase heard in this Dimension").font(.system(size: 14, design: .serif))
                        .foregroundStyle(Self.lavender.opacity(0.75))
                }
                Spacer()
            }
        }
    }

    private func statCard<Extra: View>(title: String, value: String, suffix: String,
                                       @ViewBuilder extra: () -> Extra) -> some View {
        glass {
            VStack(alignment: .leading, spacing: 10) {
                Text(title).font(.system(size: 12, design: .serif)).tracking(2.5).foregroundStyle(Self.lavender.opacity(0.8))
                (Text(value).font(.system(size: 30, design: .serif)).foregroundColor(.white)
                 + Text(" \(suffix)").font(.system(size: 19, design: .serif)).foregroundColor(Self.lavender))
                    .lineLimit(1).minimumScaleFactor(0.7)
                extra()
            }
        }
        .accessibilityElement(children: .combine)
    }

    private func journeyCard(found: [String: Date]) -> some View {
        statCard(title: "LUNAR JOURNEY", value: "\(found.count)", suffix: "of 8 phases") {
            HStack(spacing: 5) {
                ForEach(Lunar.order, id: \.self) { id in
                    Circle().fill(found[id] != nil ? Self.lavender : .clear)
                        .overlay(Circle().stroke(Self.violet.opacity(0.8), lineWidth: 1))
                        .frame(width: 11, height: 11)
                }
            }
        }
    }

    private func secretsCard(dim: String) -> some View {
        let n = progress.secretsFound(dim)
        return statCard(title: "SECRETS", value: "\(n)", suffix: "of 5 found") {
            HStack(spacing: 8) {
                ForEach(0..<5, id: \.self) { k in
                    Rectangle().fill(k < n ? Theme.gold : .clear)
                        .overlay(Rectangle().stroke(Self.violet.opacity(0.8), lineWidth: 1))
                        .frame(width: 10, height: 10).rotationEffect(.degrees(45))
                }
            }
        }
    }

    private var globalCard: some View {
        statCard(title: "GLOBAL SECRETS", value: "\(progress.globalSecretsFound)", suffix: "found") {
            Text("Across every Dimension").font(.system(size: 13, design: .serif)).foregroundStyle(Self.lavender.opacity(0.7))
        }
    }

    static func clock(_ seconds: Double) -> String {
        let s = Int(seconds)
        return "\(s / 60):" + String(format: "%02d", s % 60)
    }
}
