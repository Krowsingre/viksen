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

## Phase 3 details

Owner's listening (2026-11-12): the first version answered taps with synthesised bells,
which were app sounds triggered by the room, not the room blended in. An audit of the
studied design (docs/research/REFERENCE_STUDY.md) found that its room is always the sound
itself: tuned resonators, granular texture, tempo echoes with a pitch-shifted copy, and
echoes of the room's own pitched sounds. The rework follows that. Every voice below is the
room's own sound, except "tune".

- `engine/environment/RoomInstrument`, fed the conditioned microphone, adding to the room's
  bus (heard only with the room blended in, following the room level, and wiped when the
  microphone stops); about 3 % of one core with every voice on.
  - Resonance: the room (80 Hz-3 kHz) rings through four tuned comb resonators (2.5 s to
    fall 60 dB, darkening as they fade, 80 ms glide between notes). Notes from the song's
    pentatonic scale: a three-note chord in scale steps from the root in octave 3, plus the
    root an octave lower; it walks degrees 0, 0, 3, 2, one step per two bars of the song
    (home chord with no song clock). A level guard keeps a long note right on a resonator
    to the level of a tap.
  - Grains: 60-200 ms Hann grains from 0.2-1.0 s back in the room, about 12 a second,
    panned at random, low-passed at 4 kHz.
  - Shifted echo: the room pitch-shifted (`engine/dsp/PitchShifter.h`, two crossfaded
    taps; an octave down by default) and echoed every 3/4 beat (0.5 s with no song),
    feedback 0.45, each repeat darker; the echo left, its shadow right.
  - Pitch echo: when the room holds a pitch for ~120 ms (a voice, a chime, a horn), the
    last 250 ms of the room is replayed retuned to the nearest scale note, on the next
    16th notes (+1, +3, +6; gains 1, 0.6, 0.35; as at 120 BPM with no song). At most one
    echo at a time; the same note again only after 1.5 s.
  - Groove: 4 s of room in memory; on each 16th note of the song, with probability
    0.2 + 0.5 x energy, a slice from 1-16 sixteenths back is replayed, panned at random.
  - Tune (kept for testing; may be removed or replaced by the room's own voice shifted
    into key): autocorrelation pitch tracking (80-800 Hz, first strong peak to avoid octave
    errors), snapped to the scale, sung by a soft two-voice tone between E3 and E5.
  - The plain room stays in, softened: MicTransform's low-passed dry path (7 kHz).
  - Levels per song under "roomBlend", each 0..1, 0 = off: `resonance` (0.45), `grains`
    (0.3), `shiftEcho` (0.3) with `shiftEchoPitch` in semitones (-12), `pitchEcho` (0.4),
    `groove` (0.4), `tune` (0.35). (`hits` is gone.)
- The beat clock comes from the song's position (tempo and bar offset); the energy from
  the phase 2 reactor; the key (scale and root) from world.json "key".
- Keys: songs without one got an estimate from their audio (chroma + key profiles; the
  method returns What Was Once as G minor, as known): the Lunar family B minor, The Fate
  of the Nine Vikron E♭ minor, Fallen of the Illuminous and Tree of Bones B♭ major
  (Tree of Bones is a low-confidence estimate). Owner-given keys are kept. Correct any
  key in world.json and the room follows it.

### Listening test (TestFlight, 2026-11-12)

Before arranging the voices (next step: one or two at a time, changing with the song, as
the studied design does), a build to hear each voice and the balance. Diagnostics →
"Room listening test", TestFlight only, not remembered:

- Room voices: all, one alone (resonance, grains, shifted echo, pitch echo, groove, tune),
  or none (the plain, softened room blend only). `RoomInstrument::solo`.
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
