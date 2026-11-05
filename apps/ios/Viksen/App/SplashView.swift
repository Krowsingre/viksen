import SwiftUI

/// PLACEHOLDER splash: the real splash design has not been made yet. Kept
/// deliberately plain (no controls, no claims) and short; replace the body when
/// the design exists. Shown once per launch, then the Home tab appears.
struct SplashView: View {
    var onFinished: () -> Void
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var visible = false

    var body: some View {
        ZStack {
            Theme.background.ignoresSafeArea()
            Text("VIKSEN")
                .font(.system(size: 34, weight: .ultraLight))
                .tracking(12)
                .foregroundStyle(.white)
                .opacity(visible ? 1 : 0)
        }
        .accessibilityLabel("Viksen")
        .task {
            // Only covers the first frame while Home is built: no fixed wait.
            withAnimation(.easeOut(duration: reduceMotion ? 0 : 0.2)) { visible = true }
            try? await Task.sleep(for: .milliseconds(250))
            onFinished()
        }
    }
}
