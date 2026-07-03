# Roger Studio — omyac tuning + view-scaler comparison environment

**Date:** 2026-07-02
**Status:** approved design, ready for implementation planning
**Owner:** jonb

## Purpose

A debug-only, interactive environment inside ScummVM for (1) tuning the omyac
pic-enhancement pipeline visually — including internal parameters that are
hard-coded constants today, and reordering the enhance-pass sequence, which the
current in-game hotkeys cannot do — and (2) judging whether scale6x is good
enough for VIEW cels by comparing it against alternative scaler chains, both in
isolation and composited on an enhanced plate.

Uses the real engine code paths (same `renderOmyac`, same `RogerAssetGen`
inputs, same resources via the live ResourceManager) without being in a game.
This is personal debug tooling — likely never shipped — so the design optimizes
for build speed and use speed, not polish. The non-negotiable constraint is
**zero behavioral change to shipping code paths**.

## Launch & hosting

- `build_and_run.ps1 -Studio` sets `ROGER_STUDIO=1` (env-first, per-launch,
  never touches scummvm.ini — same pattern as `-Mode` / `-Diag`).
- Hook point: the same startup seam as the Roger launcher dialog — SCI engine
  constructed, ResourceManager and GfxView alive, no game scripts running.
  When the env var is set, the provider enters the studio's blocking loop
  instead of the launcher/game flow.
- The studio owns the OSystem overlay directly (raw overlay mode — no
  GUI::Dialog, no theme widgets). It polls the EventManager, draws its own
  frame, `updateScreen()`, `delayMillis(10)`.
- **Esc quits the ScummVM process.** Studio is a dedicated launch mode; it does
  not continue into the game.
- Game selection = whatever target was launched (`-Game qfg1 -Studio` works).

## Code isolation (the "don't infect good code" contract)

| Change | Isolation guarantee |
|---|---|
| `engines/sci/roger/roger_studio.{h,cpp}` (new) | All studio logic. One env-gated call site in the provider startup path. Compiled always (avoids bitrot), reachable only via `ROGER_STUDIO`. |
| `OmyacParams` struct in `roger_omyac.h`, threaded through `renderOmyac()` → `enhance()` etc. | Every field defaults to today's hard-coded constant. Default-constructed params ⇒ **bit-identical output**. A unit test locks this in (render a fixture with defaults, compare against pre-change golden hash). Shipping callers pass defaults. |
| New scaler variants in `roger_scale.{h,cpp}` | Pure functions; nothing in shipping paths calls them. |
| Disk cache | Studio generates **in memory only, never reads or writes the cache** — params are not part of the cache key, so writing would poison it. Adopting a winning param set = editing the defaults + bumping `kTransformVersion`, the normal invalidation path. |
| `build_and_run.ps1` | New `-Studio` switch (env var only). |

## OmyacParams (initial field list)

Enumerated from `roger_omyac.cpp`; each field defaults to the current constant.
The studio registers each in a table (name, min, max, step) driving the HUD.

| Param | Today | Where |
|---|---|---|
| `minVotesLine` | 1 | `enhance()` vote floor, mode==line |
| `minVotesFillAll` | 2 | `enhance()` vote floor, fill/all modes |
| `fillSuppressLineNeighbours` | 3 | `suppressFill = lineNeighbours >= N` (9 = off) |
| `endpointMaxSame` | 2 | `detectLineEndings`: `isEndpoint = same < N` |
| `isolatedPixelPass` | on | toggle the isolated-pixel dilation in `enhance()` |
| `tieBreakBlend` | on | tie-break by BLEND_TABLE-nearest vs. first-tied |
| `diagFlankSuppress` | on | fill-anchor diagonal-flanking suppression rule |

The pass **sequence** (ordered list of fill/line/all) stays a separate argument
as today, but gains a visual editor (below). More fields can be added during
implementation as stages are re-read; the pattern is fixed by the first seven.

## Studio modes (Tab cycles)

### 1. Pic mode — omyac tuning
- Browse the game's pic resources with ←/→ (discovered via ResourceManager).
- Plate rendered 1:1 at 1920×1140 with pan (drag / WASD) and zoom (wheel /
  +/-). Blocking re-render on any change, with a busy indicator (plate gen is
  sub-second; no threading).
- **Param panel** (HUD): up/down selects a param, left/right adjusts by step.
- **Pass-sequence editor**: the ordered sequence rendered as a strip
  (`fill fill fill line fill fill all all all all`), cursor to select a
  position, keys to insert `f`/`l`/`a`, Delete to remove, Shift+←/→ to reorder.
  Every edit re-renders.

### 2. View mode — scaler comparison
- Browse view/loop/cel (keys for each level).
- The same cel rendered through every registered scaler variant, laid out in a
  labeled grid (variant name + output size), zoomable.
- Initial variant registry: `scale6x` (scale3x∘scale2x — today's path),
  `scale2x∘scale3x` (order swapped), `scaleNearest×6`, `scale2x + nearest×3`,
  `scale3x + nearest×2`. Non-6x-total factors are allowed in this mode and
  display at native output size. Registry is a table in studio code — adding a
  variant is one line plus (if new) a pure function in `roger_scale`.
- Cels are read via the same de-undither path as `generateViewCel`
  (`egaDeUndither`), so what you judge is what the engine ships.

### 3. Combined mode — view in context
- Current pic (current params) as the backdrop; the chosen view cel composited
  on top at true 6× game scale, movable with arrows/mouse.
- Scaler variant switchable in place — judge the sprite against the plate it
  will actually sit on. Only 6x-total variants are offered here (scale must
  match the plate).

## Judging tools (all modes)

- **A/B flip** (one key): toggles current ↔ previous render full-screen. The
  studio always keeps the previous render.
- **Pin baseline** (P): snapshots the current render + its param/variant label.
- **Split view** (S): current on the left, pinned baseline on the right, shared
  pan/zoom.
- **Export** (E): writes the current render to `screenshotpath` as PNG, param
  set / variant stamped in the filename, e.g.
  `studio-pic002-mv1-2-fs3-passes_fffl-ffaaaa.png`,
  `studio-view000-l0c0-scale2x3x.png`. Uses `Image::writePNG`.

## HUD

Minimal text overlay along one edge: mode, current pic/view ids, param values
with the selected one highlighted, pass sequence with cursor, last render time
in ms, baseline label if pinned, one-line keymap hint (full keymap on `?`).
Drawn with existing Roger text machinery.

## Error handling

- A pic/cel that fails to parse or render shows an inline HUD error and skips
  to the next resource; never crashes the loop.
- Any renderer returning null leaves the previous frame + an error line.
- Params are clamped to their registered min/max; no combination may crash
  `renderOmyac` (out-of-range values are prevented at the input layer).

## Testing

- **Unit (CxxTest, `test/sci/roger/`):** default-`OmyacParams` golden test
  (bit-identical to pre-change output on a fixture); new scaler variants get
  the same style of small-fixture tests the existing scalers have.
- **Manual:** studio is itself the test rig; a smoke pass is: launch
  `-Studio`, browse pics, edit a param, reorder passes, flip A/B, pin + split,
  export, Tab through all three modes, Esc.
- **Regression guard:** a normal (non-studio) launch after the change behaves
  identically — same plates from the same cache keys (no `kTransformVersion`
  bump in this work).

## Out of scope

- Running omyac on view cels (explicitly deferred).
- Threaded/async rendering.
- Shipping any studio UI; persisting studio state between launches.
- Cache-key changes / `kTransformVersion` bump (happens later, only when a
  winning param set is adopted into the defaults).
