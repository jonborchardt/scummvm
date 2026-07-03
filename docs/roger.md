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

**F10** (or Ctrl+Shift+U) cycles three display modes: **Enhanced** (hires overlay) →
**Original** (native 320×200) → **Side-by-Side** → Enhanced. In side-by-side the **left
panel shows the enhanced view** (backgrounds, upscaled cels, dialogs, live screen updates)
and the **right panel is a passive native mirror** of the original pics, views, and
animations. It's a comparison / screenshot view for testing intros and old-vs-new art — a
screenshot grabs both panels in one image. It shows a single cursor that floats under the
pointer over either panel, but does not remap clicks — switch back to **Enhanced** with F10
to play. The status banner and overlay follow the toggle.

## What's enhanced

- Backgrounds (the room "plate")
- Ego / props / inventory item images (upscaled native cels, where hires art exists)
- `kAddToPic` static props (captured semantically via Feeder A — same hires sprite path as animate-list cels)
- Other native draws not covered above (Feeder B — upscaled nearest-neighbour)
- Dialog windows (with a black border matching the original SCI window)
- The score / "Space Quest" status banner (always hires, on load and after F10)
- The menu bar titles (the graphical Sierra icon is left native)
- Parser text-input fields (top-aligned to match the native field, live caret)
- The mouse cursor uses the native hardware cursor (arrow / wait / hand), shown
  smoothly over the overlay; the letterbox edges are filled opaque black so the
  native cursor cannot leak there.

## Native-extras capture

Roger now captures native draws that were not previously hooked into the hires overlay, fixing the "many views missing per room" bug (most visible in QFG1 EGA). Two feeders feed the existing compositor automatically whenever the overlay is active — no config knob is needed.

**Feeder A — addToPic props (semantic, not blocky).** `kAddToPic` cels are static views baked into a room's picture data, outside the animate list. Roger captures these via a hook in `GfxAnimate` and stores them in a per-room list (cleared on room change). Each frame they are merged with the regular animate cast by priority (static cels draw before animate cels at equal priority, matching native draw order) and rendered through the same hires Sprite path as ego and props — ViewCache upscaling + priority-map occlusion. Static cels without hires art fall back to the rendered native cel.

**Feeder B — generic native regions (blocky, intentional).** The remaining unhooked native draws (kGraph primitives, etc.) are captured two ways:

1. A `bitsShow` rect hook records which screen regions were shown, gated by re-entrancy guards so draws Roger already composites semantically (the picture render, standalone cels, animate-list shows) are not double-captured.
2. A pixel-diff backstop snapshots the native visual buffer after the animate update and at composite time diffs the current buffer against that snapshot. Any changed rectangle not already recorded by the hook is upscaled nearest-neighbour and composited onto the overlay.

Captured generic regions are intentionally blocky (nearest-neighbour upscale) because their content is arbitrary native pixels. Inter-room animated sequences (ship flyovers, death sequences) are out of scope.

## Launcher

The Roger launcher dialog appears at engine startup (before the game is loaded). It displays a list of detected SCI games from ScummVM's config, showing the cache status for each (e.g., "55 pics cached"). Select a game and click **Launch** to proceed; the dialog is required — there is no Skip button.

For each game, the launcher shows:
- **Game name** (from the domain or script fallback)
- **Cache status** (number of cached pics/views)
- **Per-game settings** that update when a different game is selected:
  - **Pre-cache** (Off/Pictures/All) — controls what is generated on demand before launch
  - **Enhancement** (number of omyac passes)
  - **Font** (cycles through the same shortlist as Ctrl+Shift+F in-game)
  - **Fallback** (hardware cursor / cursor size)

The **Delete** button (right of the game list) removes the selected game from ScummVM's config.

**Auto-precache on first launch:** If the selected game has no cache files and Pre-cache is not Off, the launcher will precache automatically before launching. Progress is shown in the dialog (e.g., "Caching pic 23/87...").

The **Precache Now** button is always available for manual precaching, regardless of whether the game already has a cache.

## Config knobs

All are optional `scummvm.ini` keys (only read when present).

