import SwiftUI

/// Home, after the owner's concept (2026-11-04): the night sky, the wordmark (with
/// "After Dark" at night), the fox in the owner's moon ring (tonight's moon glows from
/// behind; the play fox starts the music, the waveform fox stops it), the lunar day, and
/// a Now Playing card. The menu button opens every other screen.
///
/// Each part is its own view reading only what it shows, so the 30 Hz meter updates
/// redraw the waveform and the card, never the whole page.
struct HomeView: View {
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav
    @Environment(\.horizontalSizeClass) private var sizeClass

    private var wide: Bool { sizeClass == .regular }
    static let lavender = Color(red: 0.90, green: 0.86, blue: 1)

    var body: some View {
        GeometryReader { geo in
            let ringWidth = min(geo.size.width - 24, wide ? 460 : 420, geo.size.height * 0.46)
            let theme = DimensionTheme.for(engine.activeDimensionID)
            ZStack(alignment: .top) {
                HomeSkyBackground(theme: theme).ignoresSafeArea()
                    .animation(.easeInOut(duration: 1.2), value: theme)
                VStack(spacing: 0) {
                    Spacer(minLength: wide ? 24 : 12)
                    HomeWordmark(wide: wide)
                        .onGeometryChange(for: CGFloat.self) { $0.frame(in: .global).maxY } action: { nav.headerBottom = $0 }
                    Spacer(minLength: 10)
                    HomeFoxRing(width: ringWidth)
                    Spacer(minLength: 10)
                    LunarDayLine(wide: wide)
                    Spacer(minLength: 14)
                    if engine.previousRunEndedUnexpectedly { RestoreCard().padding(.horizontal, 24).padding(.bottom, 12) }
                    NowPlayingCard()
                        .frame(maxWidth: wide ? 640 : .infinity)
                        .padding(.horizontal, wide ? 48 : 20)
                    RoomBlendToggle().padding(.top, 10)
                    Spacer(minLength: wide ? 24 : 12)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .overlay(alignment: .topTrailing) {
            MenuButton().padding(.trailing, 18).padding(.top, 4)
        }
        .preferredColorScheme(.dark)
    }
}

// MARK: Wordmark

private struct HomeWordmark: View {
    var wide: Bool
    @Environment(EngineController.self) private var engine
    private let lavender = HomeView.lavender

    var body: some View {
        let night = engine.nightClock.isNight
        VStack(spacing: 8) {
            Text("VIKSEN")
                .font(.system(size: wide ? 44 : 38, weight: .light, design: .serif))
                .tracking(wide ? 18 : 14)
                .foregroundStyle(lavender)
                .shadow(color: Theme.accentGlow.opacity(0.55), radius: 10)
                .accessibilityAddTraits(.isHeader)
                .accessibilityLabel(night ? "Viksen After Dark" : "Viksen")
            if night {
                Text("AFTER DARK")
                    .font(.system(size: 13, weight: .regular, design: .serif))
                    .tracking(7)
                    .foregroundStyle(Theme.gold)
                    .shadow(color: Theme.gold.opacity(0.5), radius: 6)
                    .accessibilityHidden(true)
                    .transition(.opacity)
            }
            Text("IMMERSIVE AUDITORY REALITY")
                .font(.system(size: 10, weight: .regular, design: .serif))
                .tracking(2.6)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
                .foregroundStyle(lavender.opacity(0.8))
            HStack(spacing: 8) {
                Rectangle().fill(lavender.opacity(0.35)).frame(width: 56, height: 0.6)
                ForEach(["moonphase.waxing.crescent", "moonphase.first.quarter", "moonphase.waning.crescent",
                         "moonphase.last.quarter"], id: \.self) { Image(systemName: $0).font(.system(size: 9)) }
                Rectangle().fill(lavender.opacity(0.35)).frame(width: 56, height: 0.6)
            }
            .foregroundStyle(lavender.opacity(0.8))
            .accessibilityHidden(true)
        }
        .animation(.easeInOut(duration: 1.2), value: night)
    }
}

// MARK: Fox and moon ring

/// The owner's moon ring with the fox at its heart. Tonight's moon glows from behind.
private struct HomeFoxRing: View {
    var width: CGFloat
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav

    /// The Dimension the play fox starts: the one on the Home card, else Viksen.
    private var target: AtmosWorldInfo? {
        let chosen = engine.worlds.first { $0.worldID == engine.lastDimensionID }
        if let chosen, engine.isPlayable(chosen) { return chosen }
        if chosen != nil { return nil }   // chosen but not downloaded: the fox opens Dimensions
        let playable = engine.worlds.filter(engine.isPlayable)
        return playable.first { $0.worldID == EngineController.coreDimensionID } ?? playable.first
    }

    var body: some View {
        let height = width * 1199 / 1312   // the ring artwork's aspect
        let playing = engine.activeDimensionID != nil
        let size = width * 1.08
        // The fox, its button and the moon ring take the playing Dimension's colour.
        let theme = DimensionTheme.for(engine.activeDimensionID)
        // Glow and fox are layered on the ring artwork's own frame, so the glow lines up
        // with the ring's moons and the (larger) fox can spill past it.
        Image("MoonRing").resizable()
            .frame(width: width, height: height)
            .hueRotation(.degrees(theme.hueShift))
            .background {
                MoonRingPulse(phaseID: engine.moonPhaseID, glow: theme.glow, ringWidth: width, ringHeight: height)
                    .frame(width: width, height: height)
            }
            .overlay {
                Button {
                    if playing { engine.leaveWorld() }                 // the waveform fox stops the music
                    else if let w = target { engine.enterWorld(w) }    // the play fox starts it
                    else { nav.go(.experience) }                       // nothing downloaded yet
                } label: {
                    ZStack {
                        Image("FoxEmpty").resizable().scaledToFit()
                        Group {
                            if playing {
                                FoxWaveform().frame(width: size * 0.088, height: size * 0.072)
                            } else {
                                PlayGlyph().frame(width: size * 0.052, height: size * 0.058)
                                    .offset(x: size * 0.006)
                            }
                        }
                        .position(x: size * 491 / 1024, y: size * 535 / 1024)
                    }
                    .frame(width: size, height: size)
                    .hueRotation(.degrees(theme.hueShift))
                    .contentShape(Circle().scale(0.5))
                }
                .buttonStyle(.plain)
                .offset(y: height * (0.495 - 0.5))
                .accessibilityLabel(playing ? "Stop" : (target.map { "Play \($0.name)" } ?? "Choose a Dimension"))
            }
            .animation(.easeInOut(duration: 1.2), value: theme.hueShift)
    }
}

/// Reads the meters itself, so only the bars redraw at 30 Hz.
private struct FoxWaveform: View {
    @Environment(EngineController.self) private var engine
    var body: some View {
        LiveWaveform(level: Double(max(engine.meters.outputPeakLeft, engine.meters.outputPeakRight)))
    }
}

// MARK: Lunar line (owner's concept, 2026-11-05)

/// "☾ LUNAR DAY 26", tonight's phase, and what is shaping the song right now: the time of
/// day and the listener's state ("Night · You are still"). Plain text, not a button.
private struct LunarDayLine: View {
    var wide: Bool
    @Environment(EngineController.self) private var engine
    private let lavender = HomeView.lavender

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 14) {
                Rectangle().fill(lavender.opacity(0.35)).frame(width: 64, height: 0.6)
                Image(systemName: Lunar.symbol(engine.moonPhaseID)).font(.system(size: 18)).foregroundStyle(Theme.gold)
                Text("LUNAR DAY \(engine.lunarDay)").font(.system(size: 13, design: .serif)).tracking(3)
                Rectangle().fill(lavender.opacity(0.35)).frame(width: 64, height: 0.6)
            }
            .foregroundStyle(lavender)
            Text(Lunar.phaseName(engine.moonPhaseID))
                .font(.system(size: wide ? 34 : 30, design: .serif)).foregroundStyle(.white)
            HStack(spacing: 14) {
                Image(systemName: "sparkle").font(.system(size: 9))
                ContextLine()
                Image(systemName: "sparkle").font(.system(size: 9))
            }
            .foregroundStyle(lavender.opacity(0.85))
        }
        .frame(maxWidth: .infinity)
        .multilineTextAlignment(.center)
        .accessibilityElement(children: .combine)
    }
}

