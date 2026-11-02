import StoreKit
import SwiftUI

/// Upgrade sheet. Shows real App Store prices; every button performs a real
/// purchase or restore.
struct PaywallView: View {
    var highlight: Tier = .experience
    @Environment(Store.self) private var store
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Label("Current: \(store.tier.title)", systemImage: "checkmark.seal")
                        .foregroundStyle(.secondary)
                }
                tierSection(.experience,
                            ["Room memory and frequency shifting", "Head tracking with AirPods",
                             "Activity, steps and workout intensity", "Weather-aware atmospheres (optional)",
                             "Expanded Worlds"])
                tierSection(.studio,
                            ["Everything in Experience", "Import songs and stems", "Spatial composition",
                             "Advanced effects and automation", "Export"])
                Section {
                    Button("Restore Purchases") { Task { await store.restore() } }
                    Link("Privacy Policy", destination: SiteLinks.privacy)
                } footer: {
                    Text("One-time purchases. They also unlock Viksen on Mac when it is available. Payment is charged to your Apple Account.")
                }
                if let error = store.lastError {
                    Section { Text(error).foregroundStyle(Theme.danger) }
                }
            }
            .navigationTitle("Upgrade")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
            .task { if store.products.isEmpty { await store.loadProducts() } }
        }
    }

    @ViewBuilder
    private func tierSection(_ tier: Tier, _ features: [String]) -> some View {
        Section {
            ForEach(features, id: \.self) { Label($0, systemImage: "checkmark").font(.subheadline) }
            if store.tier >= tier {
                Label("Unlocked", systemImage: "lock.open.fill").foregroundStyle(Theme.atmosphere)
            } else if let product = product(for: tier) {
                Button {
                    Task { await store.purchase(product) }
                } label: {
                    HStack {
                        Text(product.id == ProductID.studioUpgrade ? "Upgrade to Studio" : "Buy \(tier.title)")
                        Spacer()
                        if store.purchasingID == product.id { ProgressView() } else { Text(product.displayPrice).bold() }
                    }
                }
                .disabled(store.purchasingID != nil)
            } else if store.isLoading {
                ProgressView()
            } else {
                Text("Not available right now.").foregroundStyle(.secondary)
            }
        } header: {
            Text(tier.title).foregroundStyle(tier == highlight ? Theme.accent : .secondary)
        }
    }

    private func product(for tier: Tier) -> Product? {
        store.offers.first { ProductID.tier(for: $0.id) == tier }
    }
}

/// Wraps a gated control: shows it when the tier allows the feature, otherwise a
/// row that opens the upgrade sheet (a real path to purchase, not a fake control).
struct Gated<Content: View>: View {
    let feature: Feature
    let title: String
    @ViewBuilder var content: () -> Content
    @Environment(Store.self) private var store
    @State private var showPaywall = false

    var body: some View {
        if store.allows(feature) {
            content()
        } else {
            Button { showPaywall = true } label: {
                HStack {
                    Text(title).foregroundStyle(.primary)
                    Spacer()
                    Label(feature.requiredTier == .studio ? "Studio" : "Experience", systemImage: "lock.fill")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(Theme.accent)
                }
            }
            .sheet(isPresented: $showPaywall) { PaywallView(highlight: feature.requiredTier) }
        }
    }
}
