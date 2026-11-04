import SwiftUI

/// Home: on a plain dark background, the wordmark,
/// one glowing "Enter Dimension" button, Continue Listening cards (one per
/// Dimension on this device) and the lunar day. Compact iPhones show it full
/// screen; wide (unfolded) layouts put the card and the moon side by side.
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
            ZStack(alignment: .top) {
                Color.black.ignoresSafeArea()   // no wallpaper (owner, 2026-11-04)
                VStack(spacing: 0) {
                    Spacer(minLength: geo.size.height * (wide ? 0.13 : 0.20))
                    wordmark
                    Spacer(minLength: wide ? 24 : 36)
                    enterButton
                    Spacer(minLength: wide ? 20 : 28)
                    if engine.previousRunEndedUnexpectedly { restoreCard.padding(.horizontal, 24).padding(.bottom, 12) }
                    VStack(alignment: .leading, spacing: 12) {
                        if wide && !continueItems.isEmpty {
                            Text("Continue Listening").font(.title3.weight(.medium)).foregroundStyle(.white)
                        }
                        continueCards
                    }
                    .frame(maxWidth: wide ? 820 : .infinity)
                    .padding(.horizontal, wide ? 48 : 24)
                    if continueItems.count > 1 { pageDots.padding(.top, 14) }
                    moon.padding(.top, wide ? 22 : 18)
                    Spacer(minLength: wide ? 28 : 16)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .overlay(alignment: .topTrailing) {
            Button { showSettings = true } label: {
                Image(systemName: "gearshape")
                    .font(.title3)
                    .foregroundStyle(Theme.accentGlow)
                    .frame(width: 44, height: 44)
                    .background(.ultraThinMaterial.opacity(0.6), in: Circle())
                    .overlay(Circle().stroke(Theme.accent.opacity(0.4), lineWidth: 1))
            }
            .padding(.trailing, 20)
            .padding(.top, 8)
            .accessibilityLabel("Settings")
        }
        .sheet(isPresented: $showSettings) { SettingsView() }
        .sheet(isPresented: $showMoonChart) { MoonChartView() }
        .preferredColorScheme(.dark)
        .onAppear { engine.refreshWorlds() }
    }

    // MARK: Wordmark

    private var wordmark: some View {
        VStack(spacing: 14) {
            Text("VIKSEN")
                .font(.system(size: wide ? 46 : 40, weight: .ultraLight))
                .tracking(wide ? 22 : 18)
                .foregroundStyle(.white)
                .shadow(color: Theme.accentGlow.opacity(0.6), radius: 10)
                .accessibilityAddTraits(.isHeader)
                .accessibilityLabel("Viksen")
            Text("IMMERSIVE AUDITORY REALITY")
                .font(.system(size: 11, weight: .regular))
                .tracking(4)
                .foregroundStyle(.white.opacity(0.75))
            HStack(spacing: 10) {
                Rectangle().fill(Color.white.opacity(0.35)).frame(width: 40, height: 0.6)
                Image(systemName: "sparkle").font(.system(size: 11)).foregroundStyle(Theme.accentGlow)
                Rectangle().fill(Color.white.opacity(0.35)).frame(width: 40, height: 0.6)
            }
            .accessibilityHidden(true)
        }
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

    private var enterButton: some View {
        VStack(spacing: 18) {
            HStack(spacing: 22) {
            if wide { Rectangle().fill(Color.white.opacity(0.35)).frame(width: 90, height: 0.6) }
            Button {
                if inDimension { selectedTab = .experience }
                else if let w = enterTarget { engine.enterWorld(w) }
                else { selectedTab = .experience }   // nothing downloaded yet: choose one
            } label: {
                ZStack {
                    Circle().fill(Color.black.opacity(0.35))
                    Circle().stroke(Theme.accentGlow, lineWidth: 2)
                        .shadow(color: Theme.accentGlow.opacity(0.9), radius: 14)
                    Image(systemName: inDimension ? "waveform" : "play.fill")
                        .font(.system(size: 34, weight: .regular))
                        .foregroundStyle(.white)
                        .offset(x: inDimension ? 0 : 3)
                        .symbolEffect(.variableColor.iterative, isActive: inDimension)
                }
                .frame(width: wide ? 112 : 104, height: wide ? 112 : 104)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(inDimension ? "Open \(engine.worldStatus.name)" : (enterTarget.map { "Enter \($0.name)" } ?? "Choose a Dimension"))
            if wide { Rectangle().fill(Color.white.opacity(0.35)).frame(width: 90, height: 0.6) }
            }
            HStack(spacing: 16) {
                if wide { Rectangle().fill(Color.white.opacity(0.3)).frame(width: 110, height: 0.6) }
                Text(inDimension ? "IN \(engine.worldStatus.name.uppercased())" : "ENTER DIMENSION")
                    .font(.system(size: 15, weight: .light))
                    .tracking(5)
                    .foregroundStyle(.white)
                    .lineLimit(1)
                    .minimumScaleFactor(0.6)
                if wide { Rectangle().fill(Color.white.opacity(0.3)).frame(width: 110, height: 0.6) }
            }
            .accessibilityHidden(true)
        }
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
            .frame(height: wide ? 126 : 122)
        }
    }

    private func cardFor(_ w: AtmosWorldInfo) -> some View {
        let st = engine.worldStatus
        let isCurrent = st.loaded && st.worldID == w.worldID
        let progress = isCurrent && st.lengthSeconds > 0 ? st.positionSeconds / st.lengthSeconds : nil
        let title = isCurrent && st.songCount > 1 ? "\(w.name) · \(st.songTitle)" : w.name
        let time = isCurrent && st.lengthSeconds > 0 ? "\(clock(st.positionSeconds)) / \(clock(st.lengthSeconds))" : nil
        return card(art: w.worldID, title: title, subtitle: isCurrent && st.playing ? (st.sectionTitle.isEmpty ? "Now playing" : st.sectionTitle) : "Continue Listening",
                    progress: progress, wideTime: time)
    }

    private func card(art: String?, title: String, subtitle: String, progress: Double?, wideTime: String?) -> some View {
        HStack(spacing: 16) {
            DimensionThumbnail(dimensionID: art)
                .frame(width: wide ? 150 : 104, height: wide ? 94 : 96)
                .clipShape(RoundedRectangle(cornerRadius: 12))
            VStack(alignment: .leading, spacing: 6) {
                if !wide { Text(subtitle).font(.footnote).foregroundStyle(.white.opacity(0.7)) }
                Text(title).font(.title3.weight(.medium)).foregroundStyle(.white).lineLimit(1).minimumScaleFactor(0.7)
                if wide, let wideTime { Text(wideTime).font(.subheadline.monospacedDigit()).foregroundStyle(.white.opacity(0.8)) }
                else if wide { Text(subtitle).font(.subheadline).foregroundStyle(.white.opacity(0.7)) }
                if let progress {
                    GeometryReader { g in
                        ZStack(alignment: .leading) {
                            Capsule().fill(Color.white.opacity(0.15))
                            Capsule().fill(Theme.accent).frame(width: g.size.width * min(1, max(0, progress)))
                        }
                    }
                    .frame(height: 4)
                }
            }
            Spacer(minLength: 0)
            Image(systemName: "chevron.right").foregroundStyle(.white.opacity(0.7))
        }
        .padding(12)
        .background(.ultraThinMaterial.opacity(0.55), in: RoundedRectangle(cornerRadius: 20))
        .overlay(RoundedRectangle(cornerRadius: 20).stroke(Color.white.opacity(0.12), lineWidth: 1))
        .contentShape(RoundedRectangle(cornerRadius: 20))
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
        HStack(spacing: 14) {
            if wide { Rectangle().fill(Color.white.opacity(0.3)).frame(width: 90, height: 0.6) }
            Image(systemName: Lunar.symbol(engine.meters.moonPhaseId)).font(wide ? .title2 : .title3)
            Text("\(Lunar.phaseName(engine.meters.moonPhaseId))  ·  Lunar day \(engine.meters.lunarDay)")
                .font(wide ? .title3 : .subheadline)
            if wide { Rectangle().fill(Color.white.opacity(0.3)).frame(width: 90, height: 0.6) }
        }
        .foregroundStyle(.white.opacity(0.85))
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