/// The song's current context: time of day, then the listener's state (the reason the
/// music is in its current part). Reads the status itself (it changes often).
private struct ContextLine: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        let st = engine.worldStatus
        let parts = [Self.timeOfDay(night: engine.nightClock.isNight),
                     st.playing && !st.composed ? st.sectionLabel : ""].filter { !$0.isEmpty }
        Text(parts.joined(separator: " · "))
            .font(.system(size: 17, design: .serif)).italic()
            .lineLimit(1).minimumScaleFactor(0.8)
    }

    static func timeOfDay(night: Bool) -> String {
        if night { return "Night" }
        let h = Calendar.current.component(.hour, from: Date())
        switch h {
        case 4..<12: return "Morning"
        case 12..<17: return "Afternoon"
        default: return "Evening"
        }
    }
}

// MARK: Now Playing card (owner's concept, 2026-11-05)

/// Art, a label, the song title, progress, and the album link. After dark, before
/// tonight's phase is discovered here, the progress is tonight's discovery (x of 3:00);
/// otherwise it is the song's own progress. Tapping the card opens Dimensions; the album
/// row opens Apple Music.
private struct NowPlayingCard: View {
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var cardHeights: [String: CGFloat] = [:]

    var body: some View {
        // Swipe through unlocked Dimensions (owner, 2026-11-05): only once at least one
        // besides the built-in Viksen is unlocked. The card shown is what the fox plays.
        let unlocked = engine.worlds.filter { store.hasAccess($0) }
            .sorted { ($0.worldID == EngineController.coreDimensionID ? 0 : 1) < ($1.worldID == EngineController.coreDimensionID ? 0 : 1) }
        let core = unlocked.first { $0.worldID == EngineController.coreDimensionID } ?? unlocked.first
        if unlocked.count > 1 {
            let selected = engine.lastDimensionID.flatMap { id in unlocked.contains { $0.worldID == id } ? id : nil }
                ?? core?.worldID ?? ""
            // Room for the tallest card is kept at all times, so nothing above the card moves
            // when it changes size; the card hangs from the top of that room and the dots sit
            // just under it (owner, 2026-11-05).
            let tallest = cardHeights.values.max() ?? 124
            VStack(spacing: 12) {
                // Swiping past either end loops round to the other (owner, 2026-11-05).
                LoopingPager(ids: unlocked.map(\.worldID), selected: selected,
                             select: { engine.selectDimension($0) }) { id in
                    DimensionCard(world: unlocked.first { $0.worldID == id })
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.horizontal, 2)
                }
                .frame(height: cardHeights[selected] ?? 124)
                .animation(.easeInOut(duration: 0.25), value: cardHeights[selected])
                HStack(spacing: 8) {
                    ForEach(unlocked, id: \.worldID) { w in
                        Circle().fill(Color.white.opacity(w.worldID == selected ? 0.9 : 0.3)).frame(width: 7, height: 7)
                    }
                }
                .accessibilityHidden(true)
            }
            .frame(height: tallest + 19, alignment: .top)
            .background(alignment: .top) {
                // Measures every card (not just those swiped to), invisibly, so the room for
                // the tallest is known from the start.
                ZStack {
                    ForEach(unlocked, id: \.worldID) { w in
                        DimensionCard(world: w)
                            .fixedSize(horizontal: false, vertical: true)
                            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { cardHeights[w.worldID] = $0 }
                    }
                }
                .hidden()
                .accessibilityHidden(true)
            }
        } else {
            DimensionCard(world: core)
        }
    }
}

