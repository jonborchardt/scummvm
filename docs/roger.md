# Roger — hires art replacement for SCI0

Roger is a fork feature that displays **high-resolution art** for SCI0 Sierra
adventures (SQ3, QFG1 EGA) while leaving the original game logic completely
intact. The game still runs at its native 320×200 (events, pathfinding,
priority/control buffers, scripts) — only the **display** is hires.

The hires visual is shown through ScummVM's **OSystem overlay**: a higher-resolution
layer composited above the 320×200 game surface. Roger draws the upscaled
background ("plate"), the ego/props (priority-masked against the art), and the SCI
UI (dialogs, score/title banner, menus, inventory, text-input) into that overlay.

This targets the **native desktop build** (Windows/MSVC here). An earlier
web/Emscripten/PixiJS prototype was abandoned and removed.

## Supported games

Roger supports **EGA SCI0 games only** (e.g. SQ3, QFG1 EGA). VGA and SCI1+ games are
**out of scope by design** and are hard-rejected at startup / on the first non-EGA
picture. This is a permanent decision, not a deferred feature.

## Running

```powershell
.\build_and_run.ps1          # build + launch SQ3
.\build_and_run.ps1 -NoLaunch # build only
```

- Game data: `…\Space Quest Collection\sq3`
- Roger art: `…\Space Quest Collection\sq3-roger` (sibling of the game dir)

**F10** cycles three display modes: **Enhanced** (hires overlay) →
**Original** (native 320×200) → **Side-by-Side** → Enhanced. In side-by-side the **left
panel shows the enhanced view** (backgrounds, upscaled cels, dialogs, live screen updates)
and the **right panel is a passive native mirror** of the original pics, views,
animations, dialogs, and menu screens (frozen-loop draws like Print windows and the QFG1
inventory/char sheet are patched into the mirror as they draw). It's a comparison /
screenshot view for testing intros and old-vs-new art — a
screenshot grabs both panels in one image. A single composited cursor floats under the
pointer over either panel, and clicks through either panel are remapped to game
coordinates, so the game stays playable while comparing. The status banner and overlay
follow the toggle.

**F12** toggles the quick-tune panel (session-only pass-tuning debug tool):
a `log:` row mirroring the F11 diagnostic-log toggle, a `view enhance:` toggle
cycling the view-scaler modes (the shipping **6x (s2>s3)** and a plain
**nearest** A/B "before"), a `pic enhance:` toggle cycling the available OMYAC
pass modes **plus a trailing `nearest`** (the unenhanced native plate), and a
linear pass builder (`+f`/`+l`/`+a`/`clear`) whose **add** registers the built
sequence as a new pic-enhance mode and applies it. Computed modes are
memory-cached, so cycling back to one is instant. Nothing is written to
`scummvm.ini` or the generation cache. The Roger Studio (`-Studio`) shares the
same control language (`view enhance:` / `pic enhance:` / build row) per A/B
slot.

The startup mode is `roger_display_mode` (default `enhanced`); for a single launch use
`build_and_run.ps1 -Mode enhanced|original|sbs` (env `ROGER_DISPLAY_MODE` — never touches
the ini). `-Mode sbs` makes every scripted capture an enhanced-vs-native comparison shot.

## What's enhanced

- Backgrounds (the room "plate")
- Ego / props / inventory item images (upscaled native cels, where hires art exists)
- `kAddToPic` static props (captured semantically via Feeder A — same hires sprite path as animate-list cels)
- Other native draws not covered above (Feeder B — upscaled nearest-neighbour)
- Dialog windows (with a black border matching the original SCI window)
- The score / "Space Quest" status banner (always hires, on load and after F10)
- The menu bar titles (the graphical Sierra icon is left native)
- Parser text-input fields (top-aligned to match the native field, live caret)
- The mouse cursor is Roger's composited arrow, drawn into the overlay (the native
  hardware cursor is not visibly rendered over the overlay; `roger_hw_cursor=true`
  opts back into it for experimentation). The letterbox edges are filled opaque
  black so the native render cannot leak there.

## Native-extras capture

