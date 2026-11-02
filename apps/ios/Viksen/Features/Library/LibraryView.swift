import SwiftUI

struct LibraryView: View {
    @Environment(MediaLibrary.self) private var library
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var showPaywall = false

    var body: some View {
        NavigationStack {
            List {
                if library.items.isEmpty {
                    ContentUnavailableView("No audio yet", systemImage: "music.note",
                                           description: Text("Songs you import from Experience are copied here. Originals are never changed."))
                }
                ForEach(library.items) { item in
                    Button {
                        if store.allows(.musicImport) { engine.loadTrack(url: item.url) } else { showPaywall = true }
                    } label: {
                        VStack(alignment: .leading) {
                            Text(item.name).lineLimit(1)
                            Text(ByteCountFormatter.string(fromByteCount: item.byteSize, countStyle: .file))
                                .font(.caption).foregroundStyle(.secondary)
                        }
                    }
                    .accessibilityHint("Loads this file into Experience")
                }
                .onDelete { idx in idx.map { library.items[$0] }.forEach(library.delete) }

                ProjectsSection()
                RecordingsSection()
            }
            .navigationTitle("Library")
            .refreshable { library.refresh() }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: .studio) }
        }
    }
}
