# QFG1 Character-Screen Rendering Fidelity — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring Roger's hires overlay rendering of the QFG1 character-creation screen (and text-heavy SCI0 screens) to production fidelity — all native elements present, uniformly sized, correctly placed, no duplicates, no ghosts — verified against the native render by an in-engine image diff.

**Architecture:** Refine the existing generic-text + pixel-capture pipeline. Pure geometry/dedup helpers (`engines/sci/roger/roger_compositor.{h,cpp}`) are unit-tested; capture/render changes are verified in-game (user drives QFG1) and objectively by a new in-engine native-vs-overlay diff. Game-agnostic throughout: every mechanism keys off a generic SCI primitive, never a screen identity.

**Tech Stack:** C++11, ScummVM SCI engine, Roger overlay (`engines/sci/roger/`), OSystem overlay compositor. Windows/MSVC via `build_and_run.ps1`. Spec: `docs/superpowers/specs/2026-06-29-qfg1-charscreen-rendering-fidelity-design.md`.

## Global Constraints

- **Game-agnostic is #1** — every mechanism keyed to a generic SCI primitive; NO per-game/per-screen knowledge.
- **EGA SCI0 only** — permanent scope (SQ3, QFG1 EGA). No VGA/SCI1 paths.
- **Performance discipline (CLAUDE.md "read before touching the per-cycle path"):** new per-cycle hooks (erasure on `bitsRestore`/redraw, metric capture in `Box`) must be O(1)/cheap and must NOT trigger a full present or invalidate the composite cache unless the scene genuinely changed. The image diff is a gated diagnostic, never on the steady-state path.
- **All-or-native fallback:** if any capture/render path fails, the native render still shows; never a skipped/garbage frame. The pixel path is the safety net.
- **SQ3 non-regression:** message windows keep their fill; dialogs/banner/inventory/gameplay unchanged. Every task verifies SQ3.
- **C++11, tabs (width 4), no exceptions, no RTTI, GPLv3+** — `.clang-format` enforced; pointer/reference right-aligned (`int *p`); K&R attached braces.
- **Build:** `make`/`make test` are unavailable on this Windows/MSVC environment. Pure CxxTests are written and compile-verified via `.\build_and_run.ps1 -NoLaunch` (not executed); integration is verified in-game. In-game steps are driven by the USER (synthetic input is unreliable in QFG1).
- **Retained, do not revert:** the `viewId<0` guards in `generateViewCel`/`renderNativeCel`; the generic-text path (`onNativeText` capturing regardless of `show`, persistent emit, text-rendering-type dedup); the pixel path (`_textSprites`/`_foregroundRegions`/`snapshotNativeRegion`/`processForegroundCaptures`). This plan refines them.

## Background — confirmed in-game 2026-06-29

The generic-text fix renders crisp persistent TTF stats. Remaining defects (user screenshot + `ROGER-UI` dump + report): (1) non-text graphics (portrait/bars/frame) that the pixel path showed before are now gone [regression]; (2) transient gameplay text ghosts [confirmed]; (3) erratic font sizes (short strings balloon, long shrink); (4) "Name SS"/"Start Game"/"Cancel" render twice; (5) the red selection box is misaligned.

## File Structure

| File | Responsibility | Change |
|---|---|---|
| `engines/sci/roger/roger_compositor.h/cpp` | Pure overlay helpers | Add `rectCoverageFraction` + `filterForegroundCaptureRegionsCovered` (coverage-threshold exclusion, Task 1); make `dedupeGenericTextElements` overlap+text aware (Task 4). |
| `test/sci/roger/test_charscreen_fidelity.h` | CxxTest (new) | Tests for the coverage-exclusion (Task 1) and overlap-dedup (Task 4) helpers. |
| `engines/sci/roger/file_roger_art_provider.h/cpp` | Provider impl | Task 1: use coverage exclusion. Task 2: `dropGenericTextInRect` erasure. Task 3: store native font metrics on the generic element. Task 6: `roger_diff_check` capture+diff. |
| `engines/sci/roger/roger_art_provider.h` | Provider interface | Task 2: declare no-op `onNativeEraseRect`. |
| `engines/sci/graphics/text16.cpp` | Hook site | Task 3: pass native font height + single-line width to `onNativeText`. |
| `engines/sci/graphics/paint16.cpp` | Hook site | Task 2: call `onNativeEraseRect` from `bitsRestore`/`kernelGraphRedrawBox`. Task 5: highlight-box capture. Task 6: native-buffer access for the diff. |

---

## Task 1 (P0-A): Restore missing non-text graphics — coverage-threshold capture exclusion

