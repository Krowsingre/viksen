import SwiftUI
import UniformTypeIdentifiers

/// Studio (iPhone): your own stems, streamed in sync, with a mixer and 3D placement.
struct StudioView: View {
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var showImporter = false
    @State private var showPaywall = false
    @State private var showSave = false
    @State private var projectName = ""
    @State private var showSaveDimension = false
    @State private var dimensionName = ""

    var body: some View {
        @Bindable var engine = engine
        NavigationStack {
            List {
                if !store.allows(.stemImport) {
                    Section {
                        Text("Import your own songs and stems, mix them and place them in 3D around you.")
                        Button("Unlock Studio") { showPaywall = true }
                    }
                } else {
                    Section {
                        Button { showImporter = true } label: {
                            Label(engine.studioStems.isEmpty ? "Import stems\u{2026}" : "Replace stems\u{2026}",
                                  systemImage: "square.and.arrow.down.on.square")
                        }
                    } footer: {
                        Text("Choose up to 8 stems exported from the same song (WAV, AIFF, FLAC, M4A, MP3). They are copied into Viksen; your originals are never changed. Stems stay aligned at their start, including any leading silence.")
                    }

                    if engine.studioActive && !engine.studioStems.isEmpty {
                        transport
                        if engine.studioLengthsDiffer {
                            Section {
                                Label("These stems have different lengths. They are aligned at their start; shorter stems end early.",
                                      systemImage: "info.circle")
                                    .font(.footnote).foregroundStyle(.secondary)
                            }
                        }
                        mixer
                        Section("Space") {
                            Toggle("3D headphone audio", isOn: $engine.dimension3D)
                            if engine.dimension3D { SpatialCanvasView() }
                        }
                        Section {
                            Button("Save as Project\u{2026}") { projectName = ""; showSave = true }
                            Button("Save as Dimension\u{2026}") { dimensionName = ""; showSaveDimension = true }
                        } footer: {
                            Text("A Dimension of your own appears in the Dimensions tab with this mix and 3D layout.")
                        }
                    }
                }
            }
            .navigationTitle("Studio")
            .fileImporter(isPresented: $showImporter, allowedContentTypes: [.audio], allowsMultipleSelection: true) { result in
                if case let .success(urls) = result, !urls.isEmpty { engine.importStudioStems(urls) }
            }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: .studio) }
            .alert("Save as Dimension", isPresented: $showSaveDimension) {
                TextField("Name", text: $dimensionName)
                Button("Save") { engine.saveStudioAsDimension(named: dimensionName.isEmpty ? "My Dimension" : dimensionName) }
                Button("Cancel", role: .cancel) {}
            }
            .alert("Save as Project", isPresented: $showSave) {
                TextField("Name", text: $projectName)
                Button("Save") { engine.saveProject(named: projectName) }
                Button("Cancel", role: .cancel) {}
            }
        }
    }

    private var transport: some View {
        Section {
            HStack {
                Button { engine.studioTogglePlay() } label: {
                    Image(systemName: engine.studioPlaying ? "pause.circle.fill" : "play.circle.fill").font(.largeTitle)
                }
                .buttonStyle(.borderless)
                .accessibilityLabel(engine.studioPlaying ? "Pause" : "Play")
                VStack(alignment: .leading) {
                    let length = max(engine.worldStatus.lengthSeconds, engine.studioStems.map(\.lengthSeconds).max() ?? 0.001)
                    Slider(value: Binding(get: { min(engine.worldStatus.positionSeconds, length) },
                                          set: { engine.studioSeek($0) }), in: 0...max(length, 0.001))
                        .tint(Theme.music)
                        .accessibilityLabel("Position")
                    Text(String(format: "%@ / %@", time(engine.worldStatus.positionSeconds), time(length)))
                        .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                }
            }
        }
    }

    private var mixer: some View {
        Section("Mixer") {
            ForEach(Array(engine.studioStems.enumerated()), id: \.element.id) { index, stem in
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Text(stem.title).lineLimit(1)
                        Spacer()
                        Toggle("M", isOn: Binding(get: { stem.muted }, set: { engine.setStudioStem(stem.id, muted: $0) }))
                            .toggleStyle(.button).tint(Theme.danger)
                            .accessibilityLabel("Mute \(stem.title)")
                        Toggle("S", isOn: Binding(get: { stem.solo }, set: { engine.setStudioStem(stem.id, solo: $0) }))
                            .toggleStyle(.button).tint(Theme.micRaw)
                            .accessibilityLabel("Solo \(stem.title)")
                    }
                    HStack {
                        Slider(value: Binding(get: { stem.gainDb }, set: { engine.setStudioStem(stem.id, gainDb: $0) }),
                               in: -40...6)
                            .accessibilityLabel("\(stem.title) level")
                        Text(String(format: "%+.0f dB", stem.gainDb))
                            .font(.caption.monospacedDigit()).foregroundStyle(.secondary).frame(width: 52)
                    }
                    let peak = engine.worldStatus.stems.indices.contains(index) ? engine.worldStatus.stems[index].peak : 0
                    LevelMeter(peak: peak, color: Theme.music)
                }
            }
        }
    }

    private func time(_ s: Double) -> String {
        let t = Int(s)
        return String(format: "%ld:%02ld", t / 60, t % 60)
    }
}
