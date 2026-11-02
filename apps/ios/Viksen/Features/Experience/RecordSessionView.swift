import SwiftUI

/// "Record Session": explicit recording of the current experience. Nothing is
/// ever recorded without pressing Record here.
struct RecordSessionView: View {
    @Environment(EngineController.self) private var engine
    @Environment(\.dismiss) private var dismiss
    @State private var mode: EngineController.RecordMode = .fullMix

    var body: some View {
        NavigationStack {
            List {
                Section {
                    HStack {
                        Spacer()
                        Text(timeString(engine.recordingSeconds))
                            .font(.system(size: 44, weight: .light, design: .rounded).monospacedDigit())
                            .foregroundStyle(engine.isRecording ? Theme.danger : .primary)
                        Spacer()
                    }
                    .padding(.vertical, 8)
                    Text("Capture your current experience, including music, atmosphere and live environment.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("What to record") {
                    Picker("Mode", selection: $mode) {
                        ForEach(EngineController.RecordMode.allCases) { Text($0.title).tag($0) }
                    }
                    .pickerStyle(.inline)
                    .labelsHidden()
                    .disabled(engine.isRecording)
                    if mode == .environmentOnly || mode == .fullMix, engine.micEnabled {
                        Text("This recording includes the sound around you, which may include other people. Only record people with their consent.")
                            .font(.footnote).foregroundStyle(Theme.micRaw)
                    }
                }
                Section {
                    Button {
                        engine.isRecording ? engine.stopRecording() : engine.startRecording(mode)
                    } label: {
                        Label(engine.isRecording ? "Stop" : "Record",
                              systemImage: engine.isRecording ? "stop.circle.fill" : "record.circle")
                            .font(.headline)
                            .frame(maxWidth: .infinity)
                    }
                    .tint(Theme.danger)
                } footer: {
                    Text("Recordings are 24-bit WAV files saved on this iPhone (Library tab). They are never uploaded.")
                }
            }
            .navigationTitle("Record Session")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
        }
    }

    private func timeString(_ s: Double) -> String {
        let t = Int(s)
        return String(format: "%02ld:%02ld:%02ld", t / 3600, (t / 60) % 60, t % 60)
    }
}
