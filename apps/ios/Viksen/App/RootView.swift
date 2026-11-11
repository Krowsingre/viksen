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
    /// Bottom of Home's header (the wordmark block), in screen points: the menu opens
    /// below it so the header is never covered (owner, 2026-11-05).
    var headerBottom: CGFloat = 0

    /// Pages change instantly (owner: transitions felt slow); only the menu slides.
    func go(_ t: AppTab) {
        withAnimation(.easeOut(duration: 0.18)) { menuOpen = false }
        tab = t
    }
    func toggleMenu() { withAnimation(.easeOut(duration: 0.22)) { menuOpen.toggle() } }
}

struct RootView: View {
    @State private var nav = AppNavigator()
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var pip: PictureInPicture?
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        ZStack {
            #if os(iOS)
            // The Picture in Picture layer: tiny, under every page (iOS needs it on screen).
            if let pip {
                PictureInPictureHost(layer: pip.displayLayer)
                    .frame(width: 2, height: 2)
                    .accessibilityHidden(true)
            }
            #endif
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
        .onAppear {
            guard pip == nil else { return }
            let p = PictureInPicture(engine: engine)
            // Unlocked and on this device, in Home's order (Viksen first).
            p.choices = { [engine, store] in
                engine.worlds.filter { store.hasAccess($0) && engine.isPlayable($0) }
                    .sorted { ($0.worldID == EngineController.coreDimensionID ? 0 : 1) < ($1.worldID == EngineController.coreDimensionID ? 0 : 1) }
            }
            pip = p
            p.refresh()
        }
        .onChange(of: engine.activeDimensionID) { pip?.refresh() }
        .onChange(of: engine.lastDimensionID) { pip?.refresh() }
        .onChange(of: store.accessVersion) {
            // A subscription that ended (or a refund) stops a Dimension no longer included.
            if let id = engine.activeDimensionID, let w = engine.worlds.first(where: { $0.worldID == id }), !store.hasAccess(w) {
                engine.leaveWorld()
            }
            pip?.refresh()
        }
        .onChange(of: nav.tab) { pip?.refresh() }
        // Back in the app: close the floating window (iOS does not always do it itself).
        .onChange(of: scenePhase) { _, phase in
            #if os(iOS)
            if phase == .active { pip?.stopIfActive() }
            #endif
        }   // e.g. after downloading on the Dimensions page
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

/// The menu (owner's concept, 2026-11-05): a framed panel crowned with the moon, over a
/// dimmed page. Moon Chart, Dimensions, Settings, then the idle music and its volume.
struct SideMenu: View {
    @Environment(AppNavigator.self) private var nav
    static let lavender = Color(red: 0.90, green: 0.86, blue: 1)
    static let peach = Color(red: 0.98, green: 0.86, blue: 0.80)
    /// The frame image's own proportions (MenuFrame.png).
    private static let frameAspect: CGFloat = 979.0 / 1281.0

    // Every page closes back to Home, so Home is not in the menu (owner, 2026-11-05).
    private let items: [(AppTab, String, String)] = [
        (.moonChart, "Moon Chart", "moon"),
        (.experience, "Dimensions", "square.3.layers.3d"),
        (.settings, "Settings", "gearshape"),
    ]

    var body: some View {
        ZStack(alignment: .top) {
            if nav.menuOpen {
                Color.black.opacity(0.5).ignoresSafeArea()
                    .onTapGesture { nav.toggleMenu() }
                    .accessibilityLabel("Close menu")
                    .accessibilityAddTraits(.isButton)
                    .transition(.opacity)
                GeometryReader { geo in
                    // Below Home's header; shrinks a little if the screen is short.
                    let top = max(8, nav.headerBottom - geo.frame(in: .global).minY + 4)
                    let room = max(300, geo.size.height - top - 8)
                    let width = min(geo.size.width - 32, 380, room * Self.frameAspect)
                    let height = width / Self.frameAspect
                    panel(width: width, height: height)
                        .frame(maxWidth: .infinity)
                        .padding(.top, top)
                }
                .transition(.opacity.combined(with: .scale(scale: 0.96, anchor: .top)))
            }
        }
    }

    private func panel(width: CGFloat, height: CGFloat) -> some View {
        let u = width / 979   // one pixel of the frame image
        return ZStack(alignment: .top) {
            Image("MenuFrame").resizable().interpolation(.high)
                .frame(width: width, height: height)
                .accessibilityHidden(true)
            VStack(spacing: 0) {
                HStack {
                    Spacer()
                    Button { nav.toggleMenu() } label: {
                        Image(systemName: "xmark").font(.system(size: 22, weight: .light))
                            .foregroundStyle(Self.lavender)
                            .frame(width: 48, height: 48)
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Close menu")
                }
                .frame(height: 70 * u)
                .padding(.top, 145 * u)
                VStack(spacing: 24 * u) {
                    ForEach(items, id: \.0) { tab, title, icon in
                        MenuRow(title: title, icon: icon, glowing: tab == .moonChart, height: 150 * u) { nav.go(tab) }
                    }
                }
                .padding(.top, 45 * u)
                StarDivider()
                    .padding(.vertical, 38 * u)
                IdleMusicControls(rowHeight: 140 * u)
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 62 * u)
            .frame(width: width, height: height)
        }
        .frame(width: width, height: height)
        .shadow(color: Theme.accent.opacity(0.35), radius: 24)
        .accessibilityElement(children: .contain)
        .accessibilityAddTraits(.isModal)
        .accessibilityAction(.escape) { nav.toggleMenu() }
    }
}

/// One menu row: a soft rounded tile with an icon, a serif title and a chevron.
private struct MenuRow: View {
    let title: String
    let icon: String
    var glowing = false
    let height: CGFloat
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 0) {
                Image(systemName: icon)
                    .font(.system(size: 26, weight: .light))
                    .foregroundStyle(glowing ? SideMenu.peach : SideMenu.lavender)
                    .shadow(color: glowing ? SideMenu.peach.opacity(0.8) : .clear, radius: 6)
                    .frame(width: 44)
                Text(title)
                    .font(.system(size: 24, design: .serif))
                    .foregroundStyle(SideMenu.lavender)
                    .lineLimit(1).minimumScaleFactor(0.7)
                    .padding(.leading, 18)
                Spacer(minLength: 8)
                Image(systemName: "chevron.right")
                    .font(.system(size: 17, weight: .light))
                    .foregroundStyle(SideMenu.lavender.opacity(0.85))
            }
            .padding(.horizontal, 18)
            .frame(maxWidth: .infinity, minHeight: max(48, height))
            .background(Color.white.opacity(0.05), in: RoundedRectangle(cornerRadius: 22, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 22, style: .continuous).stroke(SideMenu.lavender.opacity(0.13), lineWidth: 1))
            .contentShape(Rectangle())
        }
        .buttonStyle(MenuPressStyle())
    }
}

