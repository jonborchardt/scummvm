# Roger Game Picker (launcher)

The dialog that appears at engine startup when a Roger-capable SCI game boots:
a custom-drawn game list + per-game Roger settings + precache manager. It is the
normal, human-facing way to choose which game to play and how Roger should render
it — automation and `build_and_run.ps1 -Game`/`-SkipPicker` runs bypass it
entirely (see "Skipping the picker" below).

Unlike the dev utilities under `utils/`, this is **production code**: it ships
in the normal game path and writes real, persistent settings to `scummvm.ini`.

## Source files

| File | Role |
|------|------|
| `roger_launcher.{h,cpp}` | `RogerLauncher` — domain discovery, settings model (`GameEntry`), write-through setters (`setPassesForSelected`, `setDebugLogForSelected`), precache coordination, one-shot cross-game launch keys |
| `roger_picker_model.{h,cpp}` | SCI-free layout geometry (`PickerLayout`) and stamp helpers (`parseStamp`, `formatStamp`) — unit-tested, no SCI dependencies |
| `roger_picker_view.{h,cpp}` | `PickerViewWidget` — fully custom-drawn view: background gradient, card rows, Cached/Not-Cached badges, per-row Precache/Remove buttons, pass dropdown, debug toggle, Launch button |
| `roger_launcher_dialog.{h,cpp}` | `RogerLauncherDialog` — thin `GUI::Dialog` modal shell + `PickerActionListener`; pass dropdown sub-dialog (Custom…); Remove confirm dialog |

## What it can do

- **Discover games.** `RogerLauncher::discoverGames()` walks every SCI game
  domain in `scummvm.ini` (plus the active in-memory domain, so command-line
  `scummvm -p <path> <gameid>` launches show up too) and lists them. The active
  game sorts first.
- **Show cache state.** Each row shows a green **Cached** badge when a current
  marker file `<gameid>.done.v<kTransformVersion>.<passStamp>.marker` exists in
  the game's `<gameid>-roger/cache/` directory, or an amber **Not Cached** badge
  otherwise. Multiple markers accumulate — switching passes back to a completed
  set is instantly Cached. Legacy `roger_cache_stamp` ini keys are auto-removed
  on first read. The marker is written only when a precache run completes without
  cancellation.
- **Add a game** (`+ Add Game`): opens a directory browser, runs engine-level
  MD5 detection, and falls back to a raw `resource.map`/`resource.001` check
  for versions the tables don't know. VGA games are refused (Roger is EGA SCI0
  only). A new ConfMan domain is created and persisted.
- **Remove a game** (per-row button): removes the selected ConfMan domain (game
  files and cache on disk are untouched). Enabled on every row including the
  running game.
- **Per-game settings**, written through to the selected game's ConfMan domain
  immediately on change (no separate Save/Apply step):
  - **Omyac passes** — a dropdown: Default (removes `roger_omyac_passes`, engine
    uses `kDefaultPassString`) + entries from `goodPassPattern()` registry +
    current-ini-value-if-unlisted + **Custom…** (opens a mouse-first pass builder
    with +f/+l/+a/del/clear chips and OK/Cancel in the picker's own visual style;
    seed is canonicalized to compact form). Any selection writes
    `roger_omyac_passes` to the game's section at once.
  - **Debug Logging** — toggles `roger_debug` on/off for the selected game.
  - The picker does **not** read or write `roger_precache`, `roger_gen_mode`, or
    `roger_ui_font` — those keys are managed directly in `scummvm.ini`.
- **Precache now** (inline, row 0 — the running game): builds pic+view queues
  and generates one item per GUI tick in `handleTickle`, showing a progress bar
  and a Cancel button. Always covers all pics **and** views. On completion writes
  a marker file to `<gameid>-roger/cache/`. Cache badge refreshes immediately.
- **Cross-game launch**: launching a *different* game that already has a current
  stamp skips the precache dialog and launches immediately via two one-shot
  self-consuming ini keys (`roger_picker_precache` / `roger_picker_launch`) —
  consumed and flushed at `run()` start, before any action.
- **Launch / switch games**: launching the already-active game closes the dialog
  and proceeds. Launching a different game pushes it through `ChainedGamesMan` +
  a return-to-launcher event. Double-click on a row selects and launches.

