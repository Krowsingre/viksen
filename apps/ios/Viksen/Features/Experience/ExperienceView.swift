import SwiftUI
import UniformTypeIdentifiers

/// Dimensions (owner's plan, 2026-11-05): your Dimensions first (tap to open, play from
/// the list), then the ones you can unlock. No settings here; those live in Settings.
struct ExperienceView: View {
    @Environment(EngineController.self) private var engine
    @Environment(AppNavigator.self) private var nav
    @Environment(Store.self) private var store

    private func locked(_ w: AtmosWorldInfo) -> Bool { DimensionAccess.required(w) > store.tier }

    var body: some View {
        let mine = engine.worlds.filter { !locked($0) }
        let more = engine.worlds.filter { locked($0) }
        NavigationStack {
            List {
                Section("Your Dimensions") {
                    ForEach(mine, id: \.worldID) { DimensionRow(world: $0, locked: false) }
                }
                if !more.isEmpty {
                    Section {
                        ForEach(more, id: \.worldID) { DimensionRow(world: $0, locked: true) }
                    } header: { Text("More Dimensions") } footer: {
                        Text("Open one to see how to unlock it.")
                    }
                }
            }
            .navigationTitle("Dimensions")
            .toolbar { ToolbarItem(placement: .primaryAction) { CloseButton(compact: true) { nav.go(.home) } } }
            .alert("Problem", isPresented: Binding(get: { engine.lastError != nil }, set: { if !$0 { engine.lastError = nil } })) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(engine.lastError ?? "")
            }
        }
    }
}

enum DimensionAccess {
    static func required(_ w: AtmosWorldInfo) -> Tier {
        switch w.tier { case "studio": .studio; case "experience": .experience; default: .free }
    }
}

/// One Dimension in the list: art, name, what is on this device; play from the row.
private struct DimensionRow: View {
    let world: AtmosWorldInfo
    let locked: Bool
    @Environment(EngineController.self) private var engine

    var body: some View {
        let current = engine.activeDimensionID == world.worldID
        HStack(spacing: 12) {
            NavigationLink { DimensionDetailView(world: world) } label: {
                HStack(spacing: 12) {
                    DimensionThumbnail(dimensionID: world.worldID)
                        .frame(width: 52, height: 52)
                        .clipShape(RoundedRectangle(cornerRadius: 10))
                        .overlay(alignment: .bottomTrailing) {
                            if locked {
                                Image(systemName: "lock.fill").font(.caption2).padding(4)
                                    .background(.black.opacity(0.6), in: Circle()).foregroundStyle(.white)
                            }
                        }
                    VStack(alignment: .leading, spacing: 2) {
                        Text(world.name).font(.headline)
                        Text(subtitle).font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
            if !locked && engine.isPlayable(world) {
                Button { if current { engine.leaveWorld() } else { engine.enterWorld(world) } } label: {
                    Image(systemName: current ? "stop.circle.fill" : "play.circle.fill")
                        .font(.title2).foregroundStyle(Theme.accent)
                }
                .buttonStyle(.borderless)
                .accessibilityLabel(current ? "Stop \(world.name)" : "Play \(world.name)")
            }
        }
    }

    private var subtitle: String {
        if locked { return "Included with a subscription" }
        if world.audioAvailable || world.userMade { return "Built in" }
        if engine.downloads.isDownloading(world.worldID) { return "Downloading…" }
        let bytes = engine.downloads.bytes(world.worldID)
        if engine.downloads.isAnyInstalled(world.worldID) {
            return bytes > 0 ? "On this \(Platform.deviceNoun) · " + ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file) : "On this \(Platform.deviceNoun)"
        }
        return "Not downloaded"
    }
}

/// The level sliders with live meters: their own view, so only they redraw with the meters.
struct BusSliders: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
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
}

struct StatusSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        Section {
            HStack(spacing: 12) {
                if engine.micLive {
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
                if engine.limiting {
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
            Label("\(Lunar.phaseName(engine.moonPhaseID)) · lunar day \(engine.lunarDay)",
                  systemImage: Lunar.symbol(engine.moonPhaseID))
                .font(.footnote)
                .foregroundStyle(.secondary)
            if let status = engine.statusMessage {
                Text(status).font(.footnote).foregroundStyle(.secondary)
            }
        }
    }
}

/// Live, debounced states of the surroundings. Display only.
struct EnvironmentStateChips: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        let meters = engine.meters
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

/// What the Dimension listens to. The microphone here only analyses the room.
struct SensesSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
        if engine.motionSensingAvailable {
            Toggle(isOn: $engine.motionSensing) {
                senseLabel("Motion", "Moving brings the driving parts; stillness brings the calm ones.")
            }
        }
        Toggle(isOn: Binding(get: { engine.micEnabled }, set: { engine.setMicEnabled($0) })) {
            senseLabel("Listen to the room", engine.onHeadphones
                       ? "Quiet and loud surroundings choose different parts. The room is only analysed, not played to you."
                       : "Connect headphones to let the room in. Through a speaker the microphone would hear the music.")
        }
        .disabled(!engine.onHeadphones)
        if engine.micPermission == .denied {
            Text("Microphone access is off for this app. You can turn it on in Settings.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        if engine.micEnabled || engine.motionSensing {
            EnvironmentStateChips()
        }
    }