/// Pages through the Dimension cards and loops: past the last comes the first, and before
/// the first the last. The neighbours sit either side and follow the finger.
private struct LoopingPager<Content: View>: View {
    let ids: [String]
    let selected: String
    let select: (String) -> Void
    @ViewBuilder let content: (String) -> Content
    @State private var drag: CGFloat = 0
    @State private var settling = false

    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width
            let n = ids.count
            let i = ids.firstIndex(of: selected) ?? 0
            ZStack(alignment: .top) {
                ForEach([-1, 0, 1], id: \.self) { k in
                    content(ids[(i + k + n) % n])
                        .frame(width: w, alignment: .top)
                        .offset(x: CGFloat(k) * w + drag)
                        .accessibilityHidden(k != 0)
                }
            }
            .frame(width: w, height: geo.size.height, alignment: .top)
            .contentShape(Rectangle())
            .highPriorityGesture(
                DragGesture(minimumDistance: 14)
                    .onChanged { v in if !settling { drag = v.translation.width } }
                    .onEnded { v in
                        guard !settling else { return }
                        let t = v.translation.width, p = v.predictedEndTranslation.width
                        let step = (t < -w * 0.25 || p < -w * 0.5) ? 1 : (t > w * 0.25 || p > w * 0.5) ? -1 : 0
                        turn(step, width: w)
                    })
            .accessibilityAction(named: "Next Dimension") { turn(1, width: w) }
            .accessibilityAction(named: "Previous Dimension") { turn(-1, width: w) }
        }
        .clipped()
    }

    private func turn(_ step: Int, width: CGFloat) {
        guard step != 0, ids.count > 1 else {
            withAnimation(.spring(duration: 0.3)) { drag = 0 }
            return
        }
        let n = ids.count
        let i = ids.firstIndex(of: selected) ?? 0
        settling = true
        withAnimation(.easeOut(duration: 0.22)) {
            drag = -CGFloat(step) * width
        } completion: {
            var t = Transaction()
            t.disablesAnimations = true
            withTransaction(t) {
                select(ids[(i + step + n) % n])
                drag = 0
            }
            settling = false
        }
    }
}

