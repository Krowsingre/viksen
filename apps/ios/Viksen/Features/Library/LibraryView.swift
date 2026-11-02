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
                if store.tier >= .studio {
                Section("Songs") {
                if library.items.isEmpty {
                    Text("Songs you import in Studio are copied here. Originals are never changed.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                ForEach(visibleItems) { item in
                    Button {
                        if store.allows(.musicImport) { engine.loadStudio([.init(url: item.url)]) } else { showPaywall = true }
                    } label: {
                        VStack(alignment: .leading) {
                            Text(item.name).lineLimit(1)
                            Text(ByteCountFormatter.string(fromByteCount: item.byteSize, countStyle: .file))
                                .font(.caption).foregroundStyle(.secondary)
                        }
                    }
                    .accessibilityHint("Opens this song in Studio")
                }
                .onDelete { idx in idx.map { visibleItems[$0] }.forEach(library.delete) } // indices refer to the filtered list
                }
                ProjectsSection(query: query)
                }
                RecordingsSection(query: query)
            }
            .navigationTitle("Library")
            .searchable(text: $query, prompt: store.tier >= .studio ? "Songs, projects, recordings" : "Recordings")
            .refreshable { library.refresh() }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: .studio) }
        }
    }
}
