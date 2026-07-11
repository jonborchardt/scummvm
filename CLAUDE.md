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
.\build_and_run.ps1 -Standalone              # no game target: standalone Roger picker
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
`screenshots/roger-run.log` (grep `ROGER-SCRIPT` / `ROGER-CYCLE`).

`-Studio` launches the **Roger Studio** tuning environment instead of a game
(`ROGER_STUDIO=1`, per-launch): a mouse-driven, single-scene tuner — the enhanced
plate with a view cel composited on it game-style (SQ3 defaults: pic 2, view 12
loop 1), two live A/B setting slots (params + the same `view enhance:` / `pic enhance:` mode cycles as the F12 panel (each incl. trailing nearest) + the shared +f/+l/+a/clear/add pass builder
each), Split and Diff comparison views with an automatic alignment readout,
click-to-place/drag cel, and stamped PNG export. Fully button-driven; Esc quits
and E exports (automation-only keys). Debug-only; never touches the generation
disk cache. Spec: `docs/superpowers/specs/2026-07-02-roger-studio-v2-ui-design.md`
(spec since removed from the repo, commit 9599a9c6d71; recover via git history).

Grammar
(full reference in `engines/sci/roger/roger_input.h` and `docs/roger.md`):

```
click X Y | rclick X Y | mousedown X Y | mouseup X Y | move X Y | key <token> | type "text"
wait <ms> | waituntil <key> <val> <timeoutMs> | capture <label> | snap <label>
state | assert <key> <val> | restore <slot> | fail <msg> | log <text> | quit   # '#' = comment
```

Coordinates are game space (320×200). Smoke script:
`test/sci/roger/scripts/qfg1-smoke.rin`. **Game walkthroughs for scripted play:**
`docs/walkthroughs/` (qfg1, sq3, Betrayed Alliance) — flat `command # purpose`
parser scripts, each opening with a claude-index section (phase table with unique
grep anchors, launch targets, save points, arcade/deadly sequences a `.rin` driver
must handle); grep the index to reach a location/item/scene instead of reading the
whole file. **The walkthrough `command` lines (`enter inn`, `go north`,
`ask sheriff about brigands`) are human-readable INTENT notes, NOT literal input —
you cannot `type "enter inn"` into the game.** SQ3 and QFG1 EGA are icon/mouse
driven (walk/look/hand/talk icon bar + click), not text-parser games, so a `.rin`
driver must translate each walkthrough step into `click`/`move`/`key`/icon
interactions in game space, never a typed sentence. (Confirmed with the user
2026-07-10 while chasing a phantom "QFG1 has no sound" report.) The injection seam is a registered backend
`EventSource` — keep `roger_input.{h,cpp}` free of SCI includes (engine-agnostic).
The phase-gate regression suite for the present-barrier refactor lives at
`test/sci/roger/run-regression.ps1` (manifest-driven; see the spec §8) — run it
after any change to the per-cycle or present path. KNOWN-STALE CHECK (since the
2026-07-05 stretch-mode commit dcaa7e2625a): `qfg1-menu-cycle presence:m-after`
fails deterministically with "0 differing px" — the manifest's regions are
window-height fractions and the game rect moved, so the top-4.5% band is pure
letterbox in both captures even though the menu opens fine (verified standalone;
the strip now lives at ~4.5–12%). Don't re-debug it as a code regression; the fix
is re-deriving the manifest regions (or re-baselining) — until then 41/42 is the
expected green.

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
- **Prefer `snap` over `capture` + flush-move.** `snap` grabs the presented overlay
  pixels at execution time (grabOverlay) — works mid-blocking-dialog with no `move`
  choreography. `capture` (pended, present-consumed) remains the tool when the claim
  under test is the present pipeline itself. `state` emits
  `ROGER-STATE pic=<n> ego=<x>,<y> windows=<n> mode=<m>` — grep it instead of reading
  pixels for room/arrival/dialog-count verdicts. `waituntil pic <n> <timeout>` replaces
  fixed 8–12 s room-crossing waits (timeout logs a warning and continues; pair with
  `assert` for a hard fail). `assert`/`fail` end the run with a `ROGER-SCRIPT: FAIL`
  marker, which build_and_run.ps1 surfaces as exit 125 (124 = watchdog hang).

**Screenshots:** never write screenshots (or `.rin` capture output) to the repo
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

- Design spec (single living doc): `docs/superpowers/specs/2026-06-19-roger-art-replacement-design.md` — no longer exists (it was a local-only file under the gitignored `docs/superpowers/`, never tracked, and is gone from disk); the superseded per-phase plans were removed (git history is the record of what shipped)
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

**The observer seam.** `SciGfxObserver` (`engines/sci/sci_gfx_observer.h`) is THE seam:
a neutral, engine-owned observer interface. SCI graphics chokepoints call only
null-guarded `g_sciGfxObserver` events (mechanical, no Roger types, byte-identical to
stock when null). The fork's changes outside `engines/sci/roger/` contain **ZERO Roger
references** (grep-enforced: `git grep -in "roger" -- engines/sci ':(exclude)engines/sci/roger'`
must show only upstream game text and `module.mk` object PATHS): the observer is
constructed via the neutral factory `createSciGfxObserver()` (declared in
`sci_gfx_observer.h`, defined in `roger/roger_register.cpp`), startup tooling/precache
runs through `onEngineStartup()`, and all hotkey/mouse handling goes through
`interceptEvent()`. **Never add a `roger/` include (or any concrete-provider call) to any
SCI translation unit outside `roger/`; new hooks are virtuals on `SciGfxObserver`, never
on the concrete provider.** All the old `g_sciRogerProvider`/`onNative*`/`uiPush*` names
are retired — the hook table below carries the mapping.

**How SCI0 draws (the mental model):**

- **Immediate-mode native renderer at 320×200.** SCI draws directly into three parallel
  byte buffers — **visual** (color), **priority** (z-band / occlusion), **control**
  (walkability / event zones). Roger replaces only the *display* of the visual buffer;
  priority/control stay native, which is why walkability and native occlusion "just work".
- **The overlay is retained; the native screen is immediate.** SCI "erases" transient
  content (text, dialogs) simply by **redrawing the scene underneath it**. The overlay has
  **no automatic erase** — a region repaints only when something dirties it. Since the
  present-barrier work (Phases 1–2, 2026-07-03/04), invalidation is AUTOMATIC and
  **layered**: the §3.1 exact seams (`markNativeDirty` fed by SCI's own bitsShow /
  bitsRestore / kGraphRedrawBox rects), the UI layer's per-element dirty loop
  (`_dirtyPrev`), the scene seed union, and the cycle-diff net (`roger_diff_net`) each
  independently reseed vacated regions. **Duty 3 is retired: do NOT add manual
  vacated-geometry code to new hooks.** Exactly two documented duty-3 exceptions keep a
  manual `markVacatedDirty` — no-save-under window disposals (now the `onWindowClose`
  path, formerly `uiClearToken`) and the frame box (now `onFrameBox` internal, formerly
  `uiPushFrameBox`) — classes where no bitsRestore rect ever fires and the
  net is blind (it diffs the NATIVE buffer, and a frozen cycle takes no snapshots).
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
  its clear and ghosts until the next window reuses the id. (Fix: `onText` (formerly
  `onNativeText`) appends into the `_journal` immediately; it does not wait for
  `flushGenericText` in the next cycle.)

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
  it with `_ports->offsetRect` before use (matches every controls16 hook). *Since 089dab855b2
  (2026-07-06) capture is PER LINE inside Box's draw loop*, at the exact placed rect (measured
  width/height + alignment offset + line row) — element rects are the DRAWN extent, never the
  caller's requested box. That killed three intro bug classes at once: multi-line re-wrap
  drift (lines off their native rows, SQ3 credits overlapping), rollback containment misses
  (requested-box rects were wider than the save-under restore rect, so dismissed text
  ghosted), and whole-block fit shrinking text. kernelDisplay's companion hook pushes only
  the background fill now (its whole-string element used to out-dedupe the per-line ops),
  and kDisplay's flush `bitsShow` calls are begin/endSelfDraw-bracketed (formerly
  begin/endNativeDraw) so Feeder B does not pixel-stamp doubles of already-captured text.