**Root cause hypothesis (confirm in Step 1):** `processForegroundCaptures` excludes a pixel region if a captured-text rect *intersects* it (`filterForegroundCaptureRegions` uses `Common::Rect::intersects`). Now that many wide/multi-line generic-text rects are persisted (e.g. the TAB-hint block `165,127..315,151`), they intersect adjacent graphic regions (portrait/bars/frame) and wrongly drop them from the pixel path. Fix: exclude a region only when a captured-text rect *substantially covers* it (coverage fraction ≥ threshold), not on mere intersection.

**Files:**
- Modify: `engines/sci/roger/roger_compositor.h` (declare), `engines/sci/roger/roger_compositor.cpp` (define)
- Modify: `engines/sci/roger/file_roger_art_provider.cpp` (`processForegroundCaptures` uses the new filter)
- Test: `test/sci/roger/test_charscreen_fidelity.h` (new)

**Interfaces:**
- Produces: `int Roger::rectCoverageFraction(const Common::Rect &inner, const Common::Rect &outer);` — returns the percentage (0..100) of `inner`'s area covered by its intersection with `outer`. `inner` empty → 0.
- Produces: `void Roger::filterForegroundCaptureRegionsCovered(const Common::Array<Common::Rect> &captured, const Common::Array<Common::Rect> &exclude, int minCoveragePct, Common::Array<Common::Rect> &out);` — append each `captured[i]` to `out` UNLESS some `exclude[j]` covers ≥ `minCoveragePct` of it. Does not clear `out`.
- Consumes: `Roger::collectUiTextRects` (existing).

- [ ] **Step 1: Diagnose (user-driven, no code) — confirm the over-exclusion**