private struct MenuPressStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .brightness(configuration.isPressed ? 0.08 : 0)
            .scaleEffect(configuration.isPressed ? 0.985 : 1)
    }
}

/// A thin line with a four-pointed star in the middle.
private struct StarDivider: View {
    var body: some View {
        HStack(spacing: 10) {
            line(leading: true)
            Image(systemName: "sparkle").font(.system(size: 14)).foregroundStyle(Theme.accent.opacity(0.9))
                .shadow(color: Theme.accent.opacity(0.8), radius: 4)
            line(leading: false)
        }
        .accessibilityHidden(true)
    }

    private func line(leading: Bool) -> some View {
        Rectangle()
            .fill(LinearGradient(colors: [SideMenu.lavender.opacity(0), SideMenu.lavender.opacity(0.35)],
                                 startPoint: leading ? .leading : .trailing, endPoint: leading ? .trailing : .leading))
            .frame(height: 1)
    }
}

/// At the bottom of the menu: the idle (opening) music on or off, and its own volume
/// (separate from the Dimensions).
private struct IdleMusicControls: View {
    var rowHeight: CGFloat
    @Environment(EngineController.self) private var engine
    @AppStorage(ThresholdSound.settingKey) private var on = false
    @AppStorage(ThresholdSound.volumeKey) private var volume: Double = 0.55
    private static let toggleTint = Color(red: 0.55, green: 0.42, blue: 0.95)

    var body: some View {
        VStack(spacing: 0) {
            Toggle(isOn: $on) {
                HStack(spacing: 18) {
                    Image(systemName: "music.note").font(.system(size: 26, weight: .light)).frame(width: 44)
                    Text("Idle Music").font(.system(size: 22, design: .serif))
                        .lineLimit(1).minimumScaleFactor(0.7)
                }
                .foregroundStyle(SideMenu.lavender)
            }
            .tint(Self.toggleTint)
            .onChange(of: on) { _, isOn in
                if isOn { engine.threshold.start(engineAudioRunning: engine.audioRunning) }
                else { engine.threshold.fadeOut(seconds: 0.5) }
            }
            .frame(minHeight: max(48, rowHeight))
            Rectangle().fill(SideMenu.lavender.opacity(0.1)).frame(height: 1)
            HStack(spacing: 12) {
                Image(systemName: "speaker.fill").font(.system(size: 17)).frame(width: 30)
                Slider(value: $volume, in: 0...1)
                    .tint(Self.toggleTint)
                    .onChange(of: volume) { _, v in engine.threshold.setVolume(Float(v)) }
                    .accessibilityLabel("Idle music volume")
                Image(systemName: "speaker.wave.3.fill").font(.system(size: 17))
            }
            .foregroundStyle(SideMenu.lavender.opacity(0.85))
            .frame(minHeight: max(44, rowHeight * 0.95))
            .disabled(!on)
            .opacity(on ? 1 : 0.45)
        }
        .padding(.horizontal, 18)
        .background(Color.white.opacity(0.05), in: RoundedRectangle(cornerRadius: 22, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 22, style: .continuous).stroke(SideMenu.lavender.opacity(0.13), lineWidth: 1))
    }
}
