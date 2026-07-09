# Roger Menu Mouse Slowness Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. In-game verification steps use the **roger-loop** skill (`.claude/skills/roger-loop/SKILL.md`) — invoke it before running any `.rin` evidence step.

**Goal:** Make the mouse track smoothly while a SCI0 menu is open (currently the cursor crawls and highlight changes lag badly).

**Architecture:** The bug is a present storm on the frozen-cycle path. `GfxMenu::interactiveWithMouse()` blocks the game cycle and re-pushes the whole dropdown overlay on every highlight change; each individual `uiClearToken`/`uiPushWindow`/`uiPushText` call flushes its own full `presentWithUi()` because `presentBarrier()` only defers during the animate cycle (`_inAnimateCycle`), which is frozen. Fix = (1) measure with diag-gated present telemetry, (2) add a depth-counted UI batch bracket (`beginUiBatch`/`endUiBatch`) so a multi-element re-push coalesces into ONE present, (3) drop the redundant un-highlight re-push. Verified in-game via the roger-loop skill with before/after telemetry numbers.

**Tech Stack:** C++11 (ScummVM conventions), Windows/MSVC via `build_and_run.ps1`, `.rin` input scripts + `ROGER-DIAG` telemetry for evidence, `run-regression.ps1` gate.

## Global Constraints

- Hook sites in `engines/sci/**` stay mechanical: null-guarded `g_sciRogerProvider` calls only; every new hook is a virtual no-op on the abstract provider (`roger_art_provider.h`); SCI code never names `FileRogerArtProvider`.
- Roger-off behavior must stay byte-identical to stock (all new code behind `g_sciRogerProvider` null/enabled guards or inside `engines/sci/roger/`).
- No per-cycle path may force a full present/recompose when nothing changed (Performance discipline in CLAUDE.md).
- No generation-pipeline files touched → `kTransformVersion` unchanged (state nothing about it in commits; it does not apply).
- Commit style: first line `SCI: ROGER: <summary>` ≤ 50 chars, present tense; write multi-line messages to a file with the Write tool and use `git commit -F <file>` (PowerShell here-strings corrupt messages); end with the Claude co-author footer; verify with `git log -1 --format=%B`.
- Commit immediately after each task (parallel sessions run on this repo; uncommitted work has been lost before).
- Code style: tabs (w4), attached braces, `_camelCase` members, no non-const function-local statics.
- Screenshots and scratch `.rin` scripts go in gitignored `screenshots/`, never the repo root.
- `.rin` runs always pass `-TimeoutSec`; exit 124 = watchdog, 125 = scripted assert fail.

## Diagnosis (evidence base for the tasks)

- `engines/sci/graphics/menu.cpp:1073` `interactiveWithMouse()`: a `while(true)` loop polling `getSciEvent` — the game cycle is FROZEN for the whole menu interaction.
- Every row crossing calls `invertMenuSelection(curItemId)` then `invertMenuSelection(newItemId)` (menu.cpp:1121-1126). `invertMenuSelection` (menu.cpp:837-845) calls `rogerPushMenuOverlay()` whenever `itemId != 0` — TWICE per crossing, and the first call re-pushes with the highlight unchanged (pure waste).
- `rogerPushMenuOverlay()` (menu.cpp:811-829) = `uiClearToken(0x20000000)` + `uiPushWindow` + one `uiPushText` PER menu row.
- Each of `uiClearToken` / `uiPushWindow` / `uiPushText` ends with `presentBarrier()` (file_roger_art_provider.cpp:1707/1725/1961). `presentBarrier()` (line 1517) defers only when `_inAnimateCycle` — frozen menu loop means EVERY push flushes its own `presentWithUi()`: grabPalette + `patchCompositeRegions` (TTF re-render of every dropdown row intersecting the dirty union) + `presentToOverlay`.
- Net: a 10-row dropdown ≈ 2 × (1 + 1 + 10) = 24 bounded presents per single row crossing. Mouse-move cursor updates (`onMouseMoved` → `presentBarrier`, file_roger_art_provider.cpp:3007-3030) queue behind them → the cursor crawls.
- Same storm on menu-title changes (`drawMenu` → `rogerPushMenuOverlay()` at menu.cpp:805-808) and on the bar push (`rogerPushBarOverlay()`, menu.cpp:422-446).
- The frozen mouse-menu loop DOES observe injected `.rin` MOUSEMOVE events (proven by `test/sci/roger/scripts/qfg1-menu-cycle.rin`), so the whole investigation is scriptable.