/// One Dimension's card: art, label, title, progress and the album link.
private struct DimensionCard: View {
    let world: AtmosWorldInfo?
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav
    private let lavender = HomeView.lavender

    /// Every card takes the colour of the active Dimension's badge (the one playing;
    /// Viksen's purple when none is), owner 2026-11-05, and stays see-through.
    private var theme: DimensionTheme { .for(engine.activeDimensionID) }

    var body: some View {
        let st = engine.worldStatus
        let active = world != nil && st.loaded && st.worldID == world?.worldID
        VStack(spacing: 0) {
            HStack(spacing: 16) {
                // The badge switches to this Dimension when it is not the one playing
                // (owner, 2026-11-05); the rest of the card opens Dimensions.
                Button { switchHere() } label: {
                    DimensionThumbnail(dimensionID: world?.worldID)
                        .frame(width: 84, height: 84)
                        .clipShape(RoundedRectangle(cornerRadius: 14))
                        .overlay(RoundedRectangle(cornerRadius: 14).stroke(lavender.opacity(0.3), lineWidth: 1))
                }
                .buttonStyle(.plain)
                .accessibilityLabel(world.map { engine.activeDimensionID == $0.worldID ? $0.name : "Switch to \($0.name)" } ?? "Choose a Dimension")
                Button { nav.go(.experience) } label: {
                    HStack(spacing: 16) {
                        if let world {
                            CardProgress(dimensionID: world.worldID, dimensionName: world.name, active: active)
                        } else {
                            VStack(alignment: .leading, spacing: 4) {
                                Text("BEGIN").font(.system(size: 11, design: .serif)).tracking(2.5).foregroundStyle(lavender.opacity(0.75))
                                Text("Choose a Dimension").font(.system(size: 21, design: .serif)).foregroundStyle(.white)
                            }
                        }
                        Spacer(minLength: 0)
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            .padding(16)
            // Album line only for albums live on Apple Music (owner, 2026-11-05).
            if let world, !world.album.isEmpty, engine.albums.link(for: world) != nil {
                Rectangle().fill(lavender.opacity(0.18)).frame(height: 0.6).padding(.horizontal, 16)
                AlbumRow(world: world)
            }
        }
        // Still see-through, tinted with the active Dimension's badge colour.
        .background(theme.card.opacity(0.55), in: RoundedRectangle(cornerRadius: 26))
        .background(.ultraThinMaterial.opacity(0.35), in: RoundedRectangle(cornerRadius: 26))
        .overlay(RoundedRectangle(cornerRadius: 26).stroke(theme.edge.opacity(0.3), lineWidth: 1))
        .animation(.easeInOut(duration: 1.0), value: theme)
    }
}

extension DimensionCard {
    /// Badge tap: play this Dimension if another one (or none) is playing; a Dimension not
    /// yet downloaded opens Dimensions instead.
    fileprivate func switchHere() {
        guard let world else { nav.go(.experience); return }
        guard engine.activeDimensionID != world.worldID else { return }
        if engine.isPlayable(world) { engine.selectDimension(world.worldID); engine.enterWorld(world) }
        else { nav.go(.experience) }
    }
}

/// Label, song title and the progress line of the card.
private struct CardProgress: View {
    var dimensionID: String
    var dimensionName: String
    var active: Bool
    @Environment(EngineController.self) private var engine
    private let lavender = HomeView.lavender

    var body: some View {
        let st = engine.worldStatus
        let progress = engine.progress
        let phase = engine.moonPhaseID
        let discovered = progress.discoveredPhases(dimensionID).contains(phase)
        let discovering = engine.nightClock.isNight && !discovered
        let playing = active && st.playing
        // Nothing playing: "DIMENSION" and the Dimension's name (owner, 2026-11-05).
        let label = !playing ? "DIMENSION"
            : (discovering ? "TONIGHT'S DISCOVERY" : (discovered && engine.nightClock.isNight ? "DISCOVERED TONIGHT" : "NOW PLAYING"))
        let title = playing && !st.songTitle.isEmpty ? st.songTitle : dimensionName
        let fraction: Double, detail: String
        if discovering {
            let heard = progress.phaseProgress(dimensionID) * 180
            fraction = heard / 180
            detail = "\(Self.clock(heard)) of 3:00"
        } else if active, st.lengthSeconds > 0 {
            fraction = st.positionSeconds / st.lengthSeconds
            detail = "\(Self.clock(st.positionSeconds)) of \(Self.clock(st.lengthSeconds))"
        } else {
            fraction = discovered ? 1 : 0
            detail = discovered ? "\(Lunar.phaseName(phase)) discovered" : ""
        }
        return VStack(alignment: .leading, spacing: 5) {
            Text(label).font(.system(size: 11, design: .serif)).tracking(2.5).foregroundStyle(lavender.opacity(0.75))
                .lineLimit(1).minimumScaleFactor(0.8)
            Text(title).font(.system(size: 21, design: .serif)).foregroundStyle(.white)
                .lineLimit(1).minimumScaleFactor(0.7)
            if !detail.isEmpty {
                Text(detail).font(.system(size: 16, design: .serif).monospacedDigit()).foregroundStyle(lavender.opacity(0.9))
            }
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.white.opacity(0.15))
                    Capsule().fill(Color(red: 0.70, green: 0.60, blue: 1))
                        .frame(width: g.size.width * min(1, max(0, fraction)))
                }
            }
            .frame(height: 5)
            .padding(.top, 3)
        }
        .accessibilityElement(children: .combine)
    }

