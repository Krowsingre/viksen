import SwiftUI

/// Built-in Worlds: music that re-mixes itself around the listener.
struct WorldsView: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        NavigationStack {
            List {
                if engine.worlds.isEmpty {
                    ContentUnavailableView("No Worlds yet", systemImage: "square.stack.3d.up",
                                           description: Text("Worlds will appear here."))
                }
                ForEach(engine.worlds, id: \.worldID) { world in
                    NavigationLink {
                        WorldDetailView(world: world)
                    } label: {
                        VStack(alignment: .leading, spacing: 3) {
                            HStack {
                                Text(world.name).font(.headline)
                                if engine.worldStatus.playing && engine.worldStatus.worldID == world.worldID {
                                    Image(systemName: "waveform").foregroundStyle(Theme.accent)
                                }
                            }
                            Text(subtitle(world)).font(.caption).foregroundStyle(.secondary)
                        }
                    }
                }
            }
            .navigationTitle("Worlds")
        }
    }

    private func subtitle(_ w: AtmosWorldInfo) -> String {
        var parts: [String] = []
        if !w.artist.isEmpty { parts.append(w.artist) }
        parts.append(String(format: "%.0f BPM", w.tempo))
        if !w.musicalKey.isEmpty { parts.append(w.musicalKey) }
        if !w.audioAvailable { parts.append("music not in this build") }
        return parts.joined(separator: " · ")
    }
}

struct WorldDetailView: View {
    let world: AtmosWorldInfo
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
                    Text(String(format: "%.0f BPM · %@", world.tempo, world.musicalKey))
                        .font(.subheadline).foregroundStyle(.secondary)
                }
                if !world.audioAvailable {
                    Label("This build does not include the World's music yet.", systemImage: "exclamationmark.triangle")
                        .font(.footnote).foregroundStyle(Theme.micRaw)
                } else if store.tier < requiredTier {
                    Button("Unlock with \(requiredTier.title)") { showPaywall = true }
                } else {
                    Button {
                        isPlaying ? engine.leaveWorld() : engine.enterWorld(world)
                    } label: {
                        Label(isPlaying ? "Leave World" : "Enter World", systemImage: isPlaying ? "stop.fill" : "play.fill")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                }
                if isCurrent && engine.worldStatus.lengthSeconds > 0 {
                    ProgressView(value: engine.worldStatus.positionSeconds, total: engine.worldStatus.lengthSeconds)
                        .tint(Theme.music)
                }
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
                Text("What the World listens to")
            } footer: {
                Text("With the microphone and motion sensing on, layers come and go: quiet brings strings and woodwinds, walking brings percussion, bass and guitar, loud places raise the synth, and workout intensity from Health pushes the rhythm. With them off you hear the music as composed.")
            }
        }
        .navigationTitle(world.name)
        .navigationBarTitleDisplayMode(.inline)
        .sheet(isPresented: $showPaywall) { PaywallView(highlight: requiredTier) }
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