**Decision gate:** Task 1 produces the baseline numbers. If they do NOT show a present storm correlated with menu dragging (e.g. presents are few and cheap yet the mouse is still slow), STOP after Task 1 — report the telemetry and re-diagnose instead of applying Tasks 2–3 anyway.

## File Structure

| File | Change |
|---|---|
| `engines/sci/roger/file_roger_art_provider.h` | + telemetry counters, + `_uiBatchDepth`, + `beginUiBatch`/`endUiBatch` overrides |
| `engines/sci/roger/file_roger_art_provider.cpp` | telemetry around the barrier's `presentWithUi()` call; batch guard in `presentBarrier()`; `beginUiBatch`/`endUiBatch` bodies |
| `engines/sci/roger/roger_art_provider.h` | + `beginUiBatch()`/`endUiBatch()` virtual no-ops |
| `engines/sci/graphics/menu.cpp` | bracket `rogerPushMenuOverlay`/`rogerPushBarOverlay` with begin/end batch; skip redundant re-push in `invertMenuSelection` |
| `screenshots/menu-drag-perf.rin` | scratch evidence script (gitignored; NOT committed) |

---

### Task 1: Present telemetry + baseline evidence

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.h` (private members near `_barrierDirty`, ~line 289)
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (`presentBarrier()`, ~line 1517)
- Create: `screenshots/menu-drag-perf.rin` (scratch, not committed)

**Interfaces:**
- Produces: `ROGER-DIAG[present]: n=<count> ms=<total> window=<ms>` log line, one per ~second of frozen-cycle presents, emitted only under `-Diag`. Tasks 2–4 grep this exact format for before/after comparison.

- [ ] **Step 1: Add the telemetry counters**

In `engines/sci/roger/file_roger_art_provider.h`, directly below the line `bool _frameJustComposed = false; // renderFrame composed this cycle (Task 4 uses it)` (~line 291), add:

```cpp
	// Frozen-cycle present telemetry (diag-gated, permanent): counts barrier-flushed
	// presentWithUi calls and their cumulative cost, aggregated to one
	// ROGER-DIAG[present] line per second. Zero-cost when _diag is off.
	uint32 _presentTelWindowStart = 0;
	uint32 _presentTelCount = 0;
	uint32 _presentTelMs = 0;
```

- [ ] **Step 2: Emit the aggregate from presentBarrier**

In `engines/sci/roger/file_roger_art_provider.cpp`, `presentBarrier()` currently ends with:

```cpp
	_barrierDirty = false;
	presentWithUi();
}
```

Replace those two lines with:

```cpp
	_barrierDirty = false;
	if (_diag) {
		const uint32 t0 = g_system->getMillis();
		presentWithUi();
		const uint32 t1 = g_system->getMillis();
		_presentTelCount++;
		_presentTelMs += t1 - t0;
		if (_presentTelWindowStart == 0)
			_presentTelWindowStart = t1;
		if (t1 - _presentTelWindowStart >= 1000) {
			warning("ROGER-DIAG[present]: n=%u ms=%u window=%u",
			        _presentTelCount, _presentTelMs, t1 - _presentTelWindowStart);
			_presentTelWindowStart = t1;
			_presentTelCount = 0;
			_presentTelMs = 0;
		}
	} else {
		presentWithUi();
	}
}
```

- [ ] **Step 3: Write the menu-drag evidence script**

Create `screenshots/menu-drag-perf.rin` (game-space 320×200 coords; QFG1 save 1, room 300; menu bar is y<10; crib from `test/sci/roger/scripts/qfg1-menu-cycle.rin` — press-hold + `move` drags are proven to drive the frozen menu loop):

