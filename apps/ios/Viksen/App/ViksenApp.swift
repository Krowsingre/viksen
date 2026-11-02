import SwiftUI
#if os(iOS)
import UIKit
#else
import AppKit
#endif

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
        #if os(iOS)
        NotificationCenter.default.addObserver(forName: UIApplication.didReceiveMemoryWarningNotification,
                                               object: nil, queue: .main) { _ in AppLog.warning("Memory warning") }
        #endif
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
            #if os(macOS)
            // Quitting a Mac app does not pass through .background: mark the clean stop here.
            .onReceive(NotificationCenter.default.publisher(for: NSApplication.willTerminateNotification)) { _ in
                engine.appDidEnterBackground()
            }
            #endif
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
