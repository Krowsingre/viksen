import SwiftUI

/// iPhone navigation is Experience-first (spec §12).
struct RootView: View {
    @State private var tab: Tab = .experience

    enum Tab: Hashable { case experience, studio, worlds, library, settings }

    var body: some View {
        TabView(selection: $tab) {
            ExperienceView()
                .tabItem { Label("Experience", systemImage: "waveform.circle") }
                .tag(Tab.experience)
            StudioView()
                .tabItem { Label("Studio", systemImage: "slider.horizontal.3") }
                .tag(Tab.studio)
            WorldsView()
                .tabItem { Label("Worlds", systemImage: "globe.americas") }
                .tag(Tab.worlds)
            LibraryView()
                .tabItem { Label("Library", systemImage: "square.stack") }
                .tag(Tab.library)
            SettingsView()
                .tabItem { Label("Settings", systemImage: "gearshape") }
                .tag(Tab.settings)
        }
    }
}