    static func clock(_ s: Double) -> String {
        let t = Int(max(0, s))
        return String(format: "%d:%02d", t / 60, t % 60)
    }
}

/// "From <album> · Apple Music ↗".
private struct AlbumRow: View {
    var world: AtmosWorldInfo
    @Environment(EngineController.self) private var engine
    @Environment(\.openURL) private var openURL
    private let lavender = HomeView.lavender

    var body: some View {
        let url = engine.albums.link(for: world)
        Button { if let url { openURL(url) } } label: {
            HStack(spacing: 10) {
                Image(systemName: "square.stack.3d.up").foregroundStyle(lavender.opacity(0.8))
                (Text("From \(world.album)").foregroundColor(lavender.opacity(0.85))
                 + Text(url != nil ? " · Apple Music" : "").foregroundColor(Theme.gold))
                    .font(.system(size: 15, design: .serif))
                    .lineLimit(1).minimumScaleFactor(0.7)
                Spacer(minLength: 0)
                if url != nil { Image(systemName: "arrow.up.right.square").foregroundStyle(lavender.opacity(0.85)) }
            }
            .padding(.horizontal, 18).frame(minHeight: 48)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(url == nil)
        .accessibilityLabel(url != nil ? "From \(world.album). Open in Apple Music" : "From \(world.album)")
    }
}

// MARK: Room blend

/// "Blend the room": shown only with headphones in.
private struct RoomBlendToggle: View {
    @Environment(EngineController.self) private var engine
    private let lavender = HomeView.lavender

    var body: some View {
        if engine.onHeadphones {
            let on = engine.roomBlendOn
            Button { engine.setRoomBlend(!on) } label: {
                HStack(spacing: 8) {
                    Image(systemName: on ? "waveform.and.mic" : "mic.slash")
                    Text(on ? "Room blended in" : "Blend the room")
                        .font(.system(size: 15, design: .serif))
                }
                .foregroundStyle(on ? .white : lavender.opacity(0.85))
                .padding(.horizontal, 16).frame(minHeight: 40)
                .background(on ? Theme.accent.opacity(0.35) : Color.white.opacity(0.06), in: Capsule())
                .overlay(Capsule().stroke(lavender.opacity(on ? 0.6 : 0.3), lineWidth: 1))
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Blend the room in")
            .accessibilityValue(on ? "On" : "Off")
        }
    }
}

// MARK: Restore

private struct RestoreCard: View {
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("Viksen closed unexpectedly", systemImage: "arrow.counterclockwise.circle")
                .font(.subheadline.weight(.semibold)).foregroundStyle(.white)
            Text("Play your last Dimension again? The microphone stays off until you turn it on.")
                .font(.caption).foregroundStyle(.white.opacity(0.7))
            HStack {
                Button("Play again") { engine.restoreLastSession() }
                    .buttonStyle(.borderedProminent)
                Button("Not now") { engine.dismissRestore() }
                    .buttonStyle(.bordered)
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.ultraThinMaterial.opacity(0.6), in: RoundedRectangle(cornerRadius: 16))
    }
}
