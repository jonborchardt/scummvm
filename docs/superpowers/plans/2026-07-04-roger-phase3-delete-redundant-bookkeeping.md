# Roger Phase 3: Delete Redundant Bookkeeping — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Retire duty 3 (manual vacated-geometry bookkeeping) per spec §5 Phase 3: delete the vacated-rect code provably covered by §3.1 exact invalidation, document the two retained exceptions where no seam covers them, and rewrite CLAUDE.md's invariants to describe the barrier + net + layered invalidation as the enforcement mechanism.

**Architecture:** All code changes in `engines/sci/roger/` (one file-pair plus a coords header); docs changes in `CLAUDE.md` and the gitignored `docs/superpowers/` plan/spec. No engine-side edits, no new hooks, no knobs, no scratch fault injections (Phase 2 proved the gate cannot verify invalidation marks either way — every deletion here is justified **analytically** with the covering seam named, per the dossier, and the real verification is the gate staying green plus the user's interactive soak).

**Tech Stack:** C++11 (no exceptions/RTTI, tabs, K&R), MSVC build via `.\build_and_run.ps1 -NoLaunch`, regression gate `powershell -File test\sci\roger\run-regression.ps1` (33 checks) as the per-task acceptance test. No new CxxTest surface — this phase only removes code and edits comments/docs.

## Deviation from the spec's §5 Phase 3 guesses (read first)

Spec §5 Phase 3 names three deletion targets. The Phase 3 code-facts dossier
(2026-07-04, verified at `jon-refactor3` tip `1534324182c`) shows only ONE is
provably covered; the spec's own rule ("provably covered by a §3.1 seam, each
removal justified individually") therefore governs over its examples:

| Spec's target | Verdict | Why |
|---|---|---|
| `markVacatedDirty` loop in `onNativeEraseRect` | **DELETE** (Task 1) | Every dropped rect passes `nativeRect.contains(...)`, and `markNativeDirty(nativeRect)` at the top of the same function already invalidated the superset. Provable containment. |
| `markVacatedDirty` loop in `uiClearToken` | **RETAIN + document** (Task 2) | Transparent / no-save-under windows and `reanimate==false` disposals never fire `bitsRestore` (CLAUDE.md invariant), so no §3.1 erase rect exists for them; and Feeder B `_textSprites` stamp rects can exceed the window's save-under rect. `_dirtyPrev` heals one frame late; the net is blind to freeze-bracketed round-trips. This mark is the only same-present invalidation for that class. |
| `markVacatedDirty` in `uiPushFrameBox` | **RETAIN + document** (Task 2) | The frame box has no SCI save-under at all — no `bitsRestore` ever fires for the old position — and the diff net diffs the NATIVE buffer, so it cannot see an overlay-only draw. Deletion = one-frame ghost on every keyboard-navigation step with no backstop. |

Phase 2's addendum anticipated this: "the marks remain load-bearing as the
**first** layer of a redundant stack — their unique value is intra-freeze
correctness … which the net cannot provide."

## Global Constraints

- **Containment (spec §2, hard gate):** code changes in `engines/sci/roger/` only; `CLAUDE.md`, `test/**`, and `docs/**` also allowed. **Zero** edits under `engines/sci/graphics/`, `common/`, top-level `graphics/`, `backends/`, `base/`, `gui/`. Verify per task: `git diff --stat <base>` shows only allowed paths.
- **Branch policy (spec §6):** one commit series on branch `jon-p3-cleanup`, created off `jon-refactor3` at `1534324182c` or later (must contain Phase 2). Rollback is git revert — this phase has no knob.
- **Precondition (spec §8, hard):** the user has interactively soaked BOTH games (typed commands, dialogs, inventory, ≥1 room change) on a build containing Phases 1–2. **Confirm with the user before Task 1's first commit; do not assume.** If not yet soaked, stop and hand back.
- **Perf thresholds (spec §7, gate-enforced):** `ROGER-CYCLE` period median ≤ baseline × 1.05; busy median ≤ baseline + 1 ms; p90 ≤ baseline p90 × 1.10. Baselines (`test/sci/roger/baselines/perf-baseline.json`): qfg1 period=83 p90=84 busy=17; sq3 period=83 p90=84 busy=4. Known flake: busy-only red with medians on baseline → re-run once; if red twice in a row, cool down ~5 min (parallel Claude sessions on this box inflate busy) and try once more; **never re-record baselines**.
- **Exit criterion (spec §5 Phase 3):** `engines/sci/roger` total line count **strictly decreases** vs the recorded baseline **13,128** (52 files; `file_roger_art_provider.cpp` = 2,773). Deletions must outweigh added justification comments — keep comments terse; NEVER delete functional code just to satisfy the metric.
- **No scratch fault injections in this phase.** Phase 2 proved four ways that the gate is blind to invalidation-mark changes (layered redundancy) — an injection run proves nothing here. Verification = analytic justification + gate green + interactive soak.
- **Commit discipline:** never mix the functional deletion (Task 1) with comment/docs cleanup (Tasks 2–3) in one commit — upstream rule, and it keeps each commit's justification crisp.
- **Build:** `.\build_and_run.ps1 -NoLaunch` (~1–3 min incremental; LNK1104 = a parallel session holds the exe — wait 60 s, retry once). **Gate:** `powershell -File test\sci\roger\run-regression.ps1` — 7 entries, 33 checks, ~5–10 min, exit 0 required; give gate tool calls a 600000 ms timeout.
- Commits on `jon-p3-cleanup`; `docs/superpowers/**` needs `git add -f` (gitignored by design). **Do not commit** anything under `.superpowers/` or `screenshots/`.

## Code facts (verified 2026-07-04 at `1534324182c`; full dossier: `.superpowers/sdd/phase3-code-facts.md`)

Line numbers WILL drift — match by function/member name.

- `onNativeEraseRect` (`file_roger_art_provider.cpp:1768`): first statement is the §3.1 mark `markNativeDirty(nativeRect);` (line 1772, STAYS). The generic-text eviction below it collects `droppedRects` under the guard `nativeRect.contains(els[i].nativeRect)` (line 1786) and later loops `markVacatedDirty(droppedRects[i])` (lines 1794–1795) — the provably redundant target.
- `uiClearToken` (`file_roger_art_provider.cpp:1718`): populates `removedRects` from `_uiLayer->clearToken` AND from removed Feeder B `_textSprites[i].celRect` stamps; loops `markVacatedDirty(removedRects[i])` at lines 1763–1764 (RETAINED); `presentBarrier()` at 1765 is conditional on an actual removal — that conditionality is load-bearing (the `bb65c56b75a` walking-storm fix) and is NOT touched.
- `uiPushFrameBox` (`file_roger_art_provider.cpp:1818`): `markVacatedDirty(oldFrameRect)` at line 1845 (RETAINED), then `markUiDirty(r); presentBarrier();`.
- `markVacatedDirty` (`file_roger_art_provider.cpp:1310–1328`): maps display-list geometry via `Roger::uiVacatedExtent`, adds to the dirty union, and routes to the scene seed union when `_inAnimateCycle`. Its body comment (lines 1315–1319) still claims the gate is blind "via needFullSource" — STALE since Phase 2 (truth captures no longer force full source; the gate is blind for a deeper reason: layered redundancy).
- `Roger::uiVacatedExtent` (`roger_coords.h:116–129`): doc comment carries the same stale needFullSource claim. Both `uiPaintExtent` and `uiVacatedExtent` stay live (callers: `markUiDirty` / `markVacatedDirty`).
- `presentWithUi` is called ONLY from `presentBarrier` (Phase 1 consolidation complete; no bypasses).
- The layered-redundancy mechanisms (context for comments, do not modify): `renderUiLayer`'s per-element `addDirtyRect` → `_dirtyPrev` roll (roger_compositor.cpp ~796–821 and `rollPresentDirty` ~676); the scene seed union incl. `_sceneDeferredDirty` (roger_compositor.cpp ~494–509); the diff net (`snapshotNativeBaseline`).
- CLAUDE.md targets (match by text, not line): section heading `### SCI0 rendering & UI invariants (READ BEFORE TOUCHING ANY HOOK)`; the "every removal MUST explicitly dirty the vacated dest rect" paragraph inside the "How SCI0 draws" bullets; the traps bullet `- Removing an overlay element without dirtying its vacated rect → stale pixels until the next redraw.`; the "Underused SCI signals" sentence naming "a single **frame-complete** present barrier" as future work (shipped in Phase 1); the config-knobs paragraph in Stage 1 (gains `roger_diff_net` / `roger_truth_capture` mentions).
- Line-count command (PowerShell):
  `(Get-ChildItem engines\sci\roger -Include *.cpp,*.h -Recurse | Get-Content | Measure-Object -Line).Lines`
  Baseline: **13,128**.

---

### Task 1: Phase branch + the one provable deletion

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (`onNativeEraseRect` only)

**Interfaces:**
- Consumes: `markNativeDirty(nativeRect)` already invalidating the superset rect at the top of `onNativeEraseRect` (§3.1 seam — unchanged).
- Produces: `onNativeEraseRect` without the `droppedRects` plumbing. No signature changes; later tasks rely on nothing new from this task.

- [x] **Step 1: Confirm the soak precondition, then create the phase branch**

Ask the user (or confirm from the conversation) that both games have been interactively soaked on a Phase 1+2 build. Then:

```bash
git checkout jon-refactor3
git checkout -b jon-p3-cleanup
git rev-parse HEAD   # record as <base>; must be 1534324182c or a descendant
```

- [x] **Step 2: Delete the redundant vacated marks in `onNativeEraseRect`**

In `engines/sci/roger/file_roger_art_provider.cpp`, locate `onNativeEraseRect`. Current eviction block (match by content):

```cpp
	bool removed = false;
	const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
	Common::Array<Roger::UiElement> kept;
	Common::Array<Common::Rect> droppedRects;
	for (uint i = 0; i < els.size(); i++) {
		if (isGenericTextToken(els[i].token) && nativeRect.contains(els[i].nativeRect))
			{ removed = true; droppedRects.push_back(els[i].nativeRect); continue; }
		kept.push_back(els[i]);
	}
	if (removed) {
		_uiLayer->clearAll();
		for (uint i = 0; i < kept.size(); i++)
			_uiLayer->push(kept[i]);
		for (uint i = 0; i < droppedRects.size(); i++)
			markVacatedDirty(droppedRects[i]);
		if (_diag)
```

Replace with (deletes `droppedRects` declaration, its push, and the `markVacatedDirty` loop; adds a one-line justification):

```cpp
	bool removed = false;
	const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
	Common::Array<Roger::UiElement> kept;
	for (uint i = 0; i < els.size(); i++) {
		if (isGenericTextToken(els[i].token) && nativeRect.contains(els[i].nativeRect))
			{ removed = true; continue; }
		kept.push_back(els[i]);
	}
	if (removed) {
		_uiLayer->clearAll();
		for (uint i = 0; i < kept.size(); i++)
			_uiLayer->push(kept[i]);
		// No vacated marks: the containment guard means markNativeDirty(nativeRect)
		// above already invalidated a superset of every dropped element (§3.1).
		if (_diag)
```

The `if (_diag)` warning block and the trailing `presentBarrier();` are unchanged.

- [x] **Step 3: Build**

Run: `.\build_and_run.ps1 -NoLaunch`
Expected: build succeeds (the deleted array had no other uses in the function).

- [x] **Step 4: Gate**

Run: `powershell -File test\sci\roger\run-regression.ps1` (600000 ms timeout)
Expected: ALL PASS (33 checks), exit 0. Flake rule per Global Constraints. Any pixel-check FAIL is unexpected — this deletion is containment-provable — diagnose before proceeding (the likely cause would be an eviction whose rect somehow escapes the guard; re-read the loop).

- [x] **Step 5: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.cpp
git commit -m "Roger p3: drop redundant vacated marks in onNativeEraseRect

Every dropped generic-text rect passes nativeRect.contains(), so the
markNativeDirty(nativeRect) at the top of the function already
invalidated a superset (spec 3.1 exact seam). Provable containment;
the covering seam is bitsRestore's own erase rect."
```

---

### Task 2: Document the two retained exceptions + fix Phase-2-falsified comments

Comments only — no behavioral change. Separate commit from Task 1 (never mix functional and comment changes).

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (`uiClearToken`, `uiPushFrameBox`, `markVacatedDirty` comment)
- Modify: `engines/sci/roger/roger_coords.h` (`uiVacatedExtent` doc comment)

**Interfaces:**
- Consumes: Task 1's committed state (the only remaining `markVacatedDirty` callers are the two retained sites).
- Produces: nothing new for later tasks; Task 3's CLAUDE.md text calls these "the two documented duty-3 exceptions" — both code comments must start with `RETAINED duty-3 exception` so a grep finds them.

- [x] **Step 1: Retention comment in `uiClearToken`**

Replace the current comment above the `markVacatedDirty` loop (match by content):

```cpp
	// Dirty the overlay regions the removed elements occupied so the barrier's
	// present repaints them with clean background (else they ghost until another
	// draw touches them). markVacatedDirty centralizes the extent math.
	for (uint i = 0; i < removedRects.size(); i++)
		markVacatedDirty(removedRects[i]);
```

with:

```cpp
	// RETAINED duty-3 exception (Phase 3): no-save-under / reanimate==false disposals
	// never fire bitsRestore, and Feeder B stamp rects can exceed the save-under rect —
	// this is their only same-present invalidation (net blind while frozen; _dirtyPrev a frame late).
	for (uint i = 0; i < removedRects.size(); i++)
		markVacatedDirty(removedRects[i]);
```

- [x] **Step 2: Retention comment in `uiPushFrameBox`**

Replace (match by content):

```cpp
	if (!oldFrameRect.isEmpty()) markVacatedDirty(oldFrameRect);
```

with:

```cpp
	// RETAINED duty-3 exception (Phase 3, uiClearToken's twin): no SCI save-under exists
	// for the frame box, and the net can't see overlay-only draws — old position would ghost.
	if (!oldFrameRect.isEmpty()) markVacatedDirty(oldFrameRect);
```

- [x] **Step 3: Fix the stale needFullSource claim in `markVacatedDirty`**

Replace the body comment (match by content):

```cpp
	// Exact rect + TTF pad. The compositor-overdraw ring beyond it is covered by
	// bitsRestore's exact erase rect (markNativeDirty in onNativeEraseRect) — the
	// spec's §3.1 claim, verified by interactive soak, NOT provable by the capture-based
	// gate (its capture path forces a full clean recompose via needFullSource, so a
	// missing-mark fault cannot appear in any capture by construction).
```

with:

```cpp
	// Exact rect + TTF pad. The compositor-overdraw ring beyond it is covered by
	// bitsRestore's exact erase rect (markNativeDirty in onNativeEraseRect). Phase 2
	// proved the gate cannot verify invalidation marks either way (layered redundancy:
	// _dirtyPrev loop + scene seed union) — invalidation changes are soak-verified.
```

- [x] **Step 4: Fix the stale claim in `roger_coords.h` `uiVacatedExtent`**

Replace the doc comment (match by content):

```cpp
/**
 * Overlay extent a REMOVED element must invalidate: exact native rect + the TTF
 * overshoot pad only. The compositor-overdraw ring beyond it is covered by
 * bitsRestore's exact erase rect (§3.1 exact invalidation, wired in Task 5). That
 * coverage is verified by interactive soak, NOT by the capture-based regression
 * gate: the gate's capture path forces a full clean recompose (needFullSource), so
 * a missing-mark fault is structurally invisible to any capture — it cannot prove
 * invalidation coverage.
 */
```

with:

```cpp
/**
 * Overlay extent a REMOVED element must invalidate: exact native rect + the TTF
 * overshoot pad only. The compositor-overdraw ring beyond it is covered by
 * bitsRestore's exact erase rect (§3.1). Live callers are the two documented
 * duty-3 exceptions (markVacatedDirty from uiClearToken / uiPushFrameBox), where
 * no bitsRestore rect ever fires. Gate greens cannot verify this either way
 * (Phase 2: layered redundancy) — coverage is soak-verified.
 */
```

- [x] **Step 5: Trim the historical present-storm comment in `uiClearToken`**

Replace the function-header comment (match by content — the six lines beginning "SCI calls this from bitsRestore for every save-under region…" and ending "…otherwise this is a no-op."):

```cpp
	// SCI calls this from bitsRestore for every save-under region it restores — which, while
	// walking, is ~2× per updated sprite EVERY game cycle, almost always for a token that matches
	// no UI element. presentWithUi() is a full-overlay re-present (copy + convert + copyRectToOverlay)
	// and the cache-invalidate forces a full recompose next frame, so an unconditional present here
	// dominated the cycle (~196 ms — the game ran ~2.7× slow). Only invalidate + present when an
	// element was actually removed (e.g. a dialog/look-at dismissal); otherwise this is a no-op.
```

with:

```cpp
	// bitsRestore calls this ~2× per moving sprite EVERY cycle, almost always with a
	// token matching no element — so all work below is gated on an actual removal
	// (the bb65c56b75a walking-storm class; the barrier gates presents, but staying
	// no-op on the walking path keeps the per-cycle cost trivial).
```

- [x] **Step 6: Build + gate**

Run: `.\build_and_run.ps1 -NoLaunch` then `powershell -File test\sci\roger\run-regression.ps1` (600000 ms timeout)
Expected: build OK; ALL PASS (33 checks) — comments cannot change behavior; this run guards against typos breaking the build and gives the commit a green stamp.

- [x] **Step 7: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.cpp engines/sci/roger/roger_coords.h
git commit -m "Roger p3: document retained duty-3 exceptions; fix stale gate-blindness comments

uiClearToken and uiPushFrameBox keep their markVacatedDirty calls: no
bitsRestore rect exists for no-save-under disposals or the frame box,
and the diff net cannot see overlay-only or freeze-bracketed changes.
Comments claiming the gate is blind via needFullSource are stale since
Phase 2 (truth captures); the real reason is layered redundancy."
```

---

### Task 3: Rewrite CLAUDE.md invariants — duty 3 retired, barrier + net documented

**Files:**
- Modify: `CLAUDE.md` (the "SCI0 rendering & UI invariants" section, the traps list, the "Underused SCI signals" paragraph, and the Stage 1 config-knobs paragraph)

**Interfaces:**
- Consumes: the two retained-exception comments from Task 2 (referenced as "the two documented duty-3 exceptions").
- Produces: the rewritten invariants Phase 3's exit criterion requires (spec §10).

- [x] **Step 1: Replace the "every removal MUST dirty" bullet**

In CLAUDE.md's "How SCI0 draws (the mental model)" list, replace this bullet (match by text):

```markdown
- **The overlay is retained; the native screen is immediate.** SCI "erases" transient
  content (text, dialogs) simply by **redrawing the scene underneath it**. The overlay has
  **no automatic erase** — nothing repaints a region until something dirties it. So *every
  removal* of an overlay element MUST explicitly dirty the vacated dest rect, or the
  dirty-rectangle present skips it and the pixels ghost. (Fix pattern: `clearToken`/
  `onNativeEraseRect` collect removed native rects → `addDirtyRect(sciRectToDest(...))`.)
```

with:

```markdown
- **The overlay is retained; the native screen is immediate.** SCI "erases" transient
  content (text, dialogs) simply by **redrawing the scene underneath it**. The overlay has
  **no automatic erase** — a region repaints only when something dirties it. Since the
  present-barrier work (Phases 1–2, 2026-07-03/04), invalidation is AUTOMATIC and
  **layered**: the §3.1 exact seams (`markNativeDirty` fed by SCI's own bitsShow /
  bitsRestore / kGraphRedrawBox rects), the UI layer's per-element dirty loop
  (`_dirtyPrev`), the scene seed union, and the cycle-diff net (`roger_diff_net`) each
  independently reseed vacated regions. **Duty 3 is retired: do NOT add manual
  vacated-geometry code to new hooks.** Exactly two documented duty-3 exceptions keep a
  manual `markVacatedDirty` — no-save-under window disposals (`uiClearToken`) and the
  frame box (`uiPushFrameBox`) — classes where no bitsRestore rect ever fires and the
  net is blind (it diffs the NATIVE buffer, and a frozen cycle takes no snapshots).
```

- [x] **Step 2: Replace the vacated-rect trap and add the two Phase-2 traps**

In the "Traps — do NOT re-fall into these" list, replace this bullet (match by text):

```markdown
- Removing an overlay element without dirtying its vacated rect → stale pixels until the next redraw.
```

with:

```markdown
- Assuming every disposal fires bitsRestore → transparent / no-save-under windows and the
  frame box never do; their retained `markVacatedDirty` calls (the two documented duty-3
  exceptions) are the only same-present invalidation for that class.
- Trusting a green gate on invalidation-mark changes → layered redundancy makes mark
  removal invisible to every scripted capture (Phase 2 proved it four ways, including
  real-overlay grabOverlay captures landing on the dismissal present); invalidation
  changes are verified by interactive soak, not by the gate.
```

- [x] **Step 3: Update the "Underused SCI signals" paragraph**

In the sentence listing underused signals, the frame-complete barrier item is now shipped. Replace this fragment (match by text):

```markdown
a single **frame-complete** present barrier around `updateScreen` in `kernelAnimate` (cleaner than scattered per-primitive
presents, and closes the stale-overlay-during-blocking-dialog class);
```

with:

```markdown
(the frame-complete present barrier shipped 2026-07-03 as `presentBarrier` — all presents
funnel through it, with the cycle-diff net at the same seam);
```

- [x] **Step 4: Add the Phase 1–2 knobs to the Stage 1 config-knobs paragraph**

In the Stage 1 "Config knobs" paragraph, directly after the `roger_diff_backstop` entry (match by text: `roger_diff_backstop` …`the bitsShow-hook path and addToPic capture remain on).`), insert:

```markdown
`roger_diff_net` (default on; per-cycle native-buffer diff at the animate seam invalidates
changed regions — heals missed invalidation within one cycle; invalidation-only, never
stamps pixels; escape hatch `=false` / env `ROGER_DIFF_NET=0`; `ROGER-NET sum32=<ms>
boxes=<n>` telemetry under `-CycleLog`, where `boxes` is the last cycle's count),
`roger_truth_capture` (default off; evidence mode — `.rin` captures dump the real overlay
via `grabOverlay` instead of forcing a full recompose; per-launch `-TruthCap` /
`ROGER_TRUTH_CAPTURE`; per-entry `truthCap` flag in the gate manifest).
```

- [x] **Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "Roger p3: CLAUDE.md invariants - duty 3 retired, barrier+net documented

Vacated-geometry invalidation is now automatic and layered (3.1 exact
seams, _dirtyPrev loop, scene seed union, diff net); the two retained
markVacatedDirty sites are documented exceptions. Adds the Phase 2
traps (gate blindness to mark changes; bitsRestore-skipping disposals)
and the roger_diff_net / roger_truth_capture knobs."
```

---

### Task 4: Exit evidence — twice-green gate, line-count check, addenda, hand back

**Files:**
- Modify: `docs/superpowers/plans/2026-07-04-roger-phase3-delete-redundant-bookkeeping.md` (this file — Addendum)
- Modify: `docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md` (Phase 3 result addendum)

**Interfaces:**
- Consumes: Tasks 1–3 complete on `jon-p3-cleanup`.
- Produces: the spec §5 Phase 3 exit evidence; Phase 3 is the LAST phase — on completion the spec's execution model is fulfilled.

- [x] **Step 1: Line-count check (strictly decreases)**

```powershell
(Get-ChildItem engines\sci\roger -Include *.cpp,*.h -Recurse | Get-Content | Measure-Object -Line).Lines
```

Requirement: result < **13,128** (expected ballpark: −3 to −5 net — Task 1 nets −1 line; Task 2 nets ~−2: the two stale-comment fixes and the header trim shed more lines than the two retention comments add). If NOT below baseline: trim comment verbosity in the Task 2 sites (never delete functional code to satisfy the metric) and re-commit as a comment-only amendment. Record the before/after numbers in this plan's Addendum.

- [x] **Step 2: Twice-green exit run**

Run the full gate twice back-to-back on the final build (shipping config, no env vars):

```powershell
powershell -File test\sci\roger\run-regression.ps1
powershell -File test\sci\roger\run-regression.ps1
```

Expected: ALL PASS (33 checks), exit 0, both runs. Flake rule per Global Constraints. Paste both result tables into this plan's Addendum.

- [x] **Step 3: Fill this plan's Addendum**

All bullets: deletion/retention table outcome (any deviation from the table above), line counts before/after, both gate tables.

- [x] **Step 4: Record the Phase 3 result in the spec**

Append to `docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md`:

```markdown
## Addendum: Phase 3 result (2026-07-XX)

Phase 3 shipped on branch `jon-p3-cleanup` (plan:
docs/superpowers/plans/2026-07-04-roger-phase3-delete-redundant-bookkeeping.md — evidence
in that plan's addendum). Of §5 Phase 3's three named targets, one was provably covered
and deleted (`onNativeEraseRect`'s vacated loop — containment under the same function's
§3.1 mark); two were RETAINED as documented duty-3 exceptions with the gaps named
(`uiClearToken`: no-save-under disposals skip bitsRestore + Feeder B stamp overhang;
`uiPushFrameBox`: no save-under exists and the net cannot see overlay-only draws) —
per this spec's own "provably covered" rule, which governs over its examples.
`engines/sci/roger` line count: <before> → <after>. CLAUDE.md invariants rewritten
(duty 3 retired; barrier + net + layered invalidation documented; Phase 2 traps added).
Gate twice-green. No fault injections were run (Phase 2 established the gate cannot
verify invalidation marks either way); the deletion is verified by containment proof,
gate, and the user's interactive soak. This completes the spec's four-phase execution
model; remaining manual disciplines are lifetime (window tokens / owner gating) and
the two documented duty-3 exceptions.
```

- [x] **Step 5: Commit + hand back**

```bash
git add -f docs/superpowers/plans/2026-07-04-roger-phase3-delete-redundant-bookkeeping.md docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md
git commit -m "Roger p3: phase exit - line-count check, twice-green gate, addenda"
```

Then report: Phase 3 complete on `jon-p3-cleanup`, ready to merge (human's call). Recommend a short post-deletion interactive spot-check of dialog dismissals in both games before merging — the deleted path is containment-provable, but soak is this project's ground truth for invalidation changes.

---

## Addendum: Phase 3 exit evidence

(Filled 2026-07-04 during Task 4 execution.)

- **Deletion/retention outcome:** Executed exactly per the table — `onNativeEraseRect` loop DELETED (commit `fd356b50b37`); `uiClearToken` + `uiPushFrameBox` RETAINED + documented with `RETAINED duty-3 exception` comments (commit `17b9d1fb839`); no deviation found during execution.

- **Line counts:** before 13,128 → after **11,900** total (`engines/sci/roger` *.cpp/*.h); `file_roger_art_provider.cpp` baseline 2,773 → after **2,560**.

- **Twice-green gate tables:**

Run 1:

```
Entry              Check              Result Detail
-----              -----              ------ ------
qfg1-smoke         run                PASS   exit 0
qfg1-smoke         scriptWarn         PASS
qfg1-smoke         exists:boot        PASS
qfg1-smoke         exists:after-walk  PASS
qfg1-smoke         exists:after-arrow PASS
qfg1-smoke         exists:typed       PASS
qfg1-smoke         exists:look-dialog PASS
qfg1-smoke         exists:dismissed   PASS
qfg1-cmdbox        run                PASS   exit 0
qfg1-cmdbox        scriptWarn         PASS
qfg1-cmdbox        same:before~after  PASS
sq3-dismiss-matrix run                PASS   exit 0
sq3-dismiss-matrix scriptWarn         PASS
sq3-dismiss-matrix same:m0~esc1       PASS
sq3-dismiss-matrix same:m0~esc2       PASS
sq3-dismiss-matrix same:m0~clk1       PASS
sq3-dismiss-matrix same:m0~emp1       PASS
sq3-wiggle         run                PASS   exit 0
sq3-wiggle         scriptWarn         PASS
sq3-wiggle         same:w0~w1         PASS
sq3-wiggle         same:w0~w2         PASS
qfg1-dialog-cycle  run                PASS   exit 0
qfg1-dialog-cycle  scriptWarn         PASS
qfg1-dialog-cycle  presence:d1        PASS   20000 px (>= 20000)
qfg1-dialog-cycle  presence:d2        PASS   20000 px (>= 20000)
qfg1-dialog-cycle  same:g0~g1         PASS
qfg1-dialog-cycle  same:g0~g2         PASS
qfg1-walk-perf     run                PASS   exit 0
qfg1-walk-perf     scriptWarn         PASS
qfg1-walk-perf     perf               PASS   median=83/83 p90=84/84 busy=17/17 n=166
sq3-walk-perf      run                PASS   exit 0
sq3-walk-perf      scriptWarn         PASS
sq3-walk-perf      perf               PASS   median=83/83 p90=84/84 busy=5/4 n=171

ALL PASS (33 checks)
```

Run 2:

```
Entry              Check              Result Detail
-----              -----              ------ ------
qfg1-smoke         run                PASS   exit 0
qfg1-smoke         scriptWarn         PASS
qfg1-smoke         exists:boot        PASS
qfg1-smoke         exists:after-walk  PASS
qfg1-smoke         exists:after-arrow PASS
qfg1-smoke         exists:typed       PASS
qfg1-smoke         exists:look-dialog PASS
qfg1-smoke         exists:dismissed   PASS
qfg1-cmdbox        run                PASS   exit 0
qfg1-cmdbox        scriptWarn         PASS
qfg1-cmdbox        same:before~after  PASS
sq3-dismiss-matrix run                PASS   exit 0
sq3-dismiss-matrix scriptWarn         PASS
sq3-dismiss-matrix same:m0~esc1       PASS
sq3-dismiss-matrix same:m0~esc2       PASS
sq3-dismiss-matrix same:m0~clk1       PASS
sq3-dismiss-matrix same:m0~emp1       PASS
sq3-wiggle         run                PASS   exit 0
sq3-wiggle         scriptWarn         PASS
sq3-wiggle         same:w0~w1         PASS
sq3-wiggle         same:w0~w2         PASS
qfg1-dialog-cycle  run                PASS   exit 0
qfg1-dialog-cycle  scriptWarn         PASS
qfg1-dialog-cycle  presence:d1        PASS   20000 px (>= 20000)
qfg1-dialog-cycle  presence:d2        PASS   20000 px (>= 20000)
qfg1-dialog-cycle  same:g0~g1         PASS
qfg1-dialog-cycle  same:g0~g2         PASS
qfg1-walk-perf     run                PASS   exit 0
qfg1-walk-perf     scriptWarn         PASS
qfg1-walk-perf     perf               PASS   median=83/83 p90=84/84 busy=16/17 n=166
sq3-walk-perf      run                PASS   exit 0
sq3-walk-perf      scriptWarn         PASS
sq3-walk-perf      perf               PASS   median=83/83 p90=84/84 busy=5/4 n=171

ALL PASS (33 checks)
```
