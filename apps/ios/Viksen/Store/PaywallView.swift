import StoreKit
import SwiftUI

/// Buying: one Dimension to keep (when opened from a Dimension), or the "Viksen All
/// Dimensions" subscription. Shows real App Store prices; every button performs a real
/// purchase or restore.
struct PaywallView: View {
    var dimension: AtmosWorldInfo? = nil
    @Environment(Store.self) private var store
    @Environment(\.dismiss) private var dismiss

    static let terms = URL(string: "https://www.apple.com/legal/internet-services/itunes/dev/stdeula/")!

    var body: some View {
        NavigationStack {
            List {
                if let dimension {
                    Section {
                        if store.hasAccess(dimension) {
                            Label(store.owns(dimension) ? "You own \(dimension.name)" : "Included in your subscription",
                                  systemImage: "checkmark.seal.fill")
                                .foregroundStyle(Theme.accent)
                        }
                        if !store.owns(dimension) {
                            if let product = store.product(for: dimension) {
                                buyButton(product, title: "Buy \(dimension.name)")
                            } else if store.isLoading {
                                ProgressView()
                            } else {
                                Text("Not available right now.").foregroundStyle(.secondary)
                            }
                        }
                    } header: { Text("Keep it") } footer: {
                        Text("A one-time purchase. \(dimension.name) is yours to keep, on all your devices with your Apple Account, whether or not you subscribe.")
                    }
                }
                Section {
                    if store.subscribed {
                        Label("You are subscribed", systemImage: "checkmark.seal.fill").foregroundStyle(Theme.accent)
                        if let url = URL(string: "https://apps.apple.com/account/subscriptions") {
                            Link("Manage subscription", destination: url)
                        }
                    } else if store.subscriptionProducts.isEmpty {
                        if store.isLoading { ProgressView() } else { Text("Not available right now.").foregroundStyle(.secondary) }
                    } else {
                        ForEach(store.subscriptionProducts, id: \.id) { product in
                            buyButton(product, title: product.id == ProductID.allYearly ? "Yearly" : "Monthly",
                                      detail: product.id == ProductID.allYearly ? "\(product.displayPrice) a year" : "\(product.displayPrice) a month")
                        }
                    }
                } header: { Text("Viksen All Dimensions") } footer: {
                    Text((store.trialAvailable && !store.subscribed ? "Free for one week, then the price shown. " : "")
                         + "Every Dimension, including new ones, while you are subscribed. Payment is charged to your Apple Account and renews automatically unless cancelled at least 24 hours before the end of the period; manage or cancel it in your Apple Account settings. Dimensions you bought stay yours.")
                }
                Section {
                    Button("Restore Purchases") { Task { await store.restore() } }
                    Link("Terms of Use", destination: Self.terms)
                    Link("Privacy Policy", destination: SiteLinks.privacy)
                }
                if let error = store.lastError {
                    Section { Text(error).foregroundStyle(Theme.danger) }
                }
            }
            .navigationTitle(dimension?.name ?? "Dimensions")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
            .task { if store.products.isEmpty { await store.loadProducts() } }
        }
    }

    private func buyButton(_ product: Product, title: String, detail: String? = nil) -> some View {
        Button {
            Task { await store.purchase(product) }
        } label: {
            HStack {
                Text(title)
                Spacer()
                if store.purchasingID == product.id { ProgressView() } else { Text(detail ?? product.displayPrice).bold() }
            }
        }
        .disabled(store.purchasingID != nil)
    }
}
