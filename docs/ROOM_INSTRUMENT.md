# Room instrument (branch `room-instrument`)

Goal (owner, 2026-11-12): make blending the room feel as heightened and interactive as
the reactive-audio design studied in docs/research/REFERENCE_STUDY.md: the room is the
instrument, the music accompanies it, and the music answers within moments. Built on this
branch and merged only when it works right. No TestFlight builds until all phases are
done (owner). Experimental builds say "Experimental: room instrument" in About & Help.

| Phase | What | Status |
|---|---|---|
| 1. The room leads | Mic level control + limiter (MicConditioner); no ducking by default; brighter tone (7 kHz) and a little frequency shift; room memory on per song (`roomBlend.memory`, `memoryPitch`); room level 85 % by default | Done |
| 2. The room drives the music | Continuous, fast reactions: room loudness/busyness, movement and heart rate open filters, swell stems 6–12 dB, widen the mix; section jumps rarer | |
| 3. Musical room effects | Pitch following into the song's key; beat-synced delay grooves from the room; onset-triggered one-shots (no repeats until all have played) | |
| 4. Music built on the beat | Clip/event layer on a musical clock: short loops and one-shots re-triggered on the beat | |
| 5. Tuning, then merge | Per-route calibration, levels, battery, listening sessions | |

## Phase 1 details

- `engine/environment/MicConditioner`: RMS level control toward -20 dBFS (-12…+30 dB,
  rising at most 6 dB/s, holding below -70 dBFS so hiss is not pulled up), then a peak
  limiter at -3 dBFS. Quiet/loud detection still uses the unconditioned level.
- `LiveSession::applyRoomBlend` defaults: tone 7000 Hz (was 2600), shiftMix 0.25 (was 0),
  duck 0 (was 0.6), room memory level 0.5 at -12 semitones. Songs can override each.
- Room memory is again a sound source, chosen per song rather than a listener control;
  RAM only, wiped when the microphone stops; heard only with "Blend the room" on.