```
# Evidence: presents per second while dragging down a dropdown with the mouse.
# Grep ROGER-DIAG[present] in screenshots\roger-run.log afterwards.
wait 4000
log baseline-idle-start
# 3s of idle mouse wiggling OUTSIDE any menu: the cursor-only present cost.
move 160 100
wait 300
move 200 120
wait 300
move 160 100
wait 300
move 200 120
wait 300
move 160 100
wait 300
move 200 120
wait 1000
log menu-drag-start
# Press-hold on the File title, slide to Game (more rows), then drag DOWN
# through its items row by row and back up, twice — every move crosses a row.
mousedown 15 2
wait 400
move 55 2
wait 400
move 55 15
wait 250
move 55 25
wait 250
move 55 35
wait 250
move 55 45
wait 250
move 55 55
wait 250
move 55 45
wait 250
move 55 35
wait 250
move 55 25
wait 250
move 55 15
wait 250
move 55 35
wait 250
move 55 55
wait 250
move 55 35
wait 250
move 55 15
wait 250
log menu-drag-end
# Release on the bar right of the titles: cancel-close, no item executed.
mouseup 310 2
wait 1400
state
quit
```

- [ ] **Step 4: Build and run the baseline (roger-loop skill)**

Invoke the **roger-loop** skill, then:

```powershell
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script screenshots\menu-drag-perf.rin -Diag -TimeoutSec 180
```

Expected: exit 0, and `screenshots\roger-run.log` contains `ROGER-DIAG[present]` lines. Check the log:

```powershell
Select-String -Path screenshots\roger-run.log -Pattern 'ROGER-DIAG\[present\]|ROGER-SCRIPT'
```

- [ ] **Step 5: Record the baseline numbers in the plan's worklog**

Append a `## Worklog` section to this plan file with the actual `n=` / `ms=` values for (a) the idle-wiggle window and (b) the menu-drag window. Expected shape if H1 is right: drag-window lines with `n` in the dozens+ and `ms` a large fraction of the window; idle lines cheap. **Decision gate:** if the drag window does NOT show the storm, STOP — report findings to the user instead of proceeding.

- [ ] **Step 6: Commit the telemetry**

Write the commit message to a temp file with the Write tool, then:

```powershell
git add engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp
git commit -F <msgfile>
git log -1 --format=%B
```

Message first line: `SCI: ROGER: Add diag-gated present telemetry`

---

### Task 2: UI batch bracket — one present per dropdown re-push

**Files:**
- Modify: `engines/sci/roger/roger_art_provider.h` (~line 232, after `uiClearToken`)
- Modify: `engines/sci/roger/file_roger_art_provider.h`
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (`presentBarrier()`, + two new methods)
- Modify: `engines/sci/graphics/menu.cpp` (`rogerPushMenuOverlay`, `rogerPushBarOverlay`)

**Interfaces:**
- Produces: `virtual void beginUiBatch() {}` / `virtual void endUiBatch() {}` on `RogerArtProvider` (the abstract base in `roger_art_provider.h`). Depth-counted; `presentBarrier()` defers while depth > 0; `endUiBatch()` flushes once. Task 3 relies on these existing.

- [ ] **Step 1: Declare the virtuals on the abstract provider**

In `engines/sci/roger/roger_art_provider.h`, directly after `virtual void uiClearAll() {}` (~line 233), add:

```cpp
	// Bracket a multi-element UI re-push (e.g. a menu dropdown: clear + window + one
	// text per row) so the per-push present barrier coalesces into ONE present at
	// endUiBatch. Without this, each push during a FROZEN cycle (blocking menu/dialog
	// loop, which never ticks kernelAnimate) flushes its own full present — a present
	// storm per menu highlight change. Depth-counted; no-op in the base provider.
	virtual void beginUiBatch() {}
	virtual void endUiBatch() {}
```

- [ ] **Step 2: Implement in FileRogerArtProvider**

In `engines/sci/roger/file_roger_art_provider.h`, in the public override section (near the other `ui*` overrides), add:

