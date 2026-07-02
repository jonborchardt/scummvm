# Roger: present barrier + exact invalidation (design)

**Date:** 2026-07-02
**Status:** approved design, pending implementation
**Execution model:** four phases; each phase gets its own implementation plan
(superpowers:writing-plans) and must pass the shared roger-loop regression gate
(§8) before the next phase's plan is written. Any phase that cannot hold the
perf gate (§7) is stopped and reverted; earlier shipped phases stay.

---

## 1. Problem

The overlay is retained; SCI's native renderer is immediate-mode (it "erases"
by redrawing the scene underneath). Roger bridges the two with ~15
per-primitive hooks, and today every hook carries three duties:

1. **content** — capture what to draw (hires cel, crisp text, window box);
2. **lifetime** — know when SCI considers the element gone;
3. **vacated geometry** — dirty exactly the overlay pixels the element (and any
   overdraw) occupied, so the dirty-rect present repaints them.

Nearly every recurring Roger bug is duty 2 or 3 failing for one hook:

| Bug (date) | Failed duty |
|---|---|
| Ghost dialog text through blocking Print (06-30) | lifetime (deferred to frozen animate cycle) |
| "Text doesn't clear until next message" (06-30) | vacated geometry (no dirty on removal) |
| Walking 2.7× slowdown (06-28, `bb65c56b75a`) | present discipline (unconditional present in a per-cycle hook) |
| QFG1 un-enhanced band after typed-command box (07-02) | lifetime (pixel stamps outlived their window) |
| SQ3 white line after typing (07-02) | vacated geometry (clear dirtied 2 *overlay* px vs 2 *native* px of compositor overdraw) |

Each fix was correct, but the architecture makes the class recurrent: every
new hook is a fresh chance to get duty 2 or 3 subtly wrong, and duty-3 bugs
hide from scripted repro (interactive mouse movement produces presents
mid-draw that `.rin` scripts barely do).

## 2. Goals, non-goals, hard constraints

**Goals**

- Remove duty 3 (vacated geometry) from every hook, permanently: invalidation
  becomes automatic, driven by rects SCI itself reports.
- Make present storms structurally impossible: one gated present barrier
  instead of 17 scattered `presentWithUi()` call sites.
- Add a backstop net (native-buffer cycle diff) that catches native draws no
  hook records — invalidation only, never content.
- Keep every phase independently shippable and revertible.

**Non-goals**

- Deriving element **lifetime** from pixels. Lifetime stays keyed to window
  tokens / owner objects. Content-based lifetime is the known char-sheet-popup
  trap (CLAUDE.md invariants) and is out of scope.
- Fixing capture **content** correctness (wrong-content stamps, init-cel
  promotion). Duty 1 remains semantic and manual by design.
- Any change to the omyac generation pipeline, priority/occlusion, palette,
  transitions rendering, or display modes.

**Hard constraints (gate for every phase)**

- **Containment: no footprint outside the SCI engine.** All changes live in
  `engines/sci/roger/` plus the existing Roger hook sites in
  `engines/sci/graphics/` (`animate.cpp`, `paint16.cpp`, `ports.cpp`). Zero
  changes to `common/`, `graphics/` (top-level), `backends/`, `base/`, `gui/`,
  or any other engine. No new `OSystem` API usage beyond what the compositor
  already calls. New logic goes in the provider/compositor (`roger/`); the
  `engines/sci/graphics/` hook sites may only gain calls to `g_sciRogerProvider`
  virtuals — no Roger logic inline — preserving the Stage 3 plugin-migration
  boundary (CLAUDE.md): the plugin split must not get harder because of this
  work. A phase plan that needs to violate this stops and comes back to the
  spec.
- Walking speed and render latency must not regress. Concretely: `ROGER-CYCLE`
  median period during a 10 s keyboard walk must stay within +5 % of the
  Phase-0 baseline (~83 ms) in both benchmark rooms, and median `busy` within
  +1 ms. If a phase cannot meet this after tuning, revert the phase.
- Dialog open/dismiss must present in the same cycle as today (no deferred
  first paint of a Print window).