With `roger_diag=true` and `roger_debug_capture=true`, user opens the QFG1 char screen. Controller inspects: (a) the `ROGER-DIAG[fgCapture]` rects present vs the missing graphic regions (portrait ~`26,42..63,102` per the icon element `[6]`; point bars; frame); (b) whether a persisted generic-text `nativeRect` (from the `ROGER-UI` dump) *intersects* a missing graphic region. Record the finding inline. If instead the graphics are absent from `_foregroundRegions` entirely (a capture gap, not exclusion), note it and STOP for re-scope (the coverage fix won't help). Expected (leading hypothesis): a wide text rect intersects the graphic → over-exclusion confirmed.

- [ ] **Step 2: Write the failing test**

Create `test/sci/roger/test_charscreen_fidelity.h` (GPLv3 header like the other test files):

```cpp
#include <cxxtest/TestSuite.h>
#include "common/array.h"
#include "common/rect.h"
#include "engines/sci/roger/roger_compositor.h"

using namespace Sci;
using namespace Sci::Roger;

class CharScreenFidelityTestSuite : public CxxTest::TestSuite {
public:
	void test_coverage_fraction_full_and_partial() {
		// inner fully inside outer -> 100
		TS_ASSERT_EQUALS(Roger::rectCoverageFraction(Common::Rect(10,10,20,20), Common::Rect(0,0,100,100)), 100);
		// no overlap -> 0
		TS_ASSERT_EQUALS(Roger::rectCoverageFraction(Common::Rect(10,10,20,20), Common::Rect(50,50,60,60)), 0);
		// half covered (left half of a 10x10 inner) -> 50
		TS_ASSERT_EQUALS(Roger::rectCoverageFraction(Common::Rect(0,0,10,10), Common::Rect(0,0,5,10)), 50);
	}
	void test_covered_filter_keeps_lightly_overlapped_graphic() {
		// A graphic region only clipped at its edge by a wide text rect must be KEPT.
		Common::Array<Common::Rect> captured, exclude, out;
		captured.push_back(Common::Rect(26, 42, 63, 102));   // portrait graphic
		exclude.push_back(Common::Rect(20, 95, 315, 151));   // wide multi-line text rect, clips only the bottom edge
		Roger::filterForegroundCaptureRegionsCovered(captured, exclude, 80, out);
		TS_ASSERT_EQUALS(out.size(), 1u);                    // kept (only ~12% covered)
	}
	void test_covered_filter_drops_text_region() {
		// A region a text rect substantially covers must be DROPPED (no blocky-under-crisp).
		Common::Array<Common::Rect> captured, exclude, out;
		captured.push_back(Common::Rect(170, 45, 192, 57));  // a stat-value cell
		exclude.push_back(Common::Rect(168, 44, 194, 58));   // generic text rect covering it
		Roger::filterForegroundCaptureRegionsCovered(captured, exclude, 80, out);
		TS_ASSERT_EQUALS(out.size(), 0u);                    // dropped
	}
};
```

- [ ] **Step 3: Run the test to verify it fails**

Run (make build): `make test`. Expected: FAIL — helpers undeclared. MSVC-only: note CxxTest written; compile-verify after Step 5.

- [ ] **Step 4: Declare the helpers** in `engines/sci/roger/roger_compositor.h`, near `filterForegroundCaptureRegions`:

```cpp
// Percentage (0..100) of `inner`'s area covered by its intersection with `outer`. inner empty -> 0.
int rectCoverageFraction(const Common::Rect &inner, const Common::Rect &outer);

// Append each `captured[i]` to `out` UNLESS some `exclude[j]` covers >= minCoveragePct of it.
// Coverage-threshold variant of filterForegroundCaptureRegions: a region only edge-clipped by a
// (often wide/multi-line) text rect is kept, so adjacent graphics are not lost to mere intersection.
// Does not clear `out`.
void filterForegroundCaptureRegionsCovered(const Common::Array<Common::Rect> &captured,
                                           const Common::Array<Common::Rect> &exclude,
                                           int minCoveragePct, Common::Array<Common::Rect> &out);
```

- [ ] **Step 5: Implement the helpers** in `engines/sci/roger/roger_compositor.cpp`, inside `namespace Sci { namespace Roger {`, near `filterForegroundCaptureRegions`:

```cpp
int rectCoverageFraction(const Common::Rect &inner, const Common::Rect &outer) {
	const int area = inner.width() * inner.height();
	if (area <= 0)
		return 0;
	Common::Rect isect = inner.findIntersectingRect(outer);
	const int cov = isect.width() * isect.height();
	if (cov <= 0)
		return 0;
	return (int)((cov * 100) / area);
}

void filterForegroundCaptureRegionsCovered(const Common::Array<Common::Rect> &captured,
                                           const Common::Array<Common::Rect> &exclude,
                                           int minCoveragePct, Common::Array<Common::Rect> &out) {
	for (uint i = 0; i < captured.size(); i++) {
		bool drop = false;
		for (uint j = 0; j < exclude.size(); j++) {
			if (rectCoverageFraction(captured[i], exclude[j]) >= minCoveragePct) { drop = true; break; }
		}
		if (!drop)
			out.push_back(captured[i]);
	}
}
```

> `Common::Rect::findIntersectingRect` returns the overlap rect (empty if none); confirm the exact name in `common/rect.h` and use it (it exists alongside `intersects`).

- [ ] **Step 6: Use the coverage filter in `processForegroundCaptures`**

In `engines/sci/roger/file_roger_art_provider.cpp` (`processForegroundCaptures`, ~1351), replace the `filterForegroundCaptureRegions` call so the live-cast exclusion stays exact (containment-free) but the captured-TEXT exclusion uses coverage. Split the exclude set:

```cpp
	// Live cast: exclude on any intersection (moving actors must never be pixel-stamped).
	Common::Array<Common::Rect> keep;
	Roger::filterForegroundCaptureRegions(_foregroundRegions, liveSpriteRects, keep);
	// Captured crisp text: exclude only regions a text rect SUBSTANTIALLY covers (>=80%), so a
	// graphic merely edge-clipped by a wide/multi-line text rect survives (fixes lost portrait/bars).
	Common::Array<Common::Rect> textRects;
	if (_uiLayer)
		Roger::collectUiTextRects(_uiLayer->elements(), GENERIC_TEXT_TOKEN, textRects);
	Common::Array<Common::Rect> keep2;
	Roger::filterForegroundCaptureRegionsCovered(keep, textRects, 80, keep2);
	_foregroundRegions.clear();
```

Then change the subsequent loop to iterate `keep2` instead of `keep`. (Confirm the loop variable name; everything after — snapshot + insert/update — is unchanged.)

- [ ] **Step 7: Build** — `.\build_and_run.ps1 -Game qfg1`. Expected: clean build, QFG1 launches.

- [ ] **Step 8: Verify in-game (user drives)** — char screen: the portrait, point bars, and frame graphics are back AND the crisp stat text still has no blocky pixel double under it. Record which "always missing" elements (if any) remain via the diff tool in Task 6.
Expected: graphics restored, no text-double.

- [ ] **Step 9: Commit**

```bash
git add engines/sci/roger/roger_compositor.h engines/sci/roger/roger_compositor.cpp engines/sci/roger/file_roger_art_provider.cpp test/sci/roger/test_charscreen_fidelity.h
git commit -m "Roger: coverage-threshold capture exclusion (restore lost char-screen graphics)

Pixel capture excluded any region a captured-text rect intersected; wide/multi-line
generic-text rects clipped adjacent graphics (portrait/bars/frame) and dropped them.
Exclude only regions a text rect covers >=80% (rectCoverageFraction +
filterForegroundCaptureRegionsCovered, pure + unit-tested); live-cast exclusion stays
intersection-based. Graphics return; crisp text keeps its no-double-stamp.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 2 (P0-B): Transient-text erasure (anti-ghosting)

Persisted generic text never drops when SCI erases it, so transient gameplay narration ghosts (confirmed). Add a game-agnostic erasure signal: when SCI restores saved-under bits or redraws a box over a region, drop persisted generic-text elements inside that region. Static text (never erased until room change) persists.

**Files:**
- Modify: `engines/sci/roger/roger_art_provider.h` (no-op base virtual)
- Modify: `engines/sci/roger/file_roger_art_provider.h/cpp` (override + `dropGenericTextInRect`)
- Modify: `engines/sci/graphics/paint16.cpp` (call from `bitsRestore` + `kernelGraphRedrawBox`)

**Interfaces:**
- Produces: base virtual `virtual void onNativeEraseRect(const Common::Rect &nativeRect) {}` on `RogerArtProvider`.
- Produces: override `void onNativeEraseRect(const Common::Rect &) override;` on `FileRogerArtProvider`, which removes persisted generic-text elements whose `nativeRect` is contained in the erased rect and presents only if something changed.

- [ ] **Step 1: Declare the no-op base virtual** in `engines/sci/roger/roger_art_provider.h`, beside `onNativeText`:

```cpp
	// SCI erased/redrew a native region (bitsRestore of saved-under bits, or kGraphRedrawBox).
	// The provider drops persisted generic captured text inside it so transient text does not
	// ghost in the overlay after SCI removes it. Default no-op.
	virtual void onNativeEraseRect(const Common::Rect &nativeRect) {}
```

- [ ] **Step 2: Declare the override** in `engines/sci/roger/file_roger_art_provider.h`, near `onNativeText`:

```cpp
	void onNativeEraseRect(const Common::Rect &nativeRect) override;
```

- [ ] **Step 3: Implement the override** in `engines/sci/roger/file_roger_art_provider.cpp`, near `uiClearToken`:

```cpp
void FileRogerArtProvider::onNativeEraseRect(const Common::Rect &nativeRect) {
	if (!_overlayActive || !_plate || !_uiLayer || nativeRect.isEmpty())
		return;
	// Remove persisted generic text whose box lies within the erased region. Gate the present
	// on a real removal (CLAUDE.md per-cycle discipline: bitsRestore fires ~2x/sprite/cycle).
	bool removed = false;
	const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
	Common::Array<Roger::UiElement> kept;
	for (uint i = 0; i < els.size(); i++) {
		if (els[i].token == GENERIC_TEXT_TOKEN && nativeRect.contains(els[i].nativeRect))
			{ removed = true; continue; }
		kept.push_back(els[i]);
	}
	if (!removed)
		return;
	_uiLayer->clearAll();
	for (uint i = 0; i < kept.size(); i++)
		_uiLayer->push(kept[i]);
	if (_diag)
		warning("ROGER-DIAG[eraseText]: rect=(%d,%d,%d,%d) remaining=%u",
		        nativeRect.left, nativeRect.top, nativeRect.right, nativeRect.bottom, (unsigned)kept.size());
	presentWithUi(); // UI-only present; reflects the removal without a full renderScene
}
```

> Confirm `presentWithUi()` is the existing UI-only present (it is — used by `uiClearToken`). If a leaner "rebuild without an element" exists on `RogerUiLayer`, prefer it; otherwise the clear+repush above is acceptable (generic text count is small).

- [ ] **Step 4: Call the hook from the erase sites** in `engines/sci/graphics/paint16.cpp`.

In `bitsRestore` (after the existing `uiClearToken` block, where the restored rect is known — the function restores `memoryHandle`'s saved rect; use the rect it restores):

```cpp
	if (g_sciRogerProvider && g_sciRogerProvider->enabled && !memoryHandle.isNull()) {
		Common::Rect restored;
		_screen->bitsGetRect(_segMan->getHunkPointer(memoryHandle), &restored); // global rect of the saved area
		g_sciRogerProvider->onNativeEraseRect(restored);
	}
```

In `kernelGraphRedrawBox` (after it has the global rect):

```cpp
	if (g_sciRogerProvider && g_sciRogerProvider->enabled)
		g_sciRogerProvider->onNativeEraseRect(rect); // rect already global here
```

> Confirm `bitsGetRect`/the saved-rect accessor name and that `rect` in `kernelGraphRedrawBox` is in global (320×200) coordinates at the call point (it is converted to global at the top of the function); place the call where the rect is global. Match the existing gating pattern.

- [ ] **Step 5: Build** — `.\build_and_run.ps1 -Game qfg1`. Expected: clean build.

- [ ] **Step 6: Verify in-game (user drives)** — QFG1 gameplay: trigger a transient narration message (walk into something, "look"); confirm the crisp text appears then **disappears when SCI clears it** (no ghost). Char screen: static stats still persist (not erased). SQ3: messages still appear/clear correctly. Confirm walking speed unchanged (`ROGER-DIAG[eraseText]` only fires on real removals; no per-cycle full present).
Expected: ghosting gone; static text unaffected; no perf regression.

- [ ] **Step 7: Commit**

```bash
git add engines/sci/roger/roger_art_provider.h engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp engines/sci/graphics/paint16.cpp
git commit -m "Roger: drop persisted generic text when SCI erases its region (anti-ghosting)

onNativeEraseRect, called from bitsRestore (saved-under restore) and kernelGraphRedrawBox,
removes persisted generic captured text inside the erased rect so transient gameplay text
no longer ghosts after SCI clears it. Static text (never erased until room change) persists.
Present gated on a real removal (per-cycle discipline).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 3 (P1-C): Consistent font sizing — capture native font metrics

`onNativeText` does not capture native font metrics, so `RogerTextRenderer` free-fits each string to its box → erratic sizes. Capture the native cell height + single-line width at the `Box` hook (where the font is current) and feed them to the element, exactly as `uiPushText` does, so all generic text renders at one consistent body size.

**Files:**
- Modify: `engines/sci/roger/roger_art_provider.h` (extend `onNativeText` signature)
- Modify: `engines/sci/roger/file_roger_art_provider.h/cpp` (override + populate metrics)
- Modify: `engines/sci/graphics/text16.cpp` (measure + pass metrics)

**Interfaces:**
- Produces: `onNativeText(const Common::Rect &nativeRect, const char *text, int fontId, int penColor, int align, int nativeFontH, int nativeTextW)` — extended with the native font cell height and single-line width (0 = multi-line/unknown), on both the base virtual and the override.

- [ ] **Step 1: Extend the base virtual** in `engines/sci/roger/roger_art_provider.h`:

```cpp
	virtual void onNativeText(const Common::Rect &nativeRect, const char *text,
	                          int fontId, int penColor, int align,
	                          int nativeFontH, int nativeTextW) {}
```

- [ ] **Step 2: Extend the override declaration** in `engines/sci/roger/file_roger_art_provider.h` to match.

- [ ] **Step 3: Populate the metrics in the override** in `engines/sci/roger/file_roger_art_provider.cpp` (`onNativeText`), setting the fields `uiPushText` sets:

```cpp
	e.token = GENERIC_TEXT_TOKEN;
	e.textRole = Roger::kRoleBody;     // same body size as dialog/control text
	e.nativeFontH = nativeFontH;       // native cell height -> renderer target size
	e.nativeTextW = nativeTextW;       // single-line width cap (0 = multi-line: no cap)
	_genTextPending.push_back(e);
```

- [ ] **Step 4: Measure and pass the metrics at the `Box` hook** in `engines/sci/graphics/text16.cpp`. The draw loop already computes `maxTextWidth` (widest line) and a per-line `textHeight`; track whether only one line was drawn:

```cpp
	int16 lineCount = 0;
	// ... inside the while loop, after a line is drawn (Show/Draw branch): lineCount++;
```

Then at the end-of-Box hook:

```cpp
	if (g_sciRogerProvider && g_sciRogerProvider->enabled) {
		const int nativeFontH = textHeight;                         // uniform per-line cell height
		const int nativeTextW = (lineCount <= 1) ? maxTextWidth : 0; // width cap only for single-line
		g_sciRogerProvider->onNativeText(rect, text, fontId, previousPenColor, (int)alignment,
		                                 nativeFontH, nativeTextW);
	}
```

> `textHeight` and `maxTextWidth` are the locals already computed by the loop (`Width(...textWidth, textHeight...)`, `maxTextWidth = MAX(...)`). Add the `lineCount` increment in the existing per-line branch (`if (show && !doubleByteMode) Show(...) else Draw(...)`). Confirm `textHeight` holds the line height at function end (it is the last line's height; lines are uniform).

- [ ] **Step 5: Build** — `.\build_and_run.ps1 -Game qfg1`. Expected: clean build.

- [ ] **Step 6: Verify in-game (user drives)** — char screen: all stat labels and values render at **one consistent body size** (no more giant "Luck" beside tiny "Intelligence"); footprint matches the native layout (F10 A/B). Multi-line hint text ("TAB to move around, …") still wraps/sizes sanely.
Expected: uniform, native-matching text size.

- [ ] **Step 7: Commit**

```bash
git add engines/sci/roger/roger_art_provider.h engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp engines/sci/graphics/text16.cpp
git commit -m "Roger: capture native font metrics for generic text (consistent sizing)

onNativeText now carries the native cell height + single-line width measured at the Box
hook and sets textRole=body, so RogerTextRenderer sizes generic text to the native cell
(like uiPushText) instead of free-fitting each box. Fixes erratic per-string sizes on the
QFG1 char screen.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 4 (P1-D): Overlap+text-aware dedup — eliminate double-render

The dedup uses strict containment, so an offset control label (Start Game / Cancel / Name "SS") is not dropped against its `kUiButton`/`kUiTextEdit`. Relax to substantial overlap, keyed to text-rendering element types.

**Files:**
- Modify: `engines/sci/roger/roger_compositor.h/cpp` (`dedupeGenericTextElements`)
- Test: `test/sci/roger/test_charscreen_fidelity.h` (extend)

**Interfaces:**
- Consumes: `Roger::rectCoverageFraction` (Task 1).
- Same signature `dedupeGenericTextElements(Common::Array<UiElement>&, uint32 genericToken)`; behavior change only.

- [ ] **Step 1: Write the failing test** (append to `test/sci/roger/test_charscreen_fidelity.h`):

```cpp
	void test_dedupe_drops_generic_overlapping_button_label() {
		const uint32 G = 0x60000000u, C = 0x40000000u;
		Common::Array<UiElement> elems;
		UiElement btn; btn.type = kUiButton; btn.nativeRect = Common::Rect(210, 166, 290, 178); btn.token = C;
		elems.push_back(btn);
		UiElement g; g.type = kUiText; g.nativeRect = Common::Rect(214, 167, 286, 177); g.token = G; // label, ~90% inside button
		elems.push_back(g);
		Roger::dedupeGenericTextElements(elems, G);
		TS_ASSERT_EQUALS(elems.size(), 1u);           // generic label dropped
		TS_ASSERT(elems[0].type == kUiButton);
	}
	void test_dedupe_keeps_generic_barely_overlapping() {
		const uint32 G = 0x60000000u, C = 0x40000000u;
		Common::Array<UiElement> elems;
		UiElement t; t.type = kUiText; t.nativeRect = Common::Rect(0, 0, 100, 12); t.token = C;
		elems.push_back(t);
		UiElement g; g.type = kUiText; g.nativeRect = Common::Rect(95, 0, 195, 12); g.token = G; // only ~5% overlap
		elems.push_back(g);
		Roger::dedupeGenericTextElements(elems, G);
		TS_ASSERT_EQUALS(elems.size(), 2u);           // distinct text kept
	}
```

(Keep the existing window/icon-keep and button-drop tests from `test_generic_text_capture.h` valid — they still hold under overlap.)

- [ ] **Step 2: Run to verify it fails** — `make test` (MSVC: compile-verify after Step 3).

- [ ] **Step 3: Update `dedupeGenericTextElements`** in `engines/sci/roger/roger_compositor.cpp` to use coverage overlap against text-rendering types:

```cpp
	for (uint i = 0; i < elems.size();) {
		bool drop = false;
		if (elems[i].token == genericToken) {
			for (uint j = 0; j < elems.size(); j++) {
				if (j == i || elems[j].token == genericToken)
					continue;
				const UiElementType jt = elems[j].type;
				const bool jRendersText = (jt == kUiText || jt == kUiButton || jt == kUiTextEdit);
				// A control that renders the same text usually draws its label at a small
				// offset inside its box, so dedup on substantial overlap (>=70%), not strict
				// containment. Window/icon never drop the text they enclose.
				if (jRendersText && rectCoverageFraction(elems[i].nativeRect, elems[j].nativeRect) >= 70) {
					drop = true; break;
				}
			}
		}
		if (drop)
			elems.remove_at(i);
		else
			i++;
	}
```

- [ ] **Step 4: Run to verify it passes** — `make test` (MSVC: `.\build_and_run.ps1 -NoLaunch`, clean compile).

- [ ] **Step 5: Verify in-game (user drives)** — char screen: "Name SS", "Start Game", "Cancel" each render **once**; standalone labels (stat names) unaffected.
Expected: no duplicates, no missing labels.

- [ ] **Step 6: Commit**

```bash
git add engines/sci/roger/roger_compositor.cpp test/sci/roger/test_charscreen_fidelity.h
git commit -m "Roger: overlap-based generic-text dedup (kill double-rendered control labels)

dedupeGenericTextElements now drops a generic label that overlaps a text-rendering control
(kUiText/kUiButton/kUiTextEdit) by >=70%, not only when strictly contained, so an offset
control label (Start Game/Cancel/Name) dedups against its control. Window/icon still never
drop enclosed text. Unit-tested.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 5 (P2-E): Highlight / selection box alignment

The red selection/frame box is misaligned. Investigate its native primitive and Roger's capture/mapping, then align it.

**Files:**
- Modify: `engines/sci/graphics/paint16.cpp` and/or `engines/sci/roger/file_roger_art_provider.cpp` (per Step 1 finding)

- [ ] **Step 1: Diagnose (user-driven)** — with `roger_diag=true`, observe how the red box is drawn: it is likely `GfxPaint16::kernelGraphFrameBox` (`paint16.cpp:516`) or a control highlight. Determine (a) the native rect SCI uses, (b) whether Roger captures it at all (search `kernelGraphFrameBox` for a `g_sciRogerProvider` hook — there is none today), and (c) if captured elsewhere, how its rect maps to the overlay vs where it actually appears. Record the finding inline.

- [ ] **Step 2: Apply the fix matching the finding** — two bounded outcomes:
  - **(2a) Not captured:** add a generic frame-box capture: in `kernelGraphFrameBox`, when `g_sciRogerProvider && enabled`, push a hires frame element (a `kUiWindow`/frame with `hasFrame=true`, no fill, the box rect + color) so the overlay draws the frame aligned via `sciRectToDest`. Use a dedicated room-scoped token; clear on room change. Keep it game-agnostic (any `kGraphFrameBox`).
  - **(2b) Captured but mis-mapped:** correct the native→overlay rect mapping (likely a local-vs-global coordinate or an off-by-the-frame-thickness issue) so the rendered frame lands on the native position.
  (Write the concrete code for the branch the diagnosis selects; both use existing primitives — `sciRectToDest`, the `UiElement` frame fields, `uiClearToken`.)

- [ ] **Step 3: Build** — `.\build_and_run.ps1 -Game qfg1`.

- [ ] **Step 4: Verify in-game (user drives)** — the selection box aligns with the highlighted item; TAB moves it correctly; SQ3 unaffected.

- [ ] **Step 5: Commit** — message describing 2a/2b.

```bash
git add -A
git commit -m "Roger: align the char-screen selection/frame box (kGraphFrameBox)

<2a: capture kGraphFrameBox as a hires frame element | 2b: fix native->overlay mapping>
so the selection highlight lands on the highlighted item. Game-agnostic (keyed to the
frame-box primitive). 

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 6 (P2-F): In-engine image-diff diagnostic (`roger_diff_check`)

A gated dev harness that captures the native 320×200 visual buffer and the composited overlay, downscales the overlay to native res, diffs them, and logs the regions present-in-native-but-missing-in-overlay (drives Task 1 / always-missing audit) and position-mismatched (drives Task 5 / alignment). Off by default; never on the steady-state path.

**Files:**
- Modify: `engines/sci/roger/file_roger_art_provider.h/cpp` (knob + `runDiffCheck`)

**Interfaces:**
- Consumes: the native visual buffer accessor used by `snapshotNativeRegion`/`snapshotNativeBaseline`; the composited overlay surface (`_compositeCache`); `Roger::extractChangedBoxes` (existing, for coalescing changed boxes); the existing PNG writer `Roger::dumpSurfacePng` (optional).

- [ ] **Step 1: Read the knob** in the provider constructor (beside `roger_debug_capture`):

```cpp
	_diffCheck = ConfMan.hasKey("roger_diff_check") && ConfMan.getBool("roger_diff_check");
```

with `bool _diffCheck = false;` in the header, plus `int _diffCheckedPic = -1;` (once-per-pic guard) reset at the room-change sites next to `_debugDumpedPic = -1;`.

- [ ] **Step 2: Implement `runDiffCheck`** in `engines/sci/roger/file_roger_art_provider.cpp`, called from the present path guarded by `if (_diffCheck)` and the once-per-pic guard:

```cpp
void FileRogerArtProvider::runDiffCheck() {
	if (!_diffCheck || _loadedPicId == _diffCheckedPic || !_compositeCacheValid || !_compositeCache)
		return;
	_diffCheckedPic = _loadedPicId;
	// 1) native 320x200 visual buffer (EGA index) -> a comparable RGBA at native res.
	Graphics::Surface *nat = snapshotNativeRegion(Common::Rect(0, 0, 320, 200));
	if (!nat)
		return;
	// 2) downscale the composited overlay (_compositeCache, OWxOH) to 320x200 (nearest).
	Graphics::Surface small;
	small.create(320, 200, nat->format);
	for (int y = 0; y < 200; y++)
		for (int x = 0; x < 320; x++) {
			const int sx = x * _compositeCache->w / 320, sy = y * _compositeCache->h / 200;
			small.setPixel(x, y, _compositeCache->getPixel(sx, sy));
		}
	// 3) diff: coalesce boxes where native is non-background but overlay is background (missing),
	//    using the existing changed-box extractor on a per-pixel "differs" mask.
	// (Build two index/luma buffers and call Roger::extractChangedBoxes; log each box.)
	// ... emit warning("ROGER-DIAG[diff]: missing rect=(...)") per coalesced box; optional PNG dump.
	nat->free(); delete nat;
	small.free();
}
```

> Confirm the native-buffer accessor and `_compositeCache` pixel format/size against `snapshotNativeRegion` and `ensureCompositeCache`. If a direct EGA-index compare is simpler than RGBA (native is EGA), diff on the index buffer and the overlay's nearest-source index; the goal is only coalesced "missing/misaligned" boxes in the log. Reuse `Roger::extractChangedBoxes` for coalescing; do NOT add a new diff primitive. Adapt the surface APIs to what exists; the logging is the deliverable.

- [ ] **Step 3: Build (compile-only)** — `.\build_and_run.ps1 -NoLaunch`. Expected: clean compile. Off by default → no behavior change.

- [ ] **Step 4: Use it (user, optional)** — set `roger_diff_check=true` in the qfg1 domain, open the char screen, read `ROGER-DIAG[diff]` boxes; cross-check against Task 1 (missing graphics) and Task 5 (alignment).
Expected: diff boxes localize remaining missing/misaligned elements.

- [ ] **Step 5: Commit**

```bash
git add engines/sci/roger/file_roger_art_provider.h engines/sci/roger/file_roger_art_provider.cpp
git commit -m "Roger: gated in-engine native-vs-overlay image diff (roger_diff_check)

runDiffCheck downscales the composited overlay to native 320x200, diffs against the native
visual buffer, and logs coalesced regions present-in-native-but-missing and position
mismatches (reusing extractChangedBoxes). Off by default, once per pic, off the steady-state
path. The objective verification engine for the missing-graphics + alignment work.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Regression sweep + diagnostics cleanup

Confirm the hardening did not regress flicker/perf/transitions and both games are clean; return diagnostics to defaults.

**Files:** none modified (unless a fix is needed).

- [ ] **Step 1: QFG1 char screen (user)** — all graphics present, text uniformly sized + correctly placed, no duplicates, no white-box, selection box aligned, live point allocation clean.
- [ ] **Step 2: QFG1 gameplay (user)** — walk + trigger narration: no text ghosting, no blocky foreground following the ego, walking speed/input latency unchanged.
- [ ] **Step 3: SQ3 regression (user)** — `.\build_and_run.ps1` and `-SaveSlot 1`: message windows keep fill, banner/inventory/dialogs/gameplay correct; no new flicker/dupes/ghosts.
- [ ] **Step 4: Perf (user)** — with `roger_diag=true`, walk a room: `ROGER-DIAG[eraseText]`/`[fgCapture]` quiescent in steady state; `_uiLayer`/`_textSprites` bounded; no full present per cycle.
- [ ] **Step 5: CxxTest (where a make build exists)** — `make test`: all roger tests pass incl. `test_charscreen_fidelity.h`. MSVC-only: note pure tests compiled, integration verified in Steps 1–4.
- [ ] **Step 6: Diagnostics off** — set `roger_diag`, `roger_debug`, `roger_diff_backstop`, `roger_debug_capture`, `roger_diff_check` to off in the qfg1 + sq3 `scummvm.ini` domains.
- [ ] **Step 7: Final commit** (only if Steps 1–6 required a code adjustment).

---

## Self-Review

- **Spec coverage:** A missing graphics → Task 1; B erasure → Task 2; C font sizing → Task 3; D double-render → Task 4; E highlight box → Task 5; F image diff → Task 6; regression+cleanup → Task 7. ✓
- **Placeholder scan:** pure-helper tasks (1, 3, 4) carry full code; investigation-first tasks (2 erase-rect accessor, 5 frame-box, 6 surface APIs) name the exact primitive to confirm and give concrete code to adapt, not TODOs — mirroring the prior plan's bounded-conditional style. ✓
- **Type consistency:** `rectCoverageFraction(inner, outer)→int` and `filterForegroundCaptureRegionsCovered(...)` consistent across Task 1 def/test/call and Task 4 use. `onNativeText(...,int nativeFontH,int nativeTextW)` identical in base virtual, override, and `Box` call site (Task 3). `onNativeEraseRect(const Common::Rect&)` identical across base/override/call sites (Task 2). `GENERIC_TEXT_TOKEN` reused, not redefined. ✓

## Open decision for the user

Coverage thresholds are set to **80%** (Task 1 capture exclusion) and **70%** (Task 4 label dedup) as starting values. If in-game a graphic is still wrongly dropped (raise the 80) or a duplicate label survives (lower the 70), these are the two knobs to tune — flagged, not hard-coded as final.
