import Foundation
#if os(iOS)
import WatchConnectivity
#endif

/// iPhone side of WatchConnectivity: receives live heart rate from the Viksen
/// Apple Watch app during a workout and forwards it to HealthService.
@MainActor
final class WatchLink {
    #if os(iOS)
    private let proxy = WatchSessionProxy()
    #endif
    var onHeartRate: ((Double, Date) -> Void)?
    var onWorkoutStopped: (() -> Void)?

    #if os(iOS)
    func activate() {
        guard WCSession.isSupported() else { return }
        proxy.onPayload = { [weak self] payload in self?.handle(payload) }
        WCSession.default.delegate = proxy
        WCSession.default.activate()
    }

    var isWatchAppInstalled: Bool {
        WCSession.isSupported() && WCSession.default.activationState == .activated && WCSession.default.isWatchAppInstalled
    }
    #else
    func activate() {}                       // the Watch pairs with iPhone only
    var isWatchAppInstalled: Bool { false }
    #endif

    private func handle(_ payload: [String: Any]) {
        if payload["stopped"] as? Bool == true { onWorkoutStopped?(); return }
        guard let hr = payload["hr"] as? Double, let t = payload["t"] as? Double else { return }
        let date = Date(timeIntervalSince1970: t)
        // Ignore stale application-context values (e.g. from an old workout).
        guard Date().timeIntervalSince(date) < 30 else { return }
        onHeartRate?(hr, date)
    }
}

#if os(iOS)
private final class WatchSessionProxy: NSObject, WCSessionDelegate {
    var onPayload: (@MainActor ([String: Any]) -> Void)?

    private func deliver(_ payload: [String: Any]) {
        DispatchQueue.main.async { MainActor.assumeIsolated { self.onPayload?(payload) } }
    }

    func session(_ session: WCSession, didReceiveMessage message: [String: Any]) { deliver(message) }
    func session(_ session: WCSession, didReceiveApplicationContext applicationContext: [String: Any]) { deliver(applicationContext) }
    func session(_ session: WCSession, activationDidCompleteWith activationState: WCSessionActivationState, error: Error?) {}
    func sessionDidBecomeInactive(_ session: WCSession) {}
    func sessionDidDeactivate(_ session: WCSession) { WCSession.default.activate() }
}
#endif
