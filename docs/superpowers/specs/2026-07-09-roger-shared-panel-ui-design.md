# Roger shared panel UI kit + tune panel / Studio restyle — design

**Date:** 2026-07-09
**Status:** approved (brainstorm 2026-07-09)
**Branch:** jon-new-loader

## Goal

The picker v2 work (2026-07-08/09) gave the game picker a custom-drawn look:
navy gradient background, a shared color palette, TTF fonts, alpha-blended
translucent panels, stroked borders, hover-brightening buttons, and a toggle
pill. The F12 tune panel and Roger Studio still use their older flat-gray /
yellow-highlight, bitmap-font styling. This work extracts the picker's visual
language into a shared UI kit and applies it to both tools.

Decisions made during brainstorming:

- **Scope: restyle only.** Each tool keeps its existing layout and widget
  geometry. The tune panel's click coordinates are locked by
  `test/sci/roger/test_tune_panel.h` and
  `test/sci/roger/scripts/tune-panel-smoke.rin`; they must remain valid.
- **Fonts: adopt TTF.** Tune panel and Studio text renders with the picker's
  TTF fonts (LiberationSans + GoMono), falling back to the FontMan bitmap GUI
  font when FreeType is unavailable. Studio panel text renders at full display
  resolution instead of the current 2x-upscaled half-res.
- **Structure: new `ui/` folder with a bound painter.** Shared code lives in
  `engines/sci/roger/ui/`; the painter is a small object bound to a target
  surface + font set (not stateless free functions, not an extension of
  `roger_widgets`).

## 1. Shared module — `engines/sci/roger/ui/`

New folder holding the dev-panel UI kit. Constraints: SCI-free, GUI-theme-free
(depends only on `graphics/` + `common/` + FontMan), so it is includable from
the quarantined utils (`utils/tunepanel/`, `utils/studio/`) and the launcher
without violating the quarantine contract or upstream-neutrality rules.

### `ui/roger_widgets.{h,cpp}` (moved, unchanged)

`engines/sci/roger/roger_widgets.{h,cpp}` moves here verbatim (PanelWidget,
`widId`/`widKind`/`widIndex`, `hitTestWidgets`). Include paths update in the
picker view/model, launcher dialog, tune panel, Studio, tests, `module.mk`,
and `build_tests.ps1`. Header guard updates to the new path convention.

### `ui/roger_panel_style.{h,cpp}` (new)

Three pieces:

**Palette** — today's `PickerColors` namespace (in `roger_picker_view.h`)
generalized and renamed (e.g. namespace `PanelStyle`), keeping the existing
constants:

| Constant | RGB | Role |
|---|---|---|
| `kText` | 232,236,244 | primary text |
| `kTextDim` | 154,164,184 | secondary text / hints |
| `kGreen` | 76,195,138 | positive status (Cached) |
| `kAmber` | 229,184,75 | warning status |
| `kBlue` | 100,148,237 | accent / selection / highlight |
| `kRed` | 214,86,78 | destructive (Remove/Cancel) |
| `kPanelLine` | 42,52,80 | panel/button borders |

plus named constants for fills the picker currently hardcodes inline:

| Constant | RGB | Today's inline site |
|---|---|---|
| `kGradTop` | 10,14,26 | `bakeBackground` gradient top |
| `kGradBottom` | 26,36,56 | `bakeBackground` gradient bottom |
| `kPanelFill` | 16,22,36 | list/settings panel fill (alpha 216) |
| `kCardFill` | 22,30,50 | row card fill (alpha 235) |
| `kFieldFill` | 10,14,24 | field / button-pill fill |

**`PanelFonts`** — owns the TTF role fonts, mirroring the picker's loader:
LiberationSans at title/sub/body/small sizes + GoMono, sized from a caller-
supplied reference height (the picker uses widget height; tune panel and
Studio pass their panel's dest-pixel height). `USE_FREETYPE2`-guarded; each
role falls back to `kGUIFont` (`kBigGUIFont` for title) when TTF loading
fails. Exposes `const Graphics::Font *get(Role)` and a `load(refHeight)` that
can be re-called when the reference height changes (reload only on change).

**`PanelPainter`** — bound at construction to a `Graphics::ManagedSurface &`
target and a `const PanelFonts &`. Methods lifted from
`roger_picker_view.cpp`'s private helpers, behavior-identical:

- `gradientFill(rect, top, bottom)` — vertical gradient (from `bakeBackground`)
- `blendFill(rect, rgb, alpha)` — alpha blend fill; non-32bpp overlay falls
  back to opaque fill (existing behavior)
- `strokeRect(rect, rgb)` — 1px border
- `drawTextIn(role, str, rect, rgb, align)` — vertically centered text
- `drawButton(rect, label, accentRgb, filled, enabled, hovered)` — the
  picker's `drawButtonRect` with hover passed as a bool (the painter never
  knows widget ids or hover state)
- `drawTogglePill(rect, on, hovered)` — the debug-toggle pill + knob

