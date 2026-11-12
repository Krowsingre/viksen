import SwiftUI

/// Shows the runtime log and lets the user share it (e.g. with support).
struct DiagnosticsView: View {
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var lines: [String] = []
    @State private var confirmClear = false
    @AppStorage("test.anySongIsLunar") private var anySongIsLunar = false

    var body: some View {
        @Bindable var engine = engine
        @Bindable var store = store
        List {
            Section {
                ShareLink(item: AppLog.fileURL) { Label("Share log", systemImage: "square.and.arrow.up") }
                Button("Clear log", role: .destructive) { confirmClear = true }
            } footer: {
                Text("The log records app events (audio start/stop, route changes, errors, dropouts). It never contains audio, health data, location or your files, and it stays on this \(Platform.deviceNoun) unless you share it.")
            }
            if AppBuild.isTestFlight {
                Section {
                    Toggle("Count any song toward tonight's phase", isOn: $anySongIsLunar)
                } header: { Text("Testing") } footer: {
                    Text("TestFlight only. Lets the Moon Chart's 3-minute counter run with songs that are not lunar songs yet, so it can be tested. Discoveries made this way are real and stay in your progress.")
                }
                Section {
                    Toggle("Unlock all Dimensions", isOn: $store.testUnlockAll)
                } footer: {
                    Text("TestFlight only. Every Dimension opens and downloads without buying it, so Dimensions can be tested before their purchases are set up in App Store Connect. Never in the App Store version.")
                }
                Section {
                    Picker("Room voices", selection: $engine.testRoomSolo) {
                        ForEach(EngineController.RoomVoice.allCases) { Text($0.title).tag($0) }
                    }
                    Toggle("Room forward", isOn: $engine.testRoomForward)
                } header: { Text("Room listening test") } footer: {
                    Text("TestFlight only, with \"Blend the room\" on. Room voices: hear one of the room's voices alone (the plain, softened room stays in), or none of them. Room forward: the room 6 dB louder and the music 6 dB quieter. Both apply at once and reset when the app restarts.")
                }
                Section {
                    Toggle("Echo and reverb at the loop point", isOn: $engine.loopTransitionFx)
                } footer: {
                    Text("TestFlight only. On: when a song loops back to its start it uses the same echo and reverb transition as a change of part (this is where the reverbed snap comes from). Off: a clean, very short crossfade. Changes the playing song at its next loop.")
                }
            }
            AudioRouteSection()
            Section("Recent events") {
                if lines.isEmpty { Text("No events yet").foregroundStyle(.secondary) }
                ForEach(Array(lines.reversed().enumerated()), id: \.offset) { _, line in
                    Text(line).font(.caption.monospaced()).textSelection(.enabled)
                }
            }
        }
        .navigationTitle("Diagnostics")
        .onAppear { lines = AppLog.tail() }
        .refreshable { lines = AppLog.tail() }
        .confirmationDialog("Clear the log?", isPresented: $confirmClear) {
            Button("Clear", role: .destructive) { AppLog.clear(); lines = [] }
        }
    }
}

/// The current audio route (what iOS reports) and dropouts.
private struct AudioRouteSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        Section {
            LabeledContent("Output", value: engine.route.outputName)
            LabeledContent("Input", value: engine.route.inputName)
            LabeledContent("Engine sample rate", value: String(format: "%.0f Hz", engine.sampleRate))
            LabeledContent("I/O buffer", value: String(format: "%.1f ms", engine.route.ioBufferDuration * 1000))
            LabeledContent("Reported round trip",
                           value: String(format: "%.1f ms", (engine.route.reportedInputLatency + engine.route.reportedOutputLatency + 2 * engine.route.ioBufferDuration) * 1000))
            UnderrunsRow()
        } header: { Text("Audio route") } footer: {
            Text("What iOS reports for the current route, not a measurement. Bluetooth adds delay the system may not report.")
        }
    }
}

private struct UnderrunsRow: View {
    @Environment(EngineController.self) private var engine
    var body: some View { LabeledContent("Underruns", value: "\(engine.meters.underruns)") }
}
