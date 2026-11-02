import SwiftUI

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