| Key | Default | Meaning |
|-----|---------|---------|
| `roger_ui_font_scale` | `150` | nudge multiplier (percent) on the native-metric text-size baseline; 100 = no nudge |
| `roger_ui_font` | `GoMono-Regular.ttf` | dialog/body font (from ScummVM's `fonts.dat`); can be cycled live with Ctrl+Shift+F. Per-game: set it on a game target to give each game its own font |
| `roger_ui_header_font` | `NotoSans-Regular.ttf` | header/menu/banner font (config + restart only; not affected by Ctrl+Shift+F) |
| `roger_hw_cursor` | `true` | use the native hardware cursor over the overlay; `false` = Roger's composited arrow |
| `roger_cursor_size` | `44` | composited-arrow size (only when `roger_hw_cursor=false`) |
| `roger_dirty_present` | on | re-draw only changed regions each frame (dirty-rectangle present); off = full-region present |
| `roger_transitions` | on | Mirror SCI screen transitions (fade/dissolve/wipe/scroll) and shake in the overlay. Off = hard cut (old behavior). |
| `roger_palette_live` | on | Re-apply the live SCI palette to the hires plate (cycling/fade/flash) via the preserved index map. Off = static plate colors. |
| `roger_autoshot` | off | dump the composited scene to PNG on room load (verification harness) |
| `roger_debug` | off | per-frame Roger diagnostic logging |
| `roger_selftest` | off | logs per-room structural invariant PASS/FAIL to the debug output (one line per room loaded; use with `roger_debug=true` to see output). Asserts: EGA, overlay active, plate generated + non-empty, priority map present |
| `roger_display_mode` | `enhanced` | Startup display mode: `enhanced`, `original` (native), or `sbs` (side-by-side enhanced\|native). F10 still cycles from it. Per-launch override: env `ROGER_DISPLAY_MODE` / `build_and_run.ps1 -Mode <m>` — never touches the ini. `-Mode sbs` makes every scripted capture an enhanced-vs-native comparison shot. |
| `roger_input_script` | unset | Path to a `.rin` input script; replayed from launch (also env `ROGER_INPUT_SCRIPT`). See "Input automation". |
| `roger_input_live`   | unset | Path to an append-only live command file, tailed at ~10 Hz (also env `ROGER_INPUT_LIVE`). |
| `roger_cycle_log`    | off   | Per-kernelAnimate `ROGER-CYCLE period=<ms> busy=<ms>` telemetry line (also env `ROGER_CYCLE_LOG`). |
| `roger_diag`          | off   | Structured `ROGER-DIAG` overlay-state trace at room-load/present/cel-draw seams (also env `ROGER_DIAG` / `build_and_run.ps1 -Diag` for a single launch — preferred over editing the ini). |

### Text sizing

Hires UI text size derives from each element's **native SCI font metrics** — the SCI font's cell height scaled to the overlay resolution, capped to the native string width — so hires text occupies the same on-screen footprint as the original SCI text. `roger_ui_font_scale` is a nudge multiplier on top of that baseline (100 = no nudge). Elements with no captured metric fall back to the legacy role heights.

### Body font shortlist

Press **Ctrl+Shift+F** in-game to cycle the dialog/body font live through a seven-entry shortlist (good for in-game A/B judging). Cycle order:

1. `ms_sans_serif.ttf` — clean Win9x UI sans
2. `LiberationSans-Regular.ttf` — neutral sans
3. `NotoSans-Regular.ttf` — neutral sans
4. `LiberationSerif-Regular.ttf` — storybook / manual feel
5. `GoMono-Regular.ttf` — DOS/terminal monospace (**default** — best match for the SCI0/DOS look)
6. `LiberationMono-Regular.ttf` — DOS/terminal monospace, Courier metrics
7. `SourceCodeVariable-Roman.ttf` — monospace

All fonts are sourced from ScummVM's bundled `fonts.dat`. The **header font** (`roger_ui_header_font`) is not cycled — change it in `scummvm.ini` and restart.

## Asset layout

`sq3-roger` is a sibling of the `sq3` game directory:

```
sq3-roger/
  pics/<id>/source/
    pic.<id>.png              low-res original visual
    pic.<variant>.<id>.png    hires visual (e.g. pic.omyac-upscaler.<id>.png)
    pic.<id>_p.png            SCI native priority map (320×200 grayscale)
    pic.<id>_c.png            SCI native control  map (320×200 grayscale)
    <variant>.<id>_p.png      overlay-occlusion priority map (EGA-color band-per-pixel)
  views/<id>/
    view.<id>.loop.<n>.png    upscaled hires VIEW cels (ego / props / inventory)
```

The grayscale `pic.<id>_p.png` fills SCI's native priority buffer (walkability + native occlusion). The overlay compositor's per-pixel occlusion source is now the in-engine generated hires priority map (`omyacprio` cache) — no prebuilt `_p.png` is consumed for overlay occlusion.

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

**Captures** reuse the autoshot writer: `capture <label>` fires at the next overlay
present and writes `roger-<pic>-<label>-{overlay,preview}.png` in `screenshotpath`.
Captures assume **Enhanced display mode** (the default). In Side-by-Side mode the
capture path returns before present or dumps a non-composited scene — automation
must run in Enhanced mode.

### `.rin` grammar

```
# game-space 320x200 coords; '#' comments; blank lines skipped
click X Y   | rclick X Y    # mouse down+up at (X,Y)
move X Y                    # mouse move (nudge a present)
key <token>                 # single key down+up
type "text"                 # inject characters one by one
wait <ms>                   # delay before next command
capture <label>             # dump overlay PNG at next present
log <text>                  # emit ROGER-SCRIPT: <text> to the log
quit                        # send EVENT_QUIT (clean exit)
```

Key tokens: `ENTER` `ESC` `SPACE` `TAB` `BACKSPACE` `UP` `DOWN` `LEFT` `RIGHT`
`F1`..`F12` `a`-`z` `0`-`9`

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

## Tests

Unit tests for the SCI-type-free Roger units live in `test/sci/roger/` (CxxTest).
On Windows: `.\build_tests.ps1` (reports `TESTS PASSED`). Headless visual capture:
`.\roger_run.ps1 -ShotDir <dir>` writes `roger-<id>-*.png` autoshots.
