# Roger Studio — handoff (pinned 2026-07-03)

Work paused by choice, not because anything is broken. Tree clean, everything committed
on `jon-refactor1`, tip of the studio work = `3a2e14f08dd`. 120/120 unit tests green,
including the golden bit-identity checksum (`0x40B38BFF`) proving the shipping omyac
render is byte-identical — the studio changed nothing about how games look.

## What the Studio is

An interactive tuning environment for the omyac upscaler, launched INSTEAD of a game:

```powershell
.\build_and_run.ps1 -Studio        # ROGER_STUDIO=1, per-launch; Esc quits
```

Debug-only; unreachable without the env var; never touches the generation disk cache
(its `RogerAssetGen` is `kGenMemory` with an empty cache dir).

**v1** (plan `docs/superpowers/plans/2026-07-02-roger-studio.md`) built the engine
plumbing: `OmyacParams` + param registry, scaler variants, `setOmyacParams` /
`nativeCelIndexImage` / `surfaceFromIndex` on `RogerAssetGen`, the `ROGER_STUDIO` hook.
Its keyboard/3-mode UI was rejected on first use and fully deleted.

**v2** (plan `docs/superpowers/plans/2026-07-02-roger-studio-v2-ui.md`, spec
`docs/superpowers/specs/2026-07-02-roger-studio-v2-ui-design.md`) is what exists now:
a single game-like scene (view cel composited on the enhanced plate; SQ3 defaults
pic 2, view 12 loop 1, anchored bottom-centre at 160,150), fully mouse-driven.

## Current feature set

- **Control panel** (bottom 560 overlay px, widgets built by pure `buildStudioPanel`):
  pic/view/loop/cel spinners, `@(x,y)` live cel-coords readout, scaler-variant cycle,
  plate mode (omyac | nearest-ref), view on/off, Fit, `pink:` (hot-pink unfilled/backfill
  pixels), `grid:` (plate-pixel borders, visible at zoom ≥ 3×), A/B edit tabs,
  Show A / Show B / Split / Diff, Copy A>B, Export PNG.
- **Pass chips** row: f/l/a chips with selection, `^` insertion caret (matches
  `passInsertAfter` semantics), move `<` `>`, delete `x`, `+f +l +a`, **Clear** (empty →
  wireframe) and **Reset** (defaults).
- **Hover help**: every param `[-]/[+]/[on/off]` and chip button shows a one-sentence
  explanation in the bottom line (`OmyacParamDesc.help`).
- **Scene mouse**: click-to-place / drag the cel (bottom-centre at cursor), right-drag
  pan, cursor-anchored wheel zoom, composited crosshair cursor (no HW cursor works here).
- **Two live A/B slots** (params + passes + variant + plate mode each), per-slot plate
  cache so drags recomposite instead of re-running omyac.
- **Diff view**: white-on-black per-pixel diff + `best align: dx dy` SAD readout
  (also logged as `ROGER-STUDIO diff offset dx=? dy=?`). NOTE: the map is mostly bright
  when comparing different upscalers (fill differences) — the READOUT is the geometry
  answer, not the map's density.
- **Exports** land in `screenshots/` as `studio-scene<pic>-{A,B}-<stamp>.png`,
  `...-AB-<a>-vs-<b>.png`, `...-diff-<a>-vs-<b>.png`. Keys: Esc quit, E export
  (automation-only; everything else is mouse).
- **Shift-lock tests** (`test/sci/roger/test_shift_lock.h`): centroid locks on scale6x +
  default omyac pipeline. Tolerance 1.0 hybrid px — DO NOT widen; a failure IS the
  diagnosis of a real drift.

## The original shift question — ANSWERED

No systematic drift. Centroid locks pass (omyac mass sits where the nearest-neighbour
reference puts it), and the in-engine SAD on the real SQ3 pic-2 plate measured
**dx = −1, dy = −1 overlay px = 1/6 native px** (omyac vs zero-shift nearest ref via
`RogerAssetGen::generatePlateNearest`, which is exact by construction).

## Hard-won facts (do not re-learn)

- **Mouse coordinate space = `showOverlay(inGUI)`** (`backends/graphics/windowed.h:71-90`):
  `true` → events in OVERLAY px (studio does this, identity conversion);
  `false` → GAME 320×200 (the in-game provider path). One fix shipped inverted before
  this was pinned down. Never scale `ev.mouse` by `overlayW/320` under `inGUI=true`.
- **`.rin` scripted mouse coords are GAME space** mapped over the window — they no longer
  match the studio's overlay-space input. Committed studio scripts use keys only
  (`test/sci/roger/scripts/studio-smoke.rin`); studio `capture` also produces NO PNGs
  (bypasses the provider autoshot path) — automation evidence = `key e` export + log grep.
- **Invalidation tiers** (keep exact): `invalidateScene()` all-stale incl. plates;
  `invalidateActive()` active slot plate+compose; `invalidateCelOnly()` recomposite only
  (cel place/drag, loop/cel/view/showView, pink toggle); bare `markDirty()` display-only
  (tabs, display mode, grid). All three set `_diffStale`.
- **Backfill mask**: `OmyacResult.backfilled` (recorded in `fillNullPixels`) is what the
  pink toggle renders. ~2.1 MB transient per generation, also in game mode (freed with
  the result); flag-gate it if that ever matters. Golden checksum pins output unchanged.
- `_status`/help/readout all share the bottom panel line; status clears at each
  `renderSlot`.

## Loose ends on resume (in order)

1. **Review commit `3a2e14f08dd`** (pink toggle + pixel grid) — it landed verified
   (tests/golden/build/smoke) but skipped its code review when work was stopped.
2. **Interactive drive-through never happened.** Verify by hand: crosshair cursor tracks;
   panel hover/clicks land; click-to-place puts the cel exactly under the cursor
   (the coordinate fix was never human-confirmed); chips/caret/Clear; hover help;
   `@(x,y)` updates while dragging; pink toggle (Clear passes first for the vivid case);
   grid at zoom ≥ 3; Split/Diff + readout; export from each mode.
3. Known cosmetic debt (all triaged/waived, listed in the final review):
   `static bool warned` in `drawPanel` (upstream reentrancy rule — already on CLAUDE.md's
   pre-upstream fix list), Split-mode wheel zoom anchors to the left pane, variant cycle
   regenerates the plate unnecessarily (could be cel-tier).

Full task-by-task history + review verdicts: `.superpowers/sdd/progress.md`
(section "Roger Studio v2 UI"). Commits `07cb0d47594..3a2e14f08dd` (16).
