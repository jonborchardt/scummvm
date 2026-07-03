# Roger Phase 1: Present Barrier + Exact Invalidation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Roger's ~16 scattered `presentWithUi()` call sites with one gated `presentBarrier()`, make invalidation exact (rects SCI itself reports, added unconditionally), and patch the composite cache region-bounded instead of full-frame — killing the vacated-geometry and present-storm bug classes structurally (spec §5 Phase 1).

**Architecture:** All new code lives in `engines/sci/roger/` — zero edits under `engines/sci/graphics/` (the existing hooks already deliver every rect Phase 1 needs). The barrier is the only function that pushes to the overlay; it defers while the animate cycle is mid-draw (flag set at the existing `snapshotNativeBaseline` hook, cleared at the end of `renderFromAnimateList`) and presents synchronously at the blocking seams (uiPush*/uiClear*/erase/mouse). UI changes mark dirty rects via shared pure helpers; the composite cache is patched only inside the dirty union (elements re-rendered whole via a fixpoint region expansion + window-token-group closure, so `renderUiLayer`'s element logic is untouched).

**Tech Stack:** C++11 (no exceptions/RTTI, tabs, K&R), CxxTest for pure helpers, MSVC build via `.\build_and_run.ps1 -NoLaunch`, the Phase 0 regression gate `powershell -File test\sci\roger\run-regression.ps1` as the per-task acceptance test.

## Global Constraints

- **Containment (spec §2, hard gate):** all changes in `engines/sci/roger/` only. This plan requires **zero** edits under `engines/sci/graphics/`, `common/`, top-level `graphics/`, `backends/`, `base/`, `gui/`. Verify per task: `git diff --stat <base>` shows only `engines/sci/roger/**` and `test/**` (+docs at the end). A task that needs an engine-side edit STOPS and escalates (spec: "comes back to the spec").
- **Branch policy (spec §6):** Phase 1 is one commit series on its own branch `jon-p1-barrier` (created off `jon-refactor1`). No runtime knob — rollback is `git revert`/branch abandonment.
- **Perf thresholds (spec §7, enforced by the gate):** `ROGER-CYCLE` period median ≤ baseline × 1.05; busy median ≤ baseline + 1 ms; p90 period ≤ baseline p90 × 1.10. Phase 0 recorded baselines: qfg1-walk-perf median=83 p90=84 busy=17; sq3-walk-perf median=83 p90=84 busy=4 (`test/sci/roger/baselines/perf-baseline.json`). Known flake: sq3 busy varies 4–8 ms — if the busy check alone is red, re-run once before diagnosing (see Phase 0 plan addendum); never re-record baselines in this phase.
- **Dirty-area watch-item (spec Phase 1):** walking dirty-union area per cycle must not grow > 10 % vs the Task 1 baseline numbers.
- **Dialog same-cycle paint:** `qfg1-dialog-cycle`'s `presenceDiff` checks gate this; a deferred first paint of a Print window is a phase-stopping failure.
- **The `.rin` capture contract** ("capture pends; a present consumes it; flush with a `move`") must keep working — the gate's capture-based checks verify it implicitly.
- **Build:** `.\build_and_run.ps1 -NoLaunch` (incremental MSVC, ~1–3 min). **Gate:** `powershell -File test\sci\roger\run-regression.ps1` — 7 entries, 33 checks, ~10 min, exit 0 required. Give gate tool calls a 600000 ms timeout.
- **CxxTest** tests go in `test/sci/roger/` following the existing `test_compositor.h` pattern. They execute only under a make build (`make test`); in this MSVC environment write them, keep them pure-logic, and verify the production code compiles. The regression gate is the executable verification.
- Commits on `jon-p1-barrier`; `docs/superpowers/**` needs `git add -f` (gitignored by design).

## Code facts (verified 2026-07-02, for every implementer)

- The 16 `presentWithUi()` call sites in `file_roger_art_provider.cpp`: uiPushWindow:1333, uiPushText:1351, uiPushButton:1367, uiPushTextEdit:1385, uiPushIcon:1406, onDrawCel:1450, uiPushStatus:1486, uiClearToken:1567, onNativeEraseRect:1600, uiClearAll:1610, uiPushFrameBox:1648, toggleOverlay:2214+2219, cycleBodyFont:2271, regenInPlace:2311, onMouseMoved:2442 (slow fallback). Line numbers drift as tasks land — match by function.
- `renderFrame` (file_roger_art_provider.cpp:573) is the per-cycle compose+present; `renderFromAnimateList` (…:2054) calls it last.
- `GfxPorts::removeWindow` calls `uiClearToken(0x40000000|id)` and `uiClearToken(0x60000000|id)` **before** `bitsRestore(pWnd->hSaved1)` — so the erase rect arrives *after* the token clear.
- `GfxPaint16::bitsRestore` fires `uiClearToken(handle-token)` + `uiClearToken(0x50000000u)` + `onNativeEraseRect(saved rect)` — ~2×/moving-sprite/**every** cycle while walking (the `bb65c56b75a` storm path).
- `kernelGraphRedrawBox` already routes to `onNativeEraseRect` (paint16.cpp:573) — no engine change needed for §3.1.
- `snapshotNativeBaseline()` is invoked from `kernelAnimate` every full cycle (animate.cpp:758), before `restoreAndDelete`; `renderFromAnimateList` runs after (animate.cpp:765) and also from `reAnimate` (animate.cpp:613, outside the cycle).
- `RogerCompositor::presentToOverlay` already pushes only `dirtyUnion(...)` (or full on `_bgRebuilt`/heal/no-dirty-present); `dirtyUnion` = `_dirtyCur ∪ _dirtyPrev ∪ _sceneDirtyCur ∪ _sceneDirtyPrev` coalesced.
- `renderScene` pushes **every** sprite's dest rect into `_sceneDirtyCur` each frame (roger_compositor.cpp:377) — so in any room with a visible cast, the union is non-empty every cycle.
- `renderUiLayer(dest, elems, palette, gameRect, text, altText)` accepts any element array; `text` may be null (fills only — that's how tests exercise it). Its kUiWindow border logic unions the rects of **other elements sharing the window's token** — any subset passed to it must be closed over token groups.
- `sciRectToDest(nr, gameRect)` (roger_coords.h:93) maps native 320×200 → overlay. The 07-02 shipped vacated-dirty math in uiClearToken is `n.grow(2)` native then `sciRectToDest` then `d.grow(2)` overlay.
- `InputScriptDriver` (roger_input.h) has `takeCaptureRequest(label)` (consuming) and a private `_capturePending`; no const peek yet.
- `compositeCursor` (file_roger_art_provider.cpp:1023) computes the cursor dest rect inline from the mouse position, paints, `addDirtyRect(dst)`, updates `_lastCursorDstRect`.
- `Common::Rect::grow(int)` grows in place and returns void — never chain it.

---

### Task 1: Phase branch + present/dirty-area telemetry + baseline numbers

**Files:**
- Modify: `engines/sci/roger/roger_compositor.h` (add `_presentLog` + setter; log in presentToOverlay)
- Modify: `engines/sci/roger/roger_compositor.cpp`
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (wire the flag from `_cycleLog`)

**Interfaces:**
- Consumes: existing `-CycleLog` plumbing (`_cycleLog` in the provider, set in its constructor).
- Produces: log line `ROGER-PRESENT full=<0|1> regions=<n> area=<px>` emitted once per `presentToOverlay` when present-logging is on. Task 6 compares its per-cycle area statistics against the numbers this task records.

- [ ] **Step 1: Create the phase branch**

```bash
git checkout jon-refactor1 && git pull --ff-only 2>/dev/null; git checkout -b jon-p1-barrier
```

- [ ] **Step 2: Add the telemetry**

In `roger_compositor.h`, next to `void setDiag(bool on)` add:

```cpp
	// -CycleLog: one ROGER-PRESENT line per present (full flag, region count, pushed
	// area in overlay px). Off by default; the Phase 1 dirty-area gate reads it.
	void setPresentLog(bool on) { _presentLog = on; }
```

and next to `bool _diag = false;` add:

```cpp
	bool _presentLog = false; // ROGER-PRESENT per-present telemetry (perf-gate instrumentation)
```

In `roger_compositor.cpp`, in `presentToOverlay`, immediately after the existing `if (_diag) warning("ROGER-DIAG[present]: ...")` block (after the push loop, before `g_system->showOverlay(false)`):

```cpp
	if (_presentLog) {
		uint32 area = 0;
		for (uint i = 0; i < push.size(); i++)
			area += (uint32)push[i].width() * (uint32)push[i].height();
		warning("ROGER-PRESENT full=%d regions=%u area=%u", full ? 1 : 0, (unsigned)push.size(), area);
	}
```

In `file_roger_art_provider.cpp`, in the constructor, find where `_compositor` is created (`_compositor = new Roger::RogerCompositor()`), and after the existing `_compositor->setDiag(...)`-style setup (or directly after construction if `_cycleLog` is already assigned by that point — check the constructor order; if `_cycleLog` is set *later* in the ctor, place this line after that assignment instead):

```cpp
	_compositor->setPresentLog(_cycleLog);
```

- [ ] **Step 3: Build**

Run: `.\build_and_run.ps1 -NoLaunch`
Expected: build succeeds.

- [ ] **Step 4: Record the pre-refactor dirty-area baseline**

Run both perf scripts with telemetry:

```powershell
.\build_and_run.ps1 -NoBuild -Game qfg1 -SaveSlot 1 -Script test\sci\roger\scripts\qfg1-walk-perf.rin -TimeoutSec 90 -CycleLog
```

then compute (and record) the numbers:

```powershell
$areas = Select-String screenshots\roger-run.log -Pattern 'ROGER-PRESENT full=\d regions=\d+ area=(\d+)' |
    ForEach-Object { [double]$_.Matches[0].Groups[1].Value }
$cycles = (Select-String screenshots\roger-run.log -Pattern 'ROGER-CYCLE ').Count
$sorted = @($areas | Sort-Object)
"presents=$($areas.Count) cycles=$cycles presentsPerCycle=$([math]::Round($areas.Count/[math]::Max(1,$cycles),2)) medianArea=$($sorted[[int][math]::Floor($sorted.Count/2)])"
```

Repeat with `-Game sq3-1 ... -Script test\sci\roger\scripts\sq3-walk-perf.rin`. Paste both result lines into this plan's Addendum under "Task 1 dirty-area baseline". These are the ≤ +10 % reference for Task 6.

- [ ] **Step 5: Run the full gate (proves telemetry changed nothing)**

Run: `powershell -File test\sci\roger\run-regression.ps1`
Expected: ALL PASS (33 checks), exit 0.

- [ ] **Step 6: Commit**

```bash
git add engines/sci/roger/roger_compositor.h engines/sci/roger/roger_compositor.cpp engines/sci/roger/file_roger_art_provider.cpp
git commit -m "Roger p1: ROGER-PRESENT dirty-area telemetry under -CycleLog"
```

---

### Task 2: presentBarrier skeleton + mark helpers + all 16 call-site conversions (semantics-preserving)

The barrier this task ships still performs today's full `presentWithUi()` internally — behavior is preserved except (a) presents are *gated* (skip when nothing changed), and (b) marks made mid-animate-cycle are deferred to the end-of-cycle barrier. The region-bounded compose comes in Tasks 3–4.

**Files:**
- Modify: `engines/sci/roger/roger_coords.h` (pure extent helpers)
- Modify: `engines/sci/roger/roger_input.h` (const capture peek)
- Modify: `engines/sci/roger/roger_compositor.h` (`hasPendingDirty()`)
- Modify: `engines/sci/roger/file_roger_art_provider.h`
- Modify: `engines/sci/roger/file_roger_art_provider.cpp`
- Test: `test/sci/roger/test_present_barrier.h` (new)

**Interfaces:**
- Produces (consumed by Tasks 3–5):
  - `Roger::uiPaintExtent(const Common::Rect &nr, const Common::Rect &gameRect)` → `Common::Rect` — full overlay paint extent of a UI element at native rect `nr` (native grow 2 for the compositor's window-fill overdraw, then `sciRectToDest`, then overlay grow 2 for TTF overshoot; exactly the 07-02 shipped math).
  - `Roger::uiVacatedExtent(const Common::Rect &nr, const Common::Rect &gameRect)` → `Common::Rect` — removal extent: `sciRectToDest(nr)` grown 2 overlay px only (no native grow). **Defined now, wired in Task 5.**
  - Provider: `void presentBarrier()`, `void markUiDirty(const Common::Rect &nativeRect)`, `void markVacatedDirty(const Common::Rect &nativeRect)`, `void markNativeDirty(const Common::Rect &nativeRect)`, `void markFullDirty()`, `Common::Rect cursorDstRect(const Common::Rect &gameRect)`; members `bool _barrierDirty`, `bool _inAnimateCycle`, `bool _frameJustComposed` (declared now, used in Task 4).
  - `RogerCompositor::hasPendingDirty() const` → bool.
  - `InputScriptDriver::capturePending() const` → bool.

- [ ] **Step 1: Write the CxxTest for the extent helpers**

Create `test/sci/roger/test_present_barrier.h` (mirror the include style of `test/sci/roger/test_roger_coords.h` — check its exact includes first and match them):

```cpp
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_coords.h"

using namespace Sci;

class PresentBarrierExtentTestSuite : public CxxTest::TestSuite {
public:
	// gameRect chosen so 1 native px = 9x9 overlay px exactly (320*9=2880, 200*9=1800).
	Common::Rect gr() const { return Common::Rect(0, 0, 2880, 1800); }

	void test_paint_extent_covers_shipped_overdraw_math() {
		const Common::Rect nr(10, 10, 20, 20);
		Common::Rect grown = nr;
		grown.grow(2);
		Common::Rect expected = Roger::sciRectToDest(grown, gr());
		expected.grow(2);
		TS_ASSERT_EQUALS(Roger::uiPaintExtent(nr, gr()), expected);
	}

	void test_paint_extent_strictly_contains_vacated_extent() {
		const Common::Rect nr(50, 40, 90, 60);
		const Common::Rect paint = Roger::uiPaintExtent(nr, gr());
		const Common::Rect vac = Roger::uiVacatedExtent(nr, gr());
		TS_ASSERT(paint.contains(vac));
		TS_ASSERT(paint.width() > vac.width());
	}

	void test_vacated_extent_is_exact_plus_ttf_pad() {
		const Common::Rect nr(50, 40, 90, 60);
		Common::Rect expected = Roger::sciRectToDest(nr, gr());
		expected.grow(2);
		TS_ASSERT_EQUALS(Roger::uiVacatedExtent(nr, gr()), expected);
	}
};
```

- [ ] **Step 2: Add the pure helpers to roger_coords.h**

After `sciRectToDest`:

```cpp
/**
 * Full overlay paint extent of a UI element pushed at native rect `nr`: the
 * compositor paints kUiWindow fills at nr.grow(2) native px, and TTF glyphs can
 * overshoot the native box by ~2 overlay px. Marks made with this cover every
 * pixel the element's draw can touch (the 2026-07-02 shipped overdraw math).
 */
inline Common::Rect uiPaintExtent(const Common::Rect &nr, const Common::Rect &gameRect) {
	Common::Rect n = nr;
	n.grow(2);
	Common::Rect d = sciRectToDest(n, gameRect);
	d.grow(2);
	return d;
}

/**
 * Overlay extent a REMOVED element must invalidate: exact native rect + the TTF
 * overshoot pad only. The compositor-overdraw ring beyond it is covered by
 * bitsRestore's exact erase rect once §3.1 exact invalidation is wired (Task 5
 * switches removal marks to this; the gate proves the coverage claim).
 */
inline Common::Rect uiVacatedExtent(const Common::Rect &nr, const Common::Rect &gameRect) {
	Common::Rect d = sciRectToDest(nr, gameRect);
	d.grow(2);
	return d;
}
```

- [ ] **Step 3: Add the small accessors**

`roger_input.h`, next to `takeCaptureRequest`:

```cpp
	// Non-consuming peek: a `capture` command has executed and its dump is still
	// pending. The present barrier must not skip a present while this is true.
	bool capturePending() const { return _capturePending; }
```

`roger_compositor.h`, next to `dirtyUnion`:

```cpp
	// True when any dirty accumulator is non-empty — i.e. the next present would
	// push at least one region. The present barrier's O(1) skip gate reads this.
	bool hasPendingDirty() const {
		return !_dirtyCur.empty() || !_dirtyPrev.empty() ||
		       !_sceneDirtyCur.empty() || !_sceneDirtyPrev.empty();
	}
```

- [ ] **Step 4: Add the barrier + marks to the provider**

`file_roger_art_provider.h`, in the private section near `presentWithUi()`:

```cpp
	// ── Present barrier (spec §3.2) ────────────────────────────────────────────
	// The ONLY entry point that pushes to the overlay outside transitions. O(1)
	// when nothing changed. Defers while the animate cycle is mid-draw
	// (_inAnimateCycle) — the end-of-renderFromAnimateList call flushes.
	void presentBarrier();
	void markUiDirty(const Common::Rect &nativeRect);      // element pushed/redrawn at nr
	void markVacatedDirty(const Common::Rect &nativeRect); // element removed at nr
	void markNativeDirty(const Common::Rect &nativeRect);  // §3.1: exact rect SCI touched
	void markFullDirty();                                  // room/F10/font/plate change
	// Overlay-space rect the cursor would occupy right now (empty when not drawable).
	// Extracted from compositeCursor so the barrier can detect cursor movement.
	Common::Rect cursorDstRect(const Common::Rect &gameRect);
	bool _barrierDirty = false;      // any mark since the last barrier present
	bool _inAnimateCycle = false;    // set at snapshotNativeBaseline, cleared at cycle end
	bool _frameJustComposed = false; // renderFrame composed this cycle (Task 4 uses it)
```

`file_roger_art_provider.cpp` — add the implementations (near `presentWithUi`):

```cpp
void FileRogerArtProvider::markUiDirty(const Common::Rect &nativeRect) {
	if (!overlayShown() || !_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	_compositor->addDirtyRect(Roger::uiPaintExtent(nativeRect, _lastGameRect));
}

void FileRogerArtProvider::markVacatedDirty(const Common::Rect &nativeRect) {
	// Task 2..4: same coverage as a push (status quo). Task 5 switches the body to
	// uiVacatedExtent once §3.1 exact erase rects are wired.
	markUiDirty(nativeRect);
}

void FileRogerArtProvider::markNativeDirty(const Common::Rect &nativeRect) {
	// §3.1 exact invalidation: SCI touched these native pixels. grow(1) native
	// absorbs integer-scaler rounding differences vs the sprite-path mapper.
	// O(1) accumulate; NEVER presents.
	if (!overlayShown() || !_compositor || nativeRect.isEmpty())
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	Common::Rect n = nativeRect;
	n.grow(1);
	_compositor->addDirtyRect(Roger::sciRectToDest(n, _lastGameRect));
}

void FileRogerArtProvider::markFullDirty() {
	_barrierDirty = true;
	_compositeCacheValid = false;
	if (_compositor)
		_compositor->forceFullPresent();
}

Common::Rect FileRogerArtProvider::cursorDstRect(const Common::Rect &gameRect) {
	if (!enabled || _useHwCursor || !_cursorVisible || gameRect.isEmpty())
		return Common::Rect();
	ensureCursor();
	if (!_cursorSurf)
		return Common::Rect();
	const Common::Point mp = g_system->getEventManager()->getMousePos();
	const int ox = gameRect.left + mp.x * gameRect.width() / 320;
	const int oy = gameRect.top + mp.y * gameRect.height() / 200;
	return Common::Rect(ox - _cursorHotspot.x, oy - _cursorHotspot.y,
	                    ox - _cursorHotspot.x + _cursorSurf->w,
	                    oy - _cursorHotspot.y + _cursorSurf->h);
}

void FileRogerArtProvider::presentBarrier() {
	// The single gated present (spec §3.2). Every skip path below is O(1).
	if (_inAnimateCycle)
		return; // mid-cycle marks accumulate; the end-of-cycle call flushes them
	if (!overlayShown() || !_compositor || !_haveScene || !_sceneCache)
		return;
	const bool capture = _inputDriver && _inputDriver->capturePending();
	const bool cursorMoved = cursorDstRect(_lastGameRect) != _lastCursorDstRect;
	if (!_barrierDirty && !_compositor->hasPendingDirty() && !cursorMoved && !capture)
		return;
	_barrierDirty = false;
	presentWithUi();
}
```

Refactor `compositeCursor` to reuse the extraction (replace its inline dst math):

```cpp
void FileRogerArtProvider::compositeCursor(Graphics::ManagedSurface &scene,
                                           const Common::Rect &gameRect) {
	// The native OS cursor is invisible over the OSystem overlay, so draw our own
	// arrow into the overlay scene at the mouse position.
	const Common::Rect dst = cursorDstRect(gameRect);
	if (dst.isEmpty())
		return;
	scene.blendBlitFrom(*_cursorSurf, Common::Rect(0, 0, _cursorSurf->w, _cursorSurf->h), dst);
	if (_compositor)
		_compositor->addDirtyRect(dst); // cursor moved here this frame (dirty-rect present)
	_lastCursorDstRect = dst; // fast path uses this to restore the old cursor region
}
```

- [ ] **Step 5: Set/clear the animate-cycle flag (provider methods only — no engine edits)**

Top of `FileRogerArtProvider::snapshotNativeBaseline()` (BEFORE its early returns — the flag must be set even when the snapshot itself is skipped):

```cpp
	// kernelAnimate is mid-cycle from this hook until renderFromAnimateList runs.
	// While it is, blocking-seam barrier calls defer (the cycle's own tail call
	// flushes them) — this is what makes a bitsRestore storm structurally unable
	// to present per-hook (the bb65c56b75a class).
	_inAnimateCycle = true;
```

End of `FileRogerArtProvider::renderFromAnimateList()` (after `renderFrame(merged);` and the native-surface free loop):

```cpp
	_inAnimateCycle = false; // cycle draw complete — reopen the barrier
```

**Deliberately NO `presentBarrier()` call at the tail in this task.** `renderFrame` still presents here (its `presentToOverlay` pushes the dirtyUnion, which includes any marks deferred mid-cycle, and its compose re-renders the full UI layer — so deferred removals paint correctly the same cycle). Adding a tail barrier call now would run a second full recompose every cycle and regress walking; Task 4 adds it together with the fresh-frame branch that makes it cheap. So that the barrier's gate doesn't see stale state after the cycle present, add at the very end of `renderFrame` (after `maybeScriptCapture(scene, gameRect);`, before the `_diffCheck` block):

```cpp
	_barrierDirty = false; // this cycle's present flushed all accumulated marks
```

- [ ] **Step 6: Convert all 16 call sites**

Every `presentWithUi()` call site changes to marks + `presentBarrier()`. `_compositeCacheValid = false` lines **stay** in this task (Task 4 removes them). Exact edits, by function:

1. `uiPushWindow` — before `presentWithUi();` insert `markUiDirty(r);`; then replace `presentWithUi();` → `presentBarrier();`
2. `uiPushText` — same: `markUiDirty(r); presentBarrier();`
3. `uiPushButton` — same.
4. `uiPushTextEdit` — same.
5. `uiPushIcon` — same.
6. `onDrawCel` — same (`markUiDirty(r);`).
7. `uiPushStatus` — same (`markUiDirty(r);` — the internal `clearToken` replaces at the same rect, so one mark covers push and vacate).
8. `uiClearToken` — the dirty loop over `removedRects` (the `n.grow(2)` / `sciRectToDest` / `d.grow(2)` block) is replaced wholesale by:

```cpp
	// Dirty the overlay regions the removed elements occupied so the barrier's
	// present repaints them with clean background (else they ghost until another
	// draw touches them). markVacatedDirty centralizes the extent math.
	for (uint i = 0; i < removedRects.size(); i++)
		markVacatedDirty(removedRects[i]);
```

and the tail `if (overlayShown() && _plate) presentWithUi();` → `presentBarrier();`. The `if (!removedUi && !removedStamps) return;` early-out stays exactly where it is (the walking-storm no-match path must remain mark-free and present-free).

9. `onNativeEraseRect` — the `droppedRects` dirty loop becomes `markVacatedDirty(droppedRects[i]);` (delete the inline `sciRectToDest`/`grow` lines); tail `presentWithUi(); // UI-only present...` → `presentBarrier();`
10. `uiClearAll` — `if (overlayShown() && _plate) presentWithUi();` → `markFullDirty(); presentBarrier();` (clearing every element at once — a full present is the honest cover).
11. `uiPushFrameBox` — **two** marks: the frame's old position must be dirtied when it moves (today the full recompose hides this; the bounded compose in Task 4 will not). In the existing scan loop that finds the current `FRAME_BOX_TOKEN` element, capture its rect before `break`:

```cpp
	Common::Rect oldFrameRect; // empty when no existing frame element
	for (uint i = 0; i < elems.size(); i++) {
		if (elems[i].token == FRAME_BOX_TOKEN) {
			if (elems[i].nativeRect == r && elems[i].penColor == penColor)
				return; // identical — nothing to do
			oldFrameRect = elems[i].nativeRect;
			break; // found but different — fall through to update
		}
	}
```

then before the tail: `if (!oldFrameRect.isEmpty()) markVacatedDirty(oldFrameRect);` and `markUiDirty(r);`, and `presentWithUi();` → `presentBarrier();`

12. `toggleOverlay` (both branches) — `if (_haveScene) presentWithUi();` → `if (_haveScene) { markFullDirty(); presentBarrier(); }` (the existing `forceFullPresent()` lines just above become redundant but are harmless — remove them, `markFullDirty` does it).
13. `cycleBodyFont` — `if (_haveScene) presentWithUi();` → `if (_haveScene) { markFullDirty(); presentBarrier(); }`
14. `regenInPlace` — `presentWithUi();` → `markFullDirty(); presentBarrier();`
15. `onMouseMoved` — keep the sbs branch and the composite-cache fast path exactly as they are (Task 4 subsumes the fast path); change only the last line `presentWithUi();` → `presentBarrier();`

- [ ] **Step 7: Build + gate**

Run: `.\build_and_run.ps1 -NoLaunch` then `powershell -File test\sci\roger\run-regression.ps1`
Expected: build clean; ALL PASS (33 checks), exit 0. Pay attention to `qfg1-dialog-cycle` (same-cycle paint through the new barrier) and both perf entries (the deferral must not change walking telemetry).

- [ ] **Step 8: Commit**

```bash
git add engines/sci/roger/roger_coords.h engines/sci/roger/roger_input.h engines/sci/roger/roger_compositor.h engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp test/sci/roger/test_present_barrier.h
git commit -m "Roger p1: presentBarrier + mark helpers; all 16 present call sites converted"
```

---

### Task 3: Region-expansion helpers + patchCompositeRegions (dormant)

Pure compositor machinery for §3.3, fully unit-tested, not yet called by the provider — zero behavior change this task.

**Files:**
- Modify: `engines/sci/roger/roger_compositor.h`
- Modify: `engines/sci/roger/roger_compositor.cpp`
- Test: `test/sci/roger/test_present_barrier.h` (extend)

**Interfaces:**
- Consumes: `Roger::uiPaintExtent` (Task 2), `renderUiLayer`, `coalesceDirtyRects`.
- Produces (Task 4 wires them):
  - `void expandRegionsToElements(Common::Array<Common::Rect> &regions, const Common::Array<UiElement> &elems, const Common::Rect &gameRect, const Common::Rect &bounds, Common::Array<uint> &outElemIndices)` — free function in namespace `Sci::Roger`. Grows `regions` (coalesced, clamped to `bounds`) to a fixpoint over every element whose paint extent intersects them, **closed over window-token groups**, and returns the indices (ascending, original order) of the elements to re-render.
  - `void RogerCompositor::patchCompositeRegions(Graphics::ManagedSurface &composite, Graphics::ManagedSurface &sceneNoUi, const Common::Array<UiElement> &elems, const Common::Array<Common::Rect> &regions, const byte *palette, const Common::Rect &gameRect, const RogerTextRenderer *text, const RogerTextRenderer *altText)` — patches `composite` so that, inside the expanded regions, it is byte-identical to a full `sceneNoUi` + `renderUiLayer(all elems)` recompose; pixels outside the expanded regions are untouched. (`sceneNoUi` is a non-const ref only because `ManagedSurface::surfacePtr()` is non-const; it is never written.)
  - `bool RogerCompositor::nextPresentIsFull() const` — true when the next `presentToOverlay` will take its full-present branch (dirty-present off, `_bgRebuilt`, empty `_bgGameRect`, or the periodic heal is due). The provider's bounded present path must fall back to the full compose when this is true — a full present reads the whole source surface, and the bounded path only guarantees the pushed regions are valid.

- [ ] **Step 1: Write the failing tests**

Append to `test/sci/roger/test_present_barrier.h` (add the needed includes at the top: `sci/roger/roger_compositor.h`, `sci/roger/roger_ui_layer.h`, `graphics/managed_surface.h`):

```cpp
class RegionExpansionTestSuite : public CxxTest::TestSuite {
	Roger::UiElement mkWindow(int l, int t, int r, int b, uint32 tok) {
		Roger::UiElement e;
		e.type = Roger::kUiWindow; e.nativeRect = Common::Rect(l, t, r, b);
		e.backColor = 15; e.penColor = 0; e.hasFrame = true; e.token = tok;
		return e;
	}
public:
	Common::Rect gr() const { return Common::Rect(0, 0, 2880, 1800); }
	Common::Rect bounds() const { return Common::Rect(0, 0, 2880, 1800); }

	void test_no_intersection_returns_inputs() {
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(200, 100, 250, 130, 0x40000001u)); // far from region
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(0, 0, 90, 90)); // native (10,10) area in overlay px
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 0u);
		TS_ASSERT_EQUALS(regions.size(), 1u);
	}

	void test_intersecting_element_expands_region_to_its_extent() {
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(5, 5, 40, 40, 0x40000001u));
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(80, 80, 100, 100)); // overlaps the window's overlay extent
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 1u);
		const Common::Rect want = Roger::uiPaintExtent(Common::Rect(5, 5, 40, 40), gr());
		bool covered = false;
		for (uint i = 0; i < regions.size(); i++)
			if (regions[i].contains(want)) covered = true;
		TS_ASSERT(covered);
	}

	void test_token_group_closure_pulls_in_far_member() {
		// Window A (token T) intersects the region; text B shares T but sits far away.
		// Closure must include B and grow the regions over B's extent too, so the
		// window-border content-union logic sees the whole group.
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(5, 5, 40, 40, 0x40000007u));
		Roger::UiElement b = mkWindow(200, 150, 240, 170, 0x40000007u);
		b.type = Roger::kUiText; b.text = "hi";
		elems.push_back(b);
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(80, 80, 100, 100));
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 2u);
		const Common::Rect wantB = Roger::uiPaintExtent(Common::Rect(200, 150, 240, 170), gr());
		bool covered = false;
		for (uint i = 0; i < regions.size(); i++)
			if (regions[i].contains(wantB)) covered = true;
		TS_ASSERT(covered);
	}

	void test_chain_expansion_reaches_fixpoint() {
		// A intersects the region; B intersects only A's extent; C intersects only B's.
		// All three must be selected.
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(10, 10, 30, 30, 0x40000001u));
		elems.push_back(mkWindow(31, 10, 50, 30, 0x40000002u));
		elems.push_back(mkWindow(51, 10, 70, 30, 0x40000003u));
		Common::Array<Common::Rect> regions;
		regions.push_back(Roger::uiPaintExtent(Common::Rect(10, 10, 12, 12), gr()));
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 3u);
	}
};

class PatchCompositeTestSuite : public CxxTest::TestSuite {
public:
	// patch == full recompose inside the expanded regions; untouched outside.
	void test_patch_matches_full_recompose_inside_and_preserves_outside() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		const int W = 320, H = 200; // small overlay for the test
		Graphics::ManagedSurface sceneNoUi(W, H, rgba);
		sceneNoUi.clear(rgba.ARGBToColor(255, 0, 0, 80)); // dark blue "scene"
		byte pal[256 * 3];
		for (int i = 0; i < 256; i++) { pal[i * 3] = (byte)i; pal[i * 3 + 1] = (byte)i; pal[i * 3 + 2] = (byte)i; }
		const Common::Rect gameRect(0, 0, W, H); // 1:1 native->overlay for simplicity

		Common::Array<Roger::UiElement> elems;
		Roger::UiElement w;
		w.type = Roger::kUiWindow; w.nativeRect = Common::Rect(40, 40, 120, 90);
		w.backColor = 15; w.penColor = 0; w.hasFrame = true; w.token = 0x40000001u;
		elems.push_back(w);

		Roger::RogerCompositor comp;

		// Reference: full recompose.
		Graphics::ManagedSurface full(W, H, rgba);
		full.copyFrom(sceneNoUi);
		comp.renderUiLayer(full, elems, pal, gameRect, nullptr, nullptr);

		// Composite under test: stale sentinel everywhere, then patch one region
		// overlapping the window.
		Graphics::ManagedSurface patched(W, H, rgba);
		patched.clear(rgba.ARGBToColor(255, 255, 0, 0)); // red sentinel = "stale"
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(30, 30, 70, 70));
		comp.patchCompositeRegions(patched, sceneNoUi, elems, regions, pal, gameRect, nullptr, nullptr);

		// Expanded regions cover the window's full extent; inside them the patch
		// must equal the full recompose.
		Common::Array<Common::Rect> expanded;
		expanded.push_back(Common::Rect(30, 30, 70, 70));
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(expanded, elems, gameRect, Common::Rect(0, 0, W, H), idx);
		for (uint r = 0; r < expanded.size(); r++)
			for (int y = expanded[r].top; y < expanded[r].bottom; y++)
				for (int x = expanded[r].left; x < expanded[r].right; x++)
					TS_ASSERT_EQUALS(patched.surfacePtr()->getPixel(x, y), full.surfacePtr()->getPixel(x, y));

		// A pixel far outside every expanded region keeps the sentinel.
		TS_ASSERT_EQUALS(patched.surfacePtr()->getPixel(300, 190), rgba.ARGBToColor(255, 255, 0, 0));
	}
};
```

(If `getPixel` is unavailable on `Graphics::Surface` in this tree, read via `getBasePtr` + `uint32` load — match whatever `test_compositor.h` already does for pixel asserts.)

- [ ] **Step 2: Implement `expandRegionsToElements`**

`roger_compositor.h`, with the other free-function declarations:

```cpp
// §3.3 support: grow `regions` (coalesced, clamped to `bounds`) to a fixpoint over
// every UI element whose paint extent (uiPaintExtent) intersects them, closed over
// window-token groups (renderUiLayer's kUiWindow border logic unions the rects of
// all elements sharing the window's token, so a partial group would render a
// different border than a full redraw). Appends the selected element indices
// (ascending — original draw order) to `outElemIndices`.
void expandRegionsToElements(Common::Array<Common::Rect> &regions,
                             const Common::Array<UiElement> &elems,
                             const Common::Rect &gameRect, const Common::Rect &bounds,
                             Common::Array<uint> &outElemIndices);
```

`roger_compositor.cpp` (needs `#include "sci/roger/roger_coords.h"` if not already present):

```cpp
void expandRegionsToElements(Common::Array<Common::Rect> &regions,
                             const Common::Array<UiElement> &elems,
                             const Common::Rect &gameRect, const Common::Rect &bounds,
                             Common::Array<uint> &outElemIndices) {
	Common::Array<bool> selected;
	for (uint i = 0; i < elems.size(); i++)
		selected.push_back(false);
	Common::Array<Common::Rect> extents;
	for (uint i = 0; i < elems.size(); i++)
		extents.push_back(uiPaintExtent(elems[i].nativeRect, gameRect));

	bool changed = true;
	while (changed) {
		changed = false;
		for (uint i = 0; i < elems.size(); i++) {
			if (selected[i])
				continue;
			bool hit = false;
			for (uint r = 0; r < regions.size() && !hit; r++)
				if (extents[i].intersects(regions[r]))
					hit = true;
			if (!hit)
				continue;
			// Select the whole token group so the window-border content union is
			// computed from the same set a full redraw would see.
			for (uint j = 0; j < elems.size(); j++) {
				if (selected[j] || elems[j].token != elems[i].token)
					continue;
				selected[j] = true;
				regions.push_back(extents[j]);
				changed = true;
			}
		}
		if (changed) {
			Common::Array<Common::Rect> coalesced;
			coalesceDirtyRects(regions, bounds, coalesced);
			regions = coalesced;
		}
	}
	for (uint i = 0; i < elems.size(); i++)
		if (selected[i])
			outElemIndices.push_back(i);
}
```

- [ ] **Step 3: Implement `patchCompositeRegions`**

`roger_compositor.h`, in the class, after `renderUiLayer`:

```cpp
	// §3.3 region-bounded recompose. Patch `composite` (the persistent scene+UI
	// cache) so that inside `regions` — expanded to cover every intersecting UI
	// element whole (expandRegionsToElements) — it is byte-identical to a full
	// sceneNoUi + renderUiLayer(elems) recompose. Pixels outside the expanded
	// regions are untouched. Never allocates full-frame surfaces. sceneNoUi is a
	// non-const ref only because ManagedSurface::surfacePtr() is non-const; it is
	// never written.
	void patchCompositeRegions(Graphics::ManagedSurface &composite,
	                           Graphics::ManagedSurface &sceneNoUi,
	                           const Common::Array<UiElement> &elems,
	                           const Common::Array<Common::Rect> &regions,
	                           const byte *palette, const Common::Rect &gameRect,
	                           const RogerTextRenderer *text, const RogerTextRenderer *altText);

	// True when the next presentToOverlay() will take its FULL-present branch
	// (dirty-present off, background just rebuilt, no game rect, or the periodic
	// heal is due). A full present reads the WHOLE source surface, so a caller
	// building a partially-valid present source must fall back to a full compose
	// when this is true. Must mirror presentToOverlay's own decision exactly.
	bool nextPresentIsFull() const {
		return !_dirtyPresent || _bgRebuilt || _bgGameRect.isEmpty() ||
		       _framesSinceFullPresent >= kHealFrames;
	}
```

and hoist the heal constant out of `presentToOverlay` into the class (private section):

```cpp
	static const int kHealFrames = 300; // ~5s at 60fps; periodic full-present heal
```

(then delete the local `const int kHealFrames = 300;` inside `presentToOverlay` so the two cannot drift).

`roger_compositor.cpp`:

```cpp
void RogerCompositor::patchCompositeRegions(Graphics::ManagedSurface &composite,
                                            Graphics::ManagedSurface &sceneNoUi,
                                            const Common::Array<UiElement> &elems,
                                            const Common::Array<Common::Rect> &regions,
                                            const byte *palette, const Common::Rect &gameRect,
                                            const RogerTextRenderer *text, const RogerTextRenderer *altText) {
	if (regions.empty())
		return;
	const Common::Rect bounds(0, 0, (int16)composite.w, (int16)composite.h);
	Common::Array<Common::Rect> expanded;
	coalesceDirtyRects(regions, bounds, expanded);
	Common::Array<uint> idx;
	expandRegionsToElements(expanded, elems, gameRect, bounds, idx);

	// Seed the expanded regions with the clean (UI-free) scene. Every selected
	// element's full paint extent lies inside `expanded` (that is what the
	// expansion guarantees), so re-rendering them whole below cannot double-blend
	// over a previously rendered copy of themselves.
	for (uint i = 0; i < expanded.size(); i++) {
		Common::Rect r = expanded[i];
		r.clip(bounds);
		if (r.isEmpty())
			continue;
		composite.copyRectToSurface(*sceneNoUi.surfacePtr(), r.left, r.top, r);
	}

	if (idx.empty())
		return;
	Common::Array<UiElement> subset;
	for (uint i = 0; i < idx.size(); i++)
		subset.push_back(elems[idx[i]]);
	renderUiLayer(composite, subset, palette, gameRect, text, altText);
}
```

- [ ] **Step 4: Build + tests + gate**

Run: `.\build_and_run.ps1 -NoLaunch` (must compile). Run `make test` if a make environment is available; otherwise note "compile-verified, tests execute under make only" in the report.
Run: `powershell -File test\sci\roger\run-regression.ps1`
Expected: ALL PASS (33 checks) — the new code is dormant, nothing may change.

- [ ] **Step 5: Commit**

```bash
git add engines/sci/roger/roger_compositor.h engines/sci/roger/roger_compositor.cpp test/sci/roger/test_present_barrier.h
git commit -m "Roger p1: region expansion + patchCompositeRegions (dormant, unit-tested)"
```

---

### Task 4: Wire the region-bounded present + move the per-cycle present into the barrier

The behavioral core of the phase. After this task: `presentWithUi` takes a bounded path whenever the composite cache is intact; `renderFrame` no longer presents (the barrier's fresh-frame branch does); the uiPush*/clear paths stop invalidating the whole composite.

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.cpp`
- Modify: `engines/sci/roger/file_roger_art_provider.h` (only if a helper needs declaring)

**Interfaces:**
- Consumes: `patchCompositeRegions` + `expandRegionsToElements` (Task 3), barrier/marks (Task 2), `dirtyUnion`/`hasPendingDirty` (existing/Task 2).
- Produces: the final present architecture Tasks 5–6 validate. `_frameJustComposed` protocol: `renderFrame` sets it after composing; only `presentBarrier` clears it.

- [ ] **Step 1: Rewrite `presentWithUi` with the bounded path**

Replace the body between the resize-guard block (keep it verbatim — it ends with `_lastCursorDstRect = Common::Rect();`) and the end of the function with:

```cpp
	const int OW2 = g_system->getOverlayWidth();
	const int OH2 = g_system->getOverlayHeight();
	byte pal[256 * 3];
	g_system->getPaletteManager()->grabPalette(pal, 0, 256);
	ensureCompositeCache(OW2, OH2); // invalidates _compositeCacheValid on (re)alloc
	Graphics::ManagedSurface &scene = *scratchScene(_sceneCache->w, _sceneCache->h);
	const Common::Rect fullR(0, 0, (int16)OW2, (int16)OH2);

	// The .rin capture and the -ui autoshot dump read the WHOLE present source,
	// so those presents need a fully composed frame — and so does a present that
	// presentToOverlay will decide to push FULL (heal frame / dirty-present off /
	// bg rebuild): the bounded path only makes the pushed regions valid.
	const bool needFullSource = (_inputDriver && _inputDriver->capturePending()) || _autoshot ||
	                            _compositor->nextPresentIsFull();

	if (_compositeCacheValid && !needFullSource && _mode != Roger::kModeSideBySide) {
		// §3.3 region-bounded recompose: patch the composite cache only inside the
		// dirty union, then source the present from it. No full-frame copy, no
		// full UI re-render — this is the latency win at dialog time.
		Common::Array<Common::Rect> regions;
		_compositor->dirtyUnion(fullR, regions);
		if (!regions.empty()) {
			if (_uiLayer && !_uiLayer->empty() && _textRenderer)
				_compositor->patchCompositeRegions(*_compositeCache, *_sceneCache,
				                                   _uiLayer->elements(), regions, pal,
				                                   _lastGameRect, _textRenderer, _altTextRenderer);
			else
				for (uint i = 0; i < regions.size(); i++)
					_compositeCache->copyRectToSurface(_sceneCache->rawSurface(),
					                                   regions[i].left, regions[i].top, regions[i]);
		}
		// Present source: composite pixels over every region this present pushes.
		// patchCompositeRegions may have expanded beyond `regions`; re-read the
		// union AFTER adding the expanded rects is unnecessary because expansion
		// only recomputes pixels that are bit-identical outside `regions` (the
		// re-rendered elements were unchanged there) — pushing `regions` suffices.
		for (uint i = 0; i < regions.size(); i++)
			scene.copyRectToSurface(_compositeCache->rawSurface(),
			                        regions[i].left, regions[i].top, regions[i]);
		// Cursor: vacate the old position and prepare the base under the new one.
		if (!_lastCursorDstRect.isEmpty()) {
			Common::Rect oldCur = _lastCursorDstRect;
			oldCur.clip(fullR);
			if (!oldCur.isEmpty()) {
				scene.copyRectToSurface(_compositeCache->rawSurface(), oldCur.left, oldCur.top, oldCur);
				_compositor->addDirtyRect(oldCur);
			}
		}
		Common::Rect newCur = cursorDstRect(_lastGameRect);
		newCur.clip(fullR);
		if (!newCur.isEmpty())
			scene.copyRectToSurface(_compositeCache->rawSurface(), newCur.left, newCur.top, newCur);
		compositeCursor(scene, _lastGameRect); // paints + addDirtyRect + _lastCursorDstRect
		_compositor->presentToOverlay(scene);
		maybeScriptCapture(scene, _lastGameRect); // guaranteed no-op (needFullSource)
		return;
	}

	// Legacy full path: rebuild scene+UI wholesale. Runs on room/geometry/F10/font
	// changes, resize, sbs mode, hw-cursor-invalidated caches, capture/autoshot.
	scene.copyFrom(*_sceneCache); // fully overwrites the scratch buffer
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		// (keep the existing _debugLog ROGER-UI dump block here verbatim)
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, _lastGameRect, _textRenderer, _altTextRenderer);
	}
	_compositeCache->copyFrom(scene);
	_compositeCacheValid = true;
	if (_mode == Roger::kModeSideBySide) {
		presentComparison();
		return;
	}
	compositeCursor(scene, _lastGameRect);
	_compositor->presentToOverlay(scene);
	maybeScriptCapture(scene, _lastGameRect);
	// (keep the existing -ui autoshot signature-throttled dump block here verbatim)
```

Note: the local `OW`/`OH` from the resize guard can be reused instead of `OW2`/`OH2` if still in scope — match the surrounding code.

- [ ] **Step 2: Move `renderFrame`'s present tail into the barrier's fresh-frame branch**

In `renderFrame`, delete the tail from `if (_mode == Roger::kModeSideBySide) {` through the `runDiffCheck();` call (the sbs/cursor/present + autoshot + capture + diff-check block), **including Task 2's `_barrierDirty = false;` line** (the fresh-frame branch owns that now), and replace with:

```cpp
	_frameJustComposed = true; // presentBarrier() (the renderFromAnimateList tail) presents this frame
```

Then add the per-cycle barrier seam at the end of `renderFromAnimateList`, right after the `_inAnimateCycle = false;` line Task 2 placed there:

```cpp
	presentBarrier(); // spec §3.2: the per-cycle present (fresh-frame branch — no recompose)
```

Also in `renderFrame`, remove the `if (!_useHwCursor) {` gate around the composite-cache maintenance block so the composite cache is maintained under the hardware cursor too (the barrier's bounded path depends on it; the copy is bounded to the seed union, so this is cheap):

```cpp
	// Snapshot scene+UI (no cursor) — the barrier's bounded path patches and
	// presents from this cache, so it must stay valid under BOTH cursor modes.
	const bool priorValid = _compositeCacheValid;
	ensureCompositeCache(OW, OH);
	if (fullSceneCopy || !priorValid || !_compositeCacheValid) {
		_compositeCache->copyRectToSurface(scene.rawSurface(), 0, 0, Common::Rect(0, 0, scene.w, scene.h));
	} else {
		const Common::Array<Common::Rect> &u = _compositor->lastSeedUnion();
		for (uint i = 0; i < u.size(); i++)
			_compositeCache->copyRectToSurface(scene.rawSurface(), u[i].left, u[i].top, u[i]);
	}
	_compositeCacheValid = true;
```

In `presentBarrier` (Task 2's version), insert the fresh-frame branch after the `overlayShown()` guard and before the gate:

```cpp
	if (_frameJustComposed && _scratchScene) {
		// Per-cycle present: renderFrame just composed scene+UI into _scratchScene
		// and refreshed the caches — present that frame directly. No recompose.
		_frameJustComposed = false;
		_barrierDirty = false;
		Graphics::ManagedSurface &scene = *_scratchScene;
		if (_mode == Roger::kModeSideBySide) {
			presentComparison();
		} else {
			compositeCursor(scene, _lastGameRect);
			_compositor->presentToOverlay(scene);
		}
		if (_autoshot && _autoshotPicId != _loadedPicId) {
			dumpAutoshot(scene, _lastGameRect, "");
			_autoshotPicId = _loadedPicId;
		}
		maybeScriptCapture(scene, _lastGameRect);
		if (_diffCheck)
			runDiffCheck();
		return;
	}
```

- [ ] **Step 3: Stop invalidating the composite on UI changes**

Delete the `_compositeCacheValid = false;` line from: `uiPushWindow`, `uiPushText`, `uiPushButton`, `uiPushTextEdit`, `uiPushIcon`, `uiPushStatus`, `uiClearToken`, `onNativeEraseRect`, `uiClearAll`, `uiPushFrameBox` (10 sites — exactly the UI-mutation paths; the marks + bounded patch replace them). It **stays** in: the resize guard inside `presentWithUi`, `ensureCompositeCache`, `markFullDirty`, and anywhere geometry/room state changes.

- [ ] **Step 4: Replace the onMouseMoved fast path with the barrier**

`onMouseMoved` becomes:

```cpp
void FileRogerArtProvider::onMouseMoved() {
	if (_mode == Roger::kModeSideBySide) {
		// Re-present the split layout so the single composited cursor tracks the pointer.
		if (_haveScene)
			presentComparison();
		return;
	}
	if (_useHwCursor)
		return; // hardware cursor moves itself; no recomposite needed
	// The barrier's bounded path IS the cursor fast path now: it restores the old
	// cursor region from the composite cache, repaints at the pointer, and pushes
	// only the two cursor-sized rects.
	presentBarrier();
}
```

- [ ] **Step 5: Build + gate**

Run: `.\build_and_run.ps1 -NoLaunch` then `powershell -File test\sci\roger\run-regression.ps1`
Expected: ALL PASS (33 checks), exit 0. This task is the highest-risk one — if a pixel check fails, capture the labeled previews it names (`screenshots\roger-*-<label>-preview.png`), read them, and localize before editing: dialog missing → fresh-frame/mark ordering; ghost after dismissal → vacated marks or patch seeding; corrupt band at old cursor position → cursor-rect copy set. Fix within this task and re-run the gate; do not proceed with any FAIL.

- [ ] **Step 6: Interactive sanity pass (cheap, catches what scripts miss)**

Run: `.\build_and_run.ps1 -NoBuild -Game qfg1 -SaveSlot 1` for ~2 minutes: type a command, dismiss the reply with click AND Enter, wiggle the mouse over/around a dialog, F10 through all three display modes and back. Everything must look as before with no stale rectangles or laggy dialog paint. (This mirrors the spec's interactive-only trigger class; the formal soak stays a Phase 3 gate.) Quit.

- [ ] **Step 7: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp
git commit -m "Roger p1: region-bounded present via the barrier; per-cycle present moved into the barrier"
```

---

### Task 5: Exact invalidation (§3.1) + retire the grow-workaround

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.cpp`

**Interfaces:**
- Consumes: `markNativeDirty`, `markVacatedDirty` (Task 2), the barrier architecture (Task 4).
- Produces: the final §3.1 semantics Task 6's fault injection validates.

- [ ] **Step 1: Unconditional exact rects at the erase/show seams**

`onNativeShowRect` — insert as the FIRST statement (before the depth/overlay gates; `markNativeDirty` self-gates on overlay visibility only):

```cpp
	// §3.1 exact invalidation: SCI showed these native pixels, so the overlay
	// region is stale regardless of any capture bookkeeping below. Deliberately
	// NOT gated on _nativeDrawDepth: invalidation is dumb and exact; only the
	// content capture below is scoped. O(1); never presents.
	markNativeDirty(screenRect);
```

`onNativeEraseRect` — restructure so the mark is unconditional and the barrier is always the tail. Replace the function's guard + tail structure with:

```cpp
void FileRogerArtProvider::onNativeEraseRect(const Common::Rect &nativeRect) {
	// §3.1 exact invalidation: the restored save-under rect, straight from SCI.
	// This is what makes the SQ3 white-line class structurally dead — the region
	// is invalidated no matter what any element bookkeeping thought was there.
	markNativeDirty(nativeRect);
	if (!overlayShown() || !_plate || !_uiLayer || nativeRect.isEmpty()) {
		presentBarrier(); // mid-cycle: defers; frozen-cycle: flushes the mark
		return;
	}
	// (existing generic-text containment-drop logic, unchanged, EXCEPT:
	//  - the droppedRects loop already calls markVacatedDirty (Task 2)
	//  - delete the `if (!removed) return;` early-out — fall through to the barrier)
	...
	presentBarrier();
}
```

Concretely: keep the kept/dropped rebuild loop and the `_diag` warnings as they are; the only structural edits are the top `markNativeDirty` + always reaching `presentBarrier()` at the end. (The barrier defers mid-cycle, so the bitsRestore walking storm still cannot present per-hook — the deferral, not a token match, is now what protects the cycle.)

- [ ] **Step 2: Retire the +2-native grow on removals**

Change `markVacatedDirty`'s body (Task 2 made it an alias of `markUiDirty`):

```cpp
void FileRogerArtProvider::markVacatedDirty(const Common::Rect &nativeRect) {
	if (!overlayShown() || !_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	// Exact rect + TTF pad. The compositor-overdraw ring beyond it is covered by
	// bitsRestore's exact erase rect (markNativeDirty in onNativeEraseRect) — the
	// spec's §3.1 claim; the gate proves it (see the fallback note in the plan).
	_compositor->addDirtyRect(Roger::uiVacatedExtent(nativeRect, _lastGameRect));
}
```

- [ ] **Step 3: Build + gate — the coverage proof**

Run: `.\build_and_run.ps1 -NoLaunch` then `powershell -File test\sci\roger\run-regression.ps1`
Expected: ALL PASS. The load-bearing checks: `qfg1-cmdbox` (stale band), `sq3-dismiss-matrix` (all four dismissal paths), `sq3-wiggle` (mid-lifetime presents).

**Documented fallback (do NOT improvise past it):** if a bottom-band check fails with a thin stale ring at a dismissed window's border, the spec's "bitsRestore's rect covers the overdraw" claim is empirically false for that window style. Then: revert Step 2 only (make `markVacatedDirty` use `uiPaintExtent` again), re-run the gate green, and record the finding verbatim in this plan's Addendum AND flag it for the spec's §5 Phase 1 bullet (Task 6 Step 4 copies it into the spec). Keep Step 1 regardless — exact erase rects are correct independent of the grow question.

- [ ] **Step 4: Dirty-area comparison vs Task 1 baseline**

Re-run both walk-perf scripts with `-CycleLog` (same commands as Task 1 Step 4) and compute the same statistics. Requirement (spec Phase 1 watch-item): `medianArea` within +10 % of the Task 1 numbers for both games, and the gate's perf checks green. Paste the numbers into the Addendum ("Task 5 dirty-area"). If area grew > 10 %: the erase/show marks are not coalescing with the sprite dirty — inspect `ROGER-PRESENT regions=` (should stay small, ~1–3 while walking); a `regions` explosion means `coalesceDirtyRects` is seeing disjoint rects (likely the grow(1) in `markNativeDirty` separating them from sprite rects) — fix by measurement, not by removing the marks.

- [ ] **Step 5: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.cpp
git commit -m "Roger p1: unconditional exact erase/show invalidation; retire the +2-native vacated grow"
```

---

### Task 6: Fault injection, twice-green exit, evidence, docs

**Files:**
- Modify: `docs/superpowers/plans/2026-07-02-roger-phase1-present-barrier.md` (this file — Addendum)
- Modify: `docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md` (Phase 1 result addendum)

**Interfaces:**
- Consumes: the complete Phase 1 implementation (Tasks 1–5).
- Produces: the §8 exit evidence; the green light for the Phase 2 plan.

- [ ] **Step 1: Fault injection — prove the gate detects the class (spec Phase 1 exit criterion)**

Scratch edits only — never committed. For each injection: edit → `.\build_and_run.ps1 -NoLaunch` → `powershell -File test\sci\roger\run-regression.ps1` → record result → `git checkout -- engines/sci/roger/file_roger_art_provider.cpp` → rebuild.

- Injection A: comment out the `markNativeDirty(nativeRect);` line in `onNativeEraseRect`.
- Injection B: comment out the `markVacatedDirty(removedRects[i]);` loop body in `uiClearToken`.
- Injection C (only if A and B each stay green): comment both simultaneously.

Requirement: at least one injection turns the gate red (expected: a `sameRunDiff` FAIL in qfg1-cmdbox or sq3-dismiss-matrix). Record per injection: which checks failed (or "gate stayed green — covered by <other mark>, evidence for the Phase 3 deletion list"). An injection that stays green is a *finding*, not a failure — but if ALL of A, B, C stay green, STOP: the gate cannot detect the class this phase exists to kill; escalate to the human before proceeding.

- [ ] **Step 2: Twice-green exit run**

Run the full gate twice back-to-back on the restored (clean) build. Expected: `ALL PASS (33 checks)`, exit 0, both runs. Known flake allowance: if ONLY the sq3 busy perf check is red in one run, re-run that gate once (Phase 0 documented busy=4–8 ms machine variance); any other flake means a real regression — diagnose, fix in the offending task's area, and restart this step.

- [ ] **Step 3: Paste the evidence into this plan's Addendum**

Both result tables; the Task 1 vs Task 5 dirty-area numbers; the fault-injection matrix; the grow-workaround outcome (removed cleanly / reinstated with evidence).

- [ ] **Step 4: Record the Phase 1 result in the spec**

Append to `docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md` (new addendum section at the bottom):

```markdown
## Addendum: Phase 1 result (2026-07-XX)

Phase 1 shipped on branch `jon-p1-barrier` (plan:
docs/superpowers/plans/2026-07-02-roger-phase1-present-barrier.md — gate tables,
dirty-area telemetry, and fault-injection matrix are in that plan's addendum).
<one line: grow-workaround outcome — removed cleanly, or reinstated because
bitsRestore's rect does not cover the compositor overdraw ring for <style>>.
<one line: which fault injections the gate caught — the §5 Phase 1 exit
criterion — and which marks proved redundant (input for the Phase 3 deletion
list)>.
```

Do NOT rewrite CLAUDE.md's invariants section — that is explicitly Phase 3 (spec §10).

- [ ] **Step 5: Commit + hand back**

```bash
git add -f docs/superpowers/plans/2026-07-02-roger-phase1-present-barrier.md docs/superpowers/specs/2026-07-02-roger-present-barrier-design.md
git commit -m "Roger p1: phase exit - twice-green gate, fault-injection evidence, spec addendum"
```

Then report: Phase 1 complete on `jon-p1-barrier`, ready to merge to `jon-refactor1` (merge is the human's call per finishing-a-development-branch options). Note in the report: the spec requires the user's interactive soak of both games before Phase 3 (not before Phase 2).

---

## Addendum: Phase 1 exit evidence

(Filled during execution.)

- **Task 1 dirty-area baseline:**
  - qfg1: presents=176 cycles=175 presentsPerCycle=1.01 medianArea=983202
  - sq3: presents=183 cycles=162 presentsPerCycle=1.13 medianArea=361642
- **Task 5 dirty-area:** _(same metrics; must be ≤ +10 % on medianArea)_
- **Fault-injection matrix:** _(A/B/C → which checks failed)_
- **Grow-workaround outcome:** _(removed cleanly / reinstated + evidence)_
- **Twice-green gate tables:** _(both runs)_