Roger now captures native draws that were not previously hooked into the hires overlay, fixing the "many views missing per room" bug (most visible in QFG1 EGA). Two feeders feed the existing compositor automatically whenever the overlay is active — no config knob is needed.

**Feeder A — addToPic props (semantic, not blocky).** `kAddToPic` cels are static views baked into a room's picture data, outside the animate list. Roger captures these via a hook in `GfxAnimate` and stores them in a per-room list (cleared on room change). Each frame they are merged with the regular animate cast by priority (static cels draw before animate cels at equal priority, matching native draw order) and rendered through the same hires Sprite path as ego and props — ViewCache upscaling + priority-map occlusion. Static cels without hires art fall back to the rendered native cel.

**Feeder B — generic native regions (blocky, intentional).** The remaining unhooked native draws (kGraph primitives, etc.) are captured two ways:

1. A `bitsShow` rect hook records which screen regions were shown, gated by re-entrancy guards so draws Roger already composites semantically (the picture render, standalone cels, animate-list shows) are not double-captured.
2. A pixel-diff backstop snapshots the native visual buffer after the animate update and at composite time diffs the current buffer against that snapshot. Any changed rectangle not already recorded by the hook is upscaled nearest-neighbour and composited onto the overlay.

Captured generic regions are intentionally blocky (nearest-neighbour upscale) because their content is arbitrary native pixels. Inter-room animated sequences (ship flyovers, death sequences) are out of scope.

## Launcher

The Roger game picker appears at engine startup (before the game is loaded). It
displays a custom-drawn list of detected EGA SCI0 games from ScummVM's config.
Select a game and click **Launch** to proceed. The picker can be bypassed with
`roger_no_launcher=true` in `scummvm.ini` or the `ROGER_NO_LAUNCHER` env var —
`build_and_run.ps1 -SkipPicker` sets the env var for one launch, and passing
`-Game <target>` skips it automatically.

**Background:** The picker background is a procedural dark-navy vertical gradient (no image support).

**Card rows:** each game shows a title, a second line with detection facts
(target name | platform/language | gameid | EGA | SCI version for the running
game | game path — the target name leads so identical installs configured twice
stay distinguishable),
and a badge — green **Cached** (a current marker file exists) or amber
**Not Cached**. A **Precache** button and a per-row **Remove** button appear on
each row (Remove is enabled for every row including the running game — it removes
the ConfMan entry only; game files and cache on disk are untouched). The running
game sorts first. Double-clicking a row launches the game.

