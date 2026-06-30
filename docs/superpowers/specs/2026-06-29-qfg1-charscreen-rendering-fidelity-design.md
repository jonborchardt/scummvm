# QFG1 Character-Screen Rendering Fidelity — Hardening Design

> **Status:** approved 2026-06-29. Successor to the generic-text-capture work (commits
> `039ec87a2f3`..`615b2614100` on `jon-qfg1char`). That work made SCI0 EGA display text
> (QFG1 stat labels/values) render as crisp persistent TTF in the Roger overlay. This spec
> hardens the result to production fidelity and fixes regressions surfaced in-game.

## Goal

Make Roger's hires overlay reproduce the QFG1 character-creation screen (and text-heavy
SCI0 screens generally) faithfully: every element the native render shows is present, at the
right size, in the right place, with no duplicates and no ghosts — verified against the native
render by an in-engine image diff. All mechanisms stay **game-agnostic** (keyed to generic SCI
primitives, never per-screen knowledge) and **EGA SCI0 only**.

## Background — confirmed in-game (2026-06-29)

After the generic-text fix (`onNativeText` capturing regardless of `show`, persistent emit,
text-rendering-type dedup), the char screen renders crisp stats/labels/values with live point
updates (user screenshot). Remaining defects, observed in the same screenshot + `ROGER-UI`
diag dump + user report:

1. **Missing non-text graphics (regression).** The portrait, point bars, and frame graphics
   that the pixel path (`_textSprites`, `fgCapture`) showed *before* this session's text work
   are now gone. Some elements were *always* missing (pre-existing capture gaps).
2. **Transient-text ghosting (confirmed).** In gameplay, non-blocking narration text that SCI
   later erases lingers in the overlay until room change, because generic text is now persisted
   and has no erasure signal.
3. **Inconsistent font sizing.** Same-role text renders at wildly different sizes (e.g. "Luck"
   huge, "Intelligence" small). `onNativeText` does not capture the native font metrics
   (`nativeFontH`/`nativeTextW`/`textRole`) that `uiPushText` captures, so `RogerTextRenderer`
   free-fits each string to its box → short strings balloon, long strings shrink.
4. **Double-render.** "Name: SS", "Start Game", and "Cancel" render twice — once from the
   generic `Box` capture, once from controls16's own `kUiButton`/`kUiTextEdit`. The dedup uses
   strict rect *containment*, which misses an offset control label.
5. **Highlight/selection box misalignment.** The red frame around the TAB hint / selected stat
   is misaligned/oversized.

## Constraints (carried from the Roger project)

- **Game-agnostic is #1** — every mechanism keyed to a generic SCI primitive; NO per-game or
  per-screen knowledge anywhere.
- **EGA SCI0 only** — permanent scope (SQ3, QFG1 EGA). No VGA/SCI1 paths.
- **Performance discipline (CLAUDE.md "read before touching the per-cycle path").** Any new
  per-cycle hook (erasure on `bitsRestore`/redraw, metric capture in `Box`) must be O(1)/cheap
  and must NOT trigger a full present or invalidate the composite cache unless the scene
  genuinely changed. The image diff is a gated diagnostic, never on the steady-state path.
- **All-or-native fallback** — if any capture/render path fails, the native render still shows;
  never a skipped/garbage frame. The pixel path is the safety net.
- **SQ3 non-regression** — message windows keep their fill; dialogs/banner/inventory/gameplay
  unchanged. Every task verifies SQ3.
- **C++11, tabs (width 4), no exceptions, no RTTI, GPLv3+**; `.clang-format` enforced;
  pointer/reference right-aligned; K&R braces.

## Work breakdown (priority order — may stop after any tier)

### P0 — Regressions

#### A. Restore missing non-text graphics

The non-text graphics (portrait, bars, frame) are captured by the pixel path
(`_foregroundRegions` → `_textSprites`) and merged into the cast each frame. They regressed
after the text work. **Root-cause via systematic-debugging before fixing** — do not guess.

Prime suspects (to confirm with `roger_diag`/`roger_debug_capture`, F10 A/B, and the manifest):
- **Over-exclusion:** `collectUiTextRects` (Task-4 exclusion) appends *every* `kUiText` rect —
  now including the many persisted generic-text rects — to the pixel-capture exclusion set. If
  a generic-text rect spuriously overlaps a graphic region (`filterForegroundCaptureRegions`
  uses `intersects`, not `contains`), that graphic is dropped from the pixel path. This is the
  leading hypothesis: a wide multi-line generic rect (e.g. the TAB-hint block, `165,127..315,151`)
  can intersect a graphic.
- **Occlusion/draw-order:** persisted generic-text elements drawn over the sprite region.
- **Capture-gap:** elements never captured by either feeder ("always missing" subset).

**Fix direction (pending root cause):** tighten the exclusion so it removes a pixel region only
when a captured-text rect *substantially covers* it (containment or high-overlap fraction), not
on any intersection — so graphics adjacent to or overlapped at the edge by a text rect survive.
The "always missing" subset is logged (via the diff tool, task F) and addressed only if cheap;
otherwise recorded as a known gap.

