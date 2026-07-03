# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

ScummVM uses a configure + GNU Make build system on Linux/macOS. On Windows, use `devtools/create_project` to generate IDE project files.

### Windows (quick start)

A PowerShell script handles everything — installs dependencies via vcpkg, generates the VS solution, builds, and launches SQ3:

```powershell
.\build_and_run.ps1
```

- First run: ~15–30 min (vcpkg compiles SDL2, libpng, zlib etc. from source; cached after)
- Subsequent runs: ~1–3 min incremental build + launch
- Requires: Visual Studio 2019/2022 with "Desktop development with C++" workload

Game data: `J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3`
Roger art: `J:\SteamLibrary\steamapps\common\Space Quest Collection\sq3-roger`

To jump straight into another game + save for verification (uses the configured
target's `scummvm.ini`, so Roger settings apply):

```powershell
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1   # boots QFG1 and auto-loads save slot 1
```

**The picker is bypassed automatically whenever `-Game` is passed** — a named
target means the caller already knows what to launch, so the Roger game-picker
dialog (only useful for a human choosing a game / tuning settings) is skipped.
The `-Game qfg1 -SaveSlot 1` example above therefore already boots straight into
the save with no picker. For the default SQ3 path (no `-Game`), pass `-SkipPicker`
explicitly to skip it:

```powershell
.\build_and_run.ps1 -SkipPicker              # SQ3, no picker
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1   # QFG1 save 001, picker skipped automatically
```

Either path sets the `ROGER_NO_LAUNCHER` env var for that launch only (per-process,
never touches `scummvm.ini`; the launcher gate is in `engines/sci/sci.cpp`).

**Autonomous verification loop:** `-Script <file.rin>` drives the game headlessly
from a `.rin` input script, blocking until the script's `quit` exits it; `-Live <file>`
launches in background and tails the file at ~10 Hz for interactive script authoring;
`-CycleLog` enables per-cycle `ROGER-CYCLE period=<ms> busy=<ms>` telemetry (~83 ms
period is healthy; ~225 ms was the walking-speed regression); `-Diag` enables the
`ROGER-DIAG` overlay-state trace for that launch only (`ROGER_DIAG` env var — prefer it
over flipping `roger_diag` in scummvm.ini, whose edits race a running instance's config
rewrite-on-exit); `-Mode enhanced|original|sbs`
boots straight into that display mode for this launch only (`ROGER_DISPLAY_MODE` env var /
`roger_display_mode` ini knob; F10 still cycles from it) — `-Mode sbs` makes every capture
an enhanced-vs-native comparison shot, the "Roger bug or game behavior?" evidence, with no
F10 keypress choreography. Captures land in the
game's `screenshotpath` as `roger-<pic>-<label>-{overlay,preview}.png`; the run log is
`screenshots/roger-run.log` (grep `ROGER-SCRIPT` / `ROGER-CYCLE`). Grammar
(full reference in `engines/sci/roger/roger_input.h` and `docs/roger.md`):

```
click X Y | rclick X Y | move X Y | key <token> | type "text"
wait <ms> | capture <label> | log <text> | quit        # '#' = comment
```

Coordinates are game space (320×200). Smoke script:
`test/sci/roger/scripts/qfg1-smoke.rin`. The injection seam is a registered backend
`EventSource` — keep `roger_input.{h,cpp}` free of SCI includes (engine-agnostic).
The phase-gate regression suite for the present-barrier refactor lives at
`test/sci/roger/run-regression.ps1` (manifest-driven; see the spec §8) — run it
after any change to the per-cycle or present path.

Automation rules (each violated once at real cost — don't re-learn them):

- **Capture during a blocking dialog needs a `move` after it.** `capture` only *pends*
  a dump; a present consumes it, and a blocking `Print`/`Display` window freezes the
  cycle so no presents happen while it idles. `capture label` → `move X Y` → `wait 400`
  flushes the capture with the dialog visible; without the move you get the
  post-dismiss frame. (Same lifetime-vs-cycle trap class as the ghost-text bugs above.)
- **Enhanced display mode for scene captures** (the default; a generating `roger_gen_mode`,
  not `prebuilt`) — an Original-mode capture misses the composited scene. Side-by-Side
  captures the split layout, which is exactly what you want for enhanced-vs-native
  comparison evidence: launch those runs with `-Mode sbs` instead of scripting F10 presses.
- **`confirm_exit` and `gui_return_to_launcher_at_exit` must be off** for `-Script`
  runs: the script can't answer the confirm modal, and return-to-launcher means the
  process never exits.
- `#` starts a comment anywhere on a line — `type`/`log` payloads must not contain it.
- Timings need settle margins (boot ≈4000 ms after save-restore; dialog appear ≈2500 ms).
- If setting `roger_input_script` via scummvm.ini instead of the harness, pair it with
  `roger_no_launcher` — the driver arms before the picker, so early events land in the
  picker dialog. An in-process engine restart replays the script from the top.

**Screenshots:** never write screenshots (or `roger_autoshot` output) to the repo
root. Point `screenshotpath` at the gitignored `screenshots/` folder (already in
`.gitignore`) — keep all dev/verification captures there so they are never committed.

### Configure and build (Linux/macOS)
```sh
./configure [--enable-engine=<name>] [--disable-engine=<name>]
make -j$(nproc)
```

### Generate IDE project files (Windows/macOS IDE)
```sh
# From the build directory:
/path/to/scummvm/devtools/create_project /path/to/scummvm --msvc    # Visual Studio
/path/to/scummvm/devtools/create_project /path/to/scummvm --xcode   # Xcode
```

### Run unit tests
```sh
make test
```

Tests use the CxxTest framework located in `test/cxxtest/`. Test source files are in `test/`.

## Roger Project

This fork adds the **Roger** art replacement system for SCI0 games (SQ3, QFG1 EGA). It renders high-resolution backgrounds and VIEW cels **generated in-engine** from the SCI resources (the omyac upscaler pipeline), presented through ScummVM's OSystem overlay, while leaving all game logic intact. In-engine generation is the default and only art path — no pre-generated PNGs are consumed.

- Design spec (single living doc): `docs/superpowers/specs/2026-06-19-roger-art-replacement-design.md` — done/partial/future are stratified there; the superseded per-phase plans were removed (git history is the record of what shipped)
- User-facing docs: `docs/roger.md`
- All Roger code lives in `engines/sci/roger/`

> **Native rendering only.** Roger targets the native (desktop) ScummVM build. An
> earlier web/Emscripten/PixiJS prototype was abandoned; all of that code, build
> scripts, and the `roger-canvas` HTML overlay have been removed. The hires visual
> is displayed through ScummVM's **OSystem overlay** (a higher-resolution layer
> composited above the 320×200 game surface) — not a browser canvas.

### SCI0 rendering & UI invariants (READ BEFORE TOUCHING ANY HOOK)

Most Roger bugs are the **same bug wearing different clothes**: the overlay failed to
mirror *both* the **draw** and the **lifetime** of a native element, or it fought SCI's
synchronous game cycle. The fixes for text rendering, ghost text, and walking-speed were
all this class. These invariants are load-bearing — internalize them before adding or
changing a hook. They cross-reference "Performance discipline" (below) and the Feeder
A/B capture notes in Stage 2.

**How SCI0 draws (the mental model):**

- **Immediate-mode native renderer at 320×200.** SCI draws directly into three parallel
  byte buffers — **visual** (color), **priority** (z-band / occlusion), **control**
  (walkability / event zones). Roger replaces only the *display* of the visual buffer;
  priority/control stay native, which is why walkability and native occlusion "just work".
- **The overlay is retained; the native screen is immediate.** SCI "erases" transient
  content (text, dialogs) simply by **redrawing the scene underneath it**. The overlay has
  **no automatic erase** — nothing repaints a region until something dirties it. So *every
  removal* of an overlay element MUST explicitly dirty the vacated dest rect, or the
  dirty-rectangle present skips it and the pixels ghost. (Fix pattern: `clearToken`/
  `onNativeEraseRect` collect removed native rects → `addDirtyRect(sciRectToDest(...))`.)
- **The game cycle is a single synchronous heartbeat: `kernelAnimate`.** Game *logic*
  (walking, input) advances one step per cycle. Anything reachable per-cycle must be O(1)
  and must not force a full present/recompose unless the scene actually changed — a heavy
  per-cycle path slows the *game*, not just the frame rate (see Performance discipline;
  the `bitsRestore`→full-present regression cost a 2.7× walking slowdown).
- **Blocking calls FREEZE the cycle.** `Print`/`Display` (and `kMessage`) draw their text
  and then **wait for a click without ticking `kernelAnimate`**. Corollary — the single
  most expensive lesson: **any overlay state tied to an element's lifetime must be updated
  at that element's DRAW hook, never deferred to the animate cycle.** A deferred flush of
  dialog text reaches the overlay only *after* its window is already disposed, so it misses
  its clear and ghosts until the next window reuses the id. (Fix: `onNativeText` pushes into
  `_uiLayer` immediately; it does not wait for `flushGenericText` in the next cycle.)

**The two structural lifetime signals — key off these, never off geometry or per-game knowledge:**

- **Ports & Windows are THE UI lifetime model.** `GfxPorts::openWindow` / `removeWindow`
  bracket every dialog, message, menu, and the picture port itself. Window **ids are
  reused** after dispose. Token every captured UI element by its port id
  (`0x40000000 | id` for controls/windows; `0x60000000 | id` for generic text captures)
  and clear it in `removeWindow` — the one reliable, game-agnostic **dispose** signal.
  Element lifetime = its window's lifetime: windowed text dies with its window; text on the
  persistent picture port survives until room change (this is why char-sheet stats persist
  while an over-the-sheet popup can't wipe them).
- **`GfxText16::Box` is THE text chokepoint.** All text — narration, dialogs, controls,
  status bar — flows through it. Capture there and you are game-agnostic. On SCI0 EGA
  `show == false` (text is drawn into the buffer and flushed later by `kGraphUpdateBox` /
  `bitsShow`), so gating on `show` misses everything. The `rect` is **port-local** — globalize
  it with `_ports->offsetRect` before use (matches every controls16 hook). Box hands over
  rect + fontId + penColor + alignment + line height + single-line width; for **multi-line**
  text it exposes only per-line *char counts* (`GetLongest`), not per-line rects — so Roger
  currently re-wraps (a known fidelity gap; a per-line `Draw`/`Show` hook would fix it).

**Save-under is a real but INCOMPLETE erase signal.** `bitsSave`/`bitsRestore` back most
transient overlays, and `bitsRestore` fires `onNativeEraseRect`. But transparent /
no-save-under windows and `reanimate == false` disposals **skip it** — which is exactly why
`removeWindow` (not `bitsRestore` alone) is the dependable dispose hook.

**The same content can be captured by more than one hook** (controls16 semantic + generic
`Box` + `bitsShow` pixel). Keep a dedup/lifetime discipline (namespace tokens + covered-rect
dedup) so redundant copies don't outlive each other and ghost.

**`_picNotValid` = room init.** Cels drawn while it's set bake into the picture — capture them
via `onInitCel` (Feeder A supplement) or they go missing on first visit. The init-frame cast
draws BOTH the baked decorations (their objects dispose out of the animate list after baking)
AND live actors like the ego, and no view/loop/cel identity separates them (SCI0 packs both
into one per-room view resource; both are in the cast on frame 1). The discriminator is the
**owning animate object**, tagged at capture: an init cel is promoted only while its owner is
absent from the animate list. Promoting by any weaker rule either froze a duplicate ego at the
room-entry position or wiped the room signs — both shipped as bugs once.

**Kernel drawing primitives → where Roger hooks them** (the game-agnostic seams):

| SCI primitive | What it does | Roger hook |
|---|---|---|
| `GfxPaint16::drawPicture` | room background render (fills visual/priority/control) | `pushHiresBackground` |
| `GfxAnimate::kernelAnimate` | the game cycle + full cast draw | `renderFromAnimateList` |
| `addToPicDrawCels/View` | static cels baked into the picture | `onAddToPicCel` (Feeder A) |
| cast draw / `drawCelAndShow` during `_picNotValid` | first-visit static props (owner-tagged) | `onInitCel` (Feeder A) |
| `GfxText16::Box` | **all** text-out | `onNativeText` |
| `GfxPorts::openWindow` / `removeWindow` | window create / **dispose** | `uiPushWindow` / `uiClearToken` |
| `kDrawControl` (button/text/edit/icon/list) | dialog controls | `uiPushButton`/`uiPushText`/`uiPushTextEdit` |
| `bitsShow` / `bitsRestore` | native region show / save-under restore | `onNativeShowRect` / `onNativeEraseRect` (Feeder B) |
| `kGraphFrameBox` | selection frame primitive | `uiPushFrameBox` |
| status/menu bar | top strip | `uiPushStatus` |
| transitions (fade/dissolve/wipe/scroll/shake) | scene change FX | `onTransition` |
| palette (cycling / fade) | live EGA palette | `roger_palette_live` re-apply |

**Traps — do NOT re-fall into these (each cost a debugging session):**

- Deferring lifetime-bound overlay state to the animate cycle → ghosts through blocking dialogs.
- Removing an overlay element without dirtying its vacated rect → stale pixels until the next redraw.
- Clearing/erasing by **geometry** (rect containment) instead of by **window token** → false drops (a popup over the char sheet wipes stat text beneath it).
- A per-cycle hook that forces a full present/recompose when nothing changed → walking slowdown.
- Gating the text hook on `show == true` → misses all SCI0 EGA text.
- Using port-local rects without `offsetRect` → offset text that never matches global erase rects.
- Classifying an init-frame (`_picNotValid`) draw by **resource identity** (view / view+loop /
  view+loop+cel) instead of by its **owning object's lifetime** → either a frozen duplicate ego
  at the room-entry position or wiped room signs/stocked shelves, depending on which rule you
  pick. Both actors and to-be-baked props draw through the frame-1 cast, and SCI0 packs both
  into one per-room view resource (QFG1 300: signs = view 300 loop 2, live bard/goblin = loops
  0/1/3) — NO identity rule can separate them. The only reliable signal is whether the capture's
  owner object is still in the animate list (present = drawn live, never promote; gone = baked,
  promote). Same lesson as the window-token trap above: key off object lifetime, not geometry
  or resource ids.

**Underused SCI signals worth exploiting later** (highest value first): per-line text rects
via a `Draw`/`Show` hook (kills the multi-line re-wrap drift); a single **frame-complete**
present barrier around `updateScreen` in `kernelAnimate` (cleaner than scattered per-primitive
presents, and closes the stale-overlay-during-blocking-dialog class); a **palette-vary
per-tick** hook for smooth fades/cycling (current re-apply is binary); semantic TextEdit-caret
and list-selection hooks (vs pixel/diff capture). `kMessage` exists but SCI0 (QFG1/SQ3) uses
Print/Display — low priority. Verify a signal's current hook state before adding — several are
already partially wired.

### Stage 1: Background replacement

Hook at top of `GfxPaint16::drawPicture()` checks `g_sciRogerProvider`. When non-null and `hasBackground()` returns true (i.e. a generating `roger_gen_mode` is active), it lets SCI's **native picture render run** — which fills SCI's own 320×200 priority + control buffers, so walkability and native occlusion stay correct — then calls `pushHiresBackground()`. That generates (or loads from the content cache) the hires plate for the pic and presents it to the OSystem overlay. The overlay's per-pixel sprite occlusion is derived **in-engine** by rendering SCI's **priority screen** through the *same* omyac pipeline as the visual (`RogerAssetGen::generatePriorityMap()`, the `omyacprio` cache) — priority codes are EGA colours, so the output is a colour EGA priority view, upscaled/edge-enhanced exactly like the plate; the occlusion bands are recovered from that render (nearest EGA colour → code), not from any prebuilt map.

**Status: implemented + verified.** Supports SCI0 EGA games (SQ3, QFG1 EGA) with omyac upscaling. VGA games are detected at startup and rejected with a warning (EGA-only). Generation activates with zero prebuilt files; `pushHiresBackground()` presents the generated plate to the overlay immediately on room load (no native→hires "pop"). Walkability/native occlusion ride SCI's native buffers; overlay sprite occlusion uses the priority screen upscaled through omyac.

**Key files:**

| File | Role |
|------|------|
| `engines/sci/roger/roger_art_provider.h` | Abstract interface + `g_sciRogerProvider` global; declares no-op base virtuals for native-extras: `onAddToPicCel`, `onInitCel`, `beginNativeDraw`, `endNativeDraw`, `onNativeShowRect`, `snapshotNativeBaseline`; `diagEnabled()` accessor (gated trace facility, default false) |
| `engines/sci/roger/roger_asset_gen.h/cpp` | In-engine generation: `generatePlate()` (omyac plate for EGA), `generateViewCel()` (scale6x cel, EGA only), `generatePriorityMap()` (renders SCI's priority screen through omyac; overlay occlusion bands recovered from it), `priorityBands()` (legacy native occlusion bands), `generateTextSurface()` (renders one native-font glyph via `scaleNearest` → RGBA, for the hybrid text path), backed by a content-hash disk cache (`kTransformVersion`-keyed) |
| `engines/sci/roger/roger_pic_native.{h,cpp}` + `roger_pic_parser` / `roger_omyac` / `roger_scale` / `roger_ega_blend` | The omyac pipeline: parse pic → native pre-render (exposes `NativeRef::priority`) → enhance passes → RGBA plate; scale6x for VIEW cels |
| `engines/sci/roger/file_roger_art_provider.h/cpp` | Provider: `hasBackground()` (generating-mode gate), `pushHiresBackground()` (generates+presents the plate, routes occlusion through `generatePriorityMap()` — hires omyac-aligned), `precacheAll()`, scene/UI capture (incl. `buildGlyphs()` — pre-renders each non-ASCII byte from the game font for the hybrid text path), status-banner cache, cursor policy; implements native-extras hooks: `onAddToPicCel` (Feeder A — populates `_staticSprites`), `onInitCel` (Feeder A supplement — first-visit cels drawn during `_picNotValid` that bake into the native picture, e.g. QFG1 town signs; populates `_initCels`, one capture per owner object, latest wins; a cel is promoted only while its owner object is absent from the animate list — see the `_picNotValid` invariant above; both merged with the animate cast each frame via `mergeSpritesByPriority`), `beginNativeDraw`/`endNativeDraw`/`onNativeShowRect` (Feeder B bitsShow hook), `snapshotNativeBaseline` + `drawGenericRegions` (Feeder B pixel-diff backstop) |
| `engines/sci/roger/roger_compositor.h/cpp` | Composites plate + sprites (priority-masked), generic native regions (Feeder B), and the UI display-list (dialogs/banner/buttons/edit/icons) into the overlay; opaque-black letterbox; black dialog borders; `resetForRoomChange()` (nulls `_bgPlate`, sets `_bgRebuilt`, drops all dirty accumulators — called at end of `onTransition` so a transition-entry gets the same clean first frame as a save-restore entry, preventing stale dirty-rect history from the previous room); pure helpers: `mergeSpritesByPriority`, `mapNativeRectToOverlay`, `upscaleNativeRegionNearest`, `extractChangedBoxes` |
| `engines/sci/roger/roger_text.h/cpp` | TTF text fit/draw; type scale driven by captured native SCI font metrics (per-element target cell height + single-line width cap — same on-screen footprint as the original), falling back to role heights when no metric was captured; `firstLineTop`/`vAlignTop`, and the hybrid `drawPx` layout: ASCII drawn with the TTF font, each non-ASCII byte blitted inline as the game's own font glyph (from the element's glyph map, scaled to ¾ line height) |
| `engines/sci/roger/roger_ui_layer.h` | Resolution-independent `UiElement` display-list |
| `engines/sci/roger/view_cache.h/cpp` | Serves upscaled hires VIEW cels for ego/props/inventory by generating them on first use via `RogerAssetGen::generateViewCel` and caching them (owned). No prebuilt spritesheets. |
| `engines/sci/roger/png_loader.h/cpp` | `loadGrayscale8()` / `loadSurfaceRGBA()` via `Image::PNGDecoder` |
| `engines/sci/graphics/{paint16,controls16,menu,event}.cpp` | Hook sites: picture replace, dialog/control capture, status/menu bar, F10 toggle; `paint16.cpp` also hosts the `bitsShow` hook (`onNativeShowRect`), `beginNativeDraw`/`endNativeDraw` re-entrancy guards (Feeder B), and `drawCelAndShow`→`onInitCel` (owner 0) for script kDrawCel draws during `_picNotValid` |
| `engines/sci/graphics/animate.cpp` | Hook sites: `addToPicDrawCels`/`addToPicDrawView` call `onAddToPicCel` (Feeder A — static addToPic cel capture); the cast-draw sites in `update()`/`drawCels()` call `onInitCel` during `_picNotValid`, tagged with `rogerOwnerToken(it->object)` (Feeder A supplement — owner-gated promotion) |
| `engines/sci/sci.cpp` | Provider instantiated after `initGraphics()` (with `ConfMan.getPath("path")`), destroyed in destructor |

**Launcher:** `engines/sci/roger/roger_launcher.{h,cpp}` + `roger_launcher_dialog.{h,cpp}`. `RogerLauncher` discovers SCI game domains from ConfMan, manages the `LauncherState` (selected game, precache queues, settings), and is called at engine startup via `FileRogerArtProvider`. `RogerLauncherDialog` is a `GUI::Dialog` that presents the game list, per-game settings, and precaching controls. On launch with no cache, precaching runs automatically in `handleTickle` before `handleLaunch` is called.

**Asset layout** (`sq3-roger` is a sibling of the `sq3` game directory). Nothing is *consumed* from disk anymore — the only on-disk artifacts are the content-hash generation cache:
```
sq3-roger/
  cache/
    <gameid>.omyac.v<ver>.<hash>.<passes>.png    ← generated hires plate (per pic, content-keyed)
    <gameid>.scale6x.v<ver>.<hash>.<passes>.png  ← generated hires VIEW cel (per view/loop/cel)
    <gameid>.omyacprio.v<ver>.<hash>.<passes>.png ← priority screen rendered through omyac, colour EGA priority view (per pic, content-keyed)
```
Plates and VIEW cels are generated in-engine from the SCI resources and written here on a cache miss (modes `cache`/`always`); `memory` generates without writing. The cache key embeds `kTransformVersion`, so a pipeline change invalidates stale files automatically.

**Config knobs** (all `ConfMan.hasKey(...)`-gated; see `docs/roger.md` for the full table): `roger_gen_mode` (default `cache`; `prebuilt` = native-only off-switch, `memory`, `always`), `roger_precache` (default `all`; `pics`|`views`|`off`), `roger_omyac_passes`, `roger_ui_font_scale` (default 150; percent multiplier, 100 = no nudge), `roger_ui_font`, `roger_ui_header_font`, `roger_hw_cursor`, `roger_cursor_size`, `roger_dirty_present` (default on; convert+push only the changed regions each frame — dirty-rectangle present — set false to force a full-region present), `roger_transitions` (default on; mirrors SCI fade/dissolve/wipe/scroll and shake in the overlay; Wipe/Scroll currently render as Dissolve), `roger_palette_live` (default on; re-applies the live SCI EGA palette to the hires plate each frame for cycling, fade-to-black, and flash effects), `roger_autoshot`, `roger_debug`, `roger_diag` (default off; structured overlay-state trace at room-load/present/cel-draw seams — arm per-launch with `build_and_run.ps1 -Diag` (the `ROGER_DIAG` env var, env-first like `-Mode`; preferred over an ini edit, which races a running instance's config rewrite-on-exit), then grep `ROGER-DIAG[` in the ScummVM log; covers `drawPicture`, `drawCelAndShow`, `kDrawCel`, `kGraphUpdateBox`, `bitsShow`, `addToPic`, `initCel`, `genRegions`, `pushBG`, `transition`, `renderFrame`, `toggle`; kept permanently for the overlay-diagnosis class), `roger_diff_backstop` (default off; Feeder B pixel-diff backstop for unhooked native draws — off by default because the per-frame full-buffer diff is costly and can stamp blocky native pixels around moving sprites; the bitsShow-hook path and addToPic capture remain on). The `roger_visual_variant`/`roger_priority_variant` knobs are retired (they selected prebuilt files). `roger_display_mode` (default `enhanced`; `original`|`sbs`) sets the STARTUP display mode — `build_and_run.ps1 -Mode <m>` pins it per-launch via the `ROGER_DISPLAY_MODE` env var (env-first, never touches the ini). F10 (and Ctrl+Shift+U) cycles three display modes — Enhanced → Original (native) → Side-by-Side → Enhanced. Side-by-side: left = enhanced view (backgrounds/cels/dialogs/updates); right = passive native mirror (pics/views/animations) for old-vs-new comparison and intro screenshots. It's a view-only mode — a single composited cursor floats under the pointer, but clicks aren't remapped; switch to Enhanced to play. Ctrl+Shift+[ ] / ; ' tune enhance passes live. Ctrl+Shift+F cycles the dialog/body font through a fonts.dat shortlist.

**Integration test:** Room 2 (pic resource 2). Walkability/native occlusion ride SCI's native render; overlay sprite occlusion uses the omyac-rendered priority screen (`omyacprio`, colour), so occlusion edges get the same upscaling as the plate.

**Tests:** `test/sci/roger/` (CxxTest) — require a `make`-based build to run (SCI as a static plugin).

### Stage 2: Native hires overlay compositor (implemented + verified)

The compositor draws the ego/props into the OSystem overlay at hires (upscaled native cels via the ViewCache, or rendered native cels as fallback) with SCI priority-band masking against the replacement art, and composites the SCI UI that would otherwise be hidden under the overlay: dialog windows (black border), the score/title banner (cached + re-applied on room load and F10 enable), buttons, top-aligned text-edit fields with a live caret, and inventory icons / look-at close-ups. The letterbox is filled opaque black so the native render (and its hardware cursor) cannot leak at the edges, and the cursor itself is the native hardware cursor (SCI sets arrow/wait/hand; smooth, correct over the overlay). All game logic stays at 320×200; only the display layer is hires.

The hires priority map for sub-pixel occlusion alignment is now generated in-engine (omyac-aligned `omyacprio` cache), so overlay occlusion tracks the displayed plate. Remaining art-side work: authoring better hires backgrounds and new hires VIEW art for room sprites.

**Native-extras capture (implemented + verified)** fixes the "many views missing per room" bug (notably QFG1 EGA) by routing native draws Roger did not previously hook into the compositor via two feeders. **Feeder A (addToPic + init-baked):** `kAddToPic` cels — static views baked into the room's picture, not in the animate list — are captured via `onAddToPicCel` (from `GfxAnimate::addToPicDrawCels`/`addToPicDrawView`) into a per-room `_staticSprites` array. First-visit cels drawn during `_picNotValid` (room init, before the picture is valid) — e.g. QFG1 town signs drawn at first visit that bake into the native picture — are captured via `onInitCel` (from the cast-draw sites in `GfxAnimate::update`/`drawCels`, tagged with an owner-object token, one capture per owner with the latest draw winning; and from `GfxPaint16::drawCelAndShow` for script kDrawCel draws, owner 0) into a per-room `_initCels` list. Both are cleared on room change. Each frame `renderFromAnimateList` promotes init cels whose owner object is ABSENT from the animate list into the static merge (deduped against addToPic) — a disposed-after-baking prop promotes, a live actor never does (promoting by view/loop/cel identity instead froze a duplicate ego or wiped the room signs; see the `_picNotValid` invariant) — then all statics + live cast are merged via `Roger::mergeSpritesByPriority` (static-first, stable ascending priority) and drawn through the hires Sprite path (ViewCache + priority occlusion) — not blocky. **Feeder B (generic native capture):** the long tail of unhooked native draws (kGraph primitives, etc.) is captured two ways: (1) a `bitsShow` rect hook (`onNativeShowRect` in `GfxPaint16::bitsShow`) records shown screen rects, gated by `beginNativeDraw`/`endNativeDraw` re-entrancy depth so already-composited draws are not double-captured; (2) a pixel-diff backstop — `snapshotNativeBaseline()` (called in kernelAnimate after updateScreen) snapshots the native visual buffer; at composite time `drawGenericRegions` diffs the current buffer against it via `Roger::extractChangedBoxes` and composites changed boxes not already recorded. Captured regions are mapped to overlay space via `Roger::mapNativeRectToOverlay` and upscaled nearest-neighbour with `Roger::upscaleNativeRegionNearest` (intentionally blocky). Inter-room animated sequences (ship flyovers, death sequences) are out of scope. The diff backstop is opt-in via `roger_diff_backstop` (default off) — see Config knobs.

The overlay present is **dirty-rectangle by default** (`roger_dirty_present`): each frame converts+pushes only the regions that actually changed (sprites, cursor, UI) plus the union of the previous frame's, instead of the whole game region — at 2862×1986 this cut present from ~26 ms to ~2 ms. Sprite rects are tracked at *renderScene* granularity and UI/cursor rects at *present* granularity (see `roger_compositor.cpp` `dirtyUnion`/`rollPresentDirty`), so a UI-only present (`presentWithUi`: cursor move / dialog, no `renderScene`) cannot discard sprite-erase history. Room change / geometry / F10 and a periodic heal frame still do a full present; any uncertainty falls back to a full present (never a skipped/garbage frame).

#### Performance discipline (read before touching the per-cycle path)

The overlay is a full-frame ~22 MB RGBA surface (2862×1986). A **full recompose (`renderScene`) or full present (`presentWithUi` / `_compositeCacheValid = false`) is expensive (~6–26 ms) and runs inside SCI's single-threaded game cycle** — so doing it every cycle stretches the cycle and makes the *game logic* (walking speed, input latency) physically slow. This is a throughput problem on the synchronous cycle, not a smoothness/frame-rate one.

**The rule: only recompose / full-present when something actually changed.** The dirty-rectangle path enforces this for the normal sprite/UI flow — keep it that way. **Any new native hook that runs per cycle (anything reachable from `kernelAnimate`: `bitsShow`, `bitsRestore`, `onNativeShowRect`, `onAddToPicCel`, kGraph hooks, etc.) must be O(1)/cheap and must NOT trigger a full present or invalidate the composite cache unless its work genuinely changed the scene.** Gate the present on real change (e.g. `uiClearToken` presents only when `clearToken` actually removed an element).

> **Cautionary tale (regression fixed 2026-06-28, commit `bb65c56b75a`):** `GfxPaint16::bitsRestore` calls `uiClearToken()` ~2× per moving sprite *every* cycle; it used to fire `presentWithUi()` (a full overlay present) **unconditionally**, even while walking when no UI token matched. That alone cost ~196 ms/cycle (`restoreAndDelete` was ~196 ms Roger-on vs ~0 ms prebuilt) — a ~2.7× walking slowdown (225 ms vs 83 ms cycle). The fix was to present only on an actual clear.

**Do NOT re-chase these dead ends** (measured, ruled out): the **render/present primitives are not the bottleneck** at this resolution — GPU flip ~0.3 ms, full 22 MB texture upload ~9.5 ms, full CPU recompose+upload ~20 ms; the **OpenGL backend ≈ software** when the whole overlay is re-touched each frame (CPU-frame-production-bound), so switching backends or micro-optimizing the present buys nothing until you *stop re-touching the whole surface*. `EventManager::updateScreen` fires only **~5–10×/sec (once per cycle), not 60**, so present-skip heuristics keyed on 60 fps are pointless. If a cycle-time/walking slowdown reappears, suspect a per-cycle path repeatedly invoking the full present/recompose — measure `kernelAnimate` span costs (invoke/draw/show/restore/rfal) busy-vs-sleep, don't optimize the present primitive.

### Stage 3: Fork structure & upstreaming (future direction — rules apply NOW)

This repo is a **downstream ScummVM fork** whose endgame is: (primary) ship Roger in this
fork indefinitely; (secondary) keep a credible path to upstreaming. Two cleanup passes are
planned (a fork audit/restructure pass, then a manufactured-clean-branch pass); nothing
below triggers them — it exists so day-to-day work doesn't paint us into a corner before
they run.

**What Roger is (and is not).** Roger is a **display-layer provider inside the existing SCI
engine** — NOT a new ScummVM engine. It has no engine class, no metaengine, no detection
tables, and must never grow them; detection stays SCI's. Any plan or prompt phrased in
"new engine in `engines/<name>/`" vocabulary translates as: "engine directory" →
`engines/sci/roger/`; "engine registration/wiring" → the future provider-registration API
(`setArtProvider()`); "detection/metaengine" → nothing (unchanged SCI). **Decision: never
fork `engines/sci/` into a duplicated `sci-roger` engine** — upstream would reject engine
duplication outright, two engines claiming the same games breaks detection, and it converts
a ~725-line maintained diff into a whole-engine merge burden.

**The real diff footprint** (vs `origin/master`, outside `engines/sci/roger/` which moves
wholesale). Keep this inventory current when adding hooks — it pre-answers the audit pass:

| Files | ~Lines | Category / upstream story |
|-------|--------|---------------------------|
| `graphics/paint16.{cpp,h}`, `animate.cpp`, `controls16.cpp`, `menu.{cpp,h}`, `ports.cpp`, `text16.cpp`, `transitions.cpp`, `engine/kgraphics.cpp`, `graphics/scifont.{cpp,h}` | ~590 | **Observer-seam candidates** — mechanical, null-guarded provider call sites at SCI's structural chokepoints. Upstreamable if reshaped as a neutral, engine-owned observer interface, compiled out by default. The planned frame-complete present barrier should *replace* several of these — prefer that over adding more. |
| `sci.cpp`, `module.mk` | ~70 | **Provider wiring** — becomes plugin self-registration via `setArtProvider()`; `roger/*.o` move to the plugin's own `module.mk`; `test/module.mk` relinks tests against a roger static lib |
| `event.cpp` + `gui/EventRecorder.h` | ~80 | **Separately pitchable upstream PR** — the `.rin` input driver is a generic headless scripted-input facility complementing EventRecorder; deliberately engine-agnostic (keep it that way) |
| `build_and_run.ps1`, `build_tests.ps1`, `roger_run.ps1`, `CLAUDE.md`, `.claude/`, `.gitignore` | — | **Downstream-only** dev tooling; never part of an upstream PR |

**Rules that keep the future cleanup cheap (enforce on every change):**

- Hook sites in `engines/sci/**` stay **mechanical**: a null-guarded `g_sciRogerProvider`
  call plus minimal argument marshalling. No Roger logic, no game-specific branches, no
  Roger types beyond the provider interface, inline in SCI code.
- Every new hook is a **virtual on the abstract provider** (`roger_art_provider.h`) — SCI
  code never names `FileRogerArtProvider`.
- Before adding a new scattered hook site, check whether the present-barrier /
  exact-invalidation design covers the need — shrinking the hook count is an upstreaming
  goal, not just hygiene.
- No changes to other engines; no behavior change in SCI when the provider is null
  (Roger-off must stay byte-identical to stock).
- No game assets or proprietary data in the repo, ever. Game data + generation cache live
  in sibling directories outside the repo; test fixtures must be tiny synthetic files
  (current PNG fixtures are ~75 bytes each); screenshots stay in gitignored `screenshots/`.
- Standard ScummVM GPL headers on every new source file (existing roger files comply).
- Upstream-facing / fork-maintenance docs go under `docs/roger/` (audit artifacts:
  `FORK_AUDIT.md`, `UPSTREAMING_PLAN.md`, `PR_PLAN.md`, `FORK_MAINTENANCE.md`,
  `DATA_LAYOUT.md`, `LEGAL.md` when the audit pass runs). Superpowers working specs/plans
  (`docs/superpowers/`) are downstream-only and never part of an upstream PR.

**Branch & history policy:** upstream base is `origin/master`. The `jon-*` lineage
(currently `jon-refactor1`) is the **deploy line** — merge-maintained, never rebased, must
always stay deployable. Its commit history is **raw material, not a reviewable record** —
clean upstream branches will be *manufactured from the final diff* (not cherry-picked),
short-lived, rebase allowed there only. Any history surgery requires a backup branch/tag
first and explicit user approval. Per-slice build verification on Windows uses
`build_tests.ps1` (unit tests need the make-based path) and `build_and_run.ps1 -Script`
smoke runs — the audit prompts' configure/make assumptions don't apply here.

**Upstream pitch (when the time comes):** discuss on scummvm-devel/Discord *before*
writing PRs — ScummVM has a strong talk-first culture. The story to tell is the strong
one Roger actually has: it runs original commercial games from their original data,
display-only enhancement, opt-in, byte-identical when disabled — not a new private game
(any scope-risk analysis written for that scenario should be rewritten in these terms).
Shape: one coherent engine-seam PR with a few clean commits, plus small separate PRs for
generic pieces (the input driver). If upstream declines, the fallback is this fork with a
deliberately minimized diff — never a duplicated engine.

## Code Style

- **C++11**, tabs for indentation (width 4), no column limit
- Pointer/reference aligned to the right: `int *ptr`, `void foo(int &bar)`
- Braces attached (K&R style): `if (x) {`
- `.clang-format` is present and enforces these rules
- No exceptions (`-fno-exceptions`), no RTTI
- All code must be GPLv3+ compatible

## Repository Architecture

### Core modules (shared across all engines)

| Directory | Purpose |
|-----------|---------|
| `common/` | Shared utilities: strings, streams, containers, file system, config manager, archive formats, event manager |
| `graphics/` | 2D rendering: surfaces, pixel formats, font management, scalers, Mac GUI widgets |
| `audio/` | Audio mixing, MIDI drivers, codec decoders |
| `video/` | Video codec decoders (Bink, Smacker, QuickTime, etc.) |
| `image/` | Image format decoders (PNG, JPEG, BMP, etc.) |
| `math/` | Math utilities (vectors, matrices, frustum) |
| `gui/` | ScummVM launcher GUI, dialog system, theme engine |
| `base/` | Program entry point (`main.cpp`), plugin manager, command-line parsing |

### Platform abstraction

`backends/` contains platform-specific implementations. All platforms implement the `OSystem` interface defined in `common/system.h`. The SDL backend (`backends/platform/sdl/`) is the primary desktop backend. Other backends include Android, iOS, libretro, and various consoles.

### Engine plugin system

Each game engine lives in `engines/<name>/` and integrates via:

- **`configure.engine`** — declares the engine to the build system (`add_engine` macro)
- **`module.mk`** — lists all `.o` files to compile
- **`MetaEngineDetection`** — handles game detection (can be compiled without the full engine for the detection plugin)
- **`MetaEngine`** — creates `Engine` instances, manages save states, provides GUI options
- **`Engine`** subclass — implements `run()` as the main game loop

The plugin system (`base/plugins.h`) supports both static linking and dynamic ELF plugins.

### Game detection

Most engines use the `AdvancedDetector` framework (`engines/advancedDetector.h`). Detection tables in `engines/<name>/detection_tables.h` list `ADGameDescription` entries with filename + MD5 pairs. The detector matches game files against these tables to identify the specific game version.

### Director engine (engines/director/)

The Director engine implements Macromedia/Macromedia Director games. Key components:

- **`DirectorEngine`** — top-level engine, manages windows and global state
- **`Movie`** — represents a Director movie file (`.DIR`/`.DXR`/`.MMM`)
- **`Cast`** — resource library holding all `CastMember` objects (bitmaps, sounds, scripts, text, shapes, etc.)
- **`Score`** — the timeline/sequencer; contains `Frame` objects that define which channels are active each frame
- **`Channel`** — one sprite slot in a frame
- **`Window`** — a stage or MIAW (Movie In A Window)
- **`Lingo`** — the scripting engine for the Lingo language (in `lingo/`)

The Lingo subsystem uses bison/flex (`lingo-gr.y`, `lingo-lex.l`) to parse scripts and compiles them to bytecode. XObject/XLib support is in `lingo/xlibs/` and `lingo/xtras/`.

## Developer Tools

- **`devtools/make_class.py`** — scaffolds a new C++ class in an engine (creates `.cpp`/`.h`, updates `module.mk`):
  ```sh
  python3 devtools/make_class.py scumm . LeChuck        # engines/scumm/le_chuck.{cpp,h}
  python3 devtools/make_class.py director lingo MyXObj  # engines/director/lingo/my_x_obj.{cpp,h}
  ```
- **`devtools/create_engine/`** — scaffolds a new engine
- **`devtools/dumper-companion.py`** — dumps HFS/HFS+ volumes and Mac game files
- Various `devtools/create_<engine>/` tools generate `.dat` data files for specific engines

## Adding a New Engine

1. Create `engines/<name>/` with `configure.engine`, `module.mk`, `detection.cpp`, `metaengine.cpp`, and the main engine class
2. Run `./configure` to pick up the new engine
3. Add detection entries in `detection_tables.h` using `ADGameDescription` structs and `AD_ENTRY*` macros
