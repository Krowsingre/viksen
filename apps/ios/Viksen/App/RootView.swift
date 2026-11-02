import SwiftUI

enum AppTab: Hashable { case home, experience, studio, library }

/// Tabs: Home, Experience (Dimensions live here), Studio (only once purchased), Library.
/// Settings and About open from the gear on Home.
struct RootView: View {
    @Environment(Store.self) private var store
    @State private var tab: AppTab = .home

    var body: some View {
        TabView(selection: $tab) {
            HomeView(selectedTab: $tab)
                .tabItem { Label("Home", systemImage: "house") }
                .tag(AppTab.home)
            ExperienceView()
                .tabItem { Label("Experience", systemImage: "play.circle") }
                .tag(AppTab.experience)
            if store.tier >= .studio {
                StudioView()
                    .tabItem { Label("Studio", systemImage: "slider.horizontal.3") }
                    .tag(AppTab.studio)
            }
            LibraryView()
                .tabItem { Label("Library", systemImage: "books.vertical") }
                .tag(AppTab.library)
        }
        .onChange(of: store.tier) { _, tier in
            if tier < .studio && tab == .studio { tab = .home }
        }
    }
}
