# App Store Connect: purchases and subscription

The purchase model (owner, 2026-11-07):

- **Free:** the Viksen Dimension and every feature of the app.
- **Each other Dimension: a one-time purchase, kept forever.** Standard Dimensions $4.99,
  special Dimensions $6.99.
- **Viksen All Dimensions subscription:** $2.99 a month or $24.99 a year, one-week free
  trial; every Dimension while subscribed. Bought Dimensions stay owned.
- **No Family Sharing** for now (leave it off on every product).

The app reads prices from the App Store, so changing a price in App Store Connect needs no
app update. A Dimension's edition comes from its world.json `"tier"`: `free`,
`standard` or `special`.

## 1. Business (once)

1. **Agreements, Tax, and Banking:** accept the **Paid Apps** agreement, then add bank
   account and tax forms. Purchases cannot be tested or sold until this shows *Active*.
2. Optional, recommended: join the **App Store Small Business Program** (Apple's
   commission becomes 15% while your proceeds stay under $1 million a year).

## 2. One in-app purchase per Dimension

My Apps > Viksen > Monetization > **In-App Purchases** > + > **Non-Consumable**.

| Field | The Shatter Gardens |
|---|---|
| Reference Name | The Shatter Gardens |
| Product ID | `com.krowsingre.viksen.dimension.steel-steam-and-borrowed-dreams` |
| Price | $4.99 (standard) |
| Family Sharing | Off |
| Display name | The Shatter Gardens |
| Description | An adaptive Dimension of four songs that follow day, night, the moon and the rain. |
| Review screenshot | Any screenshot of the Dimension's page |

The product ID is always `com.krowsingre.viksen.dimension.<dimension id>` (the folder
name under Resources/Worlds). For a special Dimension choose $6.99.

## 3. The subscription

Monetization > **Subscriptions** > create a **subscription group** named
`Viksen All Dimensions`, then add two subscriptions to it:

| Reference Name | Product ID | Duration | Price |
|---|---|---|---|
| All Dimensions Monthly | `com.krowsingre.viksen.all.monthly` | 1 month | $2.99 |
| All Dimensions Yearly | `com.krowsingre.viksen.all.yearly` | 1 year | $24.99 |

For each: Family Sharing off; display name and description (e.g. "Every Dimension, including
new ones, while you are subscribed."); a review screenshot of the purchase sheet; then
**Subscription Prices > Introductory Offer > Free > 1 week** for new subscribers.
Rank the yearly plan above the monthly one in the group.

Also fill in the group's **App Store localization** (display name: Viksen All Dimensions).

## 4. App Information

- **Privacy Policy URL:** https://krowsingre.github.io/viksen-docs/privacy.html
- **License agreement:** Apple's standard EULA (the purchase sheet links to it as Terms of Use).
- With the first App Store version, submit the in-app purchases and subscriptions together
  with the app (they appear in the version page's *In-App Purchases and Subscriptions* section).

## 5. Testing before release

TestFlight purchases are free sandbox purchases with your own Apple Account; subscriptions
renew quickly (a month lasts minutes). The products must be in *Ready to Submit* status
(complete metadata) to appear in TestFlight.
