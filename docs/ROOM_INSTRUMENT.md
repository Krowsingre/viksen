# Room instrument (branch `room-instrument`)

Goal (owner, 2026-11-12): make blending the room feel as heightened and interactive as
the reactive-audio design studied in docs/research/REFERENCE_STUDY.md: the room is the
instrument, the music accompanies it, and the music answers within moments. Built on this
branch and merged only when it works right. No TestFlight builds until all phases are
done (owner). Experimental builds say "Experimental: room instrument" in About & Help.

| Phase | What | Status |
|---|---|---|
| 1. The room leads | Mic level control + limiter (MicConditioner); no ducking by default; brighter tone (7 kHz) and a little frequency shift; room memory on per song (`roomBlend.memory`, `memoryPitch`); room level 85 % by default | Done |
| 2. The room drives the music | Continuous, fast reactions: room loudness/busyness, movement and heart rate open a filter on the music and swell or pull back stems up to 10 dB; section jumps half as often | Done |
| 3. Musical room effects | Pitch following into the song's key; beat-synced delay grooves from the room; onset-triggered tuned bells (no repeats until all have played) | Done |
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

## Phase 2 details

- `core/worlds/MusicReactor`: one energy (0..1) from the strongest available cue: room
  (0.6 x loudness from -60 to -20 dBFS + 0.4 x sudden sounds per second / 3), movement
  (activity 0..4), workout intensity. Rises with a 0.3 s time constant, settles over 2 s.
  Neutral (song as mixed) with no cue or with adaptive music off, fading in/out over ~1 s.
- Outputs: a low-pass on the music bus (`AudioEngine::setMusicCutoffHz`, 80 ms glide,
  bypassed when open): 5 kHz when calm, open from half energy. Stems: "energy" stems
  (drums, percussion, bass, guitar, brass) sit up to 10 dB lower when calm and come up
  with the energy; "calm" stems (strings, synth, keyboard, woodwinds) recede up to 5 dB
  near full energy; others (voices, other) are never moved.
- Per song under "react": `depthDb`, `filterClosedHz`, `stems` {id: "energy" | "calm" |
  "neutral"}.
- Section jumps: a part now stays twice as long before the planner may move on.

## Phase 3 details

- `engine/environment/RoomInstrument`, fed the conditioned microphone, adding to the room's
  sound (heard only with the room blended in); about 2 % of one core.
  - Tune: autocorrelation pitch tracking (80-800 Hz, first strong peak to avoid octave
    errors), snapped to the song's pentatonic scale, sung by a soft two-voice tone between
    E3 and E5 (60 ms glide, 0.25 s attack, 1.2 s release).
  - Groove: 4 s of room in memory; on each 16th note of the song, with probability
    0.2 + 0.5 x energy, a slice from 1-16 sixteenths back is replayed, panned at random.
  - Hits: a sudden sound (after a 2 s warm-up, 180 ms apart) rings a soft FM bell on a
    scale note drawn without repeats until all have sounded (two octaves from C4).
  - Levels per song under "roomBlend": `tune` (0.35), `groove` (0.4), `hits` (0.45); 0 = off.
- The beat clock comes from the song's position (tempo and bar offset); the energy from
  the phase 2 reactor.
- Keys: songs without one got an estimate from their audio (chroma + key profiles; the
  method returns What Was Once as G minor, as known): the Lunar family B minor, The Fate
  of the Nine Vikron E♭ minor, Fallen of the Illuminous and Tree of Bones B♭ major
  (Tree of Bones is a low-confidence estimate). Owner-given keys are kept. Correct any
  key in world.json and the room follows it.
- Placeholder hits are synthesised bells; a Dimension's own short sounds can replace them
  later (phase 4's sample layer).
