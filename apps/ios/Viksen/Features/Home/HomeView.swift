import SwiftUI

/// Home tab in the style of the owner-selected concept: violet light wave, ring and
/// wordmark, then real entry points. Every control here does something real.
struct HomeView: View {
    @Binding var selectedTab: AppTab
    @Environment(EngineController.self) private var engine
    @State private var showSettings = false
    @State private var showAbout = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 0) {
                    hero
                    VStack(alignment: .leading, spacing: 12) {
                        Text(greeting)
                            .font(.title2.weight(.semibold))
                            .foregroundStyle(.white)
                        Text("Create, enhance, transform, or simply explore.")
                            .font(.subheadline)
                            .foregroundStyle(.white.opacity(0.7))
                        destination("Experience", "Immerse in sound, enhance a song, or transform your surroundings.",
                                    "play.fill", .experience)
                        destination("Studio", "Layers, spatial audio and effects (in development).",
                                    "slider.horizontal.3", .studio)
                        destination("Worlds", "Music that re-mixes itself around you.",
                                    "square.stack.3d.up", .worlds)
                        if let name = engine.trackName {
                            nowPlaying(name)
                        }
                        Label("\(Lunar.phaseName(engine.meters.moonPhaseId)) · lunar day \(engine.meters.lunarDay)",
                              systemImage: Lunar.symbol(engine.meters.moonPhaseId))
                            .font(.footnote)
                            .foregroundStyle(.white.opacity(0.6))
                            .padding(.top, 4)
                        Button("About Viksen") { showAbout = true }
                            .font(.footnote)
                            .underline()
                            .padding(.top, 4)
                    }
                    .padding(.horizontal, 20)
                    .padding(.bottom, 32)
                }
            }
            .background(Theme.background.ignoresSafeArea())
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button { showSettings = true } label: { Image(systemName: "gearshape") }
                        .accessibilityLabel("Settings")
                }
            }
            .toolbarBackground(.hidden, for: .navigationBar)
            .sheet(isPresented: $showSettings) { SettingsView() }
            .sheet(isPresented: $showAbout) { AboutView() }
        }
        .preferredColorScheme(.dark)
    }

    private var hero: some View {
        ZStack {
            LightWave()
                .frame(height: 220)
                .accessibilityHidden(true)
            VStack(spacing: 22) {
                Circle()
                    .stroke(Theme.accentGlow, lineWidth: 2)
                    .frame(width: 72, height: 72)
                    .shadow(color: Theme.accentGlow.opacity(0.9), radius: 12)
                    .accessibilityHidden(true)
                Text("VIKSEN")
                    .font(.system(size: 38, weight: .ultraLight))
                    .tracking(12)
                    .foregroundStyle(.white)
                    .accessibilityAddTraits(.isHeader)
                Text("Atmospheric & Spatial Audio Studio\nAugmented Auditory Reality")
                    .font(.footnote)
                    .multilineTextAlignment(.center)
                    .foregroundStyle(.white.opacity(0.7))
                Button { selectedTab = .experience } label: {
                    Text("Get Started")
                        .font(.headline)
                        .frame(maxWidth: .infinity, minHeight: 48)
                        .background(Theme.accent, in: Capsule())
                        .foregroundStyle(.white)
                }
                .padding(.horizontal, 40)
                .accessibilityHint("Opens Experience")
            }
            .padding(.top, 40)
        }
        .padding(.bottom, 28)
    }

    private var greeting: String {
        switch Calendar.current.component(.hour, from: Date()) {
        case 5..<12: "Good Morning"
        case 12..<18: "Good Afternoon"
        default: "Good Evening"
        }
    }

    private func destination(_ title: String, _ subtitle: String, _ icon: String, _ tab: AppTab) -> some View {
        Button { selectedTab = tab } label: {
            HStack(spacing: 14) {
                Image(systemName: icon)
                    .font(.title3)
                    .frame(width: 44, height: 44)
                    .background(Theme.accent.opacity(0.25), in: RoundedRectangle(cornerRadius: 12))
                VStack(alignment: .leading, spacing: 2) {
                    Text(title).font(.headline).foregroundStyle(.white)
                    Text(subtitle).font(.caption).foregroundStyle(.white.opacity(0.65))
                }
                Spacer()
                Image(systemName: "chevron.right").foregroundStyle(.white.opacity(0.4))
            }
            .padding(14)
            .background(Theme.card, in: RoundedRectangle(cornerRadius: 16))
            .overlay(RoundedRectangle(cornerRadius: 16).stroke(Theme.accent.opacity(0.35), lineWidth: 1))
        }
        .buttonStyle(.plain)
    }

    private func nowPlaying(_ name: String) -> some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text("Continue Listening").font(.caption).foregroundStyle(.white.opacity(0.6))
                Text(name).font(.subheadline).foregroundStyle(.white).lineLimit(1)
            }
            Spacer()
            Button { engine.togglePlay() } label: {
                Image(systemName: engine.meters.playing ? "pause.circle.fill" : "play.circle.fill")
                    .font(.largeTitle)
                    .foregroundStyle(Theme.accent)
            }
            .accessibilityLabel(engine.meters.playing ? "Pause" : "Play")
        }
        .padding(14)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 16))
    }
}
