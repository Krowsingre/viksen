# Project manifest — schema version 1

Package layout (the extension is chosen by the app; currently `.viksen`):

```
Name.viksen/
  manifest.json            current manifest (written atomically)
  manifest.backup.json     previous manifest (recovered from if manifest.json is unreadable)
  assets/audio/            copies of imported media (originals are never modified)
  layers/ automation/ worlds/ sessions/ thumbnails/
```

```json
{
  "format": "atmos.project",
  "schemaVersion": 1,
  "id": "uuid-v4",
  "name": "Night Drive",
  "createdAt": "2026-11-02T10:00:00Z",
  "modifiedAt": "2026-11-02T10:05:00Z",
  "createdWith": { "app": "…", "version": "…", "platform": "…" },
  "session": { "sampleRate": 48000, "bpm": null },
  "sources": [{
    "id": "s1", "kind": "track|stem|sample|recording|texture",
    "storage": "packaged|external",
    "path": "assets/audio/song.wav",
    "displayName": "Song", "groupId": "song-1", "isReferenceMix": false,
    "media": { "sampleRate": 44100, "channels": 2, "frames": 1234567 }
  }],
  "layers": [{
    "id": "l1", "sourceId": "s1", "bus": "music|atmosphere|micRaw|micTransformed",
    "timelineStartFrame": 0, "gainDb": 0.0, "muted": false, "solo": false
  }],
  "buses": { "music": { "gainDb": 0, "muted": false }, "atmosphere": {…}, "micRaw": {…}, "micTransformed": {…} },
  "master": { "gainDb": -3, "limiterCeilingDb": -1 }
}
```

Rules:

- `timelineStartFrame` is in session-rate frames. Source frame 0 (including any leading
  silence) lands there. Stems from one export therefore align at 0 with their phase intact.
- `bpm` is metadata only. It never changes the playback speed of a source.
- Packaged paths are relative with forward slashes and may not escape the package.
- **Unknown keys are preserved** at every level (top level, session, buses, master,
  sources, media, layers) and written back unchanged. A file from a newer schema opens,
  is flagged `newerThanSupported`, and keeps its higher `schemaVersion` on save.
- Migrations live in `migrateManifest()`. v0 (prototype `tracks` list) → v1 is implemented and tested.
- Validation reports duplicate ids, dangling layer references, escaping paths, negative
  offsets, and a reference mix playing together with its own stems (double playback).
