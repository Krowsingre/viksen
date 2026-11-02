import SwiftUI

@main
struct ViksenApp: App {
    @State private var engine = EngineController()
    @State private var library = MediaLibrary()
    @AppStorage("appearance") private var appearance: Appearance = .dark
    @State private var showSplash = true

    var body: some Scene {
        WindowGroup {
            ZStack {
                RootView()
                    .environment(engine)
                    .environment(library)
                    .preferredColorScheme(appearance.colorScheme)
                if showSplash {
                    SplashView { withAnimation(.easeInOut(duration: 0.4)) { showSplash = false } }
                        .preferredColorScheme(.dark)
                        .transition(.opacity)
                        .zIndex(1)
                }
            }
            .tint(Theme.accent)
        }
    }
}
