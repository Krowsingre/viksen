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