#### B. Transient-text erasure (anti-ghosting)

Generic text is persisted (correct for static labels) but never dropped when SCI erases it,
so transient gameplay narration ghosts. Add a **generic erasure signal** keyed to SCI's own
erase/redraw primitives — game-agnostic:

- When SCI restores saved-under bits (`GfxPaint16::bitsRestore`) or redraws/updates a box
  (`kGraphRedrawBox` / `kGraphUpdateBox`) over screen rect `R`, remove every persisted
  generic-text element (`token == GENERIC_TEXT_TOKEN`) whose `nativeRect` is contained in `R`.
- Reuse the existing `bitsRestore` → `uiClearToken` seam (already called ~per sprite per cycle).
  The new removal must be **cheap and gated on real change** (only when an element actually
  lies in `R`), and must present only when something was removed — never an unconditional
  full present (cf. the 2026-06-28 `bitsRestore` perf regression in CLAUDE.md).

Net effect: static text (the char screen — SCI never erases it until room change) persists;
transient text disappears the moment SCI erases its background, matching native timing.

### P1 — Fidelity

#### C. Consistent font sizing

Capture the native font metrics at the `Box` hook so generic text sizes uniformly, exactly as
`uiPushText` already does for dialog text:

- At the `GfxText16::Box` call site (or in `onNativeText`, using values handed from `Box`),
  measure the drawn string with its font via the existing `GfxText16` width/height path
  (`Width`/`StringWidth`) to obtain the native cell height (`nativeFontH`) and single-line
  width (`nativeTextW`); set `textRole = kRoleBody`.
- Extend the `onNativeText` signature to carry `nativeFontH`/`nativeTextW` (or compute them in
  `Box` where the font is current) and populate the `UiElement`, so `RogerTextRenderer` drives
  size from the captured cell height + width cap (its existing metric path) instead of
  free-fitting the box. Result: all stat labels/values at one consistent body size.
- Multi-line captures (one `Box` call, embedded `\n`) keep `nativeTextW = 0` (no width cap), as
  the existing renderer convention specifies.

#### D. Eliminate double-render

Relax the generic-text dedup so an offset control label still dedups against its control:

- Change `dedupeGenericTextElements` from strict `contains` to **substantial-overlap OR
  matching-text-within-proximity** against text-rendering elements (`kUiText`/`kUiButton`/
  `kUiTextEdit`). A generic capture of "Start Game" overlapping/adjacent to the `kUiButton`
  "Start Game" is dropped; a genuine standalone label is kept.
- Keep the kUiWindow/kUiIcon exclusion (a frame/image never drops the text it encloses).
- This stays a **pure, unit-tested helper** (overlap fraction + optional text equality are
  computable from `UiElement` fields). Tests cover: offset-but-overlapping button label drops;
  distinct label far from any control keeps; window/icon never drops.

### P2 — Polish + tooling

#### E. Highlight / selection box

Investigate the red frame (likely `kGraphFrameBox` / a control highlight) and how Roger
captures+maps it; fix the capture or the native→overlay rect mapping so it aligns. Game-agnostic
(keyed to the frame-box primitive, not the screen).

#### F. In-engine image-diff diagnostic

A gated dev harness (extends `roger_autoshot`/`roger_debug_capture`; off by default behind a new
`roger_diff_check` knob) that, on demand for the current screen:
- captures the native 320×200 visual buffer and the composited overlay,
- downscales the overlay to 320×200 (nearest), diffs against native,
- emits to the log (and optional PNG) the coalesced regions that are **present-in-native but
  missing-in-overlay** (drives task A and the always-missing audit) and regions that **differ in
  position** (drives E + general alignment).
This is the verification engine for tasks A and E; it never runs on the steady-state path.

## Verification

- **In-engine image diff (task F)** is the primary objective check for A/E/alignment.
- **User in-game** (synthetic input unreliable in QFG1): char screen shows all graphics +
  uniformly-sized crisp text, no duplicates; gameplay shows no ghosting; SQ3 unchanged.
- **CxxTest** for the pure helpers (D's overlap dedup; any new pure geometry).
- **Build:** `.\build_and_run.ps1 -Game qfg1` (MSVC; `make`/`make test` unavailable on Windows —
  pure tests compile-verified, integration verified in-game).
- **Diagnostics off** at the end (`roger_diag`, `roger_diff_backstop`, `roger_debug_capture`,
  `roger_diff_check`) in the qfg1 `scummvm.ini` domain.

## Out of scope

- VGA/SCI1 paths; non-EGA games.
- Authoring new hires VIEW art (this is overlay fidelity for existing native art).
- Omyac/6× upscaling of the pixel-path graphics (they stay nearest-neighbour; quality-sensitive
  text is already semantic/crisp).
- Inter-room animated sequences.