```cpp
	void beginUiBatch() override;
	void endUiBatch() override;
```

and next to `_barrierDirty` (~line 289) add the member:

```cpp
	int _uiBatchDepth = 0; // presentBarrier defers while > 0; endUiBatch flushes
```

In `engines/sci/roger/file_roger_art_provider.cpp`, directly above `uiClearToken` (~line 1901), add:

```cpp
void FileRogerArtProvider::beginUiBatch() {
	_uiBatchDepth++;
}

void FileRogerArtProvider::endUiBatch() {
	if (_uiBatchDepth > 0 && --_uiBatchDepth == 0)
		presentBarrier();
}
```

- [ ] **Step 3: Defer the barrier while batched**

In `presentBarrier()` (file_roger_art_provider.cpp:1517), directly after the `_inAnimateCycle` early-return:

```cpp
	if (_inAnimateCycle)
		return; // mid-cycle marks accumulate; the end-of-cycle call flushes them
```

add:

```cpp
	if (_uiBatchDepth > 0)
		return; // batched UI re-push: marks accumulate; endUiBatch flushes once
```

- [ ] **Step 4: Bracket the two menu push sequences**

In `engines/sci/graphics/menu.cpp`, `rogerPushMenuOverlay()` (~line 811) becomes (only the `beginUiBatch`/`endUiBatch` lines are new; everything else is the existing body, unchanged):

```cpp
void GfxMenu::rogerPushMenuOverlay() {
	if (!g_sciRogerProvider || !g_sciRogerProvider->enabled)
		return;
	const uint32 tok = 0x20000000u; // single open dropdown at a time
	// One present for the whole re-push: the frozen menu loop means every push
	// below would otherwise flush its own full present (a storm per highlight).
	g_sciRogerProvider->beginUiBatch();
	g_sciRogerProvider->uiClearToken(tok);
	// Opaque white box with a frame (matches SCI's black-bordered white dropdown).
	g_sciRogerProvider->uiPushWindow(_rogerMenuBox, _screen->getColorWhite(), 0, 0, tok);
	for (uint i = 0; i < _rogerMenuRows.size(); i++) {
		const RogerMenuRow &r = _rogerMenuRows[i];
		const bool sel = (r.id == _rogerMenuHighlight);
		const int pen = sel ? _screen->getColorWhite() : 0;
		const int back = sel ? 0 : -1; // selected row drawn inverted (white on black)
		int16 nfw = 0, nfh = 0;
		_text16->StringWidth(r.text, 0, nfw, nfh);
		g_sciRogerProvider->uiPushText(r.rect, r.text.c_str(), pen, back, 0,
		                               SCI_TEXT16_ALIGNMENT_LEFT, tok, 0 /*body*/, true,
		                               nfh, nfw);
	}
	g_sciRogerProvider->endUiBatch();
}
```

And `rogerPushBarOverlay()` (~line 422) gets the same bracket: add `g_sciRogerProvider->beginUiBatch();` on the line directly above its `g_sciRogerProvider->uiClearToken(tok);` (line ~429), and `g_sciRogerProvider->endUiBatch();` directly after the closing brace of its `for (uint i = 0; i < _rogerBarTitles.size(); i++)` push loop (line ~448), so the clear + `uiPushWindow(barRect, ...)` + per-title `uiPushText` all coalesce.

Do NOT bracket `rogerClearMenuOverlay()` — it makes exactly one provider call (`uiClearToken`), so there is nothing to coalesce.

- [ ] **Step 5: Build + rerun the evidence script (roger-loop skill)**

```powershell
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script screenshots\menu-drag-perf.rin -Diag -TimeoutSec 180
Select-String -Path screenshots\roger-run.log -Pattern 'ROGER-DIAG\[present\]|ROGER-SCRIPT'
```

Expected: drag-window `n` drops by roughly the row count multiple (each `rogerPushMenuOverlay` now = 1 present instead of rows+2), `ms` drops proportionally. Record the numbers in the Worklog next to the baseline.

