# Room instrument (branch `room-instrument`)

Goal (owner, 2026-11-12): make blending the room feel as heightened and interactive as
the reactive-audio reference app studied privately (notes in the private assets repository (research/)): the room is the
instrument, the music accompanies it, and the music answers within moments. Built on this
branch and merged only when it works right. No TestFlight builds until all phases are
done (owner). Experimental builds say "Experimental: room instrument" in About & Help.

| Phase | What | Status |
|---|---|---|
| 1. The room leads | Mic level control + limiter (MicConditioner); no ducking by default; brighter tone (7 kHz) and a little frequency shift; room memory on per song (`roomBlend.memory`, `memoryPitch`); room level 85 % by default | Done |
| 2. The room drives the music | Continuous, fast reactions: room loudness/busyness, movement and heart rate open a filter on the music and swell or pull back stems up to 10 dB; section jumps half as often | Done |
| 3. Musical room effects | Reworked after the owner's listening: the room's own sound made musical. Resonators ringing on the song's scale, grains of the last second, a pitch-shifted echo on the beat, the room's held notes echoed in key on the 16ths, beat grooves; the synthesised bells are removed; the pitch-following tone is kept for testing | Done (rework) |
| 4. Music built on the beat | Clip/event layer on a musical clock: short loops and one-shots re-triggered on the beat; first test on Lunar Promise with slices of its own stems (option A) | Done |
| 5. Tuning, then merge | Calibration, levels, CPU (done); first experimental TestFlight build for listening; tune from the owner's feedback; merge | In progress |

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
- Section jumps: first made half as often; then stopped (owner, 2026-11-12): songs play
  straight through and loop, and no song is changed for the listener's mood. The moment is
  answered by the mix, the room and the beat clips. Song changes for dawn, dusk, weather and
  moon events, and the hand-over at a song's end, stay. The jump mechanism is kept, off
  (`LiveSession::setPartJumps`), in case rare jumps on big changes are wanted later.

## Phase 3 details: the room blend, built the way the studied dreams build it

Owner, 2026-11-12: build the room blend the way the reference app does. The earlier versions (bells,
then six voices always on together) are replaced. From the 14 dreams studied
(notes in the private assets repository (research/)), the room blend now works the way they do; all code is
original.

- **The room is always the sound.** Every effect is made from the microphone. A little plain
  room (high-passed at 200 Hz, `roomBlend.dry`, 0.3) is always underneath, and everything
  sends into one large, dark hall reverb.
- **Arranged, not stacked.** One effect plays at a time, for 9-24 bars, fading in and out
  over about a second, then the next takes over (`RoomInstrument::defaultScenes`; a song can
  give its own: `roomBlend.scenes: [{"fx": "grains" | [..], "bars": 12}]`). The order starts
  at a random scene.
- **Always moving.** Each effect re-picks its settings on the bar (or every two or four bars)
  and glides, on a 32-ticks-a-bar clock from the song's beat (100 BPM with no song).
- **The effects** (settings as studied):

| Effect | What it does |
|---|---|
| straight | the plain room, full range (0.6) |
| grains | 1 s grains from 0-0.4 s back, 0.6-3 a second (new rate each bar), spread wide |
| freqDream | frequency shift sliding toward -500 Hz and back, turning every 2 bars (right side 3 s behind) |
| delayDream | the room 2/3 beat late and a semitone up, into a one-beat echo; feedback swells to 0.89 over 16 bars, then falls to 0.4 |
| resonator | three resonators on the song's root, fifth and ninth (feedback 0.99); darker as the room gets louder (8 low-passes, 3000-380 Hz); a short ringing body and a 2/3-beat echo; slow pan drift |
| glitch | once a bar per side: pitch glides to a random interval within 2 octaves (0.5-2 s) and a 40 ms-2 s piece is caught and repeated (fading 0.8 each time) |
| spiral | two short echoes (100 / 200 ms) frequency-shifted down 100 Hz on every pass |
| ripple | the room shifted up and down on the beat, every 8, then 4, 2, 1 ticks, by a third up to an octave |
| groove | a cut-up of the last 20 s: every 1-12 ticks it cuts to another moment, panned, over the room's low rumble |
| memory | the room memory (slowed past moments) |
| tune | a soft three-voice synth on the room's pitch in the song's scale, changing on the bar (app-made; kept for testing) |

- Levels measured against the room: each effect sits between about -4 dB and 0 dB; under
  2 % of one core each, about 0.5 % for the arrangement.
- The old room transform (echo, shift) is silent when the room instrument plays
  (`MicTransform::level` 0). Room memory plays only in its scene (`RoomMemory::gate`).
- The pitch echo, the walking resonator chord and the short-grain shimmer are gone; none of
  them were in the studied dreams. Not yet built: the studied "snapshot" (onset-aligned room
  captures replayed in shuffled order).

### Listening test (TestFlight, 2026-11-12)

Before arranging the voices (next step: one or two at a time, changing with the song, as
the studied design does), a build to hear each voice and the balance. Diagnostics →
"Room listening test", TestFlight only, not remembered:

- Room effect: the arrangement, one effect held, or none (a little plain room only).
  `RoomInstrument::solo`.
- Room forward: the room +6 dB and the music -6 dB, gliding over 0.5 s
  (`AudioEngine::setRoomForward`). Tests whether the room is simply buried by the music.

Both are kept by the session across engine rebuilds.

## Phase 4 details

- `engine/audio/ClipSequencer`: up to 16 patterns, 24 voices, into the music bus (so the
  phase 2 filter applies). Each pattern: a clip, 16 or 32 steps (one or two bars of 16ths),
  gain, pan, an energy range and a chance per step. Voices fade in and out over 3 ms.
  Clock from the song's position, as for the room instrument; it snaps on a fresh start and
  after jumps, and is gently corrected otherwise. Clips follow the song's fade in/out.
- Songs list them under "clips": `{"stem", "fromBar", "beats", "pattern", "energy":
  [min, max], "gainDb", "pan", "chance"}`; the slices are cut from the song's own stems
  when it loads (and again after a sample-rate change). With no cue (no microphone,
  movement or workout) the clips rest and the song plays as written.
- Lunar Promise test set (slices chosen by measuring each stem bar by bar):
  - `perc-loop`: percussion bar 62, every bar, energy 0.55-1.
  - `drum-fill`: second half of drums bar 78, on the last half of every second bar, 0.4-1.
  - `other-hit`: one beat of "other" from bar 97, twice every two bars at 60 %, 0.7-1.
  - `keys-motif`: keyboard bar 2 (the intro), every two bars, only when calm (0-0.35).
- Owner-made loops and one-shots can later be referenced the same way (a "file" for
  clips is a small addition).

Also: after a sample-rate change the engine is rebuilt; the song's room colour, room
instrument settings and beat clips are now applied to the new engine too.

## Phase 5 details

- Calibration: instead of fixed thresholds per microphone, the reactor learns the room's
  own background level (drops at once to a quieter moment, creeps up 1 dB per 10 s) and
  measures loudness above it, so the built-in mic and headset mics react alike (a test
  shows a 15 dB more sensitive mic giving the same energy).
- Levels: the master safety limiter remains on the final output; the room's conditioner
  limits the room at -3 dBFS before any effect.
- CPU: optimised build, the engine benchmark (32 layers + atmosphere + the full room chain
  including conditioner, room memory and room instrument) renders at 30x real time, about
  3.3 % of one core.
- Next: the first experimental TestFlight build from this branch, for listening.
