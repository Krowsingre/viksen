import SwiftUI

enum AppTab: Hashable { case home, experience, studio, library }

/// Tabs: Home and Dimensions (the Experience). Studio (once purchased) and Library are
/// hidden for now by owner request; flip `showMoreTabs` to bring them back.
/// Settings and About open from the gear on Home.
enum AppTabs {
    static let showMoreTabs = false
}

struct RootView: View {
    @Environment(Store.self) private var store
    @State private var tab: AppTab = .home

    var body: some View {
        TabView(selection: $tab) {
            HomeView(selectedTab: $tab)
                .tabItem { Label("Home", systemImage: "house") }
                .tag(AppTab.home)
            ExperienceView()
                .tabItem { Label("Dimensions", systemImage: "square.stack.3d.up") }
                .tag(AppTab.experience)
            if AppTabs.showMoreTabs && store.tier >= .studio {
                StudioView()
                    .tabItem { Label("Studio", systemImage: "slider.horizontal.3") }
                    .tag(AppTab.studio)
            }
            if AppTabs.showMoreTabs {
                LibraryView()
                    .tabItem { Label("Library", systemImage: "books.vertical") }
                    .tag(AppTab.library)
            }
        }
        .onChange(of: store.tier) { _, tier in
            if tier < .studio && tab == .studio { tab = .home }
        }
    }
}