All drawing clips to the target surface (as the picker helpers do today).

## 2. Picker refactor (first consumer, pure refactor)

`roger_picker_view.{h,cpp}` and the custom-drawn pass-builder dialog in
`roger_launcher_dialog.cpp` drop their private copies of the palette, font
loading, and draw helpers, and draw through `PanelFonts`/`PanelPainter`.

**Intent: pixel-identical output.** This commit proves the extraction — no
behavior or visual change. `PickerColors` is deleted in favor of the shared
palette (a `using namespace` keeps call sites short).

## 3. Tune panel restyle (geometry frozen)

`tunePanelRect`, `buildTunePanel`, and every widget rect stay byte-identical —
the locked coordinates (log 31 / view 42 / pic 53, build row y=177) in
`test_tune_panel.h` and `tune-panel-smoke.rin` remain valid. Only
`drawTunePanel` (and its caller's caching, below) changes:

- Panel: translucent navy (`kPanelFill`, ~216 alpha) blended over the scene +
  `kPanelLine` stroke, replacing the flat opaque 22,22,30 box.
- Buttons/rows through `drawButton`: hover brighten, disabled dimming; the
  amber-yellow `hi` (255,220,120) highlight becomes the `kBlue` accent; text
  `kText`/`kTextDim`; title stays top-left in accent color.
- Text: TTF sized to the **scaled dest rects**. The panel already draws at
  overlay resolution via `sciRectToDest`, so this is purely crisper text with
  zero geometry change. `PanelFonts` reloads only when the dest scale (gameRect
  height) changes.
- **Performance guard (per CLAUDE.md performance discipline):** the panel is
  baked into a cached RGBA `ManagedSurface` re-rendered only on state or hover
  change, then `blendBlitFrom`'d into the scene each present. Per-present cost
  stays O(panel area) — the same order as today's direct fills. No new full
  presents; no change to when presents happen.

## 4. Studio restyle (rects unchanged, paint full-res)

`buildStudioPanel` and its flow-cursor geometry are untouched (its model tests
stay valid, hit-test mapping unchanged). `RogerStudio::drawPanel` changes:

- Stops rendering into the half-res `small` surface + 2x blit. Instead it
  scales each widget rect 2x and paints directly into `_display`, drawing TTF
  text at full display resolution inside the scaled rect — crisp instead of
  chunky.
- Palette swap: black panel background becomes navy panel fill + `kPanelLine`
  border; yellow `hi` becomes `kBlue` accent; hover fill uses the picker
  treatment; status/help bottom line uses `kAmber` (help prefixed
  form unchanged).
- The scene area, grid/diff/split rendering, and the crosshair cursor are
  untouched. Studio redraws only when `_dirty` (unchanged), so full-res panel
  paint has no steady-state cost.

## 5. Build, tests, verification, docs

- `engines/sci/module.mk`: path update for moved `roger_widgets.o`, add
  `ui/roger_panel_style.o`.
- `build_tests.ps1`: update `$RogerSources` for the moved `roger_widgets.cpp`;
  add `roger_panel_style.cpp` only if a test links it (the painter is
  graphics-dependent and not unit-tested; model tests don't need it). Expect
  **unchanged test count** — an unchanged count after a pure move is correct
  here; a build failure is the signal if wiring breaks.
- Verification order:
  1. `build_tests.ps1` green (same count).
  2. Picker: launch with picker enabled, confirm visually unchanged.
  3. `tune-panel-smoke.rin` green — the locked coords are the regression test
     for "geometry frozen".
  4. roger-loop capture of the open F12 panel for visual evidence.
  5. Studio: interactive user soak (`.rin` snap cannot capture Studio).
- Docs: `utils/tunepanel/README.md`, `utils/studio/README.md`,
  `docs/roger.md` styling mentions, and CLAUDE.md's key-files row for
  `roger_widgets`' new path + the new `ui/` module.
- No files under `engines/sci/roger/gen/` are touched → `kTransformVersion`
  unchanged (no cache invalidation statement needed beyond this).

### Commit sequence

1. `SCI: ROGER: Extract shared panel UI kit to ui/` — folder, move, style
   module, picker/pass-builder refactor (pixel-identical).
2. `SCI: ROGER: Restyle tune panel with shared UI kit` — includes the baked
   panel cache.
3. `SCI: ROGER: Restyle Studio panel with shared UI kit` — full-res paint.
4. `DOCS: Update Roger panel UI docs` — READMEs, roger.md, CLAUDE.md.

Each commit builds and passes tests independently (bisectability rule).

## Out of scope

- Any layout/geometry change in the tune panel or Studio (including exposing
  more studio params or moving the tune panel's locked rows).
- Eye Exam (`utils/eyetest/`) restyle — untouched.
- The in-game overlay UI (journal/compositor dialogs) — this kit is for the
  dev panels and launcher only.
- Re-deriving the stale `qfg1-menu-cycle presence:m-after` gate region
  (pre-existing, unrelated).
