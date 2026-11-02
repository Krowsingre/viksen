import Foundation
import Observation
import StoreKit

/// StoreKit 2 purchases. The current tier is derived only from verified
/// transactions Apple signs for this device's account; nothing is stored by us
/// and no server of ours is involved.
@MainActor
@Observable
final class Store {
    private(set) var tier: Tier = .free
    private(set) var products: [String: Product] = [:]
    private(set) var isLoading = false
    private(set) var purchasingID: String?
    var lastError: String?

    /// Called whenever the tier changes (e.g. after a purchase, restore or refund).
    var onTierChange: ((Tier) -> Void)?

    private var updatesTask: Task<Void, Never>?

    init() {
        updatesTask = Task { [weak self] in
            for await update in Transaction.updates {
                if case .verified(let t) = update { await t.finish() }
                await self?.refreshEntitlements()
            }
        }
        Task { await loadProducts(); await refreshEntitlements() }
    }

    func allows(_ feature: Feature) -> Bool { tier >= feature.requiredTier }

    func loadProducts() async {
        isLoading = true
        defer { isLoading = false }
        do {
            let list = try await Product.products(for: ProductID.all)
            products = Dictionary(uniqueKeysWithValues: list.map { ($0.id, $0) })
        } catch {
            lastError = "Could not load prices: \(error.localizedDescription)"
        }
    }

    func refreshEntitlements() async {
        var best: Tier = .free
        for await result in Transaction.currentEntitlements {
            guard case .verified(let t) = result, t.revocationDate == nil else { continue }
            best = max(best, ProductID.tier(for: t.productID))
        }
        if best != tier {
            tier = best
            onTierChange?(best)
        }
    }

    /// Products to offer for the current tier (the upgrade only for Experience owners).
    var offers: [Product] {
        let ids: [String]
        switch tier {
        case .free: ids = [ProductID.experience, ProductID.studio]
        case .experience: ids = [ProductID.studioUpgrade]
        case .studio: ids = []
        }
        return ids.compactMap { products[$0] }
    }

    func purchase(_ product: Product) async {
        purchasingID = product.id
        defer { purchasingID = nil }
        do {
            switch try await product.purchase() {
            case .success(let verification):
                guard case .verified(let t) = verification else {
                    lastError = "The purchase could not be verified."
                    return
                }
                await t.finish()
                await refreshEntitlements()
            case .pending:
                lastError = "The purchase is pending approval."
            case .userCancelled:
                break
            @unknown default:
                break
            }
        } catch {
            lastError = error.localizedDescription
        }
    }

    /// "Restore Purchases" (required by App Review): re-syncs with the App Store.
    func restore() async {
        do {
            try await AppStore.sync()
            await refreshEntitlements()
        } catch {
            lastError = error.localizedDescription
        }
    }
}
