import SwiftUI

/// The screens. No tab bar: the menu button on Home opens a side menu (Moon Chart,
/// Dimensions, Settings); every other page closes back to Home.
enum AppTab: Hashable { case home, experience, moonChart, settings }

/// Where the app is, shared by the side menu and the pages.
@MainActor
@Observable
final class AppNavigator {
    var tab: AppTab = .home
    var menuOpen = false

    /// Pages change instantly (owner: transitions felt slow); only the menu slides.
    func go(_ t: AppTab) {
        withAnimation(.easeOut(duration: 0.18)) { menuOpen = false }
        tab = t
    }
    func toggleMenu() { withAnimation(.easeOut(duration: 0.22)) { menuOpen.toggle() } }
}

struct RootView: View {
    @State private var nav = AppNavigator()

    var body: some View {
        ZStack {
            // Only the page being shown is built, so hidden pages cost nothing.
            Group {
                switch nav.tab {
                case .experience: ExperienceView()
                case .moonChart: MoonChartView(onClose: { nav.go(.home) })
                default: HomeView()
                }
            }
            SideMenu()
        }
        .environment(nav)
        .sheet(isPresented: Binding(get: { nav.tab == .settings }, set: { if !$0 && nav.tab == .settings { nav.tab = .home } })) {
            SettingsView()
        }
    }
}

/// The menu button (three lines). Opens the side menu.
struct MenuButton: View {
    @Environment(AppNavigator.self) private var nav
    var compact = false

    var body: some View {
        Button { nav.toggleMenu() } label: {
            Image(systemName: "line.3.horizontal")
                .font(compact ? .body : .title3.weight(.light))
                .foregroundStyle(Color(red: 0.86, green: 0.80, blue: 1))
                .frame(width: compact ? 36 : 48, height: compact ? 36 : 48)
                .background(Color.black.opacity(compact ? 0 : 0.25), in: Circle())
                .overlay(Circle().stroke(Color(red: 0.80, green: 0.70, blue: 1).opacity(compact ? 0 : 0.5), lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Menu")
    }
}

/// The close button every page but Home has (owner, 2026-11-04): an X with a generous
/// touch area that acts on touch-down speed (a plain button, no delay).
struct CloseButton: View {
    var compact = false   // in a navigation bar
    var action: () -> Void
    var body: some View {
        if compact {
            // In a navigation bar iOS 26 already draws a glass circle around the button:
            // just the X, or the two circles look stacked.
            Button(action: action) { Image(systemName: "xmark") }
                .accessibilityLabel("Close")
        } else {
            Button(action: action) {
                Image(systemName: "xmark").font(.system(size: 17, weight: .medium))
                    .foregroundStyle(Color(red: 0.62, green: 0.48, blue: 1))
                    .frame(width: 44, height: 44)
                    .background(Color.white.opacity(0.06), in: Circle())
                    .overlay(Circle().stroke(Color(red: 0.90, green: 0.86, blue: 1).opacity(0.3), lineWidth: 1))
                    .padding(6)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Close")
        }
    }
}

/// The side menu: slides in from the right over a dimmed page.
struct SideMenu: View {
    @Environment(AppNavigator.self) private var nav
    private static let lavender = Color(red: 0.90, green: 0.86, blue: 1)

    // Every page closes back to Home, so Home is not in the menu (owner, 2026-11-05).
    private let items: [(AppTab, String, String)] = [
        (.moonChart, "Moon Chart", "moonphase.waxing.gibbous"),
        (.experience, "Dimensions", "square.stack.3d.up"),
        (.settings, "Settings", "gearshape"),
    ]

    var body: some View {
        ZStack(alignment: .trailing) {
            if nav.menuOpen {
                Color.black.opacity(0.45).ignoresSafeArea()
                    .onTapGesture { nav.toggleMenu() }
                    .accessibilityLabel("Close menu")
                    .accessibilityAddTraits(.isButton)
                    .transition(.opacity)
                VStack(alignment: .leading, spacing: 6) {
                    Text("VIKSEN")
                        .font(.system(size: 22, weight: .light, design: .serif)).tracking(8)
                        .foregroundStyle(Self.lavender)
                        .padding(.bottom, 18)
                    ForEach(items, id: \.0) { tab, title, icon in
                        let on = nav.tab == tab
                        Button { nav.go(tab) } label: {
                            HStack(spacing: 14) {
                                Image(systemName: icon).frame(width: 26)
                                Text(title).font(.system(size: 19, design: .serif))
                                Spacer()
                            }
                            .foregroundStyle(on ? .white : Self.lavender.opacity(0.85))
                            .padding(.horizontal, 14).frame(minHeight: 50)
                            .background(on ? Theme.accent.opacity(0.28) : .clear, in: RoundedRectangle(cornerRadius: 14))
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                        .accessibilityAddTraits(on ? .isSelected : [])
                    }
                    Spacer()
                    OpeningSoundControls()
                        .padding(.bottom, 28)
                }
                .padding(.top, 70).padding(.horizontal, 18)
                .frame(width: 290)
                .frame(maxHeight: .infinity)
                .background(Color(red: 0.07, green: 0.04, blue: 0.13).opacity(0.96).ignoresSafeArea())
                .overlay(alignment: .leading) { Rectangle().fill(Self.lavender.opacity(0.15)).frame(width: 1).ignoresSafeArea() }
                .transition(.move(edge: .trailing))
                .accessibilityAddTraits(.isModal)
                .accessibilityAction(.escape) { nav.toggleMenu() }
            }
        }
    }
}

/// At the bottom of the side menu: the opening loops on or off, and their own volume
/// (separate from the Dimensions).
private struct OpeningSoundControls: View {
    @Environment(EngineController.self) private var engine
    @AppStorage(ThresholdSound.settingKey) private var on = false
    @AppStorage(ThresholdSound.volumeKey) private var volume: Double = 0.55
    private static let lavender = Color(red: 0.90, green: 0.86, blue: 1)

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Toggle(isOn: $on) {
                Label("Opening sound", systemImage: "music.note")
                    .font(.system(size: 17, design: .serif))
                    .foregroundStyle(Self.lavender)
            }
            .tint(Theme.accent)
            .onChange(of: on) { _, isOn in
                if isOn { engine.threshold.start(engineAudioRunning: engine.audioRunning) }
                else { engine.threshold.fadeOut(seconds: 0.5) }
            }
            HStack(spacing: 10) {
                Image(systemName: "speaker.fill").font(.caption)
                Slider(value: $volume, in: 0...1)
                    .tint(Theme.accent)
                    .onChange(of: volume) { _, v in engine.threshold.setVolume(Float(v)) }
                    .accessibilityLabel("Opening sound volume")
                Image(systemName: "speaker.wave.3.fill").font(.caption)
            }
            .foregroundStyle(Self.lavender.opacity(0.8))
            .disabled(!on)
            .opacity(on ? 1 : 0.4)
        }
        .padding(14)
        .background(Color.white.opacity(0.05), in: RoundedRectangle(cornerRadius: 14))
    }
}
