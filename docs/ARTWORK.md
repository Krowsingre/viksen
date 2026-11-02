# Artwork

Until artwork is supplied, Home and the Dimension cards use a scene drawn in code
(`Shared/DimensionArt.swift`). Owner artwork replaces it automatically:

| Image | Where it goes | Size | Used for |
|---|---|---|---|
| `HomeBackground` | asset catalog (`Resources/Assets.xcassets`, image set) | ~1290 × 2796 px, portrait | Home on iPhone |
| `HomeBackgroundWide` | asset catalog | ~2400 × 1800 px | Home on wide / unfolded layouts (falls back to `HomeBackground`) |
| `art.jpg` or `art.png` | `Resources/Worlds/<dimension id>/` | ~800 px wide, 16:10 | Continue Listening card for that Dimension |

Art must be free of UI (no text, buttons or cards) and the owner must hold the rights
to use it in the App Store app.
