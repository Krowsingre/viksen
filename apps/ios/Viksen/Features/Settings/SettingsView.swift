import SwiftUI

/// Settings (owner's plan, 2026-11-05): a short list of sub-pages. Membership holds the
/// tier and the subscription; everything about sound, the senses, the moon, the look,
/// privacy and help has its own page.
struct SettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(Store.self) private var store

    var body: some View {
        NavigationStack {
            List {
                Section {
                    NavigationLink { MembershipPage() } label: {
                        LabeledContent { Text(store.tier == .free ? "Free" : "Subscriber") } label: {
                            Label("Membership", systemImage: "crown")
                        }
                    }
                }
                Section {
                    NavigationLink { SoundPage() } label: { Label("Sound", systemImage: "speaker.wave.2") }
                    NavigationLink { SensesPage() } label: { Label("Senses", systemImage: "sensor") }
                    NavigationLink { MoonPage() } label: { Label("The Moon", systemImage: "moon") }
                    NavigationLink { AppearancePage() } label: { Label("Appearance", systemImage: "paintbrush") }
                }
                Section {
                    NavigationLink { PrivacyPage() } label: { Label("Privacy", systemImage: "hand.raised") }
                    NavigationLink { AboutPage() } label: { Label("About & Help", systemImage: "info.circle") }
                    NavigationLink { DiagnosticsView() } label: { Label("Diagnostics", systemImage: "stethoscope") }
                }
            }
            .navigationTitle("Settings")
            .toolbar { ToolbarItem(placement: .confirmationAction) { CloseButton(compact: true) { dismiss() } } }
        }
    }
}

// MARK: Membership

/// The tier, what it includes, subscribing, and restoring purchases. Single Dimensions
/// are bought from their own page under Dimensions.
struct MembershipPage: View {
    @Environment(Store.self) private var store
    @State private var showPaywall = false

    var body: some View {
        List {
            Section {
                LabeledContent("Your membership", value: store.tier == .free ? "Free" : store.tier.title)
            } footer: {
                Text(store.tier == .free
                     ? "Free includes the Viksen Dimension, the time of day, the moon, the Moon Chart and the basic senses."
                     : "Your subscription includes every Dimension except special editions, and all the senses.")
            }
            Section {
                if store.tier == .free {
                    Button("Subscribe…") { showPaywall = true }
                } else if let url = URL(string: "https://apps.apple.com/account/subscriptions") {
                    Link("Manage subscription", destination: url)
                }
                Button("Restore Purchases") { Task { await store.restore() } }
            } footer: {
                Text("Purchases belong to your Apple Account, so they come back on any of your devices with Restore Purchases.")
            }
        }
        .navigationTitle("Membership")
        .sheet(isPresented: $showPaywall) { PaywallView() }
    }
}

// MARK: Sound

struct SoundPage: View {
    @Environment(EngineController.self) private var engine
    @AppStorage(ThresholdSound.settingKey) private var openingSound = false

    var body: some View {
        @Bindable var engine = engine
        List {
            Section {
                Toggle("3D audio", isOn: $engine.dimension3D)
                Toggle("Adaptive music", isOn: $engine.adaptiveOnHeadphones)
            } header: { Text("With headphones") }
            Section {
                Toggle("3D audio", isOn: $engine.spatialOnSpeaker)
                Toggle("Adaptive music", isOn: $engine.adaptiveOnSpeaker)
            } header: { Text("On the \(Platform.deviceNoun)'s speaker") } footer: {
                Text("Adaptive music moves each song between its parts and shapes its mix with the moment: time, the moon, movement and heart rate (and, with headphones, how quiet or loud it is around you). Off, songs play as written; day, night and rain songs still change. 3D audio places the instruments around you; it is made for headphones.")
            }
            Section {
                Toggle("Blend the room", isOn: Binding(get: { engine.roomBlendOn }, set: { engine.setRoomBlend($0) }))
                    .disabled(!engine.onHeadphones)
                BlendSection()
            } header: { Text("Room blend") } footer: {
                Text(engine.onHeadphones
                     ? "Your surroundings, echoed on the song's beat and reverberated, under the music. Each song sets how the room sounds. Nothing is recorded, saved or uploaded."
                     : "Connect headphones to blend the room in: through a speaker the microphone would hear the music.")
            }
            Section {
                Toggle("Idle music", isOn: $openingSound)
                    .onChange(of: openingSound) { _, on in
                        if on { engine.threshold.start(engineAudioRunning: engine.audioRunning) } else { engine.threshold.fadeOut(seconds: 0.5) }
                    }
            } footer: {
                Text("Soft loops from Lunar Promise while no Dimension plays. They only play if nothing else is playing, never interrupt your own music, and fade away when a Dimension starts. Also in the menu, with its own volume.")
            }
            Section {
                LabeledSlider(title: "Output ceiling", value: $engine.limiterCeilingDb, range: -24...(-1), format: "%.0f dBFS")
            } header: { Text("Hearing safety") } footer: {
                Text("A safety limiter on the final output never lets the signal exceed this level. Immersive audio can mask traffic and other important sounds: stay aware of your surroundings.")
            }
        }
        .navigationTitle("Sound")
    }
}

