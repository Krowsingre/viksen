# World format (`world.json`, schema 1)

A World is built-in music made of synchronised stems plus rules that re-mix it
around the listener. Folder layout (bundled in the app under `Worlds/<id>/`):

```
Worlds/hinds-of-morrow/
  world.json
  keyboard.m4a  strings.m4a  woodwinds.m4a  synth.m4a  guitar.m4a  bass.m4a  percussion.m4a
```

Audio is owner-licensed and lives in the private repository `Krowsingre/viksen-assets`
(`worlds/<id>/*.m4a`); the TestFlight workflow copies it in at build time. It is never
committed to this public repository.

```json
{
  "format": "atmos.world", "schemaVersion": 1,
  "id": "hinds-of-morrow", "name": "The Hinds of Morrow", "artist": "", "tier": "free",
  "music": { "tempo": 110, "key": "G minor", "loop": { "fromSeconds": 0, "toSeconds": null } },
  "fadeSeconds": 4,
  "stems": [ { "id": "keyboard", "file": "keyboard.m4a", "title": "Keyboard", "baseGainDb": 0 } ],
  "rules": [
    { "stem": "strings", "when": { "all": [ { "type": "quiet" } ] },
      "gainDb": 0, "otherwiseDb": -8, "label": "Strings bloom when it is quiet" }
  ],
  "unlock": { "all": [ { "type": "moonPhase", "phase": "full" } ] }
}
```

- **Stems**: up to 16 per song (Studio imports: 8), same length and start, streamed from storage in lockstep
  (`WorldStreamPlayer` + `WorldFeeder`), so long songs need little memory.
- **Rules**: for a stem, every rule whose inputs are available yields `gainDb` when its
  conditions hold and `otherwiseDb` when not; the stem's target is the loudest of those.
  If no rule applies (e.g. mic and motion are off) the stem plays at `baseGainDb`:
  the music as composed. Changes fade at a constant rate (`fadeSeconds` for 60 dB).
- **Conditions** (shared with unlocks): `quiet`, `loud` (mic), `still`, `moving`
  (motion sensing), `intensityAtLeast` (Health / Apple Watch, 0..1), `timeWindow`,
  `moonPhase`, `lunarDay`, `listeningAtLeast`, `consecutiveWorldsAtLeast`.
  Unknown types are preserved and make the rule/unlock unsatisfiable on older builds.
- **Tier**: `free`, `experience` or `studio`.

## Adaptive sections (optional)

A Dimension can divide its song into **sections**. The listener's state chooses
which sections may play; the stream moves between them on the musical grid, so
being still, moving, in a quiet or loud place, or working out plays *different
parts of the song*.

```json
"music": {"tempo": 110, "key": "G minor", "beatsPerBar": 4, "barOffsetSeconds": 0},
"adaptive": {"jumpEveryBars": 4, "minStayBars": 8, "crossfadeBeats": 1},
"sections": [
  {"id": "first-light", "title": "First Light", "fromBar": 0, "toBar": 16,
   "when": {"all": [{"type": "still"}]}, "prefer": {"all": [{"type": "timeWindow", "from": 21, "to": 5}]},
   "label": "You are still"},
  {"id": "gathering", "title": "Gathering", "fromBar": 16, "toBar": 32}
]
```

- `fromBar`/`toBar` count bars from `barOffsetSeconds` (or use `fromSeconds`/`toSeconds`).
- `when`: the state this part belongs to. No `when` = a neutral bridge, heard when the
  song plays as composed. Use sensor states (still, moving, quiet, loud, intensityAtLeast).
- `prefer`: favours this part among the suitable ones (time of day, moon phase).
- `label`: shown to the listener as the reason this part is playing.
- If no reactive section matches (or every sense is off) the song plays as composed and
  loops from the top. Otherwise, at each `jumpEveryBars` grid point, a part that no longer
  suits (after `minStayBars`) hands over to the next suitable part in song order; at the
  end of a part the song flows on if the next part suits, else moves to another suitable
  part. Jumps crossfade over `crossfadeBeats` and keep the bar phase.
- Up to 64 sections and 16 stems.

## Several songs in one Dimension

List them under `songs`. Each song is a complete song description (its own `music`,
`adaptive`, `stems`, `sections`, `rules`); the Dimension keeps `id`, `name`, `artist`,
`tier` and `unlock`. Stem files share the Dimension folder, so prefix them by song.

```json
{"format": "atmos.world", "schemaVersion": 1, "id": "the-quiet-storm",
 "name": "…", "artist": "Krowsingre", "tier": "experience",
 "songs": [
   {"id": "the-silent-alley", "title": "The Silent Alley", "music": {"tempo": 89.145}, "stems": [{"id": "drums", "file": "silent-alley-drums.m4a"}], "sections": []},
   {"id": "in-different-times", "title": "In Different Times", "music": {"tempo": 131.045}, "stems": [], "sections": []}
 ]}
```

How the Dimension moves between its songs:
- The current song keeps playing while one of its parts suits the listener.
- When nothing in this song suits the listener's state but a part of another song does
  (held for `minStayBars`), the song fades out over 3 s and the other song fades in at
  the part that suits.
- Played as composed (no sense steering), each song plays to its natural end and the next
  song follows, in order, then around again.
- Songs keep their own tempo and key; changes between songs are fades, not beat-matched.
- Up to 8 songs.

## Shipping a Dimension (downloads)

The app contains only each Dimension's `world.json`; the audio is an Apple-hosted,
one on-demand asset pack per track (`<id>`, or `<id>--<song>` in multi-song Dimensions),
holding that track's files under `Dimensions/<id>/`.

1. Put the audio in the private assets repo: `worlds/<id>/*.m4a`.
2. Put `world.json` in `apps/ios/Viksen/Resources/Worlds/<id>/`.
3. Run the **Dimensions → App Store Connect (asset packs)** workflow with the id (or
   empty for all). It packages with `xcrun ba-package` and uploads a new version;
   once processed it is available to internal TestFlight builds.
4. For external TestFlight and the App Store, submit the asset pack version for review
   in App Store Connect (with or without an app build).

Listeners download a Dimension by choosing it, and remove it from the Dimension list
(swipe) or Settings › Downloaded Dimensions; removing frees the space immediately.
