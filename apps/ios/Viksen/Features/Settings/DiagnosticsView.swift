import SwiftUI

/// Shows the runtime log and lets the user share it (e.g. with support).
struct DiagnosticsView: View {
    @State private var lines: [String] = []
    @State private var confirmClear = false
    @AppStorage("test.anySongIsLunar") private var anySongIsLunar = false

    var body: some View {
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
            }
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
