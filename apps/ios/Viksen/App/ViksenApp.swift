import UIKit
import SwiftUI

@main
struct ViksenApp: App {
    @State private var engine = EngineController()
    @State private var library = MediaLibrary()
    @State private var store = Store()
    @AppStorage("appearance") private var appearance: Appearance = .dark
    @State private var showSplash = true
    @Environment(\.scenePhase) private var scenePhase

    init() {
        AppLog.info(AppLog.launchBanner())
        NotificationCenter.default.addObserver(forName: UIApplication.didReceiveMemoryWarningNotification,
                                               object: nil, queue: .main) { _ in AppLog.warning("Memory warning") }
    }

    var body: some Scene {
        WindowGroup {
            ZStack {
                RootView()
                    .environment(engine)
                    .environment(library)
                    .environment(store)
                    .preferredColorScheme(appearance.colorScheme)
                if showSplash {
                    SplashView { withAnimation(.easeInOut(duration: 0.4)) { showSplash = false } }
                        .preferredColorScheme(.dark)
                        .transition(.opacity)
                        .zIndex(1)
                }
            }
            .tint(Theme.accent)
            .onAppear {
                store.onTierChange = { [engine] tier in engine.enforce(tier) }
                engine.beginRunTracking()
            }
            .onChange(of: scenePhase) { _, phase in
                switch phase {
                case .background: engine.appDidEnterBackground()
                case .active: engine.appWillEnterForeground()
                default: break
                }
            }
        }
    }
}
