import SwiftUI
import UniformTypeIdentifiers

/// Studio: Dimensions you made or imported (built-ins are listed in Experience).
/// Import a Dimension folder, open one to arrange or export it, swipe to delete.
struct DimensionsManagerView: View {
    @Environment(EngineController.self) private var engine
    @State private var showImporter = false

    var body: some View {
        List {
            let mine = engine.worlds.filter(\.userMade)
            if mine.isEmpty {
                ContentUnavailableView("No Dimensions of your own yet", systemImage: "square.stack.3d.up",
                                       description: Text("Save one from your Studio mix, or import a Dimension folder."))
            }
            ForEach(mine, id: \.worldID) { world in
                NavigationLink {
                    WorldDetailView(world: world)
                } label: {
                    VStack(alignment: .leading, spacing: 3) {
                        Text(world.name).font(.headline)
                        Text(subtitle(world)).font(.caption).foregroundStyle(.secondary)
                    }
                }
                .swipeActions {
                    Button("Delete", role: .destructive) { engine.deleteDimension(world) }
                }
            }
        }
        .navigationTitle("Your Dimensions")
        .toolbar {
            ToolbarItem(placement: .barTrailing) {
                Button { showImporter = true } label: { Label("Import", systemImage: "square.and.arrow.down") }
            }
        }
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [.folder]) { result in
            if case let .success(url) = result { engine.importDimension(from: url) }
        }
        .onAppear { engine.refreshWorlds() }
    }

    private func subtitle(_ w: AtmosWorldInfo) -> String {
        var parts: [String] = []
        if !w.artist.isEmpty { parts.append(w.artist) }
        if w.tempo > 0 { parts.append(String(format: "%.0f BPM", w.tempo)) }
        if !w.musicalKey.isEmpty { parts.append(w.musicalKey) }
        if !w.audioAvailable { parts.append("music not in this build") }
        return parts.joined(separator: " · ")
    }
}

struct WorldDetailView: View {
    let world: AtmosWorldInfo
    @State private var confirmRights = false
    @State private var exportURL: URL?
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var showPaywall = false

    private var isCurrent: Bool { engine.worldStatus.loaded && engine.worldStatus.worldID == world.worldID }
    private var isPlaying: Bool { isCurrent && engine.worldStatus.playing }
    private var requiredTier: Tier { world.tier == "studio" ? .studio : world.tier == "experience" ? .experience : .free }

    var body: some View {
        @Bindable var engine = engine
        List {
            Section {
                VStack(alignment: .leading, spacing: 6) {
                    Text(world.name).font(.title2.weight(.semibold))
                    if world.tempo > 0 {
                        Text(String(format: "%.0f BPM · %@", world.tempo, world.musicalKey))
                            .font(.subheadline).foregroundStyle(.secondary)
                    }
                }
                if !world.audioAvailable {
                    Label("This build does not include this Dimension's music yet.", systemImage: "exclamationmark.triangle")
                        .font(.footnote).foregroundStyle(Theme.micRaw)
                } else if store.tier < requiredTier {
                    Button("Unlock with \(requiredTier.title)") { showPaywall = true }
                } else {
                    Button {
                        isPlaying ? engine.leaveWorld() : engine.enterWorld(world)
                    } label: {
                        Label(isPlaying ? "Leave Dimension" : "Enter Dimension", systemImage: isPlaying ? "stop.fill" : "play.fill")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                }
                if isCurrent && engine.worldStatus.lengthSeconds > 0 {
                    ProgressView(value: engine.worldStatus.positionSeconds, total: engine.worldStatus.lengthSeconds)
                        .tint(Theme.music)
                }
            }

            if world.userMade {
                Section {
                    if let exportURL {
                        ShareLink(item: exportURL) { Label("Share \u{201C}\(world.name)\u{201D}", systemImage: "square.and.arrow.up") }
                    } else {
                        Button("Export\u{2026}") { confirmRights = true }
                    }
                } footer: {
                    Text("Exports this Dimension and its audio as a .zip file.")
                }
            }

            Section {
                Toggle("3D headphone audio", isOn: $engine.dimension3D)
                if isCurrent && engine.dimension3D {
                    Gated(feature: .spatialCanvas, title: "Arrange instruments in 3D") {
                        SpatialCanvasView()
                        Button("Reset layout") { engine.resetPositions(world.worldID) }
                            .font(.footnote)
                    }
                }
                if engine.headTracker.isSupported {
                    Gated(feature: .headTracking, title: "Follow my head (AirPods)") {
                        Toggle("Follow my head (AirPods)", isOn: $engine.headTrackingEnabled)
                    }
                }
            } header: {
                Text("Space")
            } footer: {
                Text(engine.binauralHRTF
                     ? "Binaural 3D (Steam Audio HRTF). Drag an instrument to move it around you. Best with headphones."
                     : "Simplified 3D in this build (no HRTF). Drag an instrument to move it around you.")
            }

            if isCurrent {
                Section("Layers") {
                    ForEach(Array(engine.worldStatus.stems.enumerated()), id: \.offset) { _, stem in
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                Text(stem.title)
                                Spacer()
                                Text(stem.gainDb <= -59 ? "silent" : String(format: "%.0f dB", stem.gainDb))
                                    .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                            }
                            LevelMeter(peak: stem.peak, color: Theme.music)
                            if let reason = stem.reasons.first {
                                Text(reason).font(.caption).foregroundStyle(Theme.accent)
                            }
                        }
                    }
                }
            }

            Section {
                Toggle("Microphone", isOn: Binding(get: { engine.micEnabled }, set: { engine.setMicEnabled($0) }))
                if engine.motionSensingAvailable {
                    Toggle("Motion sensing", isOn: $engine.motionSensing)
                }
                if engine.micEnabled || engine.motionSensing {
                    EnvironmentChips(meters: engine.meters)
                }
            } header: {
                Text("What this Dimension listens to")
            } footer: {
                Text("With the microphone and motion sensing on, layers come and go: quiet brings strings and woodwinds, walking brings percussion, bass and guitar, loud places raise the synth, and workout intensity from Health pushes the rhythm. With them off you hear the music as composed.")
            }
        }
        .navigationTitle(world.name)
        .inlineNavigationTitle()
        .sheet(isPresented: $showPaywall) { PaywallView(highlight: requiredTier) }
        .confirmationDialog("Do you have the right to share all of this audio?", isPresented: $confirmRights,
                            titleVisibility: .visible) {
            Button("Yes, I have the rights") { exportURL = engine.exportDimension(world) }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Only share music and recordings you made or are licensed to share. Owning a copy of a song does not give you the right to redistribute it.")
        }
    }
}

/// Small read-only view of the current environment states.
struct EnvironmentChips: View {
    let meters: AtmosMeterSnapshot

    var body: some View {
        HStack(spacing: 6) {
            chip("Quiet", meters.quiet)
            chip("Loud", meters.loud)
            if meters.motionAvailable {
                chip("Still", meters.still)
                chip("Moving", meters.moving)
            }
        }
    }

    private func chip(_ t: String, _ on: Bool) -> some View {
        Text(t)
            .font(.caption.weight(.semibold))
            .padding(.horizontal, 10).padding(.vertical, 4)
            .background(on ? Theme.accent : Color.secondary.opacity(0.15), in: Capsule())
            .foregroundStyle(on ? Color.white : Color.secondary)
    }
}