**Save-under is a real but INCOMPLETE erase signal.** `bitsSave`/`bitsRestore` back most
transient overlays, and `bitsRestore` fires `onRestore` (formerly `onNativeRestoreRect`; a journal rollback —
the saved pixels are revealed, never re-captured). But transparent / no-save-under windows
and `reanimate == false` disposals **skip it** — which is exactly why `removeWindow` (not
`bitsRestore` alone) is the dependable dispose hook. *Structural since e5c0fa2d115:
restores are rollbacks; the `_revealRects` set suppresses Feeder-B re-capture of restored
pixel shows; the `beginSelfDraw` (formerly `beginNativeDraw`) suppression that previously
lived in `GfxPorts::removeWindow` is deleted — reveal rects replace it on the restore path.*

**The same content can be captured by more than one hook** (controls16 semantic + generic
`Box` + `bitsShow` pixel). Keep a dedup/lifetime discipline (namespace tokens + covered-rect
dedup) so redundant copies don't outlive each other and ghost.

**`_picNotValid` = room init.** Cels drawn while it's set bake into the picture — capture them
via `onCel(source=initBake)` (formerly `onInitCel`; Feeder A supplement) or they go missing on first visit. The init-frame cast
draws BOTH the baked decorations (their objects dispose out of the animate list after baking)
AND live actors like the ego, and no view/loop/cel identity separates them (SCI0 packs both
into one per-room view resource; both are in the cast on frame 1). The discriminator is the
**owning animate object**, tagged at capture: an init cel is promoted only while its owner is
absent from the animate list. Promoting by any weaker rule either froze a duplicate ego at the
room-entry position or wiped the room signs — both shipped as bugs once.

**Kernel drawing primitives → where Roger hooks them** (the game-agnostic seams):

The seam is the neutral engine-owned `SciGfxObserver` (`engines/sci/sci_gfx_observer.h`);
SCI code calls the null-guarded `g_sciGfxObserver` events below (the older
`g_sciRogerProvider`/`onNative*`/`uiPush*` names are all retired — see "The observer
seam" above for the mapping):

| SCI primitive | What it does | Observer event |
|---|---|---|
| `GfxPaint16::drawPicture` | room background render (fills visual/priority/control) | `onPicture` (formerly `pushHiresBackground`) |
| `GfxAnimate::kernelAnimate` | the game cycle + full cast draw | `onAnimateFrame` (formerly `renderFromAnimateList`); `onFrameStart`/`onFrameEnd` bracket the cycle |
| `addToPicDrawCels/View` | static cels baked into the picture | `onCel(source=addToPic)` (Feeder A; formerly `onAddToPicCel`) |
| cast draw / `drawCelAndShow` during `_picNotValid` | first-visit static props (owner-tagged) | `onCel(source=initBake, owner)` (Feeder A; formerly `onInitCel`) |
| `GfxText16::Box` | **all** text-out | `onText(source=textBox)` (formerly `onNativeText`) |
| `GfxPorts::openWindow` / `removeWindow` | window create / **dispose** | `onWindowOpen` / `onWindowClose` (formerly `uiPushWindow` / `uiClearToken`) |
| `kDrawControl` (button/text/edit/icon/list) | dialog controls | `onControl` (button/textEdit) / `onText(source=control/listRow)` / `onCel(source=icon)` (formerly `uiPushButton`/`uiPushText`/`uiPushTextEdit`) |
| `bitsSave` | save-under snapshot (checkpoint) | `onSave` (journal checkpoint; formerly `onNativeSaveRect`) |
| `bitsFree` | free a save-under without restore (drop) | `onFree` (journal dropCheckpoint; formerly `onNativeFreeSave`) |
| `bitsShow` / `bitsRestore` | native region show / save-under restore | `onShow` (Feeder B; formerly `onNativeShowRect`) / `onRestore` (journal rollback + reveal + dirty + barrier; formerly `onNativeRestoreRect`) |
| `kGraphRedrawBox` | in-place region erase/redraw | `onErase` (formerly `onNativeEraseRect`) |
| `kGraphFrameBox` | selection frame primitive | `onFrameBox` (formerly `uiPushFrameBox`) |
| status/menu bar | top strip | `onText(source=status/menuBar/menuRow)` via `roger_menu_model`; `onMenuHighlight` (formerly `uiPushStatus`) |
| transitions (fade/dissolve/wipe/scroll/shake) | scene change FX | `claimTransition` / `claimShake` (formerly `onTransition`) |
| palette (cycling / fade) | live EGA palette | `onPaletteChanged` → `roger_palette_live` re-apply |
| cursor set/hide (kSetCursor) | pointer visual | `claimCursor` + `onCursorShape`/`onCursorView`/`onCursorHidden` (formerly `hidesNativeCursor`) |
| self-composited draw brackets | suppress double-capture | `beginSelfDraw` / `endSelfDraw` (formerly `beginNativeDraw`/`endNativeDraw`) |
| whole-frame native snapshot (SBS panel) | native mirror | `onFrameEnd` (formerly `snapshotNativeBaseline`) |

**Traps — do NOT re-fall into these (each cost a debugging session):**

- Deferring lifetime-bound overlay state to the animate cycle → ghosts through blocking dialogs.
- Assuming every disposal fires bitsRestore → transparent / no-save-under windows and the
  frame box never do; their retained `markVacatedDirty` calls (the two documented duty-3
  exceptions) are the only same-present invalidation for that class.
- Trusting a green gate on invalidation-mark changes → layered redundancy makes mark
  removal invisible to every scripted capture (Phase 2 proved it four ways, including
  real-overlay grabOverlay captures landing on the dismissal present); invalidation
  changes are verified by interactive soak, not by the gate.
- Clearing/erasing by **geometry** (rect containment) instead of by **window token** → false drops (a popup over the char sheet wipes stat text beneath it). *Structural since 50a8522486f: the journal owns this — window brackets (open/close) own lifetime and generic text is keyed by draw-time port id inside them, while erase-rect containment does geometric removal; the two are no longer at odds.*
- A per-cycle hook that forces a full present/recompose when nothing changed → walking slowdown.
- Gating the text hook on `show == true` → misses all SCI0 EGA text.
- Using port-local rects without `offsetRect` → offset text that never matches global erase rects.
- Handing a **screen-global** rect (bitsShow / UiElement nativeRects, 320×200) to a
  `Roger::Sprite.celRect` (**picture-local**, 320×190, origin below the status strip) → the
  stamp composites `picScreenTop` rows too low; the menu bar's black underline row (screen
  row 9) stamped as a full-width dark line across the top of every scene (fixed 2026-07-04,
  `c8f3d44ecc3`). Convert at the seam: `onAnimateFrame` (formerly `renderFromAnimateList`)
  translates fg stamps to picture-local and compares the live-cast exclusion in screen space. Related: only windows
  with `hasFrame` get the compositor's black border — the status banner is frameless (SCI
  NOFRAME), and framing every `kUiWindow` drew a border line under the bar (same commit).
