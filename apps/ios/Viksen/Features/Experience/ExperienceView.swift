import SwiftUI
import UniformTypeIdentifiers

/// The everyday listening & augmented-reality surface (spec §5 "Experience").
struct ExperienceView: View {
    @Environment(EngineController.self) private var engine
    @State private var showSaveProject = false
    @State private var projectName = ""
    @Environment(Store.self) private var store
    @State private var showPaywall = false
    @State private var showBlend = false

    var body: some View {
        @Bindable var engine = engine
        NavigationStack {
            List {
                NowPlayingSection()
                DimensionPickerSection(showPaywall: $showPaywall)

                Section {
                    SensesSection()
                    BodyMotionSection()
                } header: {
                    Text("Senses")
                } footer: {
                    Text("Senses steer the music: each part of a Dimension belongs to a moment (still, moving, quiet, loud, working out), and the song moves to the part that fits you. With every sense off it plays as composed. The time of day and the moon always shape it a little. Everything stays on this \(Platform.deviceNoun).")
                }

                if engine.worldStatus.loaded && engine.worldStatus.worldID != "studio" {
                    Section("Space") {
                        Toggle("3D for headphones", isOn: $engine.dimension3D)
                        if let current = engine.worlds.first(where: { $0.worldID == engine.worldStatus.worldID }) {
                            NavigationLink("Arrange the instruments") { WorldDetailView(world: current) }
                        }
                    }
                }

                Section {
                    DisclosureGroup("Blend the room in", isExpanded: $showBlend) {
                        BlendSection()
                        Toggle("Atmosphere bed", isOn: $engine.bedEnabled)
                        if engine.bedEnabled {
                            LabeledSlider(title: "Density", value: $engine.bedLevel, range: 0...0.6, format: "%.2f")
                            LabeledSlider(title: "Colour", value: $engine.bedColourHz, range: 150...6000, format: "%.0f Hz")
                        }
                        WeatherSection()
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
                } footer: {
                    Text("Optional: hear your surroundings, transformed, under the music. Off by default. Nothing is ever recorded, saved or uploaded.")
                }

                StatusSection()

                Section {
                    Text("Immersive audio can mask traffic and other important sounds. Stay aware of your surroundings.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Dimensions")
            .toolbar {
                if store.tier >= .studio {
                    ToolbarItem(placement: .barLeading) {
                        Menu {
                            Button("Save as Project\u{2026}") { projectName = engine.worldStatus.name; showSaveProject = true }
                        } label: { Image(systemName: "ellipsis.circle") }
                        .accessibilityLabel("More")
                    }
                }
                ToolbarItem(placement: .barTrailing) {
                    Button(role: .destructive) { engine.panic() } label: {
                        Label("Stop all sound", systemImage: "speaker.slash.fill")
                    }
                    .tint(Theme.danger)
                    .accessibilityHint("Immediately silences all output")
                }
            }
            .alert("Save as Project", isPresented: $showSaveProject) {
                TextField("Name", text: $projectName)
                Button("Save") { engine.saveProject(named: projectName) }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("Saves your levels, atmosphere, microphone effects and current Dimension.")
            }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: .experience) }
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

/// What the Dimension listens to. The microphone here only analyses the room.
private struct SensesSection: View {
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
            EnvironmentStateChips(meters: engine.meters)
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
private struct BlendSection: View {
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

/// The Dimension being heard: which part of its song, and why.
private struct NowPlayingSection: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        let st = engine.worldStatus
        let inDimension = st.loaded && st.worldID != "studio"
        Section {
            if inDimension {
                HStack(alignment: .center, spacing: 14) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(st.name).font(.title3.weight(.semibold))
                        if st.songCount > 1 {
                            Text(st.songTitle).font(.subheadline)
                        }
                        if !st.sectionTitle.isEmpty {
                            Text(st.sectionTitle).font(.subheadline).foregroundStyle(Theme.accent)
                        }
                        Text(reason(st)).font(.caption).foregroundStyle(.secondary)
                        if let world = engine.worlds.first(where: { $0.worldID == st.worldID }),
                           let url = engine.albums.link(for: world) {
                            Link(destination: url) {
                                Text("From \(world.album) \u{203A}").font(.caption).foregroundStyle(Theme.accent)
                            }
                            .accessibilityLabel("Open the album \(world.album) on Apple Music")
                        }
                    }
                    Spacer()
                    Button {
                        if st.playing { engine.leaveWorld() }
                        else if let w = engine.worlds.first(where: { $0.worldID == st.worldID }) { engine.enterWorld(w) }
                    } label: {
                        Image(systemName: st.playing ? "stop.circle.fill" : "play.circle.fill")
                            .font(.system(size: 44)).foregroundStyle(Theme.accent)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel(st.playing ? "Leave \(st.name)" : "Enter \(st.name)")
                }
                .padding(.vertical, 6)
            } else {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Step into a Dimension").font(.title3.weight(.semibold))
                    Text("Choose one below. Headphones recommended.").font(.caption).foregroundStyle(.secondary)
                }
                .padding(.vertical, 6)
            }
        }
    }

    private func reason(_ st: AtmosWorldStatus) -> String {
        if !st.playing { return "Paused" }
        if !st.adaptive { return "Playing" }
        if st.composed { return "Playing as composed. Turn on a sense and the song will follow you." }
        return st.sectionLabel.isEmpty ? "Following you" : st.sectionLabel
    }
}

/// Built-in Dimensions (and your own, made in Studio). Each row shows how many of its
/// tracks are on this device; open it to download or remove tracks one by one or all.
private struct DimensionPickerSection: View {
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @Binding var showPaywall: Bool

    var body: some View {
        Section {
            if engine.worlds.isEmpty {
                Text("No Dimensions in this build yet.").foregroundStyle(.secondary)
            }
            ForEach(engine.worlds, id: \.worldID) { world in
                row(world)
            }
        } header: {
            Text("Dimensions")
        } footer: {
            Text("Open a Dimension to choose which of its tracks to download. A Dimension plays the tracks you have; remove any track to free up space, and download it again any time.")
        }
        .onAppear { engine.refreshWorlds() }
    }

    private func isBundled(_ w: AtmosWorldInfo) -> Bool { w.audioAvailable || w.userMade }

    @ViewBuilder private func row(_ world: AtmosWorldInfo) -> some View {
        let locked = requiredTier(world) > store.tier
        let current = engine.worldStatus.playing && engine.worldStatus.worldID == world.worldID
        HStack(spacing: 12) {
            NavigationLink {
                DimensionTracksView(world: world, showPaywall: $showPaywall)
            } label: {
                HStack(spacing: 12) {
                    Image(systemName: current ? "waveform" : (locked ? "lock.fill" : "square.stack.3d.up"))
                        .foregroundStyle(current ? Theme.accent : .secondary)
                        .frame(width: 24)
                        .symbolEffect(.variableColor.iterative, isActive: current)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(world.name).font(.headline)
                        Text(subtitle(world, locked: locked)).font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
            if !locked && engine.isPlayable(world) {
                Button {
                    if current { engine.leaveWorld() } else { engine.enterWorld(world) }
                } label: {
                    Image(systemName: current ? "stop.circle.fill" : "play.circle.fill")
                        .font(.title2).foregroundStyle(Theme.accent)
                }
                .buttonStyle(.borderless)
                .accessibilityLabel(current ? "Leave \(world.name)" : "Enter \(world.name)")
            }
        }
        .swipeActions {
            if world.userMade {
                Button("Delete", role: .destructive) { engine.deleteDimension(world) }
            }
        }
    }

    private func requiredTier(_ w: AtmosWorldInfo) -> Tier {
        switch w.tier { case "studio": .studio; case "experience": .experience; default: .free }
    }

    private func subtitle(_ w: AtmosWorldInfo, locked: Bool) -> String {
        var parts: [String] = []
        if !w.artist.isEmpty { parts.append(w.artist) }
        if !w.album.isEmpty && w.album != w.name { parts.append(w.album) }
        if w.userMade { parts.append("yours") }
        if locked { parts.append(requiredTier(w).title) }
        else if isBundled(w) { parts.append(w.songCount > 1 ? "\(w.songCount) tracks" : "1 track") }
        else {
            let total = engine.downloads.tracks(w.worldID).count
            let have = engine.downloads.installedCount(w.worldID)
            if engine.downloads.isDownloading(w.worldID) { parts.append("downloading") }
            parts.append(have == 0 ? "\(total) track\(total == 1 ? "" : "s") to download" : "\(have) of \(total) on this \(Platform.deviceNoun)")
        }
        return parts.joined(separator: " · ")
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

/// A Dimension's tracks: download or remove each one, or all of them, and enter it.
struct DimensionTracksView: View {
    let world: AtmosWorldInfo
    @Binding var showPaywall: Bool
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var confirmRemoveAll = false
    @State private var showMoonChart = false

    private var tracks: [DimensionDownloads.Track] { engine.downloads.tracks(world.worldID) }
    private var bundled: Bool { world.audioAvailable || world.userMade }
    private var locked: Bool {
        let need: Tier = world.tier == "studio" ? .studio : world.tier == "experience" ? .experience : .free
        return need > store.tier
    }

    var body: some View {
        let current = engine.worldStatus.playing && engine.worldStatus.worldID == world.worldID
        List {
            if !world.album.isEmpty {
                Section {
                    LabeledContent("From the album", value: world.album)
                    if !world.artist.isEmpty { LabeledContent("Artist", value: world.artist) }
                    if let url = engine.albums.link(for: world) {
                        Link(destination: url) {
                            Label("Listen to the full album on Apple Music", systemImage: "music.note")
                        }
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
            }
            Section {
                if locked {
                    Button("Unlock \(world.name)") { showPaywall = true }
                } else {
                    Button {
                        if current { engine.leaveWorld() } else { engine.enterWorld(world) }
                    } label: {
                        Label(current ? "Leave" : "Enter", systemImage: current ? "stop.fill" : "play.fill")
                    }
                    .disabled(!engine.isPlayable(world))
                }
            } footer: {
                if !locked && !engine.isPlayable(world) {
                    Text("Download at least one track to enter.")
                } else if !locked && !bundled && engine.downloads.installedCount(world.worldID) < tracks.count {
                    Text("Plays the tracks on this \(Platform.deviceNoun). Tracks you download later join the next time you enter.")
                }
            }

            Section {
                if bundled {
                    ForEach(engine.bridgeSongTitles(world), id: \.self) { Text($0) }
                } else {
                    ForEach(tracks) { t in trackRow(t) }
                }
            } header: {
                Text("Tracks")
            } footer: {
                if !bundled, engine.downloads.bytes(world.worldID) > 0 {
                    Text("Using \(ByteCountFormatter.string(fromByteCount: engine.downloads.bytes(world.worldID), countStyle: .file)) on this \(Platform.deviceNoun).")
                }
            }

            if !bundled && !locked && tracks.count > 1 {
                Section {
                    if engine.downloads.installedCount(world.worldID) < tracks.count {
                        Button("Download All Tracks", systemImage: "arrow.down.circle") { engine.downloads.downloadAll(world.worldID) }
                    }
                    if engine.downloads.installedCount(world.worldID) > 0 {
                        Button("Remove All Tracks", systemImage: "trash", role: .destructive) { confirmRemoveAll = true }
                    }
                }
            }
        }
        .navigationTitle(world.name)
        .sheet(isPresented: $showMoonChart) { MoonChartView(initialDimensionID: world.worldID) }
        .inlineNavigationTitle()
        .onAppear { engine.refreshWorlds() }
        .confirmationDialog("Remove every track of \(world.name) from this \(Platform.deviceNoun)?",
                            isPresented: $confirmRemoveAll, titleVisibility: .visible) {
            Button("Remove All Tracks", role: .destructive) { engine.removeAllTracks(world) }
        } message: {
            Text("You can download them again any time.")
        }
    }

    @ViewBuilder private func trackRow(_ t: DimensionDownloads.Track) -> some View {
        let state = engine.downloads.state(t.id)
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(t.title)
                switch state {
                case .installed:
                    Text(engine.downloads.sizes[t.id].map { ByteCountFormatter.string(fromByteCount: $0, countStyle: .file) } ?? "On this \(Platform.deviceNoun)")
                        .font(.caption).foregroundStyle(.secondary)
                case .downloading(let p):
                    Text(String(format: "Downloading %.0f%%", p * 100)).font(.caption).foregroundStyle(.secondary)
                case .failed(let why):
                    Text("Download failed: \(why)").font(.caption).foregroundStyle(Theme.danger)
                case .notInstalled:
                    EmptyView()
                }
            }
            Spacer()
            if locked {
                Image(systemName: "lock.fill").foregroundStyle(.secondary)
            } else {
                switch state {
                case .installed:
                    Button(role: .destructive) { engine.removeTrack(t) } label: {
                        Image(systemName: "trash").foregroundStyle(Theme.danger)
                    }
                    .buttonStyle(.borderless)
                    .accessibilityLabel("Remove \(t.title)")
                case .downloading(let p):
                    Button { engine.downloads.cancel(t) } label: {
                        ZStack {
                            ProgressView(value: p).progressViewStyle(.circular)
                            Image(systemName: "stop.fill").font(.system(size: 8))
                        }
                    }
                    .buttonStyle(.borderless)
                    .accessibilityLabel("Cancel download of \(t.title)")
                case .notInstalled, .failed:
                    Button { engine.downloads.download(t) } label: {
                        Image(systemName: "arrow.down.circle").font(.title2).foregroundStyle(Theme.accent)
                    }
                    .buttonStyle(.borderless)
                    .accessibilityLabel("Download \(t.title)")
                }
            }
        }
    }
}