- The `.rin` capture contract ("capture pends; a present consumes it; flush
  with a `move`") keeps working unchanged.

## 3. Target architecture (final state)

Two concerns, currently tangled in every hook, become separate subsystems:

```
INVALIDATION (where is stale?)          CONTENT (what to draw there?)
──────────────────────────────          ─────────────────────────────
exact rects from SCI seams:             plate (omyac)                 (unchanged)
  bitsShow / bitsRestore /              sprite list: cast + statics   (unchanged)
  kGraphRedrawBox / transitions           + owner-gated init cels
semantic dirty:                         UI display list: windows,     (unchanged)
  uiPush*/uiClear*, sprite moves          text, controls, stamps
cycle-diff backstop net                   (window-token lifetime)
        │                                          │
        ▼                                          ▼
   dirty-union accumulator  ──────────►  presentBarrier(): recompose ONLY the
   (RogerCompositor::addDirtyRect,       dirty union from the content layers,
    already exists)                      one gated push per barrier call
```

**3.1 Invalidation sources become dumb and exact.** `bitsShow`,
`bitsRestore`, and `kGraphRedrawBox` already hand Roger the exact native rect
SCI touched. In the final state each unconditionally adds its mapped rect to
the dirty union — no tokens, no geometry reconstruction, no per-hook "did I
remember to dirty the vacated area". The SQ3 white-line class dies here: when
a window's save-under is restored, `bitsRestore`'s own rect invalidates the
region regardless of what any bookkeeping thought was drawn there.
Accumulating a rect is O(1) and never presents.

**3.2 One present barrier.** `presentBarrier()` is the only function that
pushes to the overlay. Internally: if the dirty union is empty and no capture
is pending, return (O(1)); otherwise recompose the dirty regions from the
content layers into the cached composite and push them. Call sites:

| Seam | Why it must call the barrier |
|---|---|
| end of `renderFromAnimateList` (invoked from `kernelAnimate`, animate.cpp ~765 — an existing hook; no new engine-side call site) | the per-cycle present; also hosts the diff net (§3.4) |
| `onMouseMoved` | cursor tracking during blocking dialogs/menus (animate frozen) |
| `uiPush*` / `uiClearToken` / `uiClearAll` / `onNativeEraseRect` | blocking Print/kDisplay draws and disposals happen while animate is frozen; same-cycle paint is required |
| room change / F10 / toggle paths | full present (mark everything dirty, then barrier) |

The ~16 scattered `presentWithUi()` call sites in `file_roger_art_provider.cpp`
become `markDirty(...); presentBarrier();` — and because gating lives inside
the barrier, a hook can no longer cause a present storm (the `bb65c56b75a`
class becomes impossible rather than merely avoided). Presents during
blocking dialogs stay synchronous because those seams call the barrier
directly.

**3.3 Region-bounded recompose replaces the full-frame invalidate.** Today a
UI change sets `_compositeCacheValid = false`, forcing the next present to
recompose the whole frame (~6–26 ms). In the final state the composite cache
is patched only inside the dirty union: plate blit, sprites intersecting the
region, UI elements intersecting the region (clipped draws). Full recompose
remains only for room change, F10/geometry changes, and the periodic heal
frame — as today. This is a latency *improvement* at dialog time.

**3.4 Cycle-diff backstop net (invalidation only).** At the animate seam
where `snapshotNativeBaseline()` already sits (the one moment the native
buffer holds the complete frame), snapshot the 320×200 visual buffer and diff
it against the previous cycle's snapshot with the existing
`Roger::extractChangedBoxes`; add the changed boxes to the dirty union.
Purpose: catch native draws that arrive through no hooked seam
(unknown-unknowns), so a missing hook degrades to "briefly blocky/native-
looking region gets recomposed" instead of "stale pixels forever". Two hard
rules: (a) the diff **never stamps native pixels** — the retired
`roger_diff_backstop` stamping trap stays retired; (b) it is a *net*, not the
primary mechanism — draw+undo sequences that complete between two snapshots
(e.g. a window opened and dismissed while animate is frozen) are invisible to
it, which is exactly why §3.1's per-seam rects are the primary mechanism.
Knob: `roger_diff_net` (default on once validated; `false` = escape hatch).

**3.5 What stays manual (honest accounting).**

| Bug class | Final state |
|---|---|
| Vacated-geometry / missed dirty (SQ3 line, text-doesn't-clear) | **killed** — invalidation is automatic and exact |
| Present storms / per-cycle perf (walking slowdown) | **killed structurally** — gating lives inside the single barrier |
| Unhooked native draws (missing content) | **mitigated** — diff net recomposes the region; content may still need a semantic hook to look crisp |
| Display-list lifetime (ghost text, stale stamps) | **unchanged mechanism** (window tokens / owner gating), but failure mode softens: a leaked element survives only until its region is next invalidated, not forever |
| Capture content correctness (wrong-content stamp, init-cel promotion) | **unchanged** — inherent to semantic enrichment |

## 4. What explicitly does not change

- Window-token (`0x40000000|id`, `0x60000000|id`) and owner-object lifetime
  discipline, including the 07-02 window-scoped Feeder B stamps.
- The content hooks and their capture semantics (`onNativeText`, controls16,
  `onAddToPicCel`, `onInitCel`, ViewCache, glyph hybrid text).
- Transitions (`onTransition` runs its own present loop; it marks the full
  region dirty on completion via `resetForRoomChange` as today) and
  `roger_palette_live` (its per-frame plate re-tint marks the plate region
  dirty exactly as it does now).
- Display modes (Enhanced / Original / Side-by-Side), the sbs comparison
  snapshot, the launcher/picker, and all generation/caching.
- The `.rin` harness and capture flush contract.

## 5. Phases

Each phase below is scoped to be one implementation plan. **Between phases:
run the full §8 regression gate; all PASS before the next plan is written.**

### Phase 0 — regression suite + baselines (no engine changes)

Build the instrument the other phases are judged by.

- Promote the session's throwaway scripts into `test/sci/roger/scripts/`:
  - `qfg1-cmdbox.rin` — type command, dismiss, verify no stale band (QFG1 save 3, room 320)
  - `sq3-dismiss-matrix.rin` — ENTER / ESC / click / empty-submit dismissals (SQ3 save 1, room 2)
  - `sq3-wiggle.rin` — mouse movement interleaved with typing and dismissal (the interactive-only trigger)
  - `qfg1-walk-perf.rin` / `sq3-walk-perf.rin` — 10 s keyboard walk for `-CycleLog`
  - `qfg1-dialog-cycle.rin` — open/dismiss look dialog ×3, ghost-text check
  - reuse existing `qfg1-smoke.rin`
- A driver script `test/sci/roger/run-regression.ps1` that runs each `.rin`
  via `build_and_run.ps1 -NoBuild -TimeoutSec`, pixel-diffs labelled captures
  against committed baseline PNGs (tolerance: zero differing pixels outside
  ego/cursor exclusion rects), parses `ROGER-CYCLE` medians, and prints a
  PASS/FAIL table. Baselines and thresholds (median period per room, busy
  median) are captured on the current build and recorded in the repo.
- Exit criteria: driver runs green twice consecutively on the unmodified
  engine; baseline numbers committed.

### Phase 1 — exact invalidation + barrier consolidation

The big bug-kill; no diff involved.

- Add `presentBarrier()` to `FileRogerArtProvider`; convert the ~16
  `presentWithUi()` call sites to `markDirty + presentBarrier`. `presentWithUi`
  survives only as the barrier's internal push step. The per-cycle barrier
  call goes at the end of `renderFromAnimateList` — all Phase 1 code lives in
  `engines/sci/roger/`; the `engines/sci/graphics/` hook sites change only if
  a hook's *signature* needs a rect it doesn't already pass (none currently
  identified).
- `onNativeEraseRect` / `onNativeShowRect` / `kernelGraphRedrawBox`
  unconditionally add their (mapped) rects to the dirty union. Their existing
  side jobs (generic-text drop, Feeder B region queue) are unchanged.
- Region-bounded recompose (§3.3): patch the composite cache inside the dirty
  union instead of `_compositeCacheValid = false` full recompose. Full
  recompose remains for room change / F10 / heal frame.
- Simplification made possible immediately: the 07-02 "grow removed rects by
  2 native px" workaround in `uiClearToken` becomes redundant (bitsRestore's
  exact rect covers it) — remove it in this phase and let the gate prove it.
- Perf watch-items for the plan: bitsRestore fires ~2×/moving-sprite/cycle —
  its rects must coalesce with the sprite dirty the present already pushes
  (measure: walking dirty-union area per cycle should not grow >10 % vs
  baseline); `presentBarrier()` on the empty union must be O(1).
- Exit criteria: §8 gate green, including the two 07-02 bug scripts and the
  wiggle script; walking telemetry within thresholds; a deliberate fault
  injection (comment out one `markDirty` in a scratch build) is caught by the
  suite, proving the gate can actually detect this class.

### Phase 2 — cycle-diff backstop net

- At the `snapshotNativeBaseline()` seam in `kernelAnimate`, keep a previous-
  cycle snapshot and run `extractChangedBoxes`; add changed boxes to the dirty
  union before the barrier call. Read the visual buffer by row pointer/memcpy,
  not per-pixel `getVisual`, if the measured cost exceeds budget.
- Instrument: log the snapshot+diff cost once per N cycles under `-CycleLog`.
  Budget: < 1.0 ms median. Over budget after optimization → ship with
  `roger_diff_net=false` default and record the finding.
- Fault-injection validation: disable one legitimate invalidation source in a
  scratch build and confirm the net heals the region within one cycle (brief
  flicker acceptable, no persistent staleness).
- Exit criteria: §8 gate green with the net on; walking telemetry within
  thresholds; measured diff cost recorded in this spec's addendum.

### Phase 3 — delete redundant bookkeeping

Only after Phases 1–2 have soaked (user has played both games interactively).

- Remove per-hook vacated-rect code now covered by exact invalidation:
  the dirty-rect plumbing inside `uiClearToken`/`onNativeEraseRect` removal
  paths, `uiPushFrameBox`'s manual clear-dirty, and any `addDirtyRect` calls
  whose region is provably covered by a §3.1 seam (each removal justified
  individually in the plan, with the covering seam named).
- Update CLAUDE.md's invariants section: duty 3 is retired; document the
  barrier + net as the new load-bearing mechanism; keep the lifetime traps.
- Exit criteria: §8 gate green; `engines/sci/roger` line count strictly
  decreases; CLAUDE.md updated.

## 6. Failure / rollback policy

- A phase that fails its gate is fixed within the phase or reverted whole
  (each phase is one commit series on its own branch). Shipped earlier phases
  are never rolled back by a later phase's failure.
- `roger_diff_net=false` is the runtime escape hatch for Phase 2. Phase 1 has
  no knob — it replaces the present path; its rollback is git revert.

## 7. Perf measurement protocol (the hard gate)

- Benchmarks: QFG1 save 1 (room 300) and SQ3 save 1 (room 2), the two
  `*-walk-perf.rin` scripts, run with `-NoBuild -CycleLog`.
- Metrics: median and p90 of `ROGER-CYCLE period`, median `busy`, from
  `screenshots/roger-run.log`, computed by `run-regression.ps1`.
- Thresholds: period median ≤ baseline × 1.05; busy median ≤ baseline + 1 ms;
  p90 period ≤ baseline p90 × 1.10. Baselines from Phase 0, re-recorded only
  deliberately (never silently).
- Dialog latency: `qfg1-dialog-cycle.rin` captures must show the dialog fully
  painted on the first post-open capture (same-cycle paint), as today.

## 8. Regression gate (run between every phase)

`test/sci/roger/run-regression.ps1` executes, in order: `qfg1-smoke`,
`qfg1-cmdbox`, `qfg1-dialog-cycle`, `sq3-dismiss-matrix`, `sq3-wiggle`,
`qfg1-walk-perf`, `sq3-walk-perf`. PASS requires: every labelled capture
pixel-matches its baseline (outside declared exclusion rects), zero
`ROGER-SCRIPT` parse warnings, telemetry within §7 thresholds, exit code 0
from every run (124 = automatic FAIL). The gate result table is pasted into
the phase's plan document before the next phase's plan is written. In
addition, before Phase 3 starts, the user plays both games interactively
(typed commands, dialogs, inventory, at least one room change) — the wiggle
class proved that scripted coverage alone can miss interactive-only triggers.

## 9. Risks

| Risk | Mitigation |
|---|---|
| Barrier changes present timing in a blocking-dialog corner (first paint deferred) | blocking seams call the barrier synchronously (§3.2 table); `qfg1-dialog-cycle` + interactive soak gate it |
| bitsRestore always-dirty inflates per-cycle recompose area | rects coalesce with existing sprite dirty; dirty-area telemetry in Phase 1 exit criteria |
| Diff cost blows the cycle budget on some machine/room | measured budget + `roger_diff_net` escape hatch |
| Region-clipped UI redraw draws a window differently than full redraw (clip bugs) | Phase 1 keeps renderUiLayer's element logic untouched — only the destination clip changes; sbs comparison capture in the gate |
| The refactor itself introduces the next regression | phased shipping, per-phase gate, fault-injection tests prove the gate detects the class it guards |

## 10. Relationship to existing docs

This spec adds a subsystem-level mechanism under the living design doc
(`2026-06-19-roger-art-replacement-design.md`); it does not supersede it. On
Phase 3 completion, CLAUDE.md's "SCI0 rendering & UI invariants" section is
rewritten to describe the barrier + net as the enforcement mechanism, keeping
the lifetime traps as the remaining manual discipline.
