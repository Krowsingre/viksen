import SwiftUI

/// The screens. There is no tab bar (owner, 2026-11-04): everything opens from Home by
/// swiping. Studio and Library stay hidden.
enum AppTab: Hashable { case home, experience, moonChart, settings, studio, library }

/// Swipe navigation from Home:
///   swipe left  → Dimensions        swipe right → Moon Chart
///   swipe up    → Settings          swipe down (from the top of any page) → Home
/// Dimensions also returns Home with a swipe right, the Moon Chart with a swipe left
/// (both from the top band, so dragging inside a page never leaves it). VoiceOver: the
/// escape gesture returns Home, and Home lists the pages as actions.
struct RootView: View {
    @State private var tab: AppTab = .home
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    /// Swipes that leave a page must start in its top band (title area).
    private static let topBand: CGFloat = 150

    var body: some View {
        ZStack {
            HomeView(selectedTab: $tab)
                .accessibilityHidden(tab != .home && tab != .settings)
            if tab == .experience {
                page { ExperienceView() }
                    .transition(.move(edge: .trailing))
            }
            if tab == .moonChart {
                page { MoonChartView(onClose: { go(.home) }) }
                    .transition(.move(edge: .leading))
            }
        }
        .sheet(isPresented: Binding(get: { tab == .settings }, set: { if !$0 && tab == .settings { tab = .home } })) {
            SettingsView()
        }
        .simultaneousGesture(
            DragGesture(minimumDistance: 30, coordinateSpace: .global).onEnded(swiped)
        )
    }

    private func page<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        content()
            .background(Theme.background.ignoresSafeArea())
            .zIndex(1)
            .accessibilityAction(.escape) { go(.home) }
    }

    private func go(_ t: AppTab) {
        withAnimation(reduceMotion ? .easeInOut(duration: 0.2) : .spring(response: 0.38, dampingFraction: 0.9)) { tab = t }
    }

    private func swiped(_ g: DragGesture.Value) {
        let dx = g.translation.width, dy = g.translation.height
        let horizontal = abs(dx) > 80 && abs(dx) > 1.5 * abs(dy)
        let vertical = abs(dy) > 100 && abs(dy) > 1.5 * abs(dx)
        let fromTop = g.startLocation.y < Self.topBand
        switch tab {
        case .home:
            if horizontal { go(dx < 0 ? .experience : .moonChart) }
            else if vertical && dy < 0 { go(.settings) }
        case .experience:
            // The left edge is the system's back swipe inside a Dimension's page.
            if fromTop && ((vertical && dy > 0) || (horizontal && dx > 0 && g.startLocation.x > 30)) { go(.home) }
        case .moonChart:
            if (fromTop && vertical && dy > 0) || (horizontal && dx < 0) { go(.home) }
        default:
            break
        }
    }
}