- [ ] **Step 6: Visual sanity snap**

The menu must still LOOK right (dropdown draws, highlight tracks, closes clean). Rerun the existing regression scenario standalone:

```powershell
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script test\sci\roger\scripts\qfg1-menu-cycle.rin -NoBuild -TimeoutSec 180
```

Read `screenshots\roger-qfg1-menu-cycle` captures (`m-open`, `m-after`, `m-afterhover`): `m-open` must show the Information dropdown open with titles on the strip; `m-after`/`m-afterhover` must show the score banner restored and no dropdown stamp in the scene.

- [ ] **Step 7: Commit**

```powershell
git add engines/sci/roger/roger_art_provider.h engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp engines/sci/graphics/menu.cpp
git commit -F <msgfile>
```

Message first line: `SCI: ROGER: Coalesce menu overlay pushes into one present`
Body: name the storm (N+2 presents per highlight change during the frozen menu loop) and the batch-bracket fix.

---

### Task 3: Drop the redundant un-highlight re-push

**Files:**
- Modify: `engines/sci/graphics/menu.cpp` (`invertMenuSelection`, ~line 837)

**Interfaces:**
- Consumes: `_rogerMenuHighlight` (existing GfxMenu member) and `rogerPushMenuOverlay()` from Task 2's batched form.

- [ ] **Step 1: Skip the no-op push**

`interactiveWithMouse()` calls `invertMenuSelection(curItemId)` then `invertMenuSelection(newItemId)` on every row crossing. The first call currently re-pushes the whole dropdown with `_rogerMenuHighlight` unchanged (it sets it to the OLD id it already holds) — pure waste. In `invertMenuSelection` change:

```cpp
	// Roger: track the highlighted row and re-push the dropdown so the overlay's
	// selection follows the cursor (the native invert is hidden under the overlay).
	if (itemId != 0) {
		_rogerMenuHighlight = itemId;
		rogerPushMenuOverlay();
	}
```

to:

```cpp
	// Roger: track the highlighted row and re-push the dropdown so the overlay's
	// selection follows the cursor (the native invert is hidden under the overlay).
	// interactiveWithMouse inverts the OLD row then the NEW one; the old-row call
	// arrives with the highlight it already holds — skip that no-op re-push.
	if (itemId != 0 && itemId != _rogerMenuHighlight) {
		_rogerMenuHighlight = itemId;
		rogerPushMenuOverlay();
	}
```

(The native `invertRect` below stays unconditional — native semantics untouched. `drawMenu` resets `_rogerMenuHighlight = 0` before its push on every menu change, so the first highlight in a fresh dropdown still differs and pushes.)

- [ ] **Step 2: Build + rerun evidence (roger-loop skill)**

```powershell
.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script screenshots\menu-drag-perf.rin -Diag -TimeoutSec 180
Select-String -Path screenshots\roger-run.log -Pattern 'ROGER-DIAG\[present\]'
```

Expected: drag-window `n` roughly halves again vs Task 2 (one push per row crossing, not two). Record in the Worklog. Also confirm visually with the Task 2 Step 6 menu-cycle rerun that the highlight still follows the cursor down the dropdown (compare `m-open`).

- [ ] **Step 3: Commit**

```powershell
git add engines/sci/graphics/menu.cpp
git commit -F <msgfile>
```

Message first line: `SCI: ROGER: Skip no-op menu highlight re-push`

---

### Task 4: Regression gate + wrap-up

**Files:**
- None new (verification + Worklog only).

- [ ] **Step 1: Unit tests still build and pass**

```powershell
.\build_tests.ps1
```

Expected: same test count as before this plan (no test files added — the changed code is SCI-linked and not unit-testable; the `.rin` evidence runs are the tests here), all PASS.

- [ ] **Step 2: Present-path regression gate**

The batch guard touches `presentBarrier` — the gate is mandatory:

```powershell
powershell -File test\sci\roger\run-regression.ps1
```

