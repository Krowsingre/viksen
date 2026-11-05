# Features hidden from the app's screens (developer list)

Things the engine and code still support but listeners no longer see or control.
Kept so a Dimension can use them by design, or so they can return, possibly in
Viksen Studio (the separate creator app, which folds in Reuvo) rather than here.

| Feature | Where it lives in code | Why it is hidden | Status | Better home? |
|---|---|---|---|---|
| Atmosphere bed (generated background texture; density, colour, weather shaping) | `engine/` bed generator; `EngineController.bedEnabled/bedLevel/bedColourHz`, `weatherShapesAtmosphere` | Dimensions carry their own music; real rain/thunder covers weather; few listeners would use it | Queued for removal from Settings (owner, 2026-11-05) | Studio, or authored per Dimension |
| Arrange the instruments (move stems on a 3D canvas) | `SpatialCanvasView` (removed), `EngineController.setStemPosition`, saved positions | A creator feature | Screens removed 2026-11-05; engine positions remain | Studio |
| Studio session (import songs and stems, mixer, projects) | `EngineController` studio functions, project files, `LiveSession` studio transport | Moves to Viksen Studio | Screens removed 2026-11-05 | Studio |
| Library (imported music and saved projects) | `MediaLibrary`, project packages | Moves to Viksen Studio | Screens removed 2026-11-05 | Studio |
| Room memory (replays recent surroundings, slowed) | `engine/environment` room memory; `EngineController.roomMemory*` | Creator control | Queued to hide (owner, 2026-11-05) | Studio or per-Dimension room design |
| Frequency shift on the room sound | `MicTransform` shifter; `EngineController.micShift*` | Creator control; songs set it via "roomBlend" | Queued to hide (owner, 2026-11-05) | Per-Dimension room design |
| Room effect sliders (tone, echo time, feedback, effect mix) | `EngineController.micToneHz/micEchoSeconds/micFeedback/micWet`; `MicTransform` | Creator controls; each song sets its room colour | Queued to hide (owner, 2026-11-05) | Studio |
| Level sliders with meters (music, atmosphere, raw mic, transformed mic, master) | `EngineController.busGains/masterGain`; `BusSliders` | Mixing controls; listeners get one "Room level" | Queued to hide (owner, 2026-11-05) | Studio |
| Recording / capture modes | `engine` capture paths (capture modes) | Owner: no recording in Viksen Dimensions (copyright and privacy) | UI removed earlier; engine capture remains for tests | Studio (own music only) |

When something is hidden, add it here with where it lives and why.
