# Roger Game Picker (launcher)

The dialog that appears at engine startup when a Roger-capable SCI game boots:
a game list + per-game Roger settings + precache manager. It is the normal,
human-facing way to choose which game to play and how Roger should render it —
automation and `build_and_run.ps1 -Game`/`-SkipPicker` runs bypass it entirely
(see "Skipping the picker" below).

Unlike the dev utilities under `utils/`, this is **production code**: it ships
in the normal game path and writes real, persistent settings to `scummvm.ini`.

## What it can do

- **Discover games.** `RogerLauncher::discoverGames()` walks every SCI game
  domain in `scummvm.ini` (plus the active in-memory domain, so command-line
  `scummvm -p <path> <gameid>` launches show up too) and lists them with a
  live cache status ("not cached", "N pics + M views"). The active game sorts
  first. Each game's `<gameid>-roger/` sibling directory is created on demand.
- **Add a game** (`+ Add Game`): opens a directory browser, runs engine-level
  MD5 detection, and falls back to a raw `resource.map`/`resource.001` check
  for versions the tables don't know. VGA games are refused at add time
  (Roger is EGA-only). A new ConfMan domain is created and persisted, and the
  `<gameid>-roger/` directory is set up.
- **Delete a game**: removes the selected ConfMan domain (the game files and
  cache on disk are untouched).
- **Per-game settings**, loaded/saved per ConfMan domain on selection/launch:
  - **Pre-cache** — `off`/`pics`/`views`/`all` (`roger_precache`)
  - **Passes** — the `roger_omyac_passes` string, verbatim round-trip; the
    buttons beside the field fill it with a known-good pattern from the
    `goodPassPattern()` registry in `roger_passes.cpp` (one click per
    suggestion). An emptied field removes the key = use the default.
  - **Font** — `roger_ui_font`, from a bundled-font shortlist
  - **Fallback** — `roger_gen_mode` (`prebuilt` = native-only, `cache`,
    `memory`, `always`)
- **Precache now**: builds pic/view queues from the game's resource map and
  generates one item per GUI tick in `handleTickle`, with a progress bar,
  per-item status line, and a Cancel button. Cache counts refresh when it
  finishes. Launching a game that has **no cache at all** auto-runs a full
  precache first, then launches.
- **Launch / switch games**: launching the already-active game just closes the
  dialog and proceeds. Launching a *different* game pushes it through
  `ChainedGamesMan` + a return-to-launcher event, so ScummVM restarts cleanly
  into the selected target (its `scummvm.ini` domain — and therefore its Roger
  settings — apply). Double-click or Enter on a list row selects and launches.

All settings writes go through `flushSettingsForSelected()` at launch /
precache time and are flushed to disk — this dialog is the canonical way
per-game Roger knobs get into `scummvm.ini`.

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

## Wiring

`SciEngine::run()` (`engines/sci/sci.cpp`) constructs `Roger::RogerLauncher`
with the global art provider and calls `run()` before the game proper starts;
a `false` return means a game-switch was pushed and the engine returns
immediately. `RogerLauncherDialog` is a plain `GUI::Dialog` (overlay-pixel
layout, no fixed geometry); precache work is driven from its `handleTickle`
so progress stays visible. The launcher talks to generation only through
`RogerArtProvider::precacheOnePic()` / `precacheOneView()`.
