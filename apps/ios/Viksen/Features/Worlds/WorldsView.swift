import SwiftUI

struct WorldsView: View {
    var body: some View {
        NotYetAvailableView(
            title: "Worlds",
            systemImage: "globe.americas",
            phase: "Phase 6 · not built yet",
            summary: "Worlds will package sounds, processing, spatial motion and reactive rules into environments that respond to your surroundings. Until then, Experience offers the atmosphere bed and live microphone transformation directly.",
            planned: [
                "Starter Worlds (names provisional): Nocturnal City, Submerged, Nearfield, Lucid Forest",
                "Reactive rules driven by loudness, silence, transients and voice activity",
                "Create, duplicate, import and export World packages",
            ])
    }
}