Expected: **41/42 green** — `qfg1-menu-cycle presence:m-after` fails deterministically with "0 differing px" (KNOWN-STALE since dcaa7e2625a, manifest-region drift; do NOT debug it as a regression). Any OTHER failure: inspect the failing entry's capture PNGs BEFORE rerunning (the gate drives a live focus-stealing window; environmental contamination is a known false positive).

- [ ] **Step 3: Final Worklog entry + user handoff**

Append to the Worklog: baseline vs final `n`/`ms` for idle and drag windows, gate result, and the one thing scripted runs cannot prove — **subjective mouse feel**. Per the Phase-2 lesson (invalidation changes are verified by interactive soak, not the gate), ask the user to open a menu and drag across items to confirm the cursor now tracks smoothly. Do not merge/declare done before that soak.

- [ ] **Step 4: Report**

Summarize to the user: the diagnosed storm (presents per row crossing before), the two fixes, the after numbers, gate status, and the soak request.

---

## Worklog

### Task 1 baseline — 2026-07-08

Run: `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script screenshots\menu-drag-perf.rin -Diag -TimeoutSec 180`
Exit code: 0 (script completed cleanly).

#### (a) Idle mouse-wiggle window (between `baseline-idle-start` and `menu-drag-start`)

```
ROGER-DIAG[present]: n=5 ms=43 window=1212
```

5 presents over ~1.2 s → ~4/s; 43 ms total → ~35 ms/s of present cost. Cheap cursor-only path.

#### (b) Menu-drag windows (between `menu-drag-start` and `menu-drag-end`)

```
ROGER-DIAG[present]: n=27 ms=515  window=1004   ← first full second in drag
ROGER-DIAG[present]: n=34 ms=910  window=1004   ← peak
ROGER-DIAG[present]: n=34 ms=858  window=1018
ROGER-DIAG[present]: n=33 ms=888  window=1003
```

~27–34 presents/second; 515–910 ms of present cost per 1000 ms wall-clock second (51–91% of wall time consumed by overlay presents during menu drag). Storm confirmed — H1 stands, proceed to Task 2.

### Task 2 after (batch bracket) — 2026-07-08

Run: `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script screenshots\menu-drag-perf.rin -Diag -TimeoutSec 180`
Exit code: 0 (script completed cleanly).

#### (a) Idle window (unchanged from baseline)

```
ROGER-DIAG[present]: n=5 ms=53  window=3295
ROGER-DIAG[present]: n=4 ms=48  window=1070
ROGER-DIAG[present]: n=4 ms=57  window=1716
```

4–5 presents/s, 48–57 ms total — identical to baseline. Batch guard has zero cost on the warm animate path.

#### (b) Menu-drag windows (after batch bracket)

```
ROGER-DIAG[present]: n=13 ms=201 window=1034   ← first full second in drag
ROGER-DIAG[present]: n=12 ms=395 window=1042
ROGER-DIAG[present]: n=11 ms=331 window=1025
ROGER-DIAG[present]: n=10 ms=321 window=1005   ← last window
```

Baseline peak: `n=34 ms=910 window=1004`. After: `n=10–13 ms=201–395 window=~1025`.
Count dropped ~3× (34→10–13); ms dropped ~2.3–4.5× (910→201–395); wall fraction dropped from ~91% to ~20–38%.
Remaining presents are per-move cursor updates outside the batch — expected and correct; these are the `onMouseMoved → presentBarrier` calls that fire between row crossings.

#### (c) Visual sanity — qfg1-menu-cycle regression script

Captures (`roger-300-m-open-overlay.png`, `roger-300-m-after-overlay.png`, `roger-300-m-afterhover-overlay.png`):

- **m-open**: Information dropdown fully drawn — white box with frame, all 5 items (Inventory, Char Sheet, Time/Day, Ask about, Look at) visible; menu titles (File / Game / Action / Information) on the bar strip. PASS.
- **m-after**: Dropdown dismissed; "Quest for Glory I  [score 1 of 500]" score banner restored on the top strip; no dropdown remnant stamped in the scene. PASS.
- **m-afterhover**: Same clean state as m-after; score banner still showing, no stale overlay fragment. PASS.
