import SwiftUI

/// Opening screen, after the owner-selected concept (top-left): black field, a
/// slowly moving violet light wave, a thin ring, the wordmark and two actions.
/// "Restore Project" from the concept is intentionally absent until project
/// save/restore exists on iPhone (no placeholder controls).
struct WelcomeView: View {
    var onGetStarted: () -> Void
    @State private var showAbout = false

    var body: some View {
        ZStack {
            Theme.background.ignoresSafeArea()
            LightWave()
                .frame(height: 260)
                .frame(maxHeight: .infinity, alignment: .top)
                .padding(.top, 40)
                .ignoresSafeArea()
                .accessibilityHidden(true)

            VStack(spacing: 0) {
                Spacer().frame(height: 150)
                Circle()
                    .stroke(Theme.accentGlow, lineWidth: 2)
                    .frame(width: 92, height: 92)
                    .shadow(color: Theme.accentGlow.opacity(0.9), radius: 14)
                    .accessibilityHidden(true)
                Spacer().frame(height: 48)
                Text("VIKSEN")
                    .font(.system(size: 44, weight: .ultraLight, design: .default))
                    .tracking(14)
                    .foregroundStyle(.white)
                    .accessibilityAddTraits(.isHeader)
                Text("Atmospheric & Spatial Audio Studio\nAugmented Auditory Reality")
                    .font(.footnote)
                    .multilineTextAlignment(.center)
                    .foregroundStyle(.white.opacity(0.7))
                    .padding(.top, 14)
                Spacer()
                Button(action: onGetStarted) {
                    Text("Get Started")
                        .font(.headline)
                        .frame(maxWidth: .infinity, minHeight: 50)
                        .background(Theme.accent, in: Capsule())
                        .foregroundStyle(.white)
                }
                .padding(.horizontal, 32)
                Button("About Viksen") { showAbout = true }
                    .font(.footnote)
                    .underline()
                    .tint(Theme.accent)
                    .padding(.top, 22)
                    .padding(.bottom, 36)
            }
        }
        .preferredColorScheme(.dark)
        .sheet(isPresented: $showAbout) { AboutView() }
    }
}

/// Layered, slowly drifting sine ribbons with a soft glow.
private struct LightWave: View {
    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0)) { context in
            let t = context.date.timeIntervalSinceReferenceDate
            Canvas { ctx, size in
                for layer in 0..<4 {
                    var path = Path()
                    let phase = t * (0.25 + Double(layer) * 0.07) + Double(layer) * 1.3
                    let amp = size.height * (0.16 + 0.05 * Double(layer))
                    let mid = size.height * (0.45 + 0.06 * Double(layer))
                    for step in 0...120 {
                        let x = size.width * Double(step) / 120.0
                        let u = Double(step) / 120.0
                        let y = mid + amp * sin(u * 2.4 * .pi + phase) * (0.6 + 0.4 * sin(u * .pi))
                        if step == 0 { path.move(to: CGPoint(x: x, y: y)) } else { path.addLine(to: CGPoint(x: x, y: y)) }
                    }
                    let alpha = 0.55 - Double(layer) * 0.11
                    ctx.stroke(path, with: .color(Theme.accentGlow.opacity(alpha)), lineWidth: layer == 0 ? 2.5 : 1.2)
                }
            }
            .blur(radius: 1.2)
            .background(
                RadialGradient(colors: [Theme.accentGlow.opacity(0.28), .clear],
                               center: .center, startRadius: 10, endRadius: 260)
            )
        }
    }
}

struct AboutView: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("Viksen shapes sound as an environment you can inhabit: enhance songs with atmosphere and spatial movement, or let the sounds around you become an evolving, reactive world.")
                }
                Section("Privacy") {
                    Text("Everything runs on this iPhone. Microphone audio is processed live and never recorded or uploaded unless you explicitly record. No account, no tracking.")
                }
                Section("Safety") {
                    Text("Immersive audio can hide important sounds around you. Keep volume moderate and stay aware of your surroundings.")
                }
                Section {
                    LabeledContent("Version", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "–")
                }
            }
            .navigationTitle("About Viksen")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
        }
    }
}
