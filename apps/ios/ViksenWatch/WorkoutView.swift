import SwiftUI

struct WorkoutView: View {
    @Environment(WorkoutManager.self) private var workout

    var body: some View {
        @Bindable var workout = workout
        ScrollView {
            VStack(spacing: 10) {
                Text("VIKSEN")
                    .font(.system(size: 16, weight: .ultraLight))
                    .tracking(6)
                if workout.isRunning {
                    Text(workout.heartRate.map { String(format: "%.0f", $0) } ?? "--")
                        .font(.system(size: 44, weight: .semibold, design: .rounded))
                        .monospacedDigit()
                    Text("BPM").font(.caption2).foregroundStyle(.secondary)
                    Label(workout.phoneReachable ? "Sending to iPhone" : "iPhone not reachable",
                          systemImage: workout.phoneReachable ? "iphone.radiowaves.left.and.right" : "iphone.slash")
                        .font(.caption2)
                        .foregroundStyle(workout.phoneReachable ? .green : .secondary)
                    Button("End Workout", role: .destructive) { workout.stop() }
                } else {
                    Picker("Workout", selection: $workout.kind) {
                        ForEach(WorkoutManager.Kind.allCases) { Text($0.title).tag($0) }
                    }
                    Button("Start") { workout.start() }
                        .tint(Color(red: 0.55, green: 0.45, blue: 0.98))
                    Text("Your heart rate shapes the sound in Viksen on your iPhone.")
                        .font(.caption2)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.secondary)
                }
                if let error = workout.lastError {
                    Text(error).font(.caption2).foregroundStyle(.red)
                }
            }
        }
    }
}
