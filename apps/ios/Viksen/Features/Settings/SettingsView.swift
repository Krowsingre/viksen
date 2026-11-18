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
                        LabeledContent { Text(store.subscribed ? "All Dimensions" : store.owned.isEmpty ? "Free" : "\(store.owned.count) owned") } label: {
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

/// What the listener has: the subscription, the Dimensions they own, and buying,
/// managing and restoring. Single Dimensions are also bought from their own page.
struct MembershipPage: View {
    @Environment(Store.self) private var store
    @Environment(EngineController.self) private var engine
    @State private var showPaywall = false

    var body: some View {
        let forSale = engine.worlds.filter { Edition(tier: $0.tier) != .free }
        List {
            Section {
                if store.subscribed {
                    LabeledContent("Viksen All Dimensions", value: "Active")
                    if let renews = store.subscriptionRenewal {
                        LabeledContent("Renews or ends", value: renews.formatted(date: .abbreviated, time: .omitted))
                    }
                } else {
                    LabeledContent("Viksen All Dimensions", value: "Not subscribed")
                }
            } footer: {
                Text(store.subscribed
                     ? "Every Dimension plays while you are subscribed. Dimensions you bought stay yours if you stop."
                     : "Viksen is free. Buy any other Dimension to keep, or subscribe to play them all\(store.trialAvailable ? ", free for the first week" : "").")
            }
            Section("Dimensions") {
                ForEach(engine.worlds, id: \.worldID) { w in
                    LabeledContent(w.name) {
                        if Edition(tier: w.tier) == .free { Text("Free") }
                        else if store.owns(w) { Text("Owned") }
                        else if store.subscribed { Text("Subscription") }
                        else { Text(store.product(for: w)?.displayPrice ?? "–") }
                    }
                }
            }
            Section {
                if store.subscribed, let url = URL(string: "https://apps.apple.com/account/subscriptions") {
                    Link("Manage subscription", destination: url)
                } else if !forSale.isEmpty {
                    Button("Subscribe or buy…") { showPaywall = true }
                }
                Button("Restore Purchases") { Task { await store.restore() } }
            } footer: {
                Text("Purchases belong to your Apple Account, so they come back on any of your devices with Restore Purchases.")
            }
            if let error = store.lastError {
                Section { Text(error).foregroundStyle(Theme.danger) }
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
    @AppStorage(PictureInPicture.autoKey) private var pipAuto = true

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
            #if os(iOS)
            Section {
                Toggle("Start automatically", isOn: $pipAuto)
                    .onChange(of: pipAuto) { NotificationCenter.default.post(name: PictureInPicture.settingChanged, object: nil) }
            } header: { Text("Picture in Picture") } footer: {
                Text("When you leave Viksen while a Dimension plays, a small window with its badge stays on screen: play or pause it, and step to your other downloaded Dimensions.")
            }
            #endif
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
                Label("\(Lunar.phaseName(engine.moonPhaseID)) · lunar day \(engine.lunarDayLabel)",
                      systemImage: Lunar.symbol(engine.moonPhaseID))
                Picker("Night", selection: Bindable(engine.nightClock).rule) {
                    ForEach(NightClock.Rule.allCases) { Text($0.title).tag($0) }
                }
            } footer: { Text(nightFooter) }
            Section {
                Picker("Lunar day count", selection: Bindable(engine).lunarDayStyle) {
                    ForEach(LunarDayStyle.allCases) { Text($0.title).tag($0) }
                }
            } footer: {
                Text("How the lunar day is numbered. Traditional lunar calendars count the day of the new moon as day 1; the Moon's age counts it as day 0. Only the number changes: the phases, their songs and the Moon Chart follow the moon itself.")
            }
            Section {
                Text("Viksen follows the lunar calendar, worked out on this \(Platform.deviceNoun) in your time zone: New Moon, First Quarter, Full Moon and Last Quarter are the days they happen on, and the days between are the crescent and gibbous phases. A night belongs to the day it began on, so a phase's song plays its whole night. The fuller the moon, the longer and wider the echoes and reverb that carry the music from one part to the next, and the wider the room blend; near the new moon, changes are closer and drier.")
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