- Clipping a sprite's **dest rect** to `picRect` when it hangs off the screen edge → the full
  cel still scales into whatever rect remains, a visible squish at every edge. Clip the
  **paint** instead: `blendScaleBlitNearest`'s optional clip rect samples source coords
  against the full dest rect, cropping off-screen content like native SCI port clipping
  (fixed 2026-07-04, `a79dace6cd8`). Related: `ManagedSurface::blendBlitFrom` computes its
  right/bottom source crop against the SOURCE size instead of the dest surface, so a dest
  rect hanging off the screen's right or bottom edge empties the src rect and the whole
  blit silently no-ops — the composited cursor vanished entirely at the right screen edge
  (fixed 2026-07-09). Never blendBlitFrom anything that can reach a surface edge; use
  `blendScaleBlitNearest` (1:1 when dst == src size).
- Replacing a re-pushed UI element **in place** in the retained display list → violates
  native immediate-mode ordering (the last draw is on top). The QFG1 char-sheet selection
  frame lost its bottom edge to the next row's blank cel, which overlapped it by 1 native px
  and stayed later in first-push order. *Structural since 24fe0d5cb46: the journal owns this —
  `RogerJournal::append` is append-only and `opSupersedes` retires the old op in place of an
  in-list swap, so append order == draw order (last on top) by construction. (`RogerUiLayer`
  and its `push` are deleted; the journal replaces them.)*
- A **blanket namespace clear** on a generic event (a `0x50000000`-namespace token clear on
  every `kGraphRestoreBox`, formerly `uiClearToken(0x50000000)`) → wiped ALL kDrawCel icons
  (char-sheet portrait + stat graphics) when
  any small save-under restored. Scope removals by the ERASE RECT geometry in
  `onErase` (formerly `onNativeEraseRect`; containment), the same rule generic text uses (fixed 2026-07-04).
  *Structural since 50a8522486f: the journal owns geometric removal — erase-rect containment
  in the journal's prune path retires only the ops the erase rect covers, so a blanket
  token clear is no longer even expressible.*
- **Capture during a FROZEN cycle predates the reveal rect** — lifetime gates must run at
  process/composite time, not only at capture time. A blocking menu loop captures
  `onShow` (formerly `onNativeShowRect`) while the cycle is frozen; the `bitsRestore` (and its `_revealRects`
  entry) only arrives AFTER the menu closes. Without a second check at `processForegroundCaptures`,
  restored background content gets stamped as new overlay content. Similarly, `rollback` must
  spare persistent singletons (status `0x10000000`, frame box `0x70000000`) that repaint while
  a save-under is open — they postdate the checkpoint but are NOT save-under content.
  *Structural since 53da7951bd1: reveal suppression also runs at process time (≥90% coverage);
  rollback skips the persistent singleton tokens.*
- Classifying an init-frame (`_picNotValid`) draw by **resource identity** (view / view+loop /
  view+loop+cel) instead of by its **owning object's lifetime** → either a frozen duplicate ego
  at the room-entry position or wiped room signs/stocked shelves, depending on which rule you
  pick. Both actors and to-be-baked props draw through the frame-1 cast, and SCI0 packs both
  into one per-room view resource (QFG1 300: signs = view 300 loop 2, live bard/goblin = loops
  0/1/3) — NO identity rule can separate them. The only reliable signal is whether the capture's
  owner object is still in the animate list (present = drawn live, never promote; gone = baked,
  promote). Same lesson as the window-token trap above: key off object lifetime, not geometry
  or resource ids.
- Calling `pushHiresBackground` for anything that is NOT a real room entry → it clears the
  per-room captured sprites (`_staticSprites`/`_initCels`/`_textSprites`), which are captured
  ONCE at the room's actual entry draws and can never be re-captured mid-room — the QFG1 signs
  and seated NPCs vanished after every live pass-tuning regen until 00ca23faf36. A regen-in-place
  must carry those arrays across the call (and must empty `_textSprites` before it — its entries
  own their `celOverride` surfaces and `clearTextSprites()` frees them under shallow copies).
  Both mid-room re-push cases are now in-tree: `regenInPlace` (tuning) and
  `pushHiresBackgroundAddTo` (addTo overlay pics) carry the arrays and regenerate via
  `pushHiresBackgroundInternal`, which reads the pic STACK — never reset `_picStack` outside
  the public `pushHiresBackground`.
- Treating an **addTo pic** (`kDrawPic` with `addToFlag`) as a room entry → the plate is
  REPLACED by the overlay pic's standalone render, losing the base scene (SQ3 intro: white
  title screen, black scanner starfields — fixed addad075e6a). An addTo pic paints over the
  previous pics WITHOUT clearing: the provider keeps the kDrawPic sequence as `_picStack`
  and generates plate + priority map from the CONCATENATED command lists (exactly native
  replay semantics), content-keyed by all contributing pics' chained hashes.
