import SwiftUI

/// Saved projects (.viksen packages in Documents/Projects): open or delete.
struct ProjectsSection: View {
    @Environment(EngineController.self) private var engine
    @State private var projects: [URL] = []
    @State private var confirmOpen: URL?

    var body: some View {
        Section("Projects") {
            if projects.isEmpty {
                Text("Save a project from Experience (\u{2026} menu) to keep your song, levels, atmosphere and Dimension together.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
            ForEach(projects, id: \.self) { url in
                Button { confirmOpen = url } label: {
                    VStack(alignment: .leading) {
                        Text(url.deletingPathExtension().lastPathComponent)
                        if let d = modified(url) {
                            Text(d.formatted(date: .abbreviated, time: .shortened)).font(.caption).foregroundStyle(.secondary)
                        }
                    }
                }
            }
            .onDelete { idx in
                for i in idx { try? FileManager.default.removeItem(at: projects[i]) }
                AppLog.info("Project deleted")
                refresh()
            }
        }
        .onAppear(perform: refresh)
        .confirmationDialog("Open this project? Current settings will be replaced.",
                            isPresented: Binding(get: { confirmOpen != nil }, set: { if !$0 { confirmOpen = nil } }),
                            titleVisibility: .visible) {
            Button("Open") { if let u = confirmOpen { engine.openProject(u) }; confirmOpen = nil }
        }
    }

    private func refresh() {
        let urls = (try? FileManager.default.contentsOfDirectory(at: EngineController.projectsFolder,
                                                                  includingPropertiesForKeys: [.contentModificationDateKey])) ?? []
        projects = urls.filter { $0.pathExtension == "viksen" }
            .sorted { (modified($0) ?? .distantPast) > (modified($1) ?? .distantPast) }
    }

    private func modified(_ url: URL) -> Date? {
        try? url.appendingPathComponent("manifest.json").resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate
    }
}