**Per-game settings** (write-through to the selected game's ini section
immediately):
- **Omyac passes** — a dropdown: Default (engine default) + entries from the
  `goodPassPattern()` registry + the current ini value if not in the registry +
  a **Custom…** option that opens a mouse-first pass builder (+f/+l/+a/del/clear
  chips, OK/Cancel in the picker's own visual style). Selecting any entry writes
  `roger_omyac_passes` to the game's section at once. Selecting Default removes
  the key (= engine default).
- **Debug Logging** — toggles `roger_debug` on/off for the selected game.

**Precache** (inline, row 0 / the running game): runs a full all-pics-and-views
precache in-dialog with a progress bar and Cancel button. When complete, writes
a marker file `<gameid>-roger/cache/<gameid>.done.v<kTransformVersion>.<passStamp>.marker`
so the badge reflects current cache state. Multiple markers accumulate — switching
passes back to a completed set is instantly Cached. Launching a *different* game
that already has a current marker skips the precache dialog entirely and launches
immediately via the one-shot self-consuming keys `roger_picker_precache` and
`roger_picker_launch` (written then flushed before acting at `run()` start).

**Add Game** (`+ Add Game`): opens a directory browser, runs engine-level MD5
detection, and refuses VGA games (Roger is EGA SCI0 only).

**Remove** per row: removes the ConfMan domain (game files and cache are
untouched). Enabled on every row, including the running game — the running
session keeps playing; the game just leaves the list.

The picker does **not** read or write `roger_precache`, `roger_gen_mode`, or
`roger_ui_font` — those keys are managed directly in `scummvm.ini` and their
engine defaults are unchanged.

## Config knobs

All are optional `scummvm.ini` keys (only read when present).

| Key | Default | Meaning |
|-----|---------|---------|
| `roger_gen_mode` | `cache` | in-engine art generation mode: `cache` = generate on a miss, load from the content cache on a hit; `memory` = generate, never write; `always` = regenerate + overwrite; `prebuilt` = the off-switch (native-only render, no Roger overlay) |
| `roger_precache` | `off` | scope of the synchronous startup warm-up: `all`, `pics`, `views`, `off`. Only active when the picker is skipped; the picker manages precaching directly (see "Launcher") |
| `roger_omyac_passes` | unset (= `affffflaaa`) | enhance-pass list for the omyac pipeline. Canonical form is a compact character string, one char per pass: `f`=fill, `l`=line, `a`=all (digits `2`/`1`/`0` also accepted) — the default is `affffflaaa`. Legacy space/comma-separated tokens (`fill`/`f`/`2`, `line`/`l`/`1`, `all`/`a`/`0`) still parse. Unset = the default sequence; empty string = wireframe (zero passes); unknown tokens warn and are skipped. The picker's Passes dropdown writes this key immediately to the selected game's section. Tunable live (session-only) via the F12 quick-tune panel |
| `roger_picker_precache` | unset | One-shot self-consuming key: if present at `run()` start, triggers a full precache before launch and is then removed |
| `roger_picker_launch` | unset | One-shot self-consuming boolean key written into the target game's ini section: if present at `run()` start, triggers an immediate launch and is then removed (used for cross-game launch from the picker) |
| `roger_no_launcher` | off | skip the Roger game-picker dialog at startup (also env `ROGER_NO_LAUNCHER`; `build_and_run.ps1 -SkipPicker`, auto-set by `-Game`) |
| `roger_ui_font_scale` | `150` | nudge multiplier (percent) on the native-metric text-size baseline; 100 = no nudge |
| `roger_ui_font` | `GoMono-Regular.ttf` | dialog/body font (from ScummVM's `fonts.dat`). Per-game: set it on a game target to give each game its own font |
| `roger_ui_header_font` | `NotoSans-Regular.ttf` | header/menu/banner font (config + restart only) |
| `roger_hw_cursor` | `false` | opt back into the native hardware cursor (not visibly rendered over the overlay — experimental); `false` = Roger's composited arrow |
| `roger_cursor_size` | `44` | composited-arrow size (applies with `roger_hw_cursor=false`, the default) |
| `roger_dirty_present` | on | re-draw only changed regions each frame (dirty-rectangle present); off = full-region present |
| `roger_transitions` | on | Mirror SCI screen transitions (fade/dissolve/wipe/scroll) and shake in the overlay. Off = hard cut (old behavior). |
| `roger_palette_live` | on | Re-apply the live SCI palette to the hires plate (cycling/fade/flash) via the preserved index map. Off = static plate colors. |
| `roger_debug` | off | per-frame Roger diagnostic logging |
| `roger_selftest` | off | logs per-room structural invariant PASS/FAIL to the debug output (one line per room loaded; use with `roger_debug=true` to see output). Asserts: EGA, overlay active, plate generated + non-empty, priority map present |
| `roger_display_mode` | `enhanced` | Startup display mode: `enhanced`, `original` (native), or `sbs` (side-by-side enhanced\|native). F10 still cycles from it. Per-launch override: env `ROGER_DISPLAY_MODE` / `build_and_run.ps1 -Mode <m>` — never touches the ini. `-Mode sbs` makes every scripted capture an enhanced-vs-native comparison shot. |
| `roger_input_script` | unset | Path to a `.rin` input script; replayed from launch (also env `ROGER_INPUT_SCRIPT`). See "Input automation". |
| `roger_input_live`   | unset | Path to an append-only live command file, tailed at ~10 Hz (also env `ROGER_INPUT_LIVE`). |
| `roger_cycle_log`    | off   | Per-kernelAnimate `ROGER-CYCLE period=<ms> busy=<ms>` telemetry line (also env `ROGER_CYCLE_LOG`). |
| `roger_diag`          | off   | Structured `ROGER-DIAG` overlay-state trace at room-load/present/cel-draw seams (also env `ROGER_DIAG` / `build_and_run.ps1 -Diag` for a single launch — preferred over editing the ini). |
| `roger_truth_capture` | off | Evidence mode: `.rin` captures dump the REAL overlay pixels via `grabOverlay` (the presented pixels — what the player actually sees) instead of forcing a full clean recompose. Required for fault-injection evidence — with it off, missing invalidation marks are invisible in captures (the scratch buffer self-heals every cycle). Per-launch: `build_and_run.ps1 -TruthCap` (env `ROGER_TRUTH_CAPTURE`). |
| `roger_diff_net` | on | Cycle-diff backstop net: each cycle, diff the native visual buffer against the previous cycle and invalidate changed regions — heals any missed invalidation within one cycle. Runtime escape hatch: set to `false` (env `ROGER_DIFF_NET=0` per-launch). Cost telemetry: `ROGER-NET sum32=<ms> boxes=<n>` under `-CycleLog` (budget: sum32 ≤ 32 ≈ 1 ms/cycle). The net only *invalidates* (never stamps pixels); the always-on bitsShow-hook path (Feeder B) and addToPic capture (Feeder A) handle compositing of unhooked draws. |
| `roger_debug_capture` | off | Write a per-pic manifest + a PNG per pixel-captured graphic sprite to the screenshot dir (missing-graphics forensics). |
| `roger_diff_check` | off | Gated in-engine native-vs-overlay diff, once per pic (never on the steady-state path); logs `ROGER-DIAG[diff]` boxes for the missing-graphics audit. |

### Aspect-ratio correction

While a generating `roger_gen_mode` is active, Roger pins ScummVM's default-on
aspect-ratio correction **off**: the art is square-pixel (the plate is an exact 6× of the
320×190 picture), and the default 4:3 stretch would resample the enhanced scene ~20% too
tall. An explicit `aspect_ratio` key in `scummvm.ini` or on the command line still wins.

### Stretch modes

The enhanced overlay follows ScummVM's stretch mode (`stretch_mode` in the ini, the
in-game options dialog, or the Ctrl+Alt+S runtime cycle): Roger mirrors the backend's
own placement math for all six modes (Center, Pixel-perfect, Even-pixels, Fit, Stretch,
Fit-4:3), so Enhanced and Original render the game at the identical on-screen rect and
F10 toggles produce no positional shift. Note that "Stretch to window" fills the window
without preserving aspect and "Fit to window (4:3)" forces the CRT-tall look — for a
square-pixel comparison against the source art, keep the default "Fit to window". A
stretch-mode change is picked up on the next composited frame.

### Text sizing

Hires UI text size derives from each element's **native SCI font metrics** — the SCI font's cell height scaled to the overlay resolution, capped to the native string width — so hires text occupies the same on-screen footprint as the original SCI text. `roger_ui_font_scale` is a nudge multiplier on top of that baseline (100 = no nudge). Elements with no captured metric fall back to the legacy role heights.

### Body font shortlist

Set the dialog/body font via ``roger_ui_font`` in `scummvm.ini`. The shortlist of period-appropriate faces:

1. `ms_sans_serif.ttf` — clean Win9x UI sans
2. `LiberationSans-Regular.ttf` — neutral sans
3. `NotoSans-Regular.ttf` — neutral sans
4. `LiberationSerif-Regular.ttf` — storybook / manual feel
5. `GoMono-Regular.ttf` — DOS/terminal monospace (**default** — best match for the SCI0/DOS look)
6. `LiberationMono-Regular.ttf` — DOS/terminal monospace, Courier metrics
7. `SourceCodeVariable-Roman.ttf` — monospace

All fonts are sourced from ScummVM's bundled `fonts.dat`. The **header font** (`roger_ui_header_font`) is not cycled — change it in `scummvm.ini` and restart.

## Asset layout

Nothing is consumed from disk: plates, VIEW cels, and priority maps are all generated
in-engine from the game's own SCI resources. The only on-disk artifacts are the
content-hash generation cache, written to a directory that is a sibling of the game
directory (`sq3-roger/` next to `sq3/`):

```
sq3-roger/
  cache/    ← content-keyed generated PNGs (see "Generated cache files")
```

Walkability and native occlusion always ride SCI's own native priority/control buffers;
the overlay compositor's per-pixel sprite occlusion samples the in-engine generated hires
priority map (the `omyacprio` cache entry), so occlusion edges get the same upscaling as
the displayed plate.

## Generated cache files

On first use Roger writes content-keyed PNG files to `sq3-roger/cache/`:

| File | Transform | Contents |
|------|-----------|----------|
| `<gameid>.omyac.v<ver>.<hash>.<passes>.png` | `omyac` | hires background plate (1920×1140 RGBA) |
| `<gameid>.scale6x.v<ver>.<hash>.<passes>.png` | `scale6x` | hires VIEW cel (ego/props/inventory) |
| `<gameid>.omyacprio.v<ver>.<hash>.<passes>.png` | `omyacprio` | hires priority map (omyac-aligned) used for overlay sprite occlusion |

Cache keys embed `kTransformVersion`, so a pipeline change automatically invalidates stale files.

## Known limitations

- Better/higher-quality hires backgrounds and **new hires VIEW art** for room
  sprites are **art-side** (asset authoring), not engine work. Sprites without
  hires art are shown as upscaled native cels.
- Overlay sprite occlusion samples a hires priority map generated through the same omyac geometry as the plate, so band edges align with the displayed background.
- **Plugin migration** (Roger as its own SCI plugin) is future work; today it is
  wired into the SCI engine via the `g_sciRogerProvider` global.

## Cross-game validation

SQ3 and QFG1 EGA are the validated EGA SCI0 targets. To re-run the structural
invariant check: set `roger_selftest=true` and `roger_debug=true` in the game's
`scummvm.ini` domain, launch the game, and walk through several rooms. Each room
load emits one `ROGER selftest[<gameid>] pic N: PASS/FAIL` line to the debug output.

## Input automation

Roger includes a scripted-input driver that drives the game headlessly for
regression testing and verification. The driver is a registered backend
`EventSource` (engine-agnostic — `roger_input.{h,cpp}` contain no SCI includes)
so events arrive through the normal `pollEvent` path, including during blocking
dialogs.

**Captures**: `capture <label>` fires at the next overlay present and writes
`roger-<pic>-<label>-{overlay,preview}.png` in `screenshotpath`.
Captures work in **Enhanced** (the default) and **Side-by-Side** modes — launch with
`-Mode sbs` to make every capture an enhanced-vs-native comparison shot; an
Original-mode capture misses the composited scene. With `roger_truth_capture` on
(`-TruthCap`), captures dump the real presented overlay pixels via `grabOverlay`
instead of forcing a full clean recompose — required when gathering invalidation-fault
evidence, since a forced recompose self-heals exactly the staleness you're looking for.

To capture while a **blocking dialog** is up, follow `capture` with a `move X Y` and a
short `wait`: a blocked cycle takes no presents, so the pending capture needs the nudge
to flush while the dialog is still visible — without it you get the post-dismiss frame.

### `.rin` grammar

```
# game-space 320x200 coords; '#' comments; blank lines skipped
click X Y   | rclick X Y    # mouse down+up at (X,Y)
mousedown X Y | mouseup X Y  # press / release separately (drag gestures — drives SCI0 mouse menus; move steers while held)
move X Y                    # mouse move (nudge a present)
key <token>                 # single key down+up
type "text"                 # inject characters one by one
wait <ms>                   # delay before next command
waituntil <key> <val> <timeoutMs>  # poll until state key matches (timeout continues; pair with assert)
capture <label>             # dump overlay PNG at next present (needs flush move+wait)
snap <label>                # grab overlay pixels NOW via grabOverlay (works mid-blocking-dialog)
state                       # emit ROGER-STATE pic=<n> ego=<x>,<y> windows=<n> mode=<name>
assert <key> <val>          # fail the run (exit 125) if state key != val
restore <slot>              # load save slot (SciEngine::loadGameState delayed restore)
fail <msg>                  # unconditional fail (exit 125)
log <text>                  # emit ROGER-SCRIPT: <text> to the log
quit                        # send EVENT_QUIT (clean exit)
```

Key tokens: `ENTER` `ESC` `SPACE` `TAB` `BACKSPACE` `UP` `DOWN` `LEFT` `RIGHT`
`F1`..`F12` `a`-`z` `0`-`9`

**State keys** (`assert` / `waituntil` / `state`): `pic` (current room pic number),
`windows` (count of kUiWindow elements — note: some saves have a persistent game window,
so the baseline is not always 0; QFG1 save 1 shows `windows=1` on load — assert against
observed baseline, not assumed 0), `egox` / `egoy` (live ego position from global var 0),
`mode` (display mode as raw int: 0=enhanced, 1=original, 2=sbs). `state` emits
`ROGER-STATE pic=<n> ego=<x>,<y> windows=<n> mode=<name>` to the log.

**`waituntil` and `assert`:** `waituntil pic <n> <timeoutMs>` replaces fixed 8–12 s
room-crossing waits — it polls until the pic matches or times out. Timeout logs
`ROGER-SCRIPT: waituntil TIMEOUT ...` and **continues** (not a hard fail); pair with
`assert pic <n>` immediately after for a hard fail. On satisfy or timeout the schedule
rebases, so later waits stay relative.

**`snap` vs `capture`:** `snap <label>` grabs the presented overlay pixels immediately
(via `grabOverlay`) — works mid-blocking-dialog with no `move` choreography. Use it for
plain evidence shots. `capture <label>` pends a dump consumed at the next present — use
it when the claim under test is the present pipeline itself, and always follow it with
`move X Y` + `wait 400` to flush.

**`restore <slot>`:** loads a save from cold boot. `SciEngine::loadGameState` sets a
delayed restore that the game loop processes promptly. Verified: `restore 1` then
`waituntil pic 300 15000` succeeds in QFG1 with no character-screen interference.

**Exit codes from `build_and_run.ps1`:**
- `0` — clean game exit (all assertions passed, `quit` reached)
- `124` — watchdog timeout (`-TimeoutSec` exceeded; hung script)
- `125` — scripted assertion failure (`assert` mismatch or `fail`); the harness greps
  `ROGER-SCRIPT: FAIL` in `screenshots\roger-run.log` after the run exits and prints
  `SCRIPT FAIL: assert/fail marker in screenshots\roger-run.log`.

### Harness invocations

```powershell
# Scripted: drives the game automatically, exits on `quit`, blocks until done
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script test\sci\roger\scripts\qfg1-smoke.rin -CycleLog

# Live: launch in background, append .rin lines to cmd.txt to drive interactively
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Live cmd.txt
```

Captures land in the game's `screenshotpath` (set to `screenshots\` in the project
config). The run log is at `screenshots\roger-run.log`. The smoke script for QFG1
is `test/sci/roger/scripts/qfg1-smoke.rin`.

## Roger Studio

`build_and_run.ps1 -Studio` (env `ROGER_STUDIO=1`, per-launch) boots a **debug-only,
mouse-driven tuning environment** instead of a game: a single scene — the enhanced plate
with a view cel composited on it game-style (SQ3 defaults: pic 2, view 12 loop 1) — with
two live A/B setting slots (params + passes + scaler variant + plate mode each) — one scaler module, 6x, is registered today — Split
and Diff comparison views with an automatic alignment readout, click-to-place/drag cel,
and stamped PNG export. Everything is button-driven; Esc quits and E exports
(automation-only keys). It never touches the generation disk cache.

## Tests

Unit tests for the SCI-type-free Roger units live in `test/sci/roger/` (CxxTest).
On Windows: `.\build_tests.ps1` (reports `TESTS PASSED`). Headless visual capture:
`.\build_and_run.ps1 -Script <file.rin>` drives the game and writes
`roger-<pic>-<label>-{overlay,preview}.png` captures to `screenshotpath`.
