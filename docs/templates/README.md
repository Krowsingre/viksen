# Dimension template

`dimension.world.json` is the standard layout for a new Dimension (owner, 2026-11-07).
Copy it to `apps/ios/Viksen/Resources/Worlds/<dimension-id>/world.json`, set `id`, `name`,
`album` and `tier`, then fill the slots as the music arrives.

## The 21 standard slots

| Slot | Plays |
|---|---|
| `day` | by day |
| `dawn` | the 45 minutes after night ends (`"plays": "dawn"`) |
| `dusk` | the 45 minutes before night begins (`"plays": "dusk"`) |
| `night-new` … `night-waning-crescent` (8) | after dark during that moon phase. The night song with `"plays": "night"` (normally the New Moon song) also plays on nights whose phase has no song yet |
| `storm` | while it rains, including thunderstorms for now (`"plays": "rain"`) |
| `snow` | while it snows (`"plays": "snow"`) |
| `fog` | in fog or mist (`"plays": "fog"`) |
| `wind` | while it is windy (`"plays": "wind"`) |
| `cloudy` | while it is cloudy (`"plays": "cloudy"`) |
| `secret-1` … `secret-5` | waiting for the Secrets feature; never played yet |

Which song plays (the first that applies and has audio): rain, snow, fog, wind, cloudy
(weather plays day and night), then after dark tonight's phase song or the night song,
then dawn or dusk, then the day song. "Night", "dawn" and "dusk" follow the listener's
Night setting (7 pm to 4 am, or sunset to sunrise).

A slot with `"stems": []` is empty: it keeps its place and is skipped. A Dimension needs at
least one filled slot to play. Non-standard Dimensions may add more slots after the
standard ones (up to 24 in all), each with its own `slot` name.

To fill a slot, give it the song's `id` (a stable, lowercase name used for files and progress),
its `title`, `music`, `adaptive`, `stems`, `sections` and `rules`, as in the Viksen and The
Shatter Gardens files. Audio goes to the private viksen-assets repository, never here.

## Renaming

Names shown in the app come only from `"name"` (the Dimension) and `"title"` (a song).
Change them freely, e.g. on GitHub with the pencil icon, and the next build shows the new
name. Do not change an `id`: it names the audio files and keeps listeners' Moon Chart
progress; a renamed song keeps its id (Lunar Storm Promise is titled "Sanctuary" but its id
stays `lunar-storm-promise`).
