import SwiftUI
import UniformTypeIdentifiers

/// The everyday listening & augmented-reality surface (spec §5 "Experience").
struct ExperienceView: View {
    @Environment(EngineController.self) private var engine
    @Environment(MediaLibrary.self) private var library
    @State private var showImporter = false
    @State private var showRecorder = false
    @State private var showSaveProject = false
    @State private var projectName = ""
    @Environment(Store.self) private var store
    @State private var showPaywall = false

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

                Section {
                    WeatherSection()
                } header: {
                    Text("Weather")
                } footer: {
                    Text("Optional. Uses your approximate location to get the current weather from Apple. Your location is not stored.")
                }

                Section {
                    BodyMotionSection()
                } header: {
                    Text("Body & Motion")
                } footer: {
                    Text("Head tracking keeps room memories fixed around you as you turn. Uses compatible AirPods or Beats. Motion data stays on this iPhone.")
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
                ToolbarItem(placement: .topBarLeading) {
                    Menu {
                        Button("Save as Project\u{2026}") { projectName = engine.trackName.map { ($0 as NSString).deletingPathExtension } ?? ""; showSaveProject = true }
                    } label: { Image(systemName: "ellipsis.circle") }
                    .accessibilityLabel("More")
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        if store.allows(.sessionRecording) { showRecorder = true } else { showPaywall = true }
                    } label: {
                        Image(systemName: engine.isRecording ? "record.circle.fill" : "record.circle")
                            .foregroundStyle(engine.isRecording ? Theme.danger : Color.accentColor)
                    }
                    .accessibilityLabel(engine.isRecording ? "Recording" : "Record session")
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button(role: .destructive) { engine.panic() } label: {
                        Label("Stop all sound", systemImage: "speaker.slash.fill")
                    }
                    .tint(Theme.danger)
                    .accessibilityHint("Immediately silences all output")
                }
            }
            .sheet(isPresented: $showRecorder) { RecordSessionView() }
            .alert("Save as Project", isPresented: $showSaveProject) {
                TextField("Name", text: $projectName)
                Button("Save") { engine.saveProject(named: projectName) }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("Saves your song (copied), levels, atmosphere, microphone effects and current Dimension.")
            }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: .experience) }
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
            if engine.isRecording {
                Label(String(format: "REC %ld:%02ld", Int(engine.recordingSeconds) / 60, Int(engine.recordingSeconds) % 60),
                      systemImage: "record.circle.fill")
                    .font(.caption.weight(.bold).monospacedDigit())
                    .foregroundStyle(Theme.danger)
                    .accessibilityLabel("Recording in progress")
            }
            Label("\(Lunar.phaseName(engine.meters.moonPhaseId)) · lunar day \(engine.meters.lunarDay)",
                  systemImage: Lunar.symbol(engine.meters.moonPhaseId))
                .font(.footnote)
                .foregroundStyle(.secondary)
            if let status = engine.statusMessage {
                Text(status).font(.footnote).foregroundStyle(.secondary)
            }
        }
    }
}

/// Live, debounced states of the surroundings. Display only.
private struct EnvironmentStateChips: View {
    let meters: AtmosMeterSnapshot

    var body: some View {
        HStack(spacing: 6) {
            chip("Quiet", on: meters.quiet)
            chip("Loud", on: meters.loud)
            if meters.motionAvailable {
                chip("Still", on: meters.still)
                chip("Moving", on: meters.moving)
            }
        }
        .accessibilityElement(children: .combine)
    }

