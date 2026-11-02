import SwiftUI

@main
struct ViksenWatchApp: App {
    @State private var workout = WorkoutManager()

    var body: some Scene {
        WindowGroup {
            WorkoutView()
                .environment(workout)
        }
    }
}
