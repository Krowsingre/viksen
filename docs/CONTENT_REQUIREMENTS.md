# Content requirements (working draft for the owner)

**Owner answers (2026-11-04):**
- Q1: option **B**.
- Q2: every Dimension has the **same base Moods** (weather, time of day, activity, noise
  level); secrets are separate per Dimension.
- Q3: artwork by another LLM or a hired artist.
- Q4: **under 1 GB**.
- Q5: **3–6 minutes**.
- Q6: launch album after the Core is finished.
- Q7: **up to 12 stems**.

**Size check for the Core under option B** (4 lunar tracks + 2 hidden = 6 tracks):
- AAC 256 kb/s stereo is ~1.9 MB per stem-minute;
- 12 stems × 5 minutes × 6 tracks ≈ 690 MB;
- plus beds, ambience, clips and one-shots, ≈ 750–850 MB.

That's under 1 GB. If every track runs the full 6 minutes it's ≈ 900 MB+. The easy
margin is the approved delivery spec itself: **dry mono instrument stems** encode at
half the stereo rate, so a typical song (8 mono objects + 4 stereo beds) is ~35%
smaller.

What a Dimension needs, as audio and as data, for the approved progression (eight lunar
Phases, five secrets, at least two hidden songs) and the adaptive engine. **Draft:**
numbered questions (Q#) need the owner's answer.

## 1. What a Dimension contains

| Part | Required | Notes |
|---|---|---|
| 8 lunar-phase songs | Yes | One per phase. See §3: a "song" can be its own track **or** a lunar arrangement of a shared track |
| Hidden songs | At least 2 | Count toward the 5 secrets |
| Secrets | Exactly 5 | At least 2 are hidden songs; the rest are hidden moments, Moods or milestones (§5) |
| Moods | Optional | Contextual states, for example "storm", "deep night" |
| Core Voice beds | 2–3 | Long, loopable atmosphere shared by the Dimension (drone, air, night); keeps continuity between songs |
| Ambience recordings | 1–3 | Rain plus the Dimension's world (for example reeds/water, city rain, machinery) |
| Artwork | Yes | Moon Chart / Home art, thumbnail, badges (§6) |
| Text | Yes | Dimension, song, secret and badge names; short badge descriptions; album link |

## 2. What each song needs

**Minimum (works today):**
1. **Reference master:** full mix, WAV 48 kHz / 24-bit (kept private).
2. **Stems:** the same length, all starting at zero, summing to the master. Ideally from
   the production session; separated stems are acceptable for album Dimensions.
3. **Tempo** (or a beat map if it drifts), **key**, **sections**. The preparation tool
   drafts these and the owner corrects them.

**Rich (for "living" behaviour, M3):**
4. **Clips:** 1-, 2- or 4-bar loops and variations (for example 3 percussion variations,
   2 melodic figures) that the engine can launch on the beat.
5. **One-shots / motifs:** single gestures (bell, breath, pluck, swell) for events and
   transitions.
6. **Transition pieces** (optional): short passages written to join two songs.

## 3. Lunar songs: two ways to reach eight (Q1)

- **A. Eight distinct tracks** per Dimension. Strongest identity per Phase; the most
  production (10 or more tracks with the hidden songs).
- **B. Lunar arrangements:** fewer tracks, each heard in several lunar treatments. For
  example, 4 tracks × 2 Phases each:
  - waxing: more layers, more movement;
  - waning: sparser, receding.

  The engine plays the same recording differently: different stems, clips, effects and
  movement per Phase. About half the production; each Phase still sounds distinct.
- **Mixed:** the Core Dimension as A (it's the heart of the app); album Dimensions as B
  using the album's own tracks.

## 4. The 10 musical states per song: what each costs

| State | Achievable from stems alone? | Extra content |
|---|---|---|
| Normal | Yes (as composed) | — |
| Quiet | Yes (sparser stems, softer) | — |
| Loud | Yes (fuller, synth/brass forward) | — |
| Still | Yes (less rhythm) | — |
| Active | Yes (rhythm forward) | Better with percussion clips |
| Morning | Partly (brighter EQ, lighter layers) | A morning texture (birds, bells) helps |
| Afternoon | Partly (warm, mid density) | Optional |
| Night | Partly (darker, more space) | A night bed (crickets, drone) helps |
| Rain | No | **A rain recording** plus ideally a rain-tuned layer; subscription-only for now (#61) |
| Secret | No | **Bespoke:** an extra stem, clip set or arrangement written for it |

So **7 of 10 states come from good stems**. Rain needs one ambience per Dimension.
Secret needs one bespoke element per song. Morning and night improve with one texture
each per Dimension (shared by its songs).

## 5. The five secrets: suggested mix (Q2)

- 2 hidden songs (required).
- 1 secret version of a lunar song (the "Secret" state).
- 1 hidden moment or Mood (for example a midnight-in-the-rain passage).
- 1 listening milestone (for example three nights in a row).

**Pacing:**
- at least 1 found within a month;
- at least 1 that waits for nature (rain, dawn);
- at most 1 rare (seasonal).

## 6. Artwork per Dimension (Q3: who makes it?)

- Home / Moon Chart background (portrait + wide) and a card thumbnail.
- 8 lunar-phase emblems (or one style rendered for eight phases).
- Badges: Lunar Journey, 5 secrets, All Five, and later Global Secrets (distinct style).
- 1080×1350 and 1080×1920 share-card backgrounds.

## 7. Audio delivery spec (tentatively approved, #43)

- WAV 48 kHz / 24-bit masters and stems, the same length, starting at zero.
- **Dry mono** for instruments meant to move in 3D; **stereo** only for wide beds;
  effects tails as separate stems.
- About −18 LUFS integrated, −1 dBTP; a short gain-staging note per song.
- Ambience: at least 2 minutes, loopable, no music, no voices; licence recorded
  (CC0 / owned).
- Distribution: AAC 256 kb/s per track (on-demand download). Core Dimension bundled in
  the app.

## 8. Size and production budget (estimates)

| | Option A (8 tracks + 2 hidden) | Option B (4 tracks + 2 hidden) |
|---|---|---|
| Tracks to produce/prepare | 10 | 6 |
| Download (12 stems, ~5 min, AAC) | ~1.3–1.8 GB | ~0.8–1.1 GB |
| With 6 stems instead of 12 | ~0.7–0.9 GB | ~0.4–0.6 GB |

For the **Core bundled in the app**, size matters (Q4). Ways to keep it lean:
- 6–8 stems per song;
- shorter loop-based songs (the engine extends them by repeating sections);
- Apple Lossless only where it's audible.

## 9. Questions

- **Q1.** Lunar songs: A, B or mixed (Core = A, albums = B)?
- **Q2.** Is the secret mix in §5 a good default?
- **Q3.** Who makes the artwork (you, ChatGPT designs for approval, or commissioned)?
- **Q4.** Target size for the app with the Core Dimension inside (for example under 500 MB)?
- **Q5.** Song length for the Core: full-length (4–6 min) or shorter loop-based pieces?
- **Q6.** Which album becomes the one launch Dimension?
- **Q7.** Stems per song for the Core: up to 12, or a leaner 6–8 (smaller, cleaner 3D)?
