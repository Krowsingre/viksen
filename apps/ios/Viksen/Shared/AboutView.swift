import SwiftUI

struct AboutView: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("Viksen Dimensions turns music into worlds you inhabit: each Dimension follows the moon, the time of day and the world around you, in 3D around your headphones.")
                }
                Section("Privacy") {
                    Text("Everything runs on this \(Platform.deviceNoun). Microphone audio is processed live and never recorded, saved or uploaded. No account, no tracking.")
                }
                Section("Safety") {
                    Text("Immersive audio can hide important sounds around you. Keep volume moderate and stay aware of your surroundings.")
                }
                Section {
                    Link("Privacy Policy", destination: SiteLinks.privacy)
                    Link("Support", destination: SiteLinks.support)
                    NavigationLink("Acknowledgements") { AcknowledgementsView() }
                }
                Section {
                    LabeledContent("Version", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "–")
                }
            }
            .navigationTitle("About Viksen Dimensions")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
        }
    }
}

enum SiteLinks {
    static let privacy = URL(string: "https://krowsingre.github.io/viksen/privacy.html")!
    static let support = URL(string: "https://krowsingre.github.io/viksen/support.html")!
}

/// Licence texts for third-party software (Steam Audio and its components, PFFFT, JSON).
struct AcknowledgementsView: View {
    private let text: String = {
        guard let url = Bundle.main.url(forResource: "Acknowledgements", withExtension: "txt"),
              let t = try? String(contentsOf: url, encoding: .utf8) else { return "Acknowledgements file missing." }
        return t
    }()

    var body: some View {
        ScrollView {
            Text(text)
                .font(.caption.monospaced())
                .textSelection(.enabled)
                .padding()
        }
        .navigationTitle("Acknowledgements")
    }
}
