import SwiftUI

@main
struct ViksenApp: App {
    @State private var engine = EngineController()
    @State private var library = MediaLibrary()
    @AppStorage("appearance") private var appearance: Appearance = .dark
    @State private var showWelcome = true

    var body: some Scene {
        WindowGroup {
            Group {
                if showWelcome {
                    WelcomeView { withAnimation(.easeInOut(duration: 0.4)) { showWelcome = false } }
                        .transition(.opacity)
                } else {
                    RootView()
                        .environment(engine)
                        .environment(library)
                        .preferredColorScheme(appearance.colorScheme)
                        .transition(.opacity)
                }
            }
            .tint(Theme.accent)
        }
    }
}
