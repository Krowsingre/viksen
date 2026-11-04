# Viksen task list

The working list for the Viksen app, in the approved order (DECISIONS #46). Design
detail: `docs/TECHNICAL_PROPOSAL.md`. Decisions: `docs/DECISIONS.md`.

## Engine and content (milestone order A)
- [x] M0 Long-blend stall fix and regression test
- [ ] M0 Validator v1, offline golden renders, 2-hour stem-sync soak test
- [ ] M1 Musical clock with beat maps; overlapping song transitions; lasting effect tails and ambience; graceful Dimension change
- [ ] M2 Preparation tools (developer-only): analysis → draft package → review report → preview renders; reference mixes kept (approved)
- [ ] M3 Living compositions: clip/event layer, authored variation, transition graph and passages
- [ ] M4 Phases and context v2: selection and modification rules, priority/dwell/cooldown, weather in music rules, manual override
- [ ] M5 Spatial v2: source types, beat-synced movement, distance cues, alternating-ear preset, intensity and reduce motion
- [ ] M6 Lunar Fox Spirit: default Dimension built into the app, eight lunar songs, long-session and battery tests
- [ ] M7 Product: lock-screen and Control Center controls; store for album Dimensions; pricing (under discussion)

## Listener features (approved)
- [ ] Moon Chart: lunar wheel of the eight Phases; discovered Phases playable any time
- [ ] Discovery: a Phase is discovered by listening during its real moon; hidden Phases found by special moments
- [ ] Stay here: hold the current track (still reacts inside the track)
- [ ] iCloud sync of discoveries and settings; Restore Purchases (exists) everywhere purchases show
- [ ] Pricing: free core + subscription + album packs (+ special Dimensions sold only)

## Before launch
- [ ] **Create Viksen Studio as a separate app** (DECISIONS #47, #49): Reuvo (stem separation) + preparation tools; repository decision pending
- [ ] **Retire Studio from this app:** remove the hidden Studio code paths and Library songs/projects, and retire the Studio and Studio-upgrade purchases in App Store Connect (`com.krowsingre.viksen.studio`, `com.krowsingre.viksen.studio.upgrade`) before the first public release
- [ ] Revisit the Experience purchase once pricing is decided (the in-Dimension states are now "Phases")
- [ ] Privacy policy and support pages live (GitHub Pages) with the contact email
- [ ] App Review: submit the asset packs (album Dimensions) with the first release that sells them
- [ ] Make the repository private before launch (DECISIONS: public until go-live)

## Later
- [ ] Android (approved in principle, not scheduled)
- [ ] Windows and Mac apps catch up with the iPhone app
- [ ] Ambience field recordings (shortlist awaiting picks)
