import SwiftUI

/// The screens. There is no tab bar and no swiping (owner, 2026-11-04): a menu button
/// opens a side menu with every screen. Studio and Library stay hidden.
enum AppTab: Hashable { case home, experience, moonChart, settings, studio, library }

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

    private let items: [(AppTab, String, String)] = [
        (.home, "Home", "house"),
        (.experience, "Dimensions", "square.stack.3d.up"),
        (.moonChart, "Moon Chart", "moonphase.waxing.gibbous"),
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