- Mapping a native rect edge to overlay space with **flooring** division (`v * dst / src`) →
  sub-pixel seams at every boundary between separately-mapped content: the edge consistent
  with the top-left rational sampling ALL Roger nearest scalers use is **ceiling** division
  (`Roger::mapNativeEdge` in `roger_coords.h` — the enhanced status bar rendered 1px
  narrower than native, 2026-07-09). Every native→overlay rect conversion goes through it;
  never open-code the division. Related: the status strip is fully overlay-owned —
  `onText(source=status)` (formerly `uiPushStatus`) + `menuRebuildBar` draw the black underline row (`_menuLine`,
  `statusStripRemainder`) so no visible seam depends on how the backend samples its game
  blit (its convention differs from Roger's and is not observable).
- Composing anything into `_scratchScene` other than renderFrame/presentWithUi's own frame →
  renderScene redraws only its seed union per frame and relies on the scratch's remaining
  pixels persisting; presentComparison composing the split layout there produced a RECURSIVE
  nested split once a full `_compositeCache` copy baked the corruption in (fixed 3da842f10c4 —
  the SBS present has its own `_sbsScratch`, forced fully opaque because the backend's native
  render is misaligned with both panels and bled through the transparent status strip).
- A ScummVM GUI modal (save/load chooser, GMM) COMMANDEERS the overlay: `ThemeEngine::enable`
  → `showOverlay` + `clearAll` (=`clearOverlay` `fill(0)` + `grabOverlay`), `redrawInternal`
  calls `clearAll` UNCONDITIONALLY every full redraw, and `disable` → `hideOverlay` restoring
  nothing — all while the game cycle is FROZEN (kSaveGame runs the chooser with NO engine
  pause, so no observer event fires). Two consequences: (1) after close the overlay keeps the
  dialog pixels and only mouse-move cursor presents / the 300-present heal repaint it — fixed
  by the `onFrameStart` GUI self-heal poll (`overlayShown() && !isOverlayVisible()` →
  `markFullDirty`, commit 367b8c0b8fb); (2) the dialog BACKDROP is architectural — the OpenGL
  backend draws `_gameScreen` unconditionally then alpha-blends the overlay, and `clearOverlay`
  is `fill(0)` (transparent), so the user sees NATIVE art behind the dialog, never the enhanced
  frame (which lives only in the overlay the GUI zeroes). An `enable()`-only "grab don't clear"
  fix is overwritten by `redrawInternal`'s clearAll; the reported save flow is TWO GUI sessions
  (grid chooser closes → `SavenameDialog` opens with overlay already hidden), defeating
  enable-time capture. A real backdrop fix needs coordinated shared-GUI changes
  (enable/disable + clearAll/redrawInternal) verifiable only by a human (see below).
- **`snap`/grabOverlay captures the OVERLAY BUFFER ONLY, not the game-behind composite.** It
  cannot show or verify anything that depends on what shows THROUGH a transparent overlay
  region (e.g. the GUI-dialog backdrop = native `_gameScreen` behind a `fill(0)` overlay). A
  white/transparent snap outside a dialog does NOT mean the user sees white. Use a real
  window/desktop grab for composite verification; snap only proves overlay contents.

**Underused SCI signals worth exploiting later** (highest value first): a **palette-vary
per-tick** hook for smooth fades/cycling (current re-apply is binary); semantic TextEdit-caret
and list-selection hooks (vs pixel/diff capture). `kMessage` exists but SCI0 (QFG1/SQ3) uses
Print/Display — low priority. Verify a signal's current hook state before adding — several are
already partially wired. (Shipped from this list: the frame-complete present barrier,
2026-07-03, `presentBarrier`; per-line text rects, 2026-07-06, captured inside
`GfxText16::Box`'s draw loop.)

### Stage 1: Background replacement

Hook at top of `GfxPaint16::drawPicture()` fires `g_sciGfxObserver->onPicture()`. When the observer's provider is non-null and `hasBackground()` returns true (i.e. a generating `roger_gen_mode` is active), it lets SCI's **native picture render run** — which fills SCI's own 320×200 priority + control buffers, so walkability and native occlusion stay correct — then calls `pushHiresBackground()` (full pics) or `pushHiresBackgroundAddTo()` (addToFlag pics — the pic is APPENDED to the current scene's pic stack and the plate/priority map regenerate from the concatenated command lists; see the addTo trap above). That generates (or loads from the content cache) the hires plate for the pic and presents it to the OSystem overlay. The overlay's per-pixel sprite occlusion is derived **in-engine** by rendering SCI's **priority screen** through the *same* omyac pipeline as the visual (`RogerAssetGen::generatePriorityMap()`, the `omyacprio` cache) — priority codes are EGA colours, so the output is a colour EGA priority view, upscaled/edge-enhanced exactly like the plate; the occlusion bands are recovered from that render (nearest EGA colour → code), not from any prebuilt map.

**Status: implemented + verified.** Supports SCI0 EGA games (SQ3, QFG1 EGA) with omyac upscaling. VGA games are detected at startup and rejected with a warning (EGA-only). Generation activates with zero prebuilt files; `pushHiresBackground()` presents the generated plate to the overlay immediately on room load (no native→hires "pop"). Walkability/native occlusion ride SCI's native buffers; overlay sprite occlusion uses the priority screen upscaled through omyac.

**Key files:**

| File | Role |
|------|------|
| `engines/sci/sci_gfx_observer.h` (+ `.cpp`) | The neutral, engine-owned observer seam: `SciGfxObserver` abstract interface (~30 virtuals across L1 frame-lifecycle / L2 pixel-truth / L3 semantic / L4 claims — see the header for the full contract) + the `g_sciGfxObserver` global and `setSciGfxObserver()`/`sciGfxObserver()` registration; also hosts the token scheme (`gfxWindowToken` etc.). SCI code names only this: construction goes through the neutral `createSciGfxObserver()` factory (declared here, defined observer-side in `roger/roger_register.cpp`), startup tooling/precache through `onEngineStartup()`, and hotkeys/mouse remap/panel clicks through `interceptEvent()` — the concrete provider type appears nowhere outside `engines/sci/roger/` |
| `engines/sci/roger/overlay/roger_menu_model.{h,cpp}` | SCI-free menu-bar/dropdown state model — the exiled menu logic; fed by `onText(source=status/menuBar/menuRow)` + `onMenuHighlight`, re-composited each frame |
| `engines/sci/roger/roger_telemetry.h` | Observer-side `ROGER-CYCLE period=<ms> busy=<ms>` per-cycle telemetry (format unchanged; moved off the SCI hook sites) |
| `engines/sci/roger/gen/roger_asset_gen.h/cpp` | In-engine generation: `generatePlate()` (omyac plate for EGA), `generateViewCel()` (scale6x cel, EGA only), `generatePriorityMap()` (renders SCI's priority screen through omyac; overlay occlusion bands recovered from it), `priorityBands()` (legacy native occlusion bands), `generateTextSurface()` (renders one native-font glyph via `scaleNearest` → RGBA, for the hybrid text path), backed by a content-hash disk cache (`kTransformVersion`-keyed) |
| `engines/sci/roger/gen/roger_pic_native.{h,cpp}` + `roger_pic_parser` / `roger_omyac` / `roger_scale` / `roger_ega_blend` (all under `roger/gen/`) | The omyac pipeline: parse pic → native pre-render (exposes `NativeRef::priority`) → enhance passes → RGBA plate; scale6x for VIEW cels |
| `engines/sci/roger/file_roger_art_provider.h/cpp` | Provider: `hasBackground()` (generating-mode gate), `pushHiresBackground()` (generates+presents the plate, routes occlusion through `generatePriorityMap()` — hires omyac-aligned), `precacheAll()`, scene/UI capture (incl. `buildGlyphs()` — pre-renders each non-ASCII byte from the game font for the hybrid text path), status-banner cache, cursor policy; implements the `SciGfxObserver` virtuals: `onCel(source=addToPic)` (Feeder A — populates `_staticSprites`), `onCel(source=initBake, owner)` (Feeder A supplement — first-visit cels drawn during `_picNotValid` that bake into the native picture, e.g. QFG1 town signs; populates `_initCels`, one capture per owner object, latest wins; a cel is promoted only while its owner object is absent from the animate list — see the `_picNotValid` invariant above; both merged with the animate cast each frame via `mergeSpritesByPriority`), `beginSelfDraw`/`endSelfDraw`/`onShow` (Feeder B bitsShow hook), `drawGenericRegions` (Feeder B bitsShow-region compositing), `onFrameEnd` (whole-frame native snapshot for the Side-by-Side native panel) |
| `engines/sci/roger/overlay/roger_compositor.h/cpp` | Composites plate + sprites (priority-masked), generic native regions (Feeder B), and the UI display-list (dialogs/banner/buttons/edit/icons) into the overlay; opaque-black letterbox; black dialog borders; `resetForRoomChange()` (nulls `_bgPlate`, sets `_bgRebuilt`, drops all dirty accumulators — called at the end of the `claimTransition` handler (formerly `onTransition`) so a transition-entry gets the same clean first frame as a save-restore entry, preventing stale dirty-rect history from the previous room); pure helpers: `mergeSpritesByPriority`, `mapNativeRectToOverlay`, `upscaleNativeRegionNearest`, `extractChangedBoxes` |
| `engines/sci/roger/overlay/roger_text.h/cpp` | TTF text fit/draw; type scale driven by captured native SCI font metrics (per-element target cell height + single-line width cap — same on-screen footprint as the original), falling back to role heights when no metric was captured; `firstLineTop`/`vAlignTop`, and the hybrid `drawPx` layout: ASCII drawn with the TTF font, each non-ASCII byte blitted inline as the game's own font glyph (from the element's glyph map, scaled to ¾ line height) |
| `engines/sci/roger/overlay/roger_ui_layer.h` | Resolution-independent `UiElement` struct (the retained `RogerUiLayer` display-list class was deleted in 75a2be6fecc; the append-only `RogerJournal` in `overlay/roger_journal.{h,cpp}` owns ordering/lifetime now) |
| `engines/sci/roger/ui/roger_widgets.{h,cpp}` + `ui/roger_panel_style.{h,cpp}` | Shared panel UI kit: PanelWidget + packed-id hit-testing; PanelStyle palette, PanelFonts (TTF roles + bitmap fallback), PanelPainter -- the picker, pass builder, F12 tune panel, and Studio all draw through it |
| `engines/sci/roger/overlay/view_cache.h/cpp` | Serves upscaled hires VIEW cels for ego/props/inventory by generating them on first use via `RogerAssetGen::generateViewCel` and caching them (owned). No prebuilt spritesheets. |
| `engines/sci/roger/png_loader.h/cpp` | `loadGrayscale8()` / `loadSurfaceRGBA()` via `Image::PNGDecoder` |
| `engines/sci/sci_gfx_observer.{h,cpp}` | The seam itself — see the key-file row above; every hook site below calls its `g_sciGfxObserver` events |
| `engines/sci/graphics/{paint16,controls16,menu,palette16,cursor}.cpp` + `engines/sci/event.cpp` | Hook sites: picture replace, dialog/control capture, status/menu bar, F10 toggle; `paint16.cpp` also hosts the `bitsShow` hook (`onShow`), `beginSelfDraw`/`endSelfDraw` re-entrancy guards (Feeder B), and `drawCelAndShow`→`onCel(source=initBake)` (owner 0) for script kDrawCel draws during `_picNotValid`; `palette16.cpp` funnels `onPaletteChanged`; `cursor.cpp` fires `onCursorShape`/`onCursorView`/`onCursorHidden` and honors `claimCursor` |
| `engines/sci/graphics/animate.cpp` | Hook sites: `addToPicDrawCels`/`addToPicDrawView` call `onCel(source=addToPic)` (Feeder A — static addToPic cel capture); the cast-draw sites in `update()`/`drawCels()` call `onCel(source=initBake)` during `_picNotValid`, tagged with `gfxOwnerToken(it->object)` (Feeder A supplement — owner-gated promotion) |
| `engines/sci/sci.cpp` | Observer registered after `initGraphics()` via `setSciGfxObserver(createSciGfxObserver(...))`, then `onEngineStartup()` (early-exit when it ran a standalone tool or pushed an engine restart); the observer slot is cleared then the observer deleted in the destructor. Fully neutral — no roger/ include, no concrete type |

**Launcher:** `engines/sci/roger/launcher/` (`roger_launcher.{h,cpp}` + `roger_launcher_dialog.{h,cpp}`; capabilities documented in its README). `RogerLauncher` discovers SCI game domains from ConfMan, manages the `LauncherState` (selected game, precache queues, settings), and is called at engine startup via `FileRogerArtProvider`. `RogerLauncherDialog` is a `GUI::Dialog` that presents the game list, per-game settings, and precaching controls. On launch with no cache, precaching runs automatically in `handleTickle` before `handleLaunch` is called.

**Asset layout** (`sq3-roger` is a sibling of the `sq3` game directory). Nothing is *consumed* from disk anymore — the only on-disk artifacts are the content-hash generation cache:
```
sq3-roger/
  cache/
    <gameid>.omyac.v<ver>.<hash>.<passes>.png    ← generated hires plate (per pic, content-keyed)
    <gameid>.scale6x.v<ver>.<hash>.<passes>.png  ← generated hires VIEW cel (per view/loop/cel)
    <gameid>.omyacprio.v<ver>.<hash>.<passes>.png ← priority screen rendered through omyac, colour EGA priority view (per pic, content-keyed)
```
Plates and VIEW cels are generated in-engine from the SCI resources and written here on a cache miss (modes `cache`/`always`); `memory` generates without writing. The cache key embeds `kTransformVersion`, so a pipeline change invalidates stale files automatically.

**Cache invalidation discipline (PR flow):** cache files are validated by
NAME only — nothing ever checks their contents, so a forgotten version bump
serves stale art silently. Any commit touching the generation pipeline
(the files under `engines/sci/roger/gen/`: `roger_omyac`, `roger_scale`,
`roger_pic_parser`, `roger_pic_native`, `roger_ega_blend`, or the
`generate*()` bodies in `roger_asset_gen.cpp`)
must either bump `kTransformVersion` (same commit, history line appended at
the constant) or state "output bit-identical; kTransformVersion unchanged"
in the commit message — prove it with a `roger_gen_mode=always` run and a
byte-compare of one rewritten cache PNG. Old-version files are orphaned,
not deleted. The precache path relies on this discipline doubly: it skips
work purely on keyed-filename existence (`isPicCached`/`isViewCelCached`).
Next bump: also widen the cache hash to fnv1a64 (TODO at the probes in
`roger_asset_gen.h`).

**Config knobs** (all `ConfMan.hasKey(...)`-gated; see `docs/roger.md` for the full table): `roger_gen_mode` (default `cache`; `prebuilt` = native-only off-switch, `memory`, `always`), `roger_precache` (default `off`; `all`|`pics`|`views` — the launcher's per-game settings are the normal opt-in path; the synchronous warm-up only runs when the picker is skipped), `roger_omyac_passes` (canonical compact form: one char per pass, `f`/`l`/`a` or `2`/`1`/`0`, default `affffflaaa` = `Roger::kDefaultPassString` in `roger_passes.h` (pinned by string literal in `roger_passes.cpp`, never by registry index — an index insert once moved the default silently); legacy separated tokens `fill`/`f`/`2`, `line`/`l`/`1`, `all`/`a`/`0` still parse; unrecognized tokens warn and are skipped; unset = the default string, empty = wireframe. The picker shows a passes dropdown (default + `goodPassPattern` registry entries + current-if-unlisted + Custom…) that writes `roger_omyac_passes` to the selected game's ini section immediately on change; two one-shot ini keys support cross-game flow: `roger_picker_precache` and `roger_picker_launch` (self-consuming; consumed at `run()` start). Completed precache state is recorded as a marker file in `<gameid>-roger/cache/` (not an ini key; legacy `roger_cache_stamp` keys are auto-removed on read). The picker no longer reads or writes `roger_precache`, `roger_gen_mode`, or `roger_ui_font` (engine defaults are unchanged). Passes are part of the plate cache key, so changing the *effective* list orphans every cached plate — the next precache launch re-warms all pics, ~3 min for qfg1; give `-Script` runs `-TimeoutSec 400` after such a change), `roger_ui_font_scale` (default 150; percent multiplier, 100 = no nudge), `roger_ui_font`, `roger_ui_header_font`, `roger_hw_cursor` (default off — Roger composites its own arrow and the backend hardware cursor is actively hidden while Enhanced/Side-by-Side mode is on-screen, since it composites above the overlay and leaked at the screen edge; Original mode and `roger_hw_cursor=true` keep the stock native cursor), `roger_cursor_size`, `roger_dirty_present` (default on; convert+push only the changed regions each frame — dirty-rectangle present — set false to force a full-region present), `roger_transitions` (default on; mirrors SCI fade/dissolve/wipe/scroll/roll and shake in the overlay — each family renders faithfully via `transitionFamilyFor`, no Dissolve collapse), `roger_palette_live` (default on; re-applies the live SCI EGA palette to the hires plate each frame for cycling, fade-to-black, and flash effects), `roger_debug`, `roger_diag` (default off; structured overlay-state trace at room-load/present/cel-draw seams — arm per-launch with `build_and_run.ps1 -Diag` (the `ROGER_DIAG` env var, env-first like `-Mode`; preferred over an ini edit, which races a running instance's config rewrite-on-exit), then grep `ROGER-DIAG[` in the ScummVM log; covers `drawPicture`, `drawCelAndShow`, `kDrawCel`, `kGraphUpdateBox`, `bitsShow`, `addToPic`, `initCel`, `genRegions`, `pushBG`, `transition`, `renderFrame`, `toggle`; kept permanently for the overlay-diagnosis class), `roger_diff_net` (default on; per-cycle native-buffer diff at the animate seam invalidates changed regions — heals missed invalidation within one cycle; invalidation-only, never stamps pixels; escape hatch `=false` / env `ROGER_DIFF_NET=0`; `ROGER-NET sum32=<ms> boxes=<n>` telemetry under `-CycleLog`, where `boxes` is the last cycle's count), `roger_truth_capture` (default off; evidence mode — `.rin` captures dump the real overlay via `grabOverlay` instead of forcing a full recompose; per-launch `-TruthCap` / `ROGER_TRUTH_CAPTURE`; per-entry `truthCap` flag in the gate manifest), `roger_debug_capture` (default off; writes a per-pic manifest + a PNG per pixel-captured graphic sprite to `screenshots/`), `roger_diff_check` (default off; one-shot-per-pic in-engine native-vs-overlay diff, logs `ROGER-DIAG[diff]` boxes — the missing-graphics audit tool; never on the steady-state path), `roger_no_launcher` (default off; skips the Roger game-picker dialog — env `ROGER_NO_LAUNCHER` / `build_and_run.ps1 -SkipPicker`, auto-set when `-Game` is passed; also disables the standalone picker for no-target boots, falling through to the stock ScummVM launcher). The `roger_visual_variant`/`roger_priority_variant` knobs are retired (they selected prebuilt files). `roger_display_mode` (default `enhanced`; `original`|`sbs`) sets the STARTUP display mode — `build_and_run.ps1 -Mode <m>` pins it per-launch via the `ROGER_DISPLAY_MODE` env var (env-first, never touches the ini). F10 cycles three display modes — Enhanced → Original (native) → Side-by-Side → Enhanced. Side-by-side: left = enhanced view (backgrounds/cels/dialogs/updates); right = passive native mirror (pics/views/animations) for old-vs-new comparison and intro screenshots. A single composited cursor floats under the pointer, and clicks through either panel are remapped to game coordinates (`remapComparisonMouse`, applied inside the provider's `interceptEvent`), so the game stays playable while comparing. F11 toggles per-frame Roger diagnostic logging. The former Ctrl+Shift+* live-tuning aliases (pass edit / config reload / font cycle) were removed in favour of the F12 panel and config knobs (`roger_omyac_passes`, `roger_ui_font`). **F12 toggles the quick-tune panel** — a mouse-driven, enhanced-mode-only, session-only pass-tuning debug tool, quarantined as a kept dev utility at `engines/sci/roger/utils/tunepanel/` (its README states the quarantine contract; spec `docs/superpowers/specs/2026-07-05-roger-tune-panel-design.md`, a local-only gitignored file since removed; no ini/cache writes): a `log:` row mirrors the F11 diagnostic-log toggle; a `view enhance:` toggle cycles the view-scaler modes — the `roger_view_scaler.h` registry scalers (shipping 6x) plus a synthetic nearest (`kViewScalerNearest`, plain no-enhancement upscale, cache-bypassed) — applying on click; a `pic enhance:` toggle cycles the available OMYAC pass modes (a session list seeded from the `goodPassPattern()` registry in `roger_passes.cpp`, shared with the picker's suggestion buttons), applying on click; a linear pass builder (`+f`/`+l`/`+a` append, `clear` empties, chips are display-only) whose `add` registers the built sequence as a new pic-enhance mode, selects it, and applies (forces kGenMemory while off-config, restores the prior mode at config); the pic-enhance cycle also carries a trailing `nearest` (zero-enhancement native plate via `setPlateNearest`/`generatePlateNearestStack`), and RogerAssetGen memory-caches kGenMemory plates + priority maps (bounded FIFO keyed by content+passes+params) so revisiting a computed mode is a copy, not a regen; a title-row `<`/`>` button docks it left/right. Panel geometry is fixed game-space, locked by `test_tune_panel.h` so `test/sci/roger/scripts/tune-panel-smoke.rin` clicks stay valid.

**Integration test:** Room 2 (pic resource 2). Walkability/native occlusion ride SCI's native render; overlay sprite occlusion uses the omyac-rendered priority screen (`omyacprio`, colour), so occlusion edges get the same upscaling as the plate.

**Tests:** `test/sci/roger/` (CxxTest). On Windows, `.\build_tests.ps1` builds and
runs them (pass `-Regenerate` on the first build after adding/removing source files).
**Adding a test file takes TWO registrations:** `test/module.mk` globs `test/sci/roger/*.h`
for the make build, but `build_tests.ps1` feeds cxxtestgen an EXPLICIT list — add the
new header to `$RogerTestHeaders` and any new SCI-free `.cpp` it links to `$RogerSources`,
or the suite silently never runs (the tune-panel tests shipped green for four tasks
without ever executing; the count even "grew" from an unrelated parallel commit).
Sanity-check: the runner prints its test COUNT — after adding N tests, expect the
count to grow by N; an unchanged count means your file isn't wired in.

**Committing on Windows:** don't pass multi-line commit messages through PowerShell
here-strings (quotes/parens inside get mangled — it has corrupted two commits) or
`Out-File -Encoding utf8` (BOM poisons `git commit -F`). Write the message to a file
with the Write tool, then `git commit -F <file>`, and verify with `git log -1 --format=%B`.

### Stage 2: Native hires overlay compositor (implemented + verified)

The compositor draws the ego/props into the OSystem overlay at hires (upscaled native cels via the ViewCache, or rendered native cels as fallback) with SCI priority-band masking against the replacement art, and composites the SCI UI that would otherwise be hidden under the overlay: dialog windows (black border), the score/title banner (cached + re-applied on room load and F10 enable), buttons, top-aligned text-edit fields with a live caret, and inventory icons / look-at close-ups. The letterbox is filled opaque black so the native render cannot leak at the edges, and the cursor is Roger's composited arrow drawn into the overlay (the default — the backend hardware cursor is actively hidden while Enhanced/Side-by-Side mode is on-screen; Original mode and `roger_hw_cursor=true` keep the stock native cursor). All game logic stays at 320×200; only the display layer is hires.

The hires priority map for sub-pixel occlusion alignment is now generated in-engine (omyac-aligned `omyacprio` cache), so overlay occlusion tracks the displayed plate. Remaining art-side work: authoring better hires backgrounds and new hires VIEW art for room sprites.

**Native-extras capture (implemented + verified)** fixes the "many views missing per room" bug (notably QFG1 EGA) by routing native draws Roger did not previously hook into the compositor via two feeders. **Feeder A (addToPic + init-baked):** `kAddToPic` cels — static views baked into the room's picture, not in the animate list — are captured via `onCel(source=addToPic)` (formerly `onAddToPicCel`; from `GfxAnimate::addToPicDrawCels`/`addToPicDrawView`) into a per-room `_staticSprites` array. First-visit cels drawn during `_picNotValid` (room init, before the picture is valid) — e.g. QFG1 town signs drawn at first visit that bake into the native picture — are captured via `onCel(source=initBake)` (formerly `onInitCel`; from the cast-draw sites in `GfxAnimate::update`/`drawCels`, tagged with an owner-object token, one capture per owner with the latest draw winning; and from `GfxPaint16::drawCelAndShow` for script kDrawCel draws, owner 0) into a per-room `_initCels` list. Both are cleared on room change. Each frame `onAnimateFrame` (formerly `renderFromAnimateList`) promotes init cels whose owner object is ABSENT from the animate list into the static merge (deduped against addToPic) — a disposed-after-baking prop promotes, a live actor never does (promoting by view/loop/cel identity instead froze a duplicate ego or wiped the room signs; see the `_picNotValid` invariant) — then all statics + live cast are merged via `Roger::mergeSpritesByPriority` (static-first, stable ascending priority) and drawn through the hires Sprite path (ViewCache + priority occlusion) — not blocky. **Feeder B (generic native capture):** the long tail of unhooked native draws (kGraph primitives, etc.) is captured via a `bitsShow` rect hook (`onShow`, formerly `onNativeShowRect`, in `GfxPaint16::bitsShow`) that records shown screen rects, gated by `beginSelfDraw`/`endSelfDraw` (formerly `beginNativeDraw`/`endNativeDraw`) re-entrancy depth so already-composited draws are not double-captured. At composite time `drawGenericRegions` upscales those recorded regions into the overlay — mapped to overlay space via `Roger::mapNativeRectToOverlay` and upscaled nearest-neighbour with `Roger::upscaleNativeRegionNearest` (intentionally blocky). Inter-room animated sequences (ship flyovers, death sequences) are out of scope. (The former `roger_diff_backstop` per-frame pixel-diff backstop was removed; `roger_diff_net` now heals missed invalidation each cycle, and the whole native frame is snapshotted at `onFrameEnd` (formerly `snapshotNativeBaseline`) only for the Side-by-Side native panel.)

The overlay present is **dirty-rectangle by default** (`roger_dirty_present`): each frame converts+pushes only the regions that actually changed (sprites, cursor, UI) plus the union of the previous frame's, instead of the whole game region — at 2862×1986 this cut present from ~26 ms to ~2 ms. Sprite rects are tracked at *renderScene* granularity and UI/cursor rects at *present* granularity (see `roger_compositor.cpp` `dirtyUnion`/`rollPresentDirty`), so a UI-only present (`presentWithUi`: cursor move / dialog, no `renderScene`) cannot discard sprite-erase history. Room change / geometry / F10 and a periodic heal frame still do a full present; any uncertainty falls back to a full present (never a skipped/garbage frame).

#### Performance discipline (read before touching the per-cycle path)

The overlay is a full-frame ~22 MB RGBA surface (2862×1986). A **full recompose (`renderScene`) or full present (`presentWithUi` / `_compositeCacheValid = false`) is expensive (~6–26 ms) and runs inside SCI's single-threaded game cycle** — so doing it every cycle stretches the cycle and makes the *game logic* (walking speed, input latency) physically slow. This is a throughput problem on the synchronous cycle, not a smoothness/frame-rate one.

**The rule: only recompose / full-present when something actually changed.** The dirty-rectangle path enforces this for the normal sprite/UI flow — keep it that way. **Any new observer event that runs per cycle (anything reachable from `kernelAnimate`: `bitsShow`, `bitsRestore`, `onShow`, `onCel`, kGraph hooks, etc.) must be O(1)/cheap and must NOT trigger a full present or invalidate the composite cache unless its work genuinely changed the scene.** Gate the present on real change (e.g. the `onWindowClose`/restore clear path — formerly `uiClearToken` — presents only when a token was actually removed).

> **Cautionary tale (regression fixed 2026-06-28, commit `bb65c56b75a`):** `GfxPaint16::bitsRestore` fires the restore/clear path (then `onNativeRestoreRect`, now `onRestore`) ~2× per moving sprite *every* cycle; it used to fire `presentWithUi()` (a full overlay present) **unconditionally**, even while walking when no UI token matched. That alone cost ~196 ms/cycle (`restoreAndDelete` was ~196 ms Roger-on vs ~0 ms prebuilt) — a ~2.7× walking slowdown (225 ms vs 83 ms cycle). The fix was to present only on an actual clear.

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
`engines/sci/roger/`; "engine registration/wiring" → the observer-registration seam
(`setSciGfxObserver()`); "detection/metaengine" → nothing (unchanged SCI). **Decision: never
fork `engines/sci/` into a duplicated `sci-roger` engine** — upstream would reject engine
duplication outright, two engines claiming the same games breaks detection, and it converts
a ~725-line maintained diff into a whole-engine merge burden.

**The real diff footprint** (vs `origin/master`, outside `engines/sci/roger/` which moves
wholesale). Keep this inventory current when adding hooks — it pre-answers the audit pass:

| Files | ~Lines | Category / upstream story |
|-------|--------|---------------------------|
| `sci_gfx_observer.{h,cpp}`, `graphics/paint16.cpp`, `animate.cpp`, `controls16.cpp`, `menu.{cpp,h}`, `ports.cpp`, `text16.cpp`, `transitions.cpp`, `palette16.cpp`, `cursor.cpp`, `engine/kgraphics.cpp`, `graphics/scifont.{cpp,h}` | ~1058 raw / ≈1033 rule-adjusted (see FORK_AUDIT §10) | **The observer seam — reshape LANDED on jon-observer.** The scattered provider call sites are now mechanical, null-guarded `g_sciGfxObserver` events against the neutral engine-owned `SciGfxObserver` interface (`sci_gfx_observer.h`), the concrete provider named nowhere in SCI code. Compiled in unconditionally but byte-identical to stock when the observer is null. (Phase 2 draw-journal: restore path consolidated to `onRestore`; `ports.cpp` `beginSelfDraw` suppression deleted; `bitsSave`/`bitsFree` → `onSave`/`onFree`.) |
| `sci.cpp`, `module.mk` | ~70 | **Provider wiring** — fully neutral: the observer is constructed via `createSciGfxObserver()` (defined observer-side in `roger/roger_register.cpp`), registered via `setSciGfxObserver()`, and startup tooling runs through `onEngineStartup()`; event handling goes through `interceptEvent()` in event.cpp. Becomes plugin self-registration later; provider objects are inlined between the DISPLAY-ENHANCEMENT PROVIDER markers in `module.mk` (path names only; move to the plugin's own `module.mk`); `test/module.mk` relinks tests against a provider static lib |
| `event.cpp` + `gui/EventRecorder.h` | ~35 | **Separately pitchable upstream PR** — the `.rin` input driver is a generic headless scripted-input facility complementing EventRecorder; deliberately engine-agnostic (keep it that way) |
| `base/main.cpp` | ~13 | **Fork-only launcher seam** — guarded (`PLUGIN_ENABLED_STATIC(SCI)`) call running the Roger picker as the launcher round; replaces the stock launcher by default, `roger_no_launcher` opts out. Never upstreamable as-is. |
| `build_and_run.ps1`, `build_tests.ps1`, `CLAUDE.md`, `.claude/`, `.gitignore` | — | **Downstream-only** dev tooling; never part of an upstream PR |

**Rules that keep the future cleanup cheap (enforce on every change):**

- Hook sites in `engines/sci/**` stay **mechanical**: a null-guarded `g_sciGfxObserver`
  call plus minimal argument marshalling. No Roger logic, no game-specific branches, no
  observer/Roger types beyond the `SciGfxObserver` interface, inline in SCI code. The
  fork's changes outside `engines/sci/roger/` carry ZERO Roger references (grep-enforced;
  `module.mk` object paths and upstream game text are the only exceptions) — never add a
  `roger/` include or a concrete-provider call to any SCI file outside `roger/`.
- Every new hook is a **virtual on the neutral observer** (`sci_gfx_observer.h`) — SCI
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

**Branch & history policy:** upstream base is `upstream/master` (mirrored to local/
`origin` `master` — see the Git workflow below). The `jon-*` lineage
(currently `jon-refactor1`) is the **deploy line** — merge-maintained, never rebased, must
always stay deployable. Its commit history is **raw material, not a reviewable record** —
clean upstream branches will be *manufactured from the final diff* (not cherry-picked),
short-lived, rebase allowed there only. Any history surgery requires a backup branch/tag
first and explicit user approval. Per-slice build verification on Windows uses
`build_tests.ps1` (unit tests need the make-based path) and `build_and_run.ps1 -Script`
smoke runs — the audit prompts' configure/make assumptions don't apply here.

**Git workflow (remotes, `master` hygiene, upstream PR branches).** The user does most
of the actual merging himself — Claude's job is to know this layout and never violate
it. It is load-bearing for the manufactured-clean-branch pass
(`docs/future-prompts/2-actual-fixing-of-commit-history.md`).

- **Remotes:** `origin` = the fork (`jonborchardt/scummvm`, the push target);
  `upstream` = `scummvm/scummvm` (fetch-only — its push URL is deliberately DISABLED).
  Check branch tracking with `git branch -vv`; expected layout: `master` tracks
  `upstream/master`, every working branch tracks `origin/<branch-name>`.
- **`master` is a pristine mirror of `upstream/master`** and must always match it.
  Refresh it with:

  ```powershell
  git checkout master
  git fetch upstream
  git reset --hard upstream/master
  git push origin master
  ```

  (The `reset --hard` is safe *only because* `master` never carries local work — that
  is the invariant being protected.) Never develop on `master`, never merge feature
  branches into it, never open upstream PRs from it. Refresh it periodically, then
  `git checkout my-working-branch` to return to work.
- **Day-to-day work happens on development branches** (the `jon-*` lineage etc.);
  commit and push normally (`git add -p`, `git commit`, `git push`). Dev branches may
  be long-lived, experimental, or have messy history — they are raw material, not PR
  candidates. Do not merge them into `master`, and avoid merging `master` into a dev
  branch unless integration testing requires it — for upstream submission, always
  create a fresh branch from the current upstream `master` instead.
- **Upstream PR branches are manufactured fresh from `upstream/master`,** containing
  only the changes for one reviewable PR:

  ```powershell
  git checkout master
  git fetch upstream
  git reset --hard upstream/master
  git checkout -b sci-short-description
  ```

  Three ways to bring work over from a dev branch:
  - a clean existing commit: `git cherry-pick <commit-hash>`
  - a commit whose changes need reorganizing:
    `git cherry-pick --no-commit <commit-hash>`, then `git reset`, `git add -p`,
    `git commit`
  - selected final files: `git checkout my-working-branch -- path\to\file.cpp
    path\to\file.h`, then `git add -p`, `git commit`

  Each commit must contain one logical change, compile on its own, avoid mixing
  formatting with functional changes, use a ScummVM-style message (rules below), avoid
  merge commits, and include documentation when required. Example message:

  ```text
  SCI: Add overlay display provider

  Route SCI room rendering through a display provider so alternate
  display implementations can be added without changing game logic.
  ```

  Review the branch before pushing — `git log --oneline upstream/master..HEAD` and
  `git diff upstream/master...HEAD` — then `git push -u origin sci-short-description`
  and open the PR from `jonborchardt:sci-short-description` into
  `scummvm/scummvm:master`. Never open an upstream PR from a large development branch
  unless its entire commit history already satisfies ScummVM's commit and review
  requirements.

**Upstream pitch (when the time comes):** discuss on scummvm-devel/Discord *before*
writing PRs — ScummVM has a strong talk-first culture. The story to tell is the strong
one Roger actually has: it runs original commercial games from their original data,
display-only enhancement, opt-in, byte-identical when disabled — not a new private game
(any scope-risk analysis written for that scenario should be rewritten in these terms).
Shape: one coherent engine-seam PR with a few clean commits, plus small separate PRs for
generic pieces (the input driver). If upstream declines, the fallback is this fork with a
deliberately minimized diff — never a duplicated engine.

**Upstream contribution rules (apply to all new Roger code NOW, so the PR pass is cheap):**
Canonical sources are `CONTRIBUTING.md` → the wiki's Developer Central pages
([coding style](https://wiki.scummvm.org/index.php/Code_Formatting_Conventions),
[portability](https://wiki.scummvm.org/index.php/Coding_Conventions),
[commit messages](https://wiki.scummvm.org/index.php/Commit_Guidelines)) + GPLv3+;
questions go to scummvm-devel@lists.scummvm.org. The wiki is bot-gated (Anubis; fetch
tools fail) but the rules below were verified against the wiki text on 2026-07-02:

- **Commit messages:** first line `SUBSYSTEM: Short summary`, **≤ 50 chars**, present
  tense ("Fix bug" not "Fixed bug"); then a blank line; body wrapped at ~72 chars.
  Subsystem = engine name in caps (`SCI:` for all Roger seam work; two-level prefixes like
  `SCI: ROGER:` match upstream practice, cf. `SCUMM: RA2:`), or backend name (`SDL:`),
  `OSYSTEM`, `GUI`, `I18N`, `DEVTOOLS`, `MIDI`, `BUILD`, `DOCS`, `DOXYGEN`, `ALL`
  (multi-subsystem), `JANITORIAL` (non-functional cleanup only). Messages must make sense
  *without* the diff ("Move Pajama3 to supported games", never "not needed").
- **Commit discipline:** every commit must compile (bisectability); no unrelated changes
  in one commit; **never mix style/whitespace changes with functional changes**; **no
  merge commits** — upstream enforces linear history (consistent with our manufactured
  short-lived PR branches; the merge-maintained `jon-*` deploy line can never be submitted
  as-is). User-facing changes need accompanying docs (`DOCS`); contributions to common
  code outside engines/backends require Doxygen comments (JavaDoc style, `@param` etc.) —
  this applies to the `.rin` input-driver PR since it touches `gui/`/event code. The
  Claude-attribution footer used on this fork's commits must be stripped from anything
  submitted upstream.
- **Portability** (compiler-enforced repo-wide by `common/forbidden.h` symbol poisoning —
  never add a `FORBIDDEN_SYMBOL_EXCEPTION_*`): no `printf`/`fopen`/`FILE`/`exit`/`system`/
  `getenv`/`time.h`/`rand`/`strcpy`/`sprintf`/`setjmp` etc. Use the ScummVM equivalents:
  `Common::File`/`SaveFileManager`, `Common::String`, `debug()`/`warning()`/`error()`,
  `g_system->getMillis()`, `Common::RandomSource`. C++11 subset only: no exceptions, no
  global objects with constructors (POD/pointer globals like `g_sciGfxObserver` are
  fine); new code uses `Common::` classes directly (the `Std::` wrappers in `common/std/`
  are only for porting codebases that already use STL). Endian-safe data access
  (`READ_LE_UINT32` etc. from `common/endian.h`, or stream `readUint32LE` methods — never
  pointer-cast struct overlays); packed structs only via `common/pack-start.h` /
  `pack-end.h` + `PACKED_STRUCT`, never a raw `#pragma`.
- **Reentrancy (strict):** non-const `static` locals inside function bodies are
  **forbidden** — return-to-launcher / in-process restart keeps their stale state. Non-const
  globals are strongly discouraged and need a comment saying why + where they're re-set at
  engine start (else mark `// FIXME: non-const global var`). The two known Roger
  violations (warn-once flag in `roger_studio.cpp`, diag-dedup signature in
  `file_roger_art_provider.cpp`) were fixed by moving them to member state — keep it
  that way; grep `static (bool|int|uint32)` under `engines/sci/roger/` before any
  upstream slice.
- **Naming:** `camelCase` functions/methods/locals, `_camelCase` member variables,
  `g_camelCase` globals, `CamelCase` types; constants either `kCamelCase` or `ALL_CAPS`
  (prefer enum/`const` over `#define`). Layout rules are in Code Style below (tabs w4,
  attached braces, right-aligned `*`/`&`); notable extras: spaces around binary operators
  and after keywords/commas, mandatory `{}` on empty loop bodies, no composite one-liners
  (`if (x) doThing();`), switch fall-through marked with exactly `// fall through`, marker
  keywords `FIXME`/`TODO`/`WORKAROUND` (a WORKAROUND must explain what original-game bug
  it works around, with tracker refs where applicable).
- Before the manufactured-branch pass: audit `engines/sci/roger/` for naming-convention
  drift, the static-local violations above, and stray TODO/debug leftovers — fixing them
  in the fork beforehand is cheaper than during upstream review. A first conformance
  pass (Prompt 0, plan `docs/future-prompts/0-conform-code-to-scummvm-guidelines.md`)
  ran 2026-07-11 on branch `jon-first-pass-prompt0` (10 commits, 00dc60541d9..b69fffeb828): ~110
  composite one-liners expanded, mojibake eliminated, GPL headers added to two files,
  history-narration comments stripped; no code logic changed.

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
