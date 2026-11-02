import SwiftUI

struct LibraryView: View {
    @Environment(MediaLibrary.self) private var library
    @Environment(EngineController.self) private var engine
    @Environment(Store.self) private var store
    @State private var showPaywall = false
    @State private var query = ""

    private var visibleItems: [MediaLibrary.Item] { library.items.filter { matches($0.name) } }

    private func matches(_ name: String) -> Bool {
        query.trimmingCharacters(in: .whitespaces).isEmpty || name.localizedCaseInsensitiveContains(query)
    }

    var body: some View {
        NavigationStack {
            List {
                if library.items.isEmpty {
                    ContentUnavailableView("No audio yet", systemImage: "music.note",
                                           description: Text("Songs you import from Experience are copied here. Originals are never changed."))
                }
                ForEach(visibleItems) { item in
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
                .onDelete { idx in idx.map { visibleItems[$0] }.forEach(library.delete) } // indices refer to the filtered list

                ProjectsSection(query: query)
                RecordingsSection(query: query)
            }
            .navigationTitle("Library")
            .searchable(text: $query, prompt: "Songs, projects, recordings")
            .refreshable { library.refresh() }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: .studio) }
        }
    }
}
