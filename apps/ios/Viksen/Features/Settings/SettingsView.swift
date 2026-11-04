import SwiftUI

struct SettingsView: View {
    @Environment(EngineController.self) private var engine
    @AppStorage("appearance") private var appearance: Appearance = .dark
    @Environment(\.dismiss) private var dismiss
    @Environment(Store.self) private var store
    @State private var showPaywall = false

    var body: some View {
        @Bindable var engine = engine
        NavigationStack {
            Form {
                Section("Purchases") {
                    LabeledContent("Edition", value: store.tier.title)
                    if store.tier < .studio {
                        Button(store.tier == .free ? "Upgrade…" : "Upgrade to Studio…") { showPaywall = true }
                    }
                    Button("Restore Purchases") { Task { await store.restore() } }
                }

                DownloadsSettingsSection()

                Section {
                    Label("\(Lunar.phaseName(engine.meters.moonPhaseId)) · lunar day \(engine.meters.lunarDay) of 30",
                          systemImage: Lunar.symbol(engine.meters.moonPhaseId))
                    Text("Viksen follows the lunar calendar, worked out on this \(Platform.deviceNoun). The fuller the moon, the longer and wider the echoes and reverb wash that carry the music from one part to the next; near the new moon, changes are closer and drier. Each Dimension also has parts it favours under the full moon and at the new moon, and some favour the night.")
                        .font(.footnote).foregroundStyle(.secondary)
                } header: {
                    Text("The Moon")
                }

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
                    Text("All processing happens on this \(Platform.deviceNoun). Nothing is ever recorded or saved, and nothing is uploaded. No account is required.")
                        .font(.footnote).foregroundStyle(.secondary)
                }

                Section("Appearance") {
                    Picker("Theme", selection: $appearance) {
                        ForEach(Appearance.allCases) { Text($0.title).tag($0) }
                    }
                }

                Section("Support") {
                    NavigationLink("Diagnostics") { DiagnosticsView() }
                }

                Section("About") {
                    LabeledContent("Version", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "–")
                }
            }
            .navigationTitle("Settings")
            .sheet(isPresented: $showPaywall) { PaywallView() }
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
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

/// Downloaded tracks per Dimension and the space they use; open one to remove tracks.
private struct DownloadsSettingsSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        let downloaded = engine.worlds.filter { !$0.userMade && !$0.audioAvailable && engine.downloads.isAnyInstalled($0.worldID) }
        Section {
            if downloaded.isEmpty {
                Text("No tracks downloaded.").foregroundStyle(.secondary)
            }
            ForEach(downloaded, id: \.worldID) { w in
                NavigationLink {
                    DimensionTracksView(world: w, showPaywall: .constant(false))
                } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(w.name)
                            Text("\(engine.downloads.installedCount(w.worldID)) of \(engine.downloads.tracks(w.worldID).count) tracks")
                                .font(.caption).foregroundStyle(.secondary)
                        }
                        Spacer()
                        Text(ByteCountFormatter.string(fromByteCount: engine.downloads.bytes(w.worldID), countStyle: .file))
                            .foregroundStyle(.secondary).monospacedDigit()
                    }
                }
            }
        } header: {
            Text("Downloaded Tracks")
        } footer: {
            Text("Open a Dimension to remove tracks one by one or all at once. Download them again from Dimensions any time.")
        }
        .onAppear { engine.refreshWorlds() }
    }
}
