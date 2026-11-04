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
    @Environment(\.horizontalSizeClass) private var sizeClass

    private var wide: Bool { sizeClass == .regular }
    static let lavender = Color(red: 0.90, green: 0.86, blue: 1)

    var body: some View {
        GeometryReader { geo in
            let ringWidth = min(geo.size.width - 24, wide ? 460 : 420, geo.size.height * 0.46)
            ZStack(alignment: .top) {
                HomeSkyBackground().ignoresSafeArea()
                VStack(spacing: 0) {
                    Spacer(minLength: wide ? 24 : 12)
                    HomeWordmark(wide: wide)
                    Spacer(minLength: 10)
                    HomeFoxRing(width: ringWidth)
                    Spacer(minLength: 10)
                    LunarDayLine(wide: wide)
                    Spacer(minLength: 14)
                    if engine.previousRunEndedUnexpectedly { RestoreCard().padding(.horizontal, 24).padding(.bottom, 12) }
                    NowPlayingCard()
                        .frame(maxWidth: wide ? 640 : .infinity)
                        .padding(.horizontal, wide ? 48 : 20)
                    Spacer(minLength: wide ? 24 : 12)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .overlay(alignment: .topTrailing) {
            MenuButton().padding(.trailing, 18).padding(.top, 4)
        }
        .preferredColorScheme(.dark)
        .onAppear { engine.refreshWorlds() }
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

    /// The Dimension the play fox starts: the last one entered, else the first playable.
    private var target: AtmosWorldInfo? {
        let playable = engine.worlds.filter(engine.isPlayable)
        return playable.first { $0.worldID == engine.lastDimensionID } ?? playable.first
    }

    var body: some View {
        let height = width * 1199 / 1312   // the ring artwork's aspect
        let playing = engine.activeDimensionID != nil
        let size = width * 1.08
        // Glow and fox are layered on the ring artwork's own frame, so the glow lines up
        // with the ring's moons and the (larger) fox can spill past it.
        Image("MoonRing").resizable()
            .frame(width: width, height: height)
            .background {
                MoonRingPulse(phaseID: engine.moonPhaseID, ringWidth: width, ringHeight: height)
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
                    .contentShape(Circle().scale(0.5))
                }
                .buttonStyle(.plain)
                .offset(y: height * (0.495 - 0.5))
                .accessibilityLabel(playing ? "Stop" : (target.map { "Play \($0.name)" } ?? "Choose a Dimension"))
            }
    }
}

/// Reads the meters itself, so only the bars redraw at 30 Hz.
private struct FoxWaveform: View {
    @Environment(EngineController.self) private var engine
    var body: some View {
        LiveWaveform(level: Double(max(engine.meters.outputPeakLeft, engine.meters.outputPeakRight)))
    }
}

// MARK: Lunar day

private struct LunarDayLine: View {
    var wide: Bool
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav

    var body: some View {
        Button { nav.go(.moonChart) } label: {
            VStack(spacing: 10) {
                Text("Lunar Day \(engine.lunarDay)")
                    .font(.system(size: wide ? 26 : 22, weight: .regular, design: .serif))
                HStack(spacing: 10) {
                    Rectangle().fill(HomeView.lavender.opacity(0.3)).frame(width: 110, height: 0.6)
                    Image(systemName: "sparkle").font(.system(size: 11)).foregroundStyle(Theme.gold)
                    Rectangle().fill(HomeView.lavender.opacity(0.3)).frame(width: 110, height: 0.6)
                }
                .accessibilityHidden(true)
            }
            .foregroundStyle(HomeView.lavender)
            .frame(maxWidth: .infinity)
            .multilineTextAlignment(.center)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityHint("Opens the Moon Chart")
    }
}

// MARK: Now Playing

/// The active Dimension, its song and progress, and (the suggested extra) tonight's
/// lunar discovery progress. Tapping it opens Dimensions.
private struct NowPlayingCard: View {
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav
    private let lavender = HomeView.lavender

    var body: some View {
        let st = engine.worldStatus
        let active = st.loaded && st.worldID != "studio"
        let last = engine.worlds.first { $0.worldID == (active ? st.worldID : engine.lastDimensionID) }
        Button { nav.go(.experience) } label: {
            VStack(alignment: .leading, spacing: 6) {
                Text(active && st.playing ? "NOW PLAYING" : (last != nil ? "LAST PLAYED" : "BEGIN"))
                    .font(.system(size: 11, design: .serif)).tracking(2.5)
                    .foregroundStyle(lavender.opacity(0.75))
                Text(last?.name ?? "Choose a Dimension")
                    .font(.system(size: 21, design: .serif)).foregroundStyle(.white)
                    .lineLimit(1).minimumScaleFactor(0.7)
                if active, !st.songTitle.isEmpty {
                    Text(st.songTitle).font(.system(size: 15, design: .serif)).foregroundStyle(lavender.opacity(0.85))
                        .lineLimit(1)
                } else if last == nil {
                    Text("Open Dimensions to download one").font(.system(size: 15, design: .serif))
                        .foregroundStyle(lavender.opacity(0.75))
                }
                if active, st.lengthSeconds > 0 {
                    VStack(spacing: 4) {
                        GeometryReader { g in
                            ZStack(alignment: .leading) {
                                Capsule().fill(Color.white.opacity(0.15))
                                Capsule().fill(LinearGradient(colors: [lavender, Theme.accent], startPoint: .leading, endPoint: .trailing))
                                    .frame(width: g.size.width * min(1, max(0, st.positionSeconds / st.lengthSeconds)))
                            }
                        }
                        .frame(height: 4)
                        HStack {
                            Text(Self.clock(st.positionSeconds))
                            Spacer()
                            Text(Self.clock(st.lengthSeconds))
                        }
                        .font(.caption2.monospacedDigit()).foregroundStyle(lavender.opacity(0.6))
                    }
                    .padding(.top, 4)
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("\(Self.clock(st.positionSeconds)) of \(Self.clock(st.lengthSeconds))")
                }
                if let last { TonightLine(dimensionID: last.worldID).padding(.top, 4) }
            }
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color(red: 0.12, green: 0.07, blue: 0.20).opacity(0.55), in: RoundedRectangle(cornerRadius: 24))
            .background(.ultraThinMaterial.opacity(0.35), in: RoundedRectangle(cornerRadius: 24))
            .overlay(RoundedRectangle(cornerRadius: 24).stroke(lavender.opacity(0.2), lineWidth: 1))
            .contentShape(RoundedRectangle(cornerRadius: 24))
        }
        .buttonStyle(.plain)
    }

    static func clock(_ s: Double) -> String {
        let t = Int(max(0, s))
        return String(format: "%d:%02d", t / 60, t % 60)
    }
}

/// Tonight's phase in this Dimension: discovered, how much of the 3 minutes is heard, or
/// that the moon's song waits for dark.
private struct TonightLine: View {
    var dimensionID: String
    @Environment(EngineController.self) private var engine

    var body: some View {
        let progress = engine.progress
        let phase = engine.moonPhaseID
        let name = Lunar.phaseName(phase)
        let text: String
        if progress.discoveredPhases(dimensionID).contains(phase) {
            text = "\(name) discovered here"
        } else if engine.nightClock.isNight {
            let heard = Int(progress.phaseProgress(dimensionID) * 180)
            text = "\(name) · \(heard / 60):\(String(format: "%02d", heard % 60)) of 3:00 heard"
        } else {
            text = "\(name)'s song plays after dark"
        }
        return HStack(spacing: 8) {
            Image(systemName: Lunar.symbol(phase)).foregroundStyle(Theme.gold)
            Text(text).font(.system(size: 13, design: .serif)).foregroundStyle(HomeView.lavender.opacity(0.85))
        }
        .accessibilityElement(children: .combine)
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
            Text("Restore your last session? The microphone stays off until you turn it on.")
                .font(.caption).foregroundStyle(.white.opacity(0.7))
            HStack {
                Button("Restore") { engine.restoreAutosave(); nav.go(.experience) }
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
