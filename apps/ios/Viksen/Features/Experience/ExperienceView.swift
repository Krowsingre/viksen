import SwiftUI
import UniformTypeIdentifiers

/// The everyday listening & augmented-reality surface (spec §5 "Experience").
struct ExperienceView: View {
    @Environment(EngineController.self) private var engine
    @Environment(MediaLibrary.self) private var library
    @State private var showImporter = false

    var body: some View {
        @Bindable var engine = engine
        NavigationStack {
            List {
                StatusSection()

                Section("Music") {
                    MusicSourceRow(showImporter: $showImporter)
                }

                Section("Atmosphere") {
                    Toggle("Atmosphere bed", isOn: $engine.bedEnabled)
                    if engine.bedEnabled {
                        LabeledSlider(title: "Density", value: $engine.bedLevel, range: 0...0.6, format: "%.2f")
                        LabeledSlider(title: "Colour", value: $engine.bedColourHz, range: 150...6000, format: "%.0f Hz")
                    }
                }

                Section {
                    EnvironmentSection()
                } header: {
                    Text("Environment")
                } footer: {
                    Text("Microphone audio is processed on this device only and is never recorded or uploaded.")
                }

                Section("Levels") {
                    ForEach(EngineController.BusLevel.allCases) { bus in
                        BusSlider(title: bus.title,
                                  color: Theme.color(for: bus),
                                  value: Binding(get: { engine.busGains[bus.rawValue] },
                                                 set: { engine.setGain($0, for: bus) }),
                                  peak: engine.meters.busPeaks.indices.contains(bus.rawValue)
                                      ? engine.meters.busPeaks[bus.rawValue].floatValue : 0)
                    }
                    BusSlider(title: "Master", color: .primary, value: $engine.masterGain,
                              peak: max(engine.meters.outputPeakLeft, engine.meters.outputPeakRight))
                }

                Section {
                    Text("Immersive audio can mask traffic and other important sounds. Stay aware of your surroundings.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Experience")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button(engine.audioRunning ? "Stop audio" : "Start audio") {
                        if engine.audioRunning { engine.stopAudio() } else { engine.startAudio() }
                    }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button(role: .destructive) { engine.panic() } label: {
                        Label("Stop all sound", systemImage: "speaker.slash.fill")
                    }
                    .tint(Theme.danger)
                    .accessibilityHint("Immediately silences all output")
                }
            }
            .fileImporter(isPresented: $showImporter, allowedContentTypes: [.audio], allowsMultipleSelection: false) { result in
                if case let .success(urls) = result, let url = urls.first, let local = library.importCopy(of: url) {
                    engine.loadTrack(url: local)
                }
            }
            .alert("Problem", isPresented: Binding(get: { engine.lastError != nil }, set: { if !$0 { engine.lastError = nil } })) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(engine.lastError ?? "")
            }
        }
    }
}

private struct StatusSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        Section {
            HStack(spacing: 12) {
                if engine.meters.micActive {
                    Label("MIC LIVE", systemImage: "mic.fill")
                        .font(.caption.weight(.bold))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 8).padding(.vertical, 4)
                        .background(Theme.danger, in: Capsule())
                        .accessibilityLabel("Microphone is live")
                }
                VStack(alignment: .leading, spacing: 2) {
                    Text(engine.route.outputName).font(.subheadline)
                    Text(engine.audioRunning
                         ? String(format: "%.1f kHz · buffer %.1f ms", engine.route.sampleRate / 1000, engine.route.ioBufferDuration * 1000)
                         : "Audio stopped")
                        .font(.caption).foregroundStyle(.secondary)
                }
                Spacer()
                if engine.meters.limiterGain < 0.98 {
                    Text("LIMIT").font(.caption2.weight(.bold)).foregroundStyle(Theme.micRaw)
                        .accessibilityLabel("Safety limiter active")
                }
            }
            if engine.micEnabled && engine.isOnSpeaker {
                Label("Using the speaker with the mic on can cause feedback. Wired headphones are recommended.",
                      systemImage: "exclamationmark.triangle")
                    .font(.footnote).foregroundStyle(Theme.micRaw)
            }
            if engine.micEnabled && engine.isOnBluetooth {
                Label("Bluetooth adds noticeable delay to live monitoring. Wired headphones give the lowest latency.",
                      systemImage: "info.circle")
                    .font(.footnote).foregroundStyle(.secondary)
            }
            if let status = engine.statusMessage {
                Text(status).font(.footnote).foregroundStyle(.secondary)
            }
        }
    }
}

private struct MusicSourceRow: View {
    @Environment(EngineController.self) private var engine
    @Binding var showImporter: Bool

    var body: some View {
        @Bindable var engine = engine
        if let name = engine.trackName {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(name).lineLimit(1)
                    Spacer()
                    Button { engine.togglePlay() } label: {
                        Image(systemName: engine.meters.playing ? "pause.circle.fill" : "play.circle.fill")
                            .font(.title)
                    }
                    .buttonStyle(.borderless)
                    .accessibilityLabel(engine.meters.playing ? "Pause" : "Play")
                }
                let duration = max(engine.meters.durationSeconds, 0.001)
                Slider(value: Binding(get: { min(engine.meters.positionSeconds, duration) },
                                      set: { engine.seek(to: $0) }),
                       in: 0...duration)
                    .tint(Theme.music)
                HStack {
                    Text(timeString(engine.meters.positionSeconds))
                    Spacer()
                    Text(timeString(duration))
                }
                .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                Toggle("Loop", isOn: $engine.looping)
            }
            Button("Replace song…") { showImporter = true }
            Button("Remove song", role: .destructive) { engine.unloadTrack() }
        } else if engine.isLoadingTrack {
            HStack { ProgressView(); Text("Decoding…").foregroundStyle(.secondary) }
        } else {
            Button { showImporter = true } label: { Label("Import a song…", systemImage: "square.and.arrow.down") }
            Text("Or play atmosphere and environment on their own — no music needed.")
                .font(.footnote).foregroundStyle(.secondary)
        }
    }

    private func timeString(_ s: Double) -> String {
        let t = Int(s.rounded(.down))
        return String(format: "%ld:%02ld", t / 60, t % 60)
    }
}

private struct EnvironmentSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
        Toggle("Microphone", isOn: Binding(get: { engine.micEnabled }, set: { engine.setMicEnabled($0) }))
        if engine.micPermission == .denied {
            Text("Microphone access is off for this app. You can turn it on in iOS Settings.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        if engine.micEnabled {
            HStack {
                Text("Surroundings")
                Spacer()
                Text(engine.meters.environmentSilent ? "quiet" : String(format: "%.0f dBFS", engine.meters.environmentLevelDb))
                    .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                Text("· \(engine.meters.transientCount) events")
                    .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
            }
            LabeledSlider(title: "Tone", value: $engine.micToneHz, range: 200...12000, format: "%.0f Hz")
            LabeledSlider(title: "Echo time", value: $engine.micEchoSeconds, range: 0.05...1.5, format: "%.2f s")
            LabeledSlider(title: "Echo feedback", value: $engine.micFeedback, range: 0...0.9, format: "%.2f")
            LabeledSlider(title: "Effect mix", value: $engine.micWet, range: 0...1, format: "%.2f")
        }
    }
}

struct LabeledSlider: View {
    var title: String
    @Binding var value: Float
    var range: ClosedRange<Float>
    var format: String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text(title).font(.subheadline)
                Spacer()
                Text(String(format: format, value)).font(.caption.monospacedDigit()).foregroundStyle(.secondary)
            }
            Slider(value: $value, in: range).accessibilityLabel(title)
        }
    }
}
