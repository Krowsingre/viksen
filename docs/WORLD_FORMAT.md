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

- **Stems**: up to 8, same length and start, streamed from storage in lockstep
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
