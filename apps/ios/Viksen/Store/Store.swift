import Foundation
import Observation
import StoreKit

/// StoreKit 2 purchases. What the listener may play is derived only from verified
/// transactions Apple signs for their Apple Account; nothing is stored by us and no
/// server of ours is involved.
@MainActor
@Observable
final class Store {
    /// Dimensions bought (ids), kept forever.
    private(set) var owned: Set<String> = []
    /// An active "Viksen All Dimensions" subscription (including its free trial).
    private(set) var subscribed = false
    private(set) var subscriptionRenewal: Date?
    /// Changes whenever access changes (purchase, restore, renewal, expiry, refund).
    private(set) var accessVersion = 0
    private(set) var products: [String: Product] = [:]
    private(set) var isLoading = false
    private(set) var purchasingID: String?
    private(set) var trialAvailable = false
    var lastError: String?

    private var dimensionIDs: [String] = []
    private var updatesTask: Task<Void, Never>?

    init() {
        updatesTask = Task { [weak self] in
            for await update in Transaction.updates {
                if case .verified(let t) = update { await t.finish() }
                await self?.refreshEntitlements()
            }
        }
        Task { await refreshEntitlements() }
    }

    /// The Dimensions on sale (all but the free ones); call when the list is known.
    func configure(dimensionIDs ids: [String]) {
        guard ids != dimensionIDs else { return }
        dimensionIDs = ids
        Task { await loadProducts() }
    }

    func hasAccess(_ w: AtmosWorldInfo) -> Bool {
        Edition(tier: w.tier) == .free || subscribed || owned.contains(w.worldID) || testUnlockAll
    }

    /// TestFlight-only switch (Settings > Diagnostics > Testing): every Dimension plays
    /// without a purchase, to test before the products exist. Ignored in App Store builds.
    static let testUnlockKey = "test.unlockAll"
    var testUnlockAll: Bool = AppBuild.isTestFlight && UserDefaults.standard.bool(forKey: Store.testUnlockKey) {
        didSet {
            UserDefaults.standard.set(testUnlockAll, forKey: Self.testUnlockKey)
            accessVersion += 1
        }
    }
    func owns(_ w: AtmosWorldInfo) -> Bool { owned.contains(w.worldID) }

    func product(for w: AtmosWorldInfo) -> Product? { products[ProductID.dimension(w.worldID)] }
    var subscriptionProducts: [Product] { ProductID.subscriptions.compactMap { products[$0] } }

    func loadProducts() async {
        isLoading = true
        defer { isLoading = false }
        do {
            let list = try await Product.products(for: ProductID.subscriptions + dimensionIDs.map(ProductID.dimension))
            products = Dictionary(uniqueKeysWithValues: list.map { ($0.id, $0) })
            if let sub = products[ProductID.allMonthly]?.subscription ?? products[ProductID.allYearly]?.subscription {
                trialAvailable = await sub.isEligibleForIntroOffer
            }
        } catch {
            lastError = "Could not load prices: \(error.localizedDescription)"
        }
    }

    func refreshEntitlements() async {
        var bought: Set<String> = []
        var active = false
        var renewal: Date?
        for await result in Transaction.currentEntitlements {
            guard case .verified(let t) = result, t.revocationDate == nil else { continue }
            if let id = ProductID.dimensionID(for: t.productID) {
                bought.insert(id)
            } else if ProductID.subscriptions.contains(t.productID), (t.expirationDate ?? .distantFuture) > Date() {
                active = true
                renewal = max(renewal ?? .distantPast, t.expirationDate ?? .distantPast)
            }
        }
        if bought != owned || active != subscribed {
            owned = bought
            subscribed = active
            accessVersion += 1
            AppLog.info("Access: \(bought.count) Dimension(s) owned, subscription \(active ? "active" : "off")")
        }
        subscriptionRenewal = renewal
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
                lastError = "The purchase is waiting for approval."
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
