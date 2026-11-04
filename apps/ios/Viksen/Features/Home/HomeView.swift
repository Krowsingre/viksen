import SwiftUI

/// Home, after the owner's concept (2026-11-04): the night sky, the wordmark (with
/// "After Dark" at night), the fox in the owner's moon ring (tonight's phase pulses; the
/// fox's button plays, then shows a live waveform), the lunar day (opens the Moon Chart)
/// and Continue Listening cards, one per Dimension on this device.
struct HomeView: View {
    @Binding var selectedTab: AppTab
    @Environment(EngineController.self) private var engine
    @Environment(\.horizontalSizeClass) private var sizeClass
    @State private var showSettings = false
    @State private var showMoonChart = false
    @State private var page = 0

    private var wide: Bool { sizeClass == .regular }

    var body: some View {
        GeometryReader { geo in
            let ringWidth = min(geo.size.width - 24, wide ? 460 : 420, geo.size.height * 0.46)
            ZStack(alignment: .top) {
                HomeSkyBackground().ignoresSafeArea()
                VStack(spacing: 0) {
                    Spacer(minLength: wide ? 24 : 12)
                    wordmark
                    Spacer(minLength: 10)
                    foxRing(width: ringWidth)
                    Spacer(minLength: 10)
                    moon
                    Spacer(minLength: 14)
                    if engine.previousRunEndedUnexpectedly { restoreCard.padding(.horizontal, 24).padding(.bottom, 12) }
                    continueCards
                        .frame(maxWidth: wide ? 640 : .infinity)
                        .padding(.horizontal, wide ? 48 : 20)
                    if continueItems.count > 1 { pageDots.padding(.top, 10) }
                    Spacer(minLength: wide ? 24 : 12)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .overlay(alignment: .topTrailing) {
            Button { showSettings = true } label: {
                Image(systemName: "line.3.horizontal")
                    .font(.title3.weight(.light))
                    .foregroundStyle(Color(red: 0.86, green: 0.80, blue: 1))
                    .frame(width: 48, height: 48)
                    .background(Color.black.opacity(0.25), in: Circle())
                    .overlay(Circle().stroke(Color(red: 0.80, green: 0.70, blue: 1).opacity(0.5), lineWidth: 1))
                    .shadow(color: Theme.accentGlow.opacity(0.5), radius: 8)
            }
            .padding(.trailing, 18)
            .padding(.top, 4)
            .accessibilityLabel("Menu")
        }
        .sheet(isPresented: $showSettings) { SettingsView() }
        .sheet(isPresented: $showMoonChart) { MoonChartView() }
        .preferredColorScheme(.dark)
        .onAppear { engine.refreshWorlds() }
    }

    // MARK: Wordmark

    private static let lavender = Color(red: 0.90, green: 0.86, blue: 1)

    private var wordmark: some View {
        VStack(spacing: 8) {
            Text("VIKSEN")
                .font(.system(size: wide ? 44 : 38, weight: .light, design: .serif))
                .tracking(wide ? 18 : 14)
                .foregroundStyle(Self.lavender)
                .shadow(color: Theme.accentGlow.opacity(0.55), radius: 10)
                .accessibilityAddTraits(.isHeader)
                .accessibilityLabel(engine.nightClock.isNight ? "Viksen After Dark" : "Viksen")
            if engine.nightClock.isNight {
                Text("AFTER DARK")
                    .font(.system(size: 13, weight: .regular, design: .serif))
                    .tracking(7)
                    .foregroundStyle(Theme.gold)
                    .shadow(color: Theme.gold.opacity(0.5), radius: 6)
                    .accessibilityHidden(true)
                    .transition(.opacity)
            }
            Text("IMMERSIVE AUDITORY REALITY")
                .font(.system(size: 11, weight: .regular, design: .serif))
                .tracking(3.5)
                .foregroundStyle(Self.lavender.opacity(0.8))
            HStack(spacing: 8) {
                Rectangle().fill(Self.lavender.opacity(0.35)).frame(width: 56, height: 0.6)
                ForEach(["moonphase.waxing.crescent", "moonphase.first.quarter", "moonphase.waning.crescent",
                         "moonphase.last.quarter"], id: \.self) { Image(systemName: $0).font(.system(size: 9)) }
                Rectangle().fill(Self.lavender.opacity(0.35)).frame(width: 56, height: 0.6)
            }
            .foregroundStyle(Self.lavender.opacity(0.8))
            .accessibilityHidden(true)
        }
        .animation(.easeInOut(duration: 1.2), value: engine.nightClock.isNight)
    }

    // MARK: Fox and moon ring

    /// The owner's moon ring (MoonRing) with the fox at its heart: the fox holds the play
    /// button, and while a Dimension plays, a live waveform. Tonight's phase pulses.
    private func foxRing(width: CGFloat) -> some View {
        let height = width * 1199 / 1312   // the ring artwork's aspect
        return ZStack {
            Image("MoonRing").resizable().scaledToFit()
            MoonRingPulse(phaseID: engine.meters.moonPhaseId, ringWidth: width, ringHeight: height)
            foxButton(size: width * 1.08)
                .offset(y: height * (0.495 - 0.5))
        }
        .frame(width: width, height: height)
    }

    private func foxButton(size: CGFloat) -> some View {
        Button {
            if inDimension { selectedTab = .experience }
            else if let w = enterTarget { engine.enterWorld(w) }
            else { selectedTab = .experience }   // nothing downloaded yet: choose one
        } label: {
            ZStack {
                Image(inDimension ? "FoxEmpty" : "FoxPlay").resizable().scaledToFit()
                if inDimension {
                    LiveWaveform(level: Double(max(engine.meters.outputPeakLeft, engine.meters.outputPeakRight)))
                        .frame(width: size * 0.088, height: size * 0.072)
                        .position(x: size * 491 / 1024, y: size * 535 / 1024)
                }
            }
            .frame(width: size, height: size)
            .contentShape(Circle().scale(0.5))
        }
        .buttonStyle(.plain)
        .accessibilityLabel(inDimension ? "Open \(engine.worldStatus.name)" : (enterTarget.map { "Enter \($0.name)" } ?? "Choose a Dimension"))
    }

    // MARK: Enter Dimension

    /// The Dimension "Enter" goes to: the last one entered, else the first on this device.
    private var enterTarget: AtmosWorldInfo? {
        let playable = engine.worlds.filter(engine.isPlayable)
        return playable.first { $0.worldID == engine.lastDimensionID } ?? playable.first
    }

    private var inDimension: Bool {
        engine.worldStatus.playing && engine.worldStatus.loaded && engine.worldStatus.worldID != "studio"
    }

    // MARK: Continue Listening

    private var continueItems: [AtmosWorldInfo] {
        let playable = engine.worlds.filter(engine.isPlayable)
        let current = engine.worldStatus.loaded ? engine.worldStatus.worldID : engine.lastDimensionID
        return playable.sorted { a, b in (a.worldID == current ? 0 : 1) < (b.worldID == current ? 0 : 1) }
    }

    @ViewBuilder private var continueCards: some View {
        let items = continueItems
        if items.isEmpty {
            Button { selectedTab = .experience } label: {
                card(art: nil, title: "Choose your first Dimension", subtitle: "Download one to begin", progress: nil, wideTime: nil)
            }
            .buttonStyle(.plain)
        } else {
            TabView(selection: $page) {
                ForEach(Array(items.enumerated()), id: \.element.worldID) { index, w in
                    Button { selectedTab = .experience; if !(engine.worldStatus.worldID == w.worldID && engine.worldStatus.playing) { engine.enterWorld(w) } } label: {
                        cardFor(w)
                    }
                    .buttonStyle(.plain)
                    .tag(index)
                }
            }
            .tabViewStyle(.page(indexDisplayMode: .never))
            .frame(height: 128)
        }
    }

    private func cardFor(_ w: AtmosWorldInfo) -> some View {
        let st = engine.worldStatus
        let isCurrent = st.loaded && st.worldID == w.worldID
        let progress = isCurrent && st.lengthSeconds > 0 ? st.positionSeconds / st.lengthSeconds : nil
        let title = isCurrent && st.songCount > 1 ? "\(w.name) · \(st.songTitle)" : w.name
        let time = isCurrent && st.lengthSeconds > 0 ? "\(clock(st.positionSeconds)) / \(clock(st.lengthSeconds))" : nil
        return card(art: w.worldID, title: title,
                    subtitle: isCurrent && st.playing ? (st.sectionTitle.isEmpty ? "Now playing" : st.sectionTitle) : (w.album.isEmpty ? w.artist : w.album),
                    progress: progress, wideTime: isCurrent && st.playing ? nil : time)
    }

    private func card(art: String?, title: String, subtitle: String, progress: Double?, wideTime: String?) -> some View {
        HStack(spacing: 16) {
            DimensionThumbnail(dimensionID: art)
                .frame(width: 92, height: 92)
                .clipShape(RoundedRectangle(cornerRadius: 14))
                .overlay(RoundedRectangle(cornerRadius: 14).stroke(Self.lavender.opacity(0.25), lineWidth: 1))
            VStack(alignment: .leading, spacing: 5) {
                Text("CONTINUE LISTENING").font(.system(size: 11, design: .serif)).tracking(2.5)
                    .foregroundStyle(Self.lavender.opacity(0.75))
                Text(title).font(.system(size: 21, weight: .regular, design: .serif)).foregroundStyle(.white)
                    .lineLimit(1).minimumScaleFactor(0.7)
                Text(wideTime ?? subtitle).font(.system(size: 14, design: .serif)).foregroundStyle(Self.lavender.opacity(0.75))
                    .lineLimit(1)
                if let progress {
                    GeometryReader { g in
                        ZStack(alignment: .leading) {
                            Capsule().fill(Color.white.opacity(0.15))
                            Capsule().fill(LinearGradient(colors: [Self.lavender, Theme.accent], startPoint: .leading, endPoint: .trailing))
                                .frame(width: g.size.width * min(1, max(0, progress)))
                        }
                    }
                    .frame(height: 4)
                    .padding(.top, 4)
                }
            }
            Spacer(minLength: 0)
            Image(systemName: "chevron.right").font(.body.weight(.light))
                .foregroundStyle(Self.lavender)
                .frame(width: 40, height: 40)
                .overlay(Circle().stroke(Self.lavender.opacity(0.5), lineWidth: 1))
        }
        .padding(14)
        .background(Color(red: 0.12, green: 0.07, blue: 0.20).opacity(0.55), in: RoundedRectangle(cornerRadius: 24))
        .background(.ultraThinMaterial.opacity(0.35), in: RoundedRectangle(cornerRadius: 24))
        .overlay(RoundedRectangle(cornerRadius: 24).stroke(Self.lavender.opacity(0.2), lineWidth: 1))
        .contentShape(RoundedRectangle(cornerRadius: 24))
    }

    private var pageDots: some View {
        HStack(spacing: 10) {
            ForEach(0..<continueItems.count, id: \.self) { i in
                Circle().fill(i == page ? Color.white : Color.white.opacity(0.3)).frame(width: 6, height: 6)
            }
        }
        .accessibilityHidden(true)
    }

    // MARK: Moon

    private var moon: some View {
        Button { showMoonChart = true } label: { moonLine }
            .buttonStyle(.plain)
            .accessibilityHint("Opens the Moon Chart")
    }

    private var moonLine: some View {
        VStack(spacing: 10) {
            HStack(spacing: 18) {
                Image(systemName: Lunar.symbol(engine.meters.moonPhaseId)).font(.title3)
                Text(Lunar.phaseName(engine.meters.moonPhaseId))
                    .font(.system(size: wide ? 26 : 22, weight: .regular, design: .serif))
                Rectangle().fill(Self.lavender.opacity(0.5)).frame(width: 0.8, height: 26)
                Text("Lunar Day \(engine.meters.lunarDay)")
                    .font(.system(size: wide ? 19 : 16, weight: .regular, design: .serif))
            }
            HStack(spacing: 10) {
                Rectangle().fill(Self.lavender.opacity(0.3)).frame(width: 110, height: 0.6)
                Image(systemName: "sparkle").font(.system(size: 11)).foregroundStyle(Theme.gold)
                Rectangle().fill(Self.lavender.opacity(0.3)).frame(width: 110, height: 0.6)
            }
            .accessibilityHidden(true)
        }
        .foregroundStyle(Self.lavender)
        .frame(maxWidth: .infinity)
        .contentShape(Rectangle())
        .accessibilityElement(children: .combine)
    }

    // MARK: Restore

    private var restoreCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("Viksen closed unexpectedly", systemImage: "arrow.counterclockwise.circle")
                .font(.subheadline.weight(.semibold)).foregroundStyle(.white)
            Text("Restore your last session? The microphone stays off until you turn it on.")
                .font(.caption).foregroundStyle(.white.opacity(0.7))
            HStack {
                Button("Restore") { engine.restoreAutosave(); selectedTab = .experience }
                    .buttonStyle(.borderedProminent)
                Button("Not now") { engine.dismissRestore() }
                    .buttonStyle(.bordered)
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.ultraThinMaterial.opacity(0.6), in: RoundedRectangle(cornerRadius: 16))
    }

    private func clock(_ s: Double) -> String {
        let t = Int(max(0, s))
        return String(format: "%02d:%02d", t / 60, t % 60)
    }
}