    private func chip(_ title: String, on: Bool) -> some View {
        Text(title)
            .font(.caption.weight(.semibold))
            .padding(.horizontal, 10).padding(.vertical, 4)
            .background(on ? Theme.accent : Color.secondary.opacity(0.15), in: Capsule())
            .foregroundStyle(on ? Color.white : Color.secondary)
            .accessibilityLabel("\(title) \(on ? "yes" : "no")")
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
            Gated(feature: .musicImport, title: "Replace song…") {
                Button("Replace song…") { showImporter = true }
            }
            Button("Remove song", role: .destructive) { engine.unloadTrack() }
        } else if engine.isLoadingTrack {
            HStack { ProgressView(); Text("Decoding…").foregroundStyle(.secondary) }
        } else {
            Gated(feature: .musicImport, title: "Import a song…") {
                Button { showImporter = true } label: { Label("Import a song…", systemImage: "square.and.arrow.down") }
            }
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
        if engine.motionSensingAvailable {
            Toggle("Motion sensing", isOn: $engine.motionSensing)
        }
        if engine.micEnabled || engine.motionSensing {
            EnvironmentStateChips(meters: engine.meters)
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
            Gated(feature: .frequencyShift, title: "Frequency shift") {
                LabeledSlider(title: "Frequency shift", value: $engine.micShiftHz, range: -600...600, format: "%.0f Hz")
                LabeledSlider(title: "Shift amount", value: $engine.micShiftMix, range: 0...1, format: "%.2f")
            }
            Gated(feature: .roomMemory, title: "Room memory") {
                Toggle("Room memory", isOn: $engine.roomMemoryEnabled)
            }
            if engine.roomMemoryEnabled {
                Text("Replays the last few seconds of your surroundings, slowed and faded, as a drifting texture. Held in memory only and erased when the microphone turns off.")
                    .font(.footnote).foregroundStyle(.secondary)
                LabeledSlider(title: "Memory level", value: $engine.roomMemoryLevel, range: 0...1, format: "%.2f")
                LabeledSlider(title: "Memory pitch", value: $engine.roomMemoryPitch, range: -12...12, format: "%.0f st")
                if engine.meters.roomMemoryVoices > 0 {
                    Text("Replaying \(engine.meters.roomMemoryVoices) memor\(engine.meters.roomMemoryVoices == 1 ? "y" : "ies")")
                        .font(.caption).foregroundStyle(Theme.micTransformed)
                }
            }
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

private struct BodyMotionSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
        let head = engine.headTracker
        let activity = engine.activity

        if head.isSupported {
            Gated(feature: .headTracking, title: "Head tracking") {
                Toggle("Head tracking", isOn: $engine.headTrackingEnabled)
            }
            if engine.headTrackingEnabled {
                HStack {
                    Text(head.headphonesConnected ? String(format: "Facing %+.0f°", head.yawDegrees)
                                                  : "Waiting for compatible headphones…")
                        .font(.footnote.monospacedDigit())
                        .foregroundStyle(.secondary)
                    Spacer()
                    Button("Recenter") { head.recenter() }
                        .buttonStyle(.bordered)
                        .disabled(!head.headphonesConnected)
                }
                if let error = head.lastError {
                    Text(error).font(.footnote).foregroundStyle(Theme.danger)
                }
            }
        } else {
            Text("Head tracking is not available on this device.")
                .font(.footnote).foregroundStyle(.secondary)
        }

        if activity.isSupported {
            Gated(feature: .activity, title: "Activity") {
                Toggle("Activity", isOn: $engine.activityEnabled)
            }
            if engine.activityEnabled {
                if activity.permissionDenied {
                    Text("Motion & Fitness access is off. You can turn it on in iOS Settings > Privacy & Security > Motion & Fitness.")
                        .font(.footnote).foregroundStyle(.secondary)
                } else {
                    HStack {
                        Text(activity.activity.title)
                        Spacer()
                        Text(activity.cadenceStepsPerMinute.map { String(format: "%.0f steps/min", $0) } ?? "no steps")
                            .font(.footnote.monospacedDigit())
                            .foregroundStyle(.secondary)
                    }
                    Toggle("Echo follows your steps", isOn: $engine.echoFollowsSteps)
                }
            }
        }

        let health = engine.health
        if health.isAvailable {
            Gated(feature: .health, title: "Workout intensity") {
                Toggle("Workout intensity (Health)", isOn: $engine.healthEnabled)
            }
            if engine.healthEnabled {
                HStack {
                    Text(health.heartRate.map { String(format: "%.0f bpm", $0) } ?? "No recent heart rate")
                    Spacer()
                    if let i = health.intensity {
                        Text(String(format: "Intensity %.0f%%", i * 100))
                            .font(.footnote.monospacedDigit()).foregroundStyle(.secondary)
                    }
                }
                Toggle("Intensity shapes the atmosphere", isOn: $engine.intensityShapesAtmosphere)
                Text("Live heart rate comes from the Viksen app on Apple Watch during a workout. Without it, Health readings may arrive minutes late.")
                    .font(.footnote).foregroundStyle(.secondary)
                if let error = health.lastError {
                    Text(error).font(.footnote).foregroundStyle(Theme.danger)
                }
            }
        }
    }
}

private struct WeatherSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
        let weather = engine.weather
        Gated(feature: .weather, title: "Weather-aware atmosphere") {
            Toggle("Use local weather", isOn: $engine.weatherEnabled)
        }
        if engine.weatherEnabled {
            if weather.locationDenied {
                Text("Location access is off for Viksen. You can allow approximate location in iOS Settings.")
                    .font(.footnote).foregroundStyle(.secondary)
            } else if let condition = weather.conditionText {
                HStack {
                    Text(condition)
                    Spacer()
                    if let t = weather.temperatureText { Text(t).foregroundStyle(.secondary) }
                }
                Toggle("Weather shapes the atmosphere", isOn: $engine.weatherShapesAtmosphere)
            } else if let error = weather.lastError {
                Text(error).font(.footnote).foregroundStyle(Theme.danger)
            } else {
                HStack { ProgressView(); Text("Getting the weather…").foregroundStyle(.secondary) }
            }
            // Apple Weather attribution (required wherever WeatherKit data is shown).
            if let mark = weather.attributionMarkURL {
                AsyncImage(url: mark) { image in image.resizable().scaledToFit() } placeholder: { Color.clear }
                    .frame(height: 14)
                    .accessibilityLabel("Apple Weather")
            }
            if let legal = weather.attributionLegalURL {
                Link("Weather data sources", destination: legal).font(.footnote)
            }
        }
    }
}
