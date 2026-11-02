import SwiftUI

enum AppTab: Hashable { case home, experience, studio, worlds, library }

/// Tab bar from the owner-selected concepts: Home, Experience, Studio, Dimensions,
/// Library. Settings opens from the gear on Home.
struct RootView: View {
    @State private var tab: AppTab = .home

    var body: some View {
        TabView(selection: $tab) {
            HomeView(selectedTab: $tab)
                .tabItem { Label("Home", systemImage: "house") }
                .tag(AppTab.home)
            ExperienceView()
                .tabItem { Label("Experience", systemImage: "play.circle") }
                .tag(AppTab.experience)
            StudioView()
                .tabItem { Label("Studio", systemImage: "slider.horizontal.3") }
                .tag(AppTab.studio)
            WorldsView()
                .tabItem { Label("Dimensions", systemImage: "square.stack.3d.up") }
                .tag(AppTab.worlds)
            LibraryView()
                .tabItem { Label("Library", systemImage: "books.vertical") }
                .tag(AppTab.library)
        }
    }
}