// MARK: Senses

struct SensesPage: View {
    var body: some View {
        List {
            Section {
                SensesSection()
                BodyMotionSection()
            } footer: {
                Text("Senses steer the music: each part of a Dimension belongs to a moment (still, moving, quiet, loud, working out). Everything stays on this \(Platform.deviceNoun).")
            }
            Section {
                WeatherSection()
            } header: { Text("Weather") } footer: {
                Text("When it rains where you are, a Dimension plays its storm song, with rain and thunder under it.")
            }
        }
        .navigationTitle("Senses")
    }
}

// MARK: The Moon

struct MoonPage: View {
    @Environment(EngineController.self) private var engine

    private var nightFooter: String {
        var t = "By day each Dimension plays its day track. After dark it plays its night song, and that listening counts toward tonight's phase on the Moon Chart."
        if engine.nightClock.rule == .sunsetSunrise {
            t += engine.nightClock.locationDenied
                ? " Sunset and sunrise need your approximate location, which is off, so 7 pm to 4 am is used."
                : " Uses your approximate location, kept on this \(Platform.deviceNoun) only for this."
        }
        return t
    }

    var body: some View {
        List {
            Section {
                Label("\(Lunar.phaseName(engine.moonPhaseID)) · lunar day \(engine.lunarDay) of 30",
                      systemImage: Lunar.symbol(engine.moonPhaseID))
                Picker("Night", selection: Bindable(engine.nightClock).rule) {
                    ForEach(NightClock.Rule.allCases) { Text($0.title).tag($0) }
                }
            } footer: { Text(nightFooter) }
            Section {
                Text("Viksen follows the lunar calendar, worked out on this \(Platform.deviceNoun). The fuller the moon, the longer and wider the echoes and reverb that carry the music from one part to the next, and the wider the room blend; near the new moon, changes are closer and drier.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
        }
        .navigationTitle("The Moon")
    }
}

// MARK: Appearance

struct AppearancePage: View {
    @AppStorage("appearance") private var appearance: Appearance = .dark
    @AppStorage(HomeSkyBackground.settingKey) private var animated = true

    var body: some View {
        List {
            Section {
                Toggle("Animated background", isOn: $animated)
            } footer: {
                Text("The night sky on Home drifts slowly and its swirls brighten and dim. Also off when Reduce Motion is on.")
            }
            Section {
                Picker("Theme", selection: $appearance) {
                    ForEach(Appearance.allCases) { Text($0.title).tag($0) }
                }
            }
        }
        .navigationTitle("Appearance")
    }
}

// MARK: Privacy, About

struct PrivacyPage: View {
    @Environment(EngineController.self) private var engine

    var body: some View {
        List {
            Section {
                LabeledContent("Microphone", value: permissionText)
            } footer: {
                Text("All processing happens on this \(Platform.deviceNoun). Nothing is ever recorded or saved, and nothing is uploaded. Your Moon Chart syncs only through your own iCloud. No account is required.")
            }
        }
        .navigationTitle("Privacy")
    }

    private var permissionText: String {
        switch engine.micPermission {
        case .granted: "Allowed"
        case .denied: "Denied"
        default: "Not requested"
        }
    }
}

struct AboutPage: View {
    var body: some View {
        List {
            Section {
                LabeledContent("Version", value: (Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "–")
                               + " (" + (Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "–") + ")")
                LabeledContent("Music", value: "Krowsingre")
            }
        }
        .navigationTitle("About & Help")
    }
}
