# Roger Studio v2 — Single-Scene, Mouse-Driven UI (Design)

**Date:** 2026-07-02
**Supersedes:** the *UI layer* of `2026-07-02-roger-studio-design.md` (three keyboard-driven modes, pin/flip/split judging). The engine-side plumbing from v1 — `OmyacParams`, the param registry / scaler variants / stamp helpers, `RogerAssetGen::setOmyacParams` / `nativeCelIndexImage` / `surfaceFromIndex`, the `ROGER_STUDIO=1` hook, `-Studio` flag — is unchanged and reused as-is.

## Motivation (user feedback on v1)

1. Defaults land on an arbitrary first resource; SQ3 tuning always starts by hunting for pic 2 / view 12.
2. The keyboard-driven UI is unusable: no discoverability, no idea how to move around.
3. Viewing a cel in isolation (variant grid) is not how art is judged — the view should sit **on the pic, like the real game**.
4. Pass editing (cursor + f/l/Shift+A/Del/Shift+9/0) is far too clunky.
5. "Pin" froze pixels, not settings — you could not tune both sides of a comparison. Wanted: **two live sets of settings**.

## Core model

### One scene, two setting slots

There are no modes. The studio shows a single game-like scene: the enhanced plate (current pic) with the selected view cel composited on top (optional). Two **setting slots, A and B**, each hold a full independent configuration:

```
Slot = { OmyacParams params; Common::Array<int> passes; int variant /* factor-6 only */;
         Graphics::Surface *render /* cached */; bool stale; }
```

The **scene state is shared** between slots — pic id, view id, loop, cel, cel position, show-view toggle, zoom/pan — so a comparison is always the same scene under two different settings. Scene state that feeds the render (pic/view/loop/cel, cel position, show-view) marks *both* slots stale when changed; zoom/pan is display-side only and never invalidates a render. Changing a setting marks only the *edited* slot stale. A stale slot re-renders when (and only when) it needs to be displayed; the other slot's cached surface is reused untouched.

### Judging

- `[A] [B]` **edit tab**: selects which slot the settings panel edits. The param rows, pass chips, and variant button always reflect the active edit slot.
- **Display buttons:** `Show A` | `Show B` | `Split A|B` (A left half, B right half, same zoom/pan, slot letter + stamp label drawn on each side). Toggling Show A / Show B in place is the blink-comparison ("flip").
- **`Copy A→B`** seeds B's settings from A (branch off a good baseline). This replaces v1's Pin; `_baseline`/`_previous`/`[PREV]` and the pin concept are removed.
- **Export PNG** writes what is displayed. Single view: `studio-scene<pic#>-<slot>-<paramStamp>-<passStamp>.png`. Split: one side-by-side image, `studio-scene<pic#>-AB-<stampA>-vs-<stampB>.png`. Same sanitization rules as v1 (keep alnum + `-`, else `_`); files go to `screenshotpath` as before.

### Defaults

Per-game defaults table with fallback:

| Game | Pic | View / loop / cel | Cel position (native) |
|------|-----|-------------------|-----------------------|
| `sq3` | 2 | 12 / 1 / 0 | (160, 150) |
| anything else | first pic id | first view id / 0 / 0 | (160, 150) |

If the preferred id is absent from the resource list, fall back to the first entry. Both slots start with default params + `defaultPasses()` + `kScaler6x`.

### Placement, zoom, pan (all mouse)

- **Click on the plate** places the cel with its **bottom-center at the click point** (feet where you click, game-like). **Dragging** continues to move it. Clicking/dragging that starts on the cel's current bounds moves it without jumping.
- Coordinates map overlay→native (inverse of the fit transform, ÷6), clamped to x 0..319 / y 0..189.
- **Wheel** zooms in steps centered on the cursor; **right-drag** pans while zoomed; a **`Fit`** button resets to auto-fit. The plate auto-fits the image area on startup and after `Fit`.
- **Show view** toggle hides/shows the cel (judge the plate alone).

### Keyboard: dropped

Buttons are the UI. Only two keys survive, undocumented in the UI, purely for the autonomous `.rin` smoke harness: **Esc** (quit) and **E** (export current display). Everything else — mode keys, param arrows, pass editing keys, F1 keymap — is removed along with the keymap overlay.

## Control panel

A bottom panel (full overlay width; height set in the plan, ~500 px at 2862×1986 — panel text keeps the v1 render-small-then-2x-blit approach). Sections, left to right / top to bottom:

1. **Scene row:** `pic ◀ 002 ▶` · `view ◀ 012 ▶` · `loop ◀ 1 ▶` · `cel ◀ 0 ▶` · `variant: [scale6x]` (click cycles the five factor-6 variants; 4x/8x helpers remain in code/tests but get no button) · `show view [on]` · `Fit`
2. **Edit tab + judge row:** `edit: [A] [B]` · `Show A` `Show B` `Split` · `Copy A→B` · `Export PNG` · render-ms readout · one-line status.
3. **Params (active slot):** one row per registry entry: `minVotesLine   [−] 1 [+]`; bool params render as a single `[on]/[off]` toggle. Every change re-renders the active slot.
4. **Pass chips (active slot):** `[f ×] [f ×] [l ×] …` — click a chip to select it, click its `×` to delete it, `◀ ▶` move the selected chip, `+f` `+l` `+a` append, `Reset` restores `defaultPasses()`. Empty strip shows `(none — wireframe)`.

Numeric ids in the browse spinners wrap around, exactly like v1's PgUp/PgDn, with the same empty-list guards.

## Architecture

| Unit | Kind | Responsibility |
|------|------|----------------|
| `roger_studio_render.h/.cpp` (extend) | SCI-free, unit-tested | **Panel layout + hit-testing**: given panel rect + a plain state snapshot (param count/values, pass list, active slot, toggles), produce a flat list of `{Common::Rect rect; uint32 id; label; state}` widgets; `hitTest(widgets, x, y) → id`. Widget ids encode kind + index (e.g. `kWidParamMinus | paramIdx`, `kWidChip | chipIdx`) so one dispatch switch handles parameterized rows. Also: the v2 export-name builders and the per-game defaults table (`studioDefaultsForGame(gameId)`). |
| `roger_studio.h/.cpp` (rework) | engine | Owns the two slots + shared scene state; event loop (mouse-first); calls the layout helper each frame, draws widgets from its output (hover highlight from mouse position), dispatches clicks by widget id; `renderSlot()` composes plate + cel via the existing Task-3 plumbing (`generatePlate`, `nativeCelIndexImage`, `applyScalerVariant`, `surfaceFromIndex`, `blendBlitFrom`); display composition (single / split); click-to-place / drag / zoom / pan; export. |
| everything else | — | unchanged (asset gen, omyac params, sci.cpp hook, build scripts). |

The layout helper is deliberately pure so widget geometry, id encoding, hit-testing, wrap-around index math, and the defaults table are all CxxTest-covered without an engine. Drawing and event polling stay thin in `roger_studio.cpp`.

### What is deleted from v1's studio

The three-mode enum + dispatchers, `renderViewMode()`'s variant grid, the shared/mode-gated keyboard blocks, the F1 keymap overlay, `_baseline`/`_previous`/pin/flip state and their buttons/labels. (`test_studio_render.h` keeps its existing helper tests; tests tied to removed UI concepts do not exist — v1 had none for the studio loop.)

## Constraints carried forward (binding, from v1)

- Studio never touches the disk generation cache (`kGenMemory`, empty cache dir; `setOmyacParams` force-switch stays).
- Zero behavioral change to shipping paths; no `kTransformVersion` bump; `sci.cpp` hook stays inert without `ROGER_STUDIO`.
- C++11, tabs (width 4), K&R, right-aligned ptr/ref, no exceptions/RTTI, GPL headers.
- Screenshots/exports only under `screenshotpath` (gitignored `screenshots/`).
- Per-cycle perf discipline is not implicated (the studio never runs inside `kernelAnimate`), but the dirty-flag redraw loop stays: repaint only on state change / hover change.

## Testing

- **Unit (CxxTest):** layout produces non-overlapping widgets inside the panel rect; id encoding round-trips; hit-test hits the right widget and misses gaps; spinner wrap math; defaults table (sq3 → 2/12/1/0, unknown game → fallback); export-name builders incl. split naming; pass-chip edit operations expressed as pure list ops if factored that way.
- **Autonomous smoke (`.rin`):** update `studio-smoke.rin` — wait for boot, `key e` (exports the default SQ3 scene: pic 2 plate + view 12 cel composited), wait, `key esc`, quit. Run on SQ3 (`-Game sq3` implicit default) since defaults are SQ3-specific; assert exit-on-own + PNG exists.
- **Manual drive-through (user):** click-place/drag the cel; tune params on A, `Copy A→B`, diverge B, blink Show A/Show B, Split, export both ways.

## Out of scope

- True in-game (x, y) capture for cel placement (would require recording positions from a live scripted session; possible later feature).
- Priority-band occlusion of the cel against the plate (the cel draws on top; occlusion judging stays in-game).
- Multiple cels on the scene at once; palette-cycling preview; non-factor-6 variants in the UI.
