import SwiftUI

@main
struct ViksenApp: App {
    @State private var engine = EngineController()
    @State private var library = MediaLibrary()
    @AppStorage("appearance") private var appearance: Appearance = .dark

    var body: some Scene {
        WindowGroup {
            RootView()
                .environment(engine)
                .environment(library)
                .preferredColorScheme(appearance.colorScheme)
                .tint(Theme.accent)
        }
    }
}
