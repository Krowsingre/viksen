import AVFoundation
import SwiftUI

/// Session recordings stored on this iPhone: play, share, delete.
struct RecordingsSection: View {
    var query: String = ""
    private var visible: [URL] {
        files.filter { query.isEmpty || $0.deletingPathExtension().lastPathComponent.localizedCaseInsensitiveContains(query) }
    }
    @State private var files: [URL] = []
    @State private var player: AVAudioPlayer?
    @State private var playing: URL?
    @State private var errorText: String?

    var body: some View {
        Section("Recordings") {
            if files.isEmpty {
                Text("Recordings you make with Record Session appear here.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
            ForEach(visible, id: \.self) { url in
                HStack {
                    Button {
                        toggle(url)
                    } label: {
                        Image(systemName: playing == url ? "stop.circle.fill" : "play.circle.fill").font(.title2)
                    }
                    .buttonStyle(.borderless)
                    .accessibilityLabel(playing == url ? "Stop" : "Play")
                    VStack(alignment: .leading) {
                        Text(url.deletingPathExtension().lastPathComponent).lineLimit(1)
                        Text(details(url)).font(.caption).foregroundStyle(.secondary)
                    }
                    Spacer()
                    ShareLink(item: url) { Image(systemName: "square.and.arrow.up") }
                        .buttonStyle(.borderless)
                }
            }
            .onDelete { idx in
                for i in idx {
                    if playing == visible[i] { stop() }
                    try? FileManager.default.removeItem(at: visible[i])
                    AppLog.info("Recording deleted")
                }
                refresh()
            }
            if let errorText { Text(errorText).font(.footnote).foregroundStyle(Theme.danger) }
        }
        .onAppear(perform: refresh)
        .onDisappear(perform: stop)
    }

    private func refresh() {
        let dir = EngineController.recordingsFolder
        let urls = (try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.creationDateKey])) ?? []
        files = urls.filter { $0.pathExtension.lowercased() == "wav" }
            .sorted { ($0.creationDate ?? .distantPast) > ($1.creationDate ?? .distantPast) }
    }

    private func toggle(_ url: URL) {
        if playing == url { stop(); return }
        stop()
        do {
            let p = try AVAudioPlayer(contentsOf: url)
            p.play()
            player = p
            playing = url
            errorText = nil
        } catch {
            errorText = error.localizedDescription
        }
    }

    private func stop() {
        player?.stop()
        player = nil
        playing = nil
    }

    private func details(_ url: URL) -> String {
        let size = (try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0
        // 24-bit stereo at the recording's rate: show size; duration from the file header.
        let seconds = (try? AVAudioFile(forReading: url)).map { Double($0.length) / $0.fileFormat.sampleRate } ?? 0
        return String(format: "%ld:%02ld · %@", Int(seconds) / 60, Int(seconds) % 60,
                      ByteCountFormatter.string(fromByteCount: Int64(size), countStyle: .file))
    }
}

private extension URL {
    var creationDate: Date? { try? resourceValues(forKeys: [.creationDateKey]).creationDate }
}
