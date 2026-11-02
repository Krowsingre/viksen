import SwiftUI

enum AppTab: Hashable { case home, experience, studio, library }

/// Tabs: Home and Dimensions (the Experience). Studio (once purchased) and Library are
/// hidden for now by owner request; flip `showMoreTabs` to bring them back.
/// Compact iPhones show the floating tab bar; wide (unfolded) layouts show the tabs
/// as a sidebar (owner's concept). Settings opens from the gear.
enum AppTabs {
    static let showMoreTabs = false
}

struct RootView: View {
    @Environment(Store.self) private var store
    @State private var tab: AppTab = .home

    var body: some View {
        TabView(selection: $tab) {
            Tab("Home", systemImage: "house", value: AppTab.home) {
                HomeView(selectedTab: $tab)
            }
            Tab("Dimensions", systemImage: "square.stack.3d.up", value: AppTab.experience) {
                ExperienceView()
            }
            if AppTabs.showMoreTabs && store.tier >= .studio {
                Tab("Studio", systemImage: "slider.horizontal.3", value: AppTab.studio) {
                    StudioView()
                }
            }
            if AppTabs.showMoreTabs {
                Tab("Library", systemImage: "books.vertical", value: AppTab.library) {
                    LibraryView()
                }
            }
        }
        .tabViewStyle(.sidebarAdaptable)
        // Swipe right from the left edge of any screen to return Home.
        .simultaneousGesture(
            DragGesture(minimumDistance: 24, coordinateSpace: .global)
                .onEnded { g in
                    guard tab != .home, g.startLocation.x < 28,
                          g.translation.width > 90, abs(g.translation.height) < 80 else { return }
                    withAnimation(.easeInOut(duration: 0.25)) { tab = .home }
                }
        )
        .onChange(of: store.tier) { _, tier in
            if tier < .studio && tab == .studio { tab = .home }
        }
    }
}