    private func senseLabel(_ title: String, _ detail: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
            Text(detail).font(.caption).foregroundStyle(.secondary)
        }
    }
}

/// Optional: hear the surroundings, transformed, under the music.
struct BlendSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        @Bindable var engine = engine
        if !engine.onHeadphones {
            Label("Connect headphones to blend the room into the music.", systemImage: "headphones")
                .font(.footnote).foregroundStyle(.secondary)
        }
        Toggle("Hear the room", isOn: $engine.hearRoom)
            .disabled(!engine.onHeadphones)
        if engine.hearRoom && !engine.micEnabled {
            Text("Turn on \u{201C}Listen to the room\u{201D} above to hear it.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        if engine.hearRoom && engine.micEnabled && engine.onHeadphones {
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

struct BodyMotionSection: View {
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

struct WeatherSection: View {
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

/// One Dimension: play it, download or remove it (the whole Dimension: its songs are
/// discovered by listening), the album, and its Moon Chart. Locked: how to unlock it.
struct DimensionDetailView: View {
    let world: AtmosWorldInfo
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var confirmRemove = false
    @State private var showMoonChart = false
    @State private var showPaywall = false

    private var bundled: Bool { world.audioAvailable || world.userMade }
    private var locked: Bool { DimensionAccess.required(world) > store.tier }

    var body: some View {
        let current = engine.activeDimensionID == world.worldID
        let downloads = engine.downloads
        let installed = downloads.isAnyInstalled(world.worldID)
        List {
            Section {
                HStack(spacing: 16) {
                    DimensionThumbnail(dimensionID: world.worldID)
                        .frame(width: 96, height: 96)
                        .clipShape(RoundedRectangle(cornerRadius: 16))
                    VStack(alignment: .leading, spacing: 4) {
                        Text(world.name).font(.title3.weight(.semibold))
                        if !world.artist.isEmpty { Text(world.artist).foregroundStyle(.secondary) }
                        if !world.album.isEmpty && world.album != world.name {
                            Text(world.album).font(.footnote).foregroundStyle(.secondary)
                        }
                    }
                }
                .listRowBackground(Color.clear)
            }
            if locked {
                Section {
                    Button("Subscribe to unlock…") { showPaywall = true }
                } footer: {
                    Text("\(world.name) is included with a Viksen subscription.")
                }
            } else {
                Section {
                    if engine.isPlayable(world) {
                        Button { if current { engine.leaveWorld() } else { engine.enterWorld(world) } } label: {
                            Label(current ? "Stop" : "Play", systemImage: current ? "stop.fill" : "play.fill")
                        }
                    }
                    if !bundled {
                        if downloads.isDownloading(world.worldID) {
                            HStack {
                                ProgressView(value: downloads.progress(world.worldID))
                                Button("Cancel") { downloads.cancelAll(world.worldID) }.buttonStyle(.borderless)
                            }
                        } else if !installed {
                            Button("Download", systemImage: "arrow.down.circle") { downloads.downloadAll(world.worldID) }
                        } else {
                            Button("Remove from this \(Platform.deviceNoun)", systemImage: "trash", role: .destructive) { confirmRemove = true }
                        }
                        if let failure = downloads.failure(world.worldID) {
                            Text("Download failed: \(failure)").font(.footnote).foregroundStyle(Theme.danger)
                        }
                    }
                } footer: {
                    if bundled { Text("Built into Viksen.") }
                    else if installed, downloads.bytes(world.worldID) > 0 {
                        Text("Using \(ByteCountFormatter.string(fromByteCount: downloads.bytes(world.worldID), countStyle: .file)). Its songs reveal themselves as you listen.")
                    } else {
                        Text("Download it to play. Its songs reveal themselves as you listen.")
                    }
                }
            }
            Section {
                Button { showMoonChart = true } label: {
                    LabeledContent {
                        Text("\(engine.progress.discoveredPhases(world.worldID).count) of 8")
                    } label: {
                        Label("Moon Chart", systemImage: "moonphase.waxing.gibbous")
                    }
                }
                if let url = engine.albums.link(for: world) {
                    Link(destination: url) { Label("Listen to the album on Apple Music", systemImage: "music.note") }
                }
            }
        }
        .navigationTitle(world.name)
        .inlineNavigationTitle()
        .sheet(isPresented: $showMoonChart) { MoonChartView(initialDimensionID: world.worldID) }
        .sheet(isPresented: $showPaywall) { PaywallView(highlight: .experience) }
        .confirmationDialog("Remove \(world.name) from this \(Platform.deviceNoun)?", isPresented: $confirmRemove, titleVisibility: .visible) {
            Button("Remove", role: .destructive) { engine.removeAllTracks(world) }
        } message: {
            Text("Your Moon Chart progress stays. You can download it again any time.")
        }
    }
}
