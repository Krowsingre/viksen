import SwiftUI

struct SettingsView: View {
    @Environment(EngineController.self) private var engine
    @AppStorage("appearance") private var appearance: Appearance = .dark

    var body: some View {
        @Bindable var engine = engine
        NavigationStack {
            Form {
                Section("Audio route") {
                    LabeledContent("Output", value: engine.route.outputName)
                    LabeledContent("Input", value: engine.route.inputName)
                    LabeledContent("Engine sample rate", value: String(format: "%.0f Hz", engine.sampleRate))
                    LabeledContent("I/O buffer", value: String(format: "%.1f ms", engine.route.ioBufferDuration * 1000))
                    LabeledContent("Reported round trip",
                                   value: String(format: "%.1f ms", (engine.route.reportedInputLatency + engine.route.reportedOutputLatency + 2 * engine.route.ioBufferDuration) * 1000))
                    LabeledContent("Underruns", value: "\(engine.meters.underruns)")
                }
                Section {
                    Text("Latency above is what iOS reports for the current route, not a measurement. Bluetooth headphones add delay the system may not report accurately; wired headphones are best for live monitoring.")
                        .font(.footnote).foregroundStyle(.secondary)
                }

                Section("Hearing safety") {
                    LabeledSlider(title: "Output ceiling", value: $engine.limiterCeilingDb, range: -24...(-1), format: "%.0f dBFS")
                    Text("A safety limiter on the final output never lets the signal exceed this level.")
                        .font(.footnote).foregroundStyle(.secondary)
                }

                Section("Privacy") {
                    LabeledContent("Microphone permission", value: permissionText)
                    Text("All processing happens on this iPhone. Nothing is recorded unless you explicitly start a recording, and nothing is uploaded. No account is required.")
                        .font(.footnote).foregroundStyle(.secondary)
                }

                Section("Appearance") {
                    Picker("Theme", selection: $appearance) {
                        ForEach(Appearance.allCases) { Text($0.title).tag($0) }
                    }
                }

                Section("About") {
                    LabeledContent("Version", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "–")
                }
            }
            .navigationTitle("Settings")
        }
    }

    private var permissionText: String {
        switch engine.micPermission {
        case .granted: "Allowed"
        case .denied: "Denied"
        default: "Not requested"
        }
    }
}
