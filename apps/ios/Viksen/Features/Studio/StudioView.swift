import SwiftUI

struct StudioView: View {
    var body: some View {
        NotYetAvailableView(
            title: "Studio",
            systemImage: "slider.horizontal.3",
            phase: "Phase 2–3 · not built yet",
            summary: "The touch Studio will edit the same projects as the desktop app. The engine already supports multiple aligned layers, independent buses and project save/reopen; the editing UI is next.",
            planned: [
                "Touch timeline with layers and stems",
                "Layer gain, mute, solo and A/B against the original",
                "Spatial canvas: drag sources around the listener",
                "Effect and texture inspector",
                "Stereo / binaural export",
            ])
    }
}
