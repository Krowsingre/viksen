# Artwork

Home uses the owner's art (decision #71): `HomeSky` (night sky), `MoonRing` (the eight
moons; tonight's is made to pulse in code), `FoxPlay` (fox with play button) and
`FoxEmpty` (the same fox with the button's circle emptied, so the app draws a live
waveform in it while playing). Until artwork is supplied, the Dimension cards use a
scene drawn in code (`Shared/DimensionArt.swift`).
Owner artwork replaces it automatically:

| Image | Where it goes | Size | Used for |
|---|---|---|---|
| `art.jpg` or `art.png` | `Resources/Worlds/<dimension id>/` | ~800 px wide, 16:10 | Continue Listening card for that Dimension |

Art must be free of UI (no text, buttons or cards) and the owner must hold the rights
to use it in the App Store app.