## Keys and files owned by the picker

| Key / file | Written when | Format |
|------------|-------------|--------|
| `roger_omyac_passes` | Passes dropdown selection (per-game section) | compact pass string or absent (= default) |
| `roger_debug` | Debug toggle (per-game section) | `true`/`false` or absent |
| `<gameid>-roger/cache/<gameid>.done.v<ver>.<passStamp>.marker` | Precache completes without cancel | empty file; existence is the signal; multiple accumulate |
| `roger_picker_precache` | Cross-game launch that needs precache (per-game section) | `true` (consumed at next `run()`) |
| `roger_picker_launch` | Cross-game launch (per-game section) | `true` — boolean one-shot written into the target game's ini section (consumed at next `run()`) |

Note: legacy `roger_cache_stamp` ini keys from earlier builds are auto-removed by
`refreshCacheState()` on first read; the marker-file scheme supersedes them.

## Skipping the picker

The picker is for a human choosing a game or tuning settings. Deterministic
launches skip it via `roger_no_launcher` (ini) or the `ROGER_NO_LAUNCHER` env
var (per-process, preferred for automation):

- `build_and_run.ps1 -SkipPicker` — default SQ3, no picker
- `build_and_run.ps1 -Game <target>` — sets it automatically (a named target
  means the caller already knows what to launch)
- `.rin` script runs always skip it (the input driver arms before the picker,
  so early events would land in the picker dialog otherwise)

When skipped, the synchronous startup warm-up honors `roger_precache` instead
of the dialog's interactive precache.

## Standalone mode (no game target)

When ScummVM starts with no game target, `base/main.cpp`'s launcher round
calls `Sci::Roger::rogerStandaloneLauncher()` (`roger_standalone.{h,cpp}`)
instead of the stock ScummVM launcher, unless `roger_no_launcher=true` is set
in the `[scummvm]` ini section or the `ROGER_NO_LAUNCHER` env var is present
(the same knob that skips the in-engine picker; it is also the escape hatch
for users with non-SCI games, which this picker does not list). This works
with **zero configured games**: the picker opens to its empty state and
`+ Add Game` is the way in.

Standalone differences from the in-engine picker:

- Constructed as `RogerLauncher(nullptr, /*standalone=*/true)` — no provider,
  no `g_sci`, `activeRow == -1` (no row is "running").
- **Every** launch is a cross-game launch: `requestCrossGame` writes the usual
  one-shot key (`roger_picker_launch` / `roger_picker_precache`) and then sets
  the active ConfMan domain (pre-engine there is no engine for
  `ChainedGamesMan` / return-to-launcher to unwind). The main loop boots the
  game and the **in-engine** picker instance consumes the one-shot: cached ->
  straight into the game; uncached -> auto-precache with progress, then launch.
- Inline precache never runs standalone (it needs the engine's ResourceManager);
  the per-row Precache button takes the one-shot path above.
- Closing the picker without launching exits ScummVM (matches the stock
  launcher's close semantics).

Testing: `build_and_run.ps1 -Standalone` launches with no target; the log
marker is `ROGER-PICKER: standalone launcher round`.

## Background

The picker background is a procedural dark-navy vertical gradient (no image support).

## Wiring

The picker has two entry points. **In-engine:**
`FileRogerArtProvider::onEngineStartup()` (`engines/sci/roger/roger_register.cpp`,
reached from `SciEngine::run()` through the neutral observer seam) constructs
`Roger::RogerLauncher` with the provider and calls `run()` before the game
proper starts; a `false` return means
a game-switch was pushed and the engine returns immediately. **Pre-engine
(standalone):** `base/main.cpp`'s `launcherDialog()` calls
`Sci::Roger::rogerStandaloneLauncher()` (`roger_standalone.{h,cpp}`) as the
first statement in its do-while, replacing the stock ScummVM launcher for each
round (see "Standalone mode" above for the differences). `RogerLauncherDialog`
is a plain `GUI::Dialog`; precache work is driven from its `handleTickle` so
progress stays visible. The launcher talks to generation only through
`FileRogerArtProvider::precacheOnePic()` / `precacheOneView()`.
