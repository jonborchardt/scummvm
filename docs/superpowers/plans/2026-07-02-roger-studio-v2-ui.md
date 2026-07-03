# Roger Studio v2 UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the Roger Studio front-end as a single game-like scene (cel on plate), fully mouse-driven with an on-screen button panel, two live A/B setting slots, and shift-diagnosis tools (nearest-reference plate, Diff display, SAD offset readout, centroid regression locks).

**Architecture:** The v1 engine plumbing (`OmyacParams`, param registry, scaler variants, `RogerAssetGen::setOmyacParams/nativeCelIndexImage/surfaceFromIndex`, the `ROGER_STUDIO` hook) is reused untouched. New pure helpers (widget layout + hit-testing, defaults table, pass-list ops, diff/SAD) go in the SCI-free `roger_studio_render.{h,cpp}` so they are CxxTest-covered. `roger_studio.{h,cpp}` is reworked: the three modes, keyboard UI, keymap, and pin/prev machinery are deleted; in their place two `Slot`s (params+passes+variant+plateMode each, cached render), shared scene state, a panel drawn from the layout helper's widget list, and mouse scene interaction. One new `RogerAssetGen` method (`generatePlateNearest`) provides the zero-shift reference plate.

**Tech Stack:** C++11, ScummVM OSystem overlay API, CxxTest (`build_tests.ps1`), MSVC build (`build_and_run.ps1 -NoLaunch`).

**Spec:** `docs/superpowers/specs/2026-07-02-roger-studio-v2-ui-design.md`

## Global Constraints

- **Zero behavioral change to shipping code paths.** The studio never runs without `ROGER_STUDIO=1`; nothing outside `engines/sci/roger/` changes except (possibly) nothing at all — no `sci.cpp` edits in this plan.
- **Never touch the disk cache from the studio.** The studio's `RogerAssetGen` stays `kGenMemory` with empty cache dir; `generatePlateNearest` must not add any cache read/write.
- **No `kTransformVersion` bump.**
- Code style: C++11, tabs (width 4), K&R braces, pointer/reference right-aligned, no exceptions/RTTI, GPLv3+ header on every new file (test headers in `test/sci/roger/` carry no GPL block, matching the existing ones).
- `docs/` is gitignored — commit plan/spec docs with `git add -f`.
- Never `git add -A`; always stage explicit paths (other agents may have unrelated files in this tree).
- New test headers must be registered in the `$RogerTestHeaders` whitelist in `build_tests.ps1` (the MSVC test harness does NOT auto-discover; `test/module.mk` globbing is make-only).
- Build check: `.\build_and_run.ps1 -NoLaunch` (must succeed). Unit tests: `.\build_tests.ps1` (all suites must pass; currently 103 tests green).
- Keyboard in the studio: ONLY Esc (quit) and E (export) survive, undocumented in the UI, for `.rin` automation.

---

## File Structure

| File | Status | Responsibility |
|---|---|---|
| `engines/sci/roger/roger_studio_render.h/.cpp` | modify | add: defaults table, pass-list ops, v2 export names, widget kinds/id encoding, `StudioPanelState`, `buildStudioPanel`, `hitTestWidgets`, diff + SAD helpers |
| `engines/sci/roger/roger_asset_gen.h/.cpp` | modify | add `generatePlateNearest(int id, uint32 &outMs)` — nearest-×6 reference plate from `NativeRef.refPixel` |
| `engines/sci/roger/roger_studio.h/.cpp` | rework | Slots, scene state, mouse loop, panel draw/dispatch, renderSlot, display composition (A/B/Split/Diff), click-place/drag/zoom/pan, export |
| `test/sci/roger/test_studio_render.h` | modify | tests for defaults, pass ops, export names, widget layout/hit-test, diff/SAD |
| `test/sci/roger/test_shift_lock.h` | create | centroid locks for scale6x + omyac default pipeline; SAD estimator lock |
| `build_tests.ps1` | modify | register `test_shift_lock.h` |
| `test/sci/roger/scripts/studio-smoke.rin` | modify | drop F1 presses (keymap gone); keep e-export + esc |
| `CLAUDE.md` | modify | refresh the `-Studio` paragraph (mouse UI, A/B slots, SQ3 defaults) |

Coordinate/units convention used throughout (document it in `roger_studio.h`):
- **native** = 320×190 SCI pixels; **plate** = 1920×1140 (`OMYAC_NATIVE_* × 6`, `OMYAC_HYBRID_*`);
- **panel-local small** = the half-resolution surface the panel is drawn into (v1's render-small-then-2x-blit HUD approach). The layout helper works entirely in panel-local small coords; `RogerStudio` maps mouse↔small by `(mx - panelLeft)/2, (my - panelTop)/2` and blits the small surface 2× into `_display`.

---

### Task 1: Pure helpers — defaults table, pass-list ops, v2 export names

**Files:**
- Modify: `engines/sci/roger/roger_studio_render.h`
- Modify: `engines/sci/roger/roger_studio_render.cpp`
- Modify: `test/sci/roger/test_studio_render.h`

**Interfaces:**
- Consumes: nothing new (`Common::Array`, `Common::String` already included).
- Produces (later tasks rely on these exact names):
  - `struct StudioDefaults { int picId; int viewId; int loopNo; int celNo; int celX; int celY; };`
  - `StudioDefaults studioDefaultsForGame(const Common::String &gameId);` — `"sq3"` → `{2, 12, 1, 0, 160, 150}`; anything else → `{-1, -1, 0, 0, 160, 150}` (−1 = "use first available id").
  - `void passInsertAfter(Common::Array<int> &passes, int &selected, int passVal);` — inserts after `selected` (or at end when `selected < 0` or `>= size`), then selects the inserted chip.
  - `void passRemoveAt(Common::Array<int> &passes, int &selected);` — removes chip `selected` if in range; pulls `selected` back when it falls off the end (min 0; −1 only when list becomes empty).
  - `bool passMove(Common::Array<int> &passes, int &selected, int dir);` — swaps `selected` with its `dir` (−1/+1) neighbour, moves `selected` with it; returns false (no-op) at the ends or out of range.
  - `Common::String studioSceneExportName(int picId, char slot, const Common::String &detail);` → `"studio-scene<3-digit pic>-<slot>-<detail>.png"`
  - `Common::String studioCompareExportName(int picId, bool diff, const Common::String &stampA, const Common::String &stampB);` → `"studio-scene<3-digit pic>-AB-<stampA>-vs-<stampB>.png"`, or `-diff-` instead of `-AB-` when `diff`.

- [ ] **Step 1: Write the failing tests**

Append to `test/sci/roger/test_studio_render.h` inside `RogerStudioRenderTestSuite`:

```cpp
	void test_defaults_table() {
		StudioDefaults sq3 = studioDefaultsForGame("sq3");
		TS_ASSERT_EQUALS(sq3.picId, 2);
		TS_ASSERT_EQUALS(sq3.viewId, 12);
		TS_ASSERT_EQUALS(sq3.loopNo, 1);
		TS_ASSERT_EQUALS(sq3.celNo, 0);
		TS_ASSERT_EQUALS(sq3.celX, 160);
		TS_ASSERT_EQUALS(sq3.celY, 150);
		StudioDefaults other = studioDefaultsForGame("qfg1");
		TS_ASSERT_EQUALS(other.picId, -1);
		TS_ASSERT_EQUALS(other.viewId, -1);
		TS_ASSERT_EQUALS(other.celX, 160);
		TS_ASSERT_EQUALS(other.celY, 150);
	}

	void test_pass_list_ops() {
		Common::Array<int> p; // empty
		int sel = -1;
		passInsertAfter(p, sel, 2);          // [f], sel 0
		TS_ASSERT_EQUALS(p.size(), 1u);
		TS_ASSERT_EQUALS(sel, 0);
		passInsertAfter(p, sel, 1);          // [f l], sel 1
		passInsertAfter(p, sel, 0);          // [f l a], sel 2
		TS_ASSERT_EQUALS(p[0], 2); TS_ASSERT_EQUALS(p[1], 1); TS_ASSERT_EQUALS(p[2], 0);
		sel = 0;
		passInsertAfter(p, sel, 2);          // [f f l a], sel 1
		TS_ASSERT_EQUALS(sel, 1);
		TS_ASSERT_EQUALS(p[1], 2);
		TS_ASSERT(passMove(p, sel, +1));     // [f l f a], sel 2
		TS_ASSERT_EQUALS(sel, 2);
		TS_ASSERT_EQUALS(p[2], 2);
		TS_ASSERT(!passMove(p, sel, +2));    // invalid dir -> no-op? dir is -1/+1 only; +2 out of contract
		sel = (int)p.size() - 1;
		TS_ASSERT(!passMove(p, sel, +1));    // at right end -> false
		passRemoveAt(p, sel);                // remove last, sel pulls back
		TS_ASSERT_EQUALS(p.size(), 3u);
		TS_ASSERT_EQUALS(sel, 2);
		sel = 5;                             // out of range -> no-op
		passRemoveAt(p, sel);
		TS_ASSERT_EQUALS(p.size(), 3u);
		sel = 0;
		passRemoveAt(p, sel); passRemoveAt(p, sel); passRemoveAt(p, sel);
		TS_ASSERT(p.empty());
		TS_ASSERT_EQUALS(sel, -1);           // empty list -> no selection
	}

	void test_v2_export_names() {
		TS_ASSERT_EQUALS(studioSceneExportName(2, 'A', "default-ffflffaaaa"),
		                 Common::String("studio-scene002-A-default-ffflffaaaa.png"));
		TS_ASSERT_EQUALS(studioCompareExportName(2, false, "default", "mvl3"),
		                 Common::String("studio-scene002-AB-default-vs-mvl3.png"));
		TS_ASSERT_EQUALS(studioCompareExportName(2, true, "default", "nref"),
		                 Common::String("studio-scene002-diff-default-vs-nref.png"));
	}
```

Note for the implementer: `test_pass_list_ops` line `!passMove(p, sel, +2)` — the contract is `dir` ∈ {−1, +1}; any other value returns false. Implement it that way.

- [ ] **Step 2: Run tests to verify they fail**

Run: `.\build_tests.ps1`
Expected: compile FAILURE — `StudioDefaults` etc. not declared.

- [ ] **Step 3: Implement**

In `roger_studio_render.h`, after the export-stamp section:

```cpp
// ── Studio v2: per-game startup defaults ─────────────────────────────────────
struct StudioDefaults {
	int picId;   // -1 = first available
	int viewId;  // -1 = first available
	int loopNo;
	int celNo;
	int celX;    // native coords, cel bottom-centre anchor
	int celY;
};

StudioDefaults studioDefaultsForGame(const Common::String &gameId);

// ── Studio v2: pass-list edit ops (pure; unit-tested) ────────────────────────
void passInsertAfter(Common::Array<int> &passes, int &selected, int passVal);
void passRemoveAt(Common::Array<int> &passes, int &selected);
bool passMove(Common::Array<int> &passes, int &selected, int dir); // dir in {-1,+1}

// ── Studio v2: export names ──────────────────────────────────────────────────
Common::String studioSceneExportName(int picId, char slot, const Common::String &detail);
Common::String studioCompareExportName(int picId, bool diff,
                                       const Common::String &stampA,
                                       const Common::String &stampB);
```

In `roger_studio_render.cpp`:

```cpp
StudioDefaults studioDefaultsForGame(const Common::String &gameId) {
	StudioDefaults d = { -1, -1, 0, 0, 160, 150 };
	if (gameId == "sq3") {
		d.picId = 2; d.viewId = 12; d.loopNo = 1; d.celNo = 0;
	}
	return d;
}

void passInsertAfter(Common::Array<int> &passes, int &selected, int passVal) {
	int at = (selected < 0 || selected >= (int)passes.size())
	         ? (int)passes.size() : selected + 1;
	passes.insert_at(at, passVal);
	selected = at;
}

void passRemoveAt(Common::Array<int> &passes, int &selected) {
	if (selected < 0 || selected >= (int)passes.size())
		return;
	passes.remove_at(selected);
	if (passes.empty())
		selected = -1;
	else if (selected >= (int)passes.size())
		selected = (int)passes.size() - 1;
}

bool passMove(Common::Array<int> &passes, int &selected, int dir) {
	if (dir != -1 && dir != 1)
		return false;
	if (selected < 0 || selected >= (int)passes.size())
		return false;
	const int to = selected + dir;
	if (to < 0 || to >= (int)passes.size())
		return false;
	SWAP(passes[selected], passes[to]);
	selected = to;
	return true;
}

Common::String studioSceneExportName(int picId, char slot, const Common::String &detail) {
	return Common::String::format("studio-scene%03d-%c-%s.png", picId, slot, detail.c_str());
}

Common::String studioCompareExportName(int picId, bool diff,
                                       const Common::String &stampA,
                                       const Common::String &stampB) {
	return Common::String::format("studio-scene%03d-%s-%s-vs-%s.png",
		picId, diff ? "diff" : "AB", stampA.c_str(), stampB.c_str());
}
```

(`SWAP` comes from `common/util.h`, already included by the .cpp.)

- [ ] **Step 4: Run tests to verify they pass**

Run: `.\build_tests.ps1`
Expected: all suites pass, including the 3 new tests (106 total).

- [ ] **Step 5: Commit**

```powershell
git add engines/sci/roger/roger_studio_render.h engines/sci/roger/roger_studio_render.cpp test/sci/roger/test_studio_render.h
git commit -m "Roger studio v2: defaults table, pass-list ops, scene export names"
```

---

### Task 2: Widget layer — kinds, id encoding, panel layout, hit-testing

**Files:**
- Modify: `engines/sci/roger/roger_studio_render.h`
- Modify: `engines/sci/roger/roger_studio_render.cpp`
- Modify: `test/sci/roger/test_studio_render.h`

**Interfaces:**
- Consumes: `omyacParamCount()/omyacParamDesc()` (existing), `Common::Rect`.
- Produces (Task 6 relies on these exact names):

```cpp
enum WidKind {
	kWidNone = 0,
	kWidPicPrev, kWidPicNext, kWidViewPrev, kWidViewNext,
	kWidLoopPrev, kWidLoopNext, kWidCelPrev, kWidCelNext,
	kWidVariantCycle, kWidPlateMode, kWidShowView, kWidFit,
	kWidTabA, kWidTabB, kWidShowA, kWidShowB, kWidSplit, kWidDiff,
	kWidCopyAB, kWidExport,
	kWidParamMinus, kWidParamPlus, kWidParamToggle,   // indexed by param
	kWidChip, kWidChipX,                              // indexed by chip
	kWidChipLeft, kWidChipRight,
	kWidChipAddF, kWidChipAddL, kWidChipAddA, kWidChipReset
};

uint32 widId(int kind, int index = 0);   // (kind << 16) | (index & 0xffff)
int widKind(uint32 id);
int widIndex(uint32 id);

struct StudioWidget {
	Common::Rect rect;      // panel-local small coords
	uint32 id;
	Common::String label;
	bool on;                // toggled/active state (drawn highlighted)
	bool enabled;
};

struct StudioPanelState {
	int picId, viewId, loopNo, celNo;
	const char *variantName;
	bool plateNearest;      // active slot's plate mode
	bool showView;
	int activeSlot;         // 0 = A, 1 = B
	int displayMode;        // 0 ShowA, 1 ShowB, 2 Split, 3 Diff
	int selectedChip;       // -1 = none
	Common::Array<int> passes;      // active slot's
	Common::Array<int> paramValues; // active slot's, omyacParamCount() entries
};

// Lays out every widget for the control panel into `out` (cleared first).
// panel = panel-local small rect, i.e. (0, 0, smallW, smallH).
// Pure and deterministic: same state -> same rects. Uses kCharW/kRowH below.
void buildStudioPanel(const Common::Rect &panel, const StudioPanelState &st,
                      Common::Array<StudioWidget> &out);

// First widget whose rect contains (x, y) and is enabled; kWidNone if none.
uint32 hitTestWidgets(const Common::Array<StudioWidget> &widgets, int x, int y);

static const int kStudioCharW = 7;   // layout char width (small px)
static const int kStudioRowH = 24;   // layout row height (small px)
```

**Layout contract** (encode in the implementation; tests assert the invariants, not exact pixels):
- Rows, top to bottom, each `kStudioRowH` tall with 2 px inner padding:
  1. Scene row: `pic` label, `<`, value, `>`, `view` label, `<`, value, `>`, `loop` `<` value `>`, `cel` `<` value `>`, `[variant: <name>]`, `[plate: omyac|nearest]`, `[view: on|off]`, `[Fit]`
  2. Judge row: `edit:` label, `[A]`, `[B]`, gap, `[Show A]`, `[Show B]`, `[Split]`, `[Diff]`, gap, `[Copy A>B]`, `[Export PNG]`
  3. …3+omyacParamCount()−1: one param per row: name label, `[-]`, value, `[+]` for ints; name label, `[on]/[off]` toggle for bools (`omyacParamDesc(i).isBool`)
  4. Chip row (last): `passes:` label, one `[f]`+`[x]` widget pair per pass (chip label `f`/`l`/`a` by value 2/1/0; `on` = selected), then `[<]`, `[>]`, `[+f]`, `[+l]`, `[+a]`, `[Reset]`
- Button width = `label.size() * kStudioCharW + 10`; x-advance = width + 6. Labels (non-clickable text like `pic`, values like `002`) are emitted as widgets with `id == kWidNone`, `enabled == false`, so the studio can draw everything from one list.
- Value labels: pic/view zero-padded 3 digits; loop/cel plain ints.
- Every widget rect must lie fully inside `panel`; enabled widgets must not overlap each other (labels may touch).

- [ ] **Step 1: Write the failing tests**

Append to `test/sci/roger/test_studio_render.h`:

```cpp
	static StudioPanelState samplePanelState() {
		StudioPanelState st;
		st.picId = 2; st.viewId = 12; st.loopNo = 1; st.celNo = 0;
		st.variantName = "scale6x (3x*2x)";
		st.plateNearest = false;
		st.showView = true;
		st.activeSlot = 0;
		st.displayMode = 0;
		st.selectedChip = 1;
		st.passes.push_back(2); st.passes.push_back(2); st.passes.push_back(1);
		OmyacParams p;
		for (int i = 0; i < omyacParamCount(); i++)
			st.paramValues.push_back(omyacParamGet(p, i));
		return st;
	}

	void test_wid_id_roundtrip() {
		uint32 id = widId(kWidParamMinus, 5);
		TS_ASSERT_EQUALS(widKind(id), (int)kWidParamMinus);
		TS_ASSERT_EQUALS(widIndex(id), 5);
		TS_ASSERT_EQUALS(widKind(widId(kWidFit)), (int)kWidFit);
		TS_ASSERT_EQUALS(widIndex(widId(kWidFit)), 0);
	}

	void test_panel_layout_invariants() {
		const Common::Rect panel(0, 0, 1400, 280);
		Common::Array<StudioWidget> w;
		buildStudioPanel(panel, samplePanelState(), w);
		TS_ASSERT(!w.empty());
		// Every expected clickable kind is present at least once.
		static const int MUST[] = {
			kWidPicPrev, kWidPicNext, kWidViewPrev, kWidViewNext,
			kWidLoopPrev, kWidLoopNext, kWidCelPrev, kWidCelNext,
			kWidVariantCycle, kWidPlateMode, kWidShowView, kWidFit,
			kWidTabA, kWidTabB, kWidShowA, kWidShowB, kWidSplit, kWidDiff,
			kWidCopyAB, kWidExport, kWidChipLeft, kWidChipRight,
			kWidChipAddF, kWidChipAddL, kWidChipAddA, kWidChipReset };
		for (uint m = 0; m < ARRAYSIZE(MUST); m++) {
			bool found = false;
			for (uint i = 0; i < w.size(); i++)
				if (widKind(w[i].id) == MUST[m]) { found = true; break; }
			TS_ASSERT(found);
		}
		// Param rows: one minus+plus per int param, one toggle per bool param.
		int minus = 0, plus = 0, toggles = 0;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kWidParamMinus) minus++;
			if (widKind(w[i].id) == kWidParamPlus) plus++;
			if (widKind(w[i].id) == kWidParamToggle) toggles++;
		}
		int intParams = 0, boolParams = 0;
		for (int i = 0; i < omyacParamCount(); i++)
			omyacParamDesc(i).isBool ? boolParams++ : intParams++;
		TS_ASSERT_EQUALS(minus, intParams);
		TS_ASSERT_EQUALS(plus, intParams);
		TS_ASSERT_EQUALS(toggles, boolParams);
		// Chips: one kWidChip + one kWidChipX per pass.
		int chips = 0, xs = 0;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kWidChip) chips++;
			if (widKind(w[i].id) == kWidChipX) xs++;
		}
		TS_ASSERT_EQUALS(chips, 3);
		TS_ASSERT_EQUALS(xs, 3);
		// Geometry: inside panel; enabled widgets pairwise non-overlapping.
		for (uint i = 0; i < w.size(); i++) {
			TS_ASSERT(w[i].rect.left >= panel.left && w[i].rect.top >= panel.top);
			TS_ASSERT(w[i].rect.right <= panel.right && w[i].rect.bottom <= panel.bottom);
			if (!w[i].enabled) continue;
			for (uint j = i + 1; j < w.size(); j++) {
				if (!w[j].enabled) continue;
				Common::Rect a = w[i].rect, b = w[j].rect;
				TS_ASSERT(!(a.left < b.right && b.left < a.right &&
				            a.top < b.bottom && b.top < a.bottom));
			}
		}
	}

	void test_panel_hit_test() {
		const Common::Rect panel(0, 0, 1400, 280);
		Common::Array<StudioWidget> w;
		buildStudioPanel(panel, samplePanelState(), w);
		for (uint i = 0; i < w.size(); i++) {
			if (!w[i].enabled) continue;
			const int cx = (w[i].rect.left + w[i].rect.right) / 2;
			const int cy = (w[i].rect.top + w[i].rect.bottom) / 2;
			TS_ASSERT_EQUALS(hitTestWidgets(w, cx, cy), w[i].id);
		}
		TS_ASSERT_EQUALS(hitTestWidgets(w, panel.right - 1, panel.bottom - 1), (uint32)kWidNone);
		TS_ASSERT_EQUALS(hitTestWidgets(w, -5, -5), (uint32)kWidNone);
	}
```

(If the bottom-right corner of the panel happens to be covered by a widget, move the miss-probe to a coordinate the layout leaves empty — e.g. `panel.right - 1, panel.top + kStudioRowH - 1` between rows — but do not delete the miss assertion.)

- [ ] **Step 2: Run tests to verify they fail**

Run: `.\build_tests.ps1` — compile FAILURE (`widId` undeclared).

- [ ] **Step 3: Implement**

Header declarations exactly as the Interfaces block above. Implementation sketch for `roger_studio_render.cpp` (complete — transcribe and keep the helper local):

```cpp
uint32 widId(int kind, int index) {
	return ((uint32)kind << 16) | ((uint32)index & 0xffff);
}
int widKind(uint32 id) { return (int)(id >> 16); }
int widIndex(uint32 id) { return (int)(id & 0xffff); }

namespace {

struct PanelCursor {
	Common::Array<StudioWidget> *out;
	int x, y;
	int rowH;
	int right;

	void newRow() { x = 4; y += rowH; }

	// Emits one widget; returns its rect. Non-clickable text -> id kWidNone.
	Common::Rect emit(const Common::String &label, uint32 id, bool on, bool enabled) {
		const int wpx = (int)label.size() * kStudioCharW + 10;
		if (x + wpx > right) newRow();          // wrap long rows defensively
		StudioWidget wgt;
		wgt.rect = Common::Rect((int16)x, (int16)(y + 2), (int16)(x + wpx), (int16)(y + rowH - 2));
		wgt.id = id; wgt.label = label; wgt.on = on; wgt.enabled = enabled;
		out->push_back(wgt);
		x += wpx + 6;
		return wgt.rect;
	}
	void text(const Common::String &label) { emit(label, kWidNone, false, false); }
	void btn(const Common::String &label, uint32 id, bool on = false) { emit(label, id, on, true); }
};

} // anonymous namespace

void buildStudioPanel(const Common::Rect &panel, const StudioPanelState &st,
                      Common::Array<StudioWidget> &out) {
	out.clear();
	PanelCursor c;
	c.out = &out; c.x = panel.left + 4; c.y = panel.top; c.rowH = kStudioRowH;
	c.right = panel.right - 4;

	// Row 1: scene
	c.text("pic");
	c.btn("<", widId(kWidPicPrev));
	c.text(Common::String::format("%03d", st.picId));
	c.btn(">", widId(kWidPicNext));
	c.text("view");
	c.btn("<", widId(kWidViewPrev));
	c.text(Common::String::format("%03d", st.viewId));
	c.btn(">", widId(kWidViewNext));
	c.text("loop");
	c.btn("<", widId(kWidLoopPrev));
	c.text(Common::String::format("%d", st.loopNo));
	c.btn(">", widId(kWidLoopNext));
	c.text("cel");
	c.btn("<", widId(kWidCelPrev));
	c.text(Common::String::format("%d", st.celNo));
	c.btn(">", widId(kWidCelNext));
	c.btn(Common::String::format("variant: %s", st.variantName), widId(kWidVariantCycle));
	c.btn(st.plateNearest ? "plate: nearest" : "plate: omyac", widId(kWidPlateMode), st.plateNearest);
	c.btn(st.showView ? "view: on" : "view: off", widId(kWidShowView), st.showView);
	c.btn("Fit", widId(kWidFit));
	c.newRow();

	// Row 2: edit tab + judge
	c.text("edit:");
	c.btn("A", widId(kWidTabA), st.activeSlot == 0);
	c.btn("B", widId(kWidTabB), st.activeSlot == 1);
	c.text(" ");
	c.btn("Show A", widId(kWidShowA), st.displayMode == 0);
	c.btn("Show B", widId(kWidShowB), st.displayMode == 1);
	c.btn("Split", widId(kWidSplit), st.displayMode == 2);
	c.btn("Diff", widId(kWidDiff), st.displayMode == 3);
	c.text(" ");
	c.btn("Copy A>B", widId(kWidCopyAB));
	c.btn("Export PNG", widId(kWidExport));
	c.newRow();

	// Param rows (active slot values)
	for (int i = 0; i < omyacParamCount(); i++) {
		const OmyacParamDesc d = omyacParamDesc(i);
		const int v = (i < (int)st.paramValues.size()) ? st.paramValues[i] : 0;
		c.text(Common::String::format("%-20s", d.name));
		if (d.isBool) {
			c.btn(v ? "on" : "off", widId(kWidParamToggle, i), v != 0);
		} else {
			c.btn("-", widId(kWidParamMinus, i));
			c.text(Common::String::format("%d", v));
			c.btn("+", widId(kWidParamPlus, i));
		}
		c.newRow();
	}

	// Chip row
	c.text("passes:");
	for (uint i = 0; i < st.passes.size(); i++) {
		const char ch = st.passes[i] == 2 ? 'f' : st.passes[i] == 1 ? 'l' : 'a';
		c.btn(Common::String::format("%c", ch), widId(kWidChip, (int)i), (int)i == st.selectedChip);
		c.btn("x", widId(kWidChipX, (int)i));
	}
	if (st.passes.empty())
		c.text("(none - wireframe)");
	c.btn("<", widId(kWidChipLeft));
	c.btn(">", widId(kWidChipRight));
	c.btn("+f", widId(kWidChipAddF));
	c.btn("+l", widId(kWidChipAddL));
	c.btn("+a", widId(kWidChipAddA));
	c.btn("Reset", widId(kWidChipReset));
}

uint32 hitTestWidgets(const Common::Array<StudioWidget> &widgets, int x, int y) {
	for (uint i = 0; i < widgets.size(); i++) {
		if (!widgets[i].enabled)
			continue;
		const Common::Rect &r = widgets[i].rect;
		if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
			return widgets[i].id;
	}
	return (uint32)kWidNone;
}
```

Note the defensive row-wrap in `emit()` — at panel small width 1400 the scene row fits, but the wrap keeps the invariant tests true at any width.

- [ ] **Step 4: Run tests to verify they pass**

Run: `.\build_tests.ps1` — all pass (3 new tests; 109 total).

- [ ] **Step 5: Commit**

```powershell
git add engines/sci/roger/roger_studio_render.h engines/sci/roger/roger_studio_render.cpp test/sci/roger/test_studio_render.h
git commit -m "Roger studio v2: widget layer - panel layout builder + hit-testing"
```

---

### Task 3: Diff + SAD helpers and shift-lock tests

**Files:**
- Modify: `engines/sci/roger/roger_studio_render.h`
- Modify: `engines/sci/roger/roger_studio_render.cpp`
- Create: `test/sci/roger/test_shift_lock.h`
- Modify: `test/sci/roger/test_studio_render.h` (diff/SAD unit tests)
- Modify: `build_tests.ps1` (register `test_shift_lock.h` in `$RogerTestHeaders`)

**Interfaces:**
- Consumes: `renderOmyac`/`nativePreRender`/`defaultPasses` (existing), `scale6x`/`scaleNearest`/`IndexImage` (existing).
- Produces (Task 8 relies on these exact names):
  - `void diffMapRGBA(const byte *a, const byte *b, int w, int h, byte *out);` — all three buffers are w*h*4 RGBA (R,G,B,A byte order as laid out by fmt(4,8,8,8,8,24,16,8,0) — i.e. compare bytes 0..2 of each 4-byte pixel, whatever channel they hold; symmetry makes channel order irrelevant). Per pixel: `m = MAX(|a0−b0|, |a1−b1|, |a2−b2|)` → out pixel = (m, m, m, 255) in the same byte layout with the 4th byte = a's 4th byte position value 255. White-on-black.
  - `bool estimateOffsetSAD(const byte *a, const byte *b, int w, int h, int radius, int &outDx, int &outDy);` — for every (dx, dy) in [−radius, +radius]², SAD over the first 3 bytes of each pixel of the overlapping region of `a` shifted by (dx, dy) against `b`, normalized by overlap pixel count; (outDx, outDy) = argmin. Ties break toward (0, 0) (smaller |dx|+|dy| wins, then smaller dy, then dx). Returns false if w or h ≤ 2*radius.

- [ ] **Step 1: Write the failing unit tests (diff/SAD)**

Append to `test/sci/roger/test_studio_render.h`:

```cpp
	void test_diff_map() {
		byte a[4 * 4], b[4 * 4], out[4 * 4]; // 2x2 px
		memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
		for (int i = 0; i < 4; i++) { a[i * 4 + 3] = 255; b[i * 4 + 3] = 255; }
		b[0] = 200; b[1] = 50; // pixel 0 differs: channel deltas 200, 50
		diffMapRGBA(a, b, 2, 2, out);
		TS_ASSERT_EQUALS(out[0], 200); // max delta
		TS_ASSERT_EQUALS(out[1], 200);
		TS_ASSERT_EQUALS(out[2], 200);
		TS_ASSERT_EQUALS(out[3], 255);
		TS_ASSERT_EQUALS(out[4], 0);   // identical pixel -> black
		TS_ASSERT_EQUALS(out[7], 255);
	}

	void test_sad_offset_zero_and_shift() {
		// 16x16 image with a bright 3x3 square at (6,6).
		const int W = 16, H = 16;
		Common::Array<byte> a, shifted;
		a.resize(W * H * 4, 0); shifted.resize(W * H * 4, 0);
		for (int i = 0; i < W * H; i++) { a[i * 4 + 3] = 255; shifted[i * 4 + 3] = 255; }
		for (int y = 6; y < 9; y++)
			for (int x = 6; x < 9; x++) {
				a[(y * W + x) * 4 + 0] = 255;
				shifted[(y * W + (x + 1)) * 4 + 0] = 255; // same square, 1 px right
			}
		int dx = 99, dy = 99;
		TS_ASSERT(estimateOffsetSAD(a.begin(), a.begin(), W, H, 3, dx, dy));
		TS_ASSERT_EQUALS(dx, 0); TS_ASSERT_EQUALS(dy, 0);
		TS_ASSERT(estimateOffsetSAD(a.begin(), shifted.begin(), W, H, 3, dx, dy));
		TS_ASSERT_EQUALS(dx, 1);  // shifting a by +1 aligns it with shifted
		TS_ASSERT_EQUALS(dy, 0);
		TS_ASSERT(!estimateOffsetSAD(a.begin(), shifted.begin(), 6, 6, 3, dx, dy)); // too small
	}
```

Sign convention (write it as a comment on `estimateOffsetSAD`): the result is the displacement to apply to `a` to best align it with `b` — "dx=+1 means b's content sits 1 px right of a's".

- [ ] **Step 2: Write the failing shift-lock tests**

Create `test/sci/roger/test_shift_lock.h` (no GPL block, matching sibling test headers):

```cpp
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_scale.h"
#include "sci/roger/roger_pic_native.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_studio_render.h"
using namespace Sci::Roger;

// Locks against sub-pixel drift ("shifting") in the upscalers. If one of these
// fails after a pipeline change, the pipeline gained a systematic dx/dy bias —
// exactly the class of bug the studio's Diff/offset tools diagnose visually.
class RogerShiftLockTestSuite : public CxxTest::TestSuite {
	// Centroid of non-zero mass, in output pixels.
	static void centroid(const Common::Array<byte> &px, int w, int h, byte zero,
	                     double &cx, double &cy) {
		double sx = 0, sy = 0; long n = 0;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				if (px[y * w + x] != zero) { sx += x; sy += y; n++; }
		cx = n ? sx / n : 0; cy = n ? sy / n : 0;
	}

public:
	// scale6x of an isolated pixel must occupy exactly its 6x6 cell:
	// centroid at (x*6+2.5, y*6+2.5).
	void test_scale6x_isolated_pixel_centered() {
		IndexImage in;
		in.w = 9; in.h = 9;
		in.pixels.resize(81, 0);
		in.pixels[4 * 9 + 4] = 7; // centre pixel set
		IndexImage out = scale6x(in);
		double cx, cy;
		centroid(out.pixels, out.w, out.h, 0, cx, cy);
		TS_ASSERT_DELTA(cx, 4 * 6 + 2.5, 0.51);
		TS_ASSERT_DELTA(cy, 4 * 6 + 2.5, 0.51);
	}

	// The default omyac pipeline over a symmetric fixture must keep the output
	// mass centred where the nearest-neighbour reference puts it. Fixture: a
	// filled square outline, symmetric under 180-degree rotation.
	//
	// TOLERANCE POLICY: 1.0 hybrid px (= 1/6 native px). If this FAILS, do NOT
	// widen the tolerance — the failure IS the diagnosis of a real drift.
	// Report the measured (dx, dy) in your task report and escalate
	// (DONE_WITH_CONCERNS); the user explicitly wants to know.
	void test_omyac_default_pipeline_centered_vs_nearest_ref() {
		Common::Array<DrawCommand> cmds;
		DrawCommand box; box.kind = kCmdPline; box.drawMode = kDrawVisual; box.drawCodes[0] = 2;
		Point p1 = {40, 40}, p2 = {120, 40}, p3 = {120, 100}, p4 = {40, 100}, p5 = {40, 40};
		box.points.push_back(p1); box.points.push_back(p2); box.points.push_back(p3);
		box.points.push_back(p4); box.points.push_back(p5);
		cmds.push_back(box);
		NativeRef ref = nativePreRender(cmds);

		// Nearest reference: refPixel (320x190 doubled-nibble, 0xff = untouched)
		// replicated x6. Its centroid is exact by construction.
		IndexImage nat;
		nat.w = OMYAC_NATIVE_W; nat.h = OMYAC_NATIVE_H;
		nat.pixels = ref.refPixel;
		IndexImage nref = scaleNearest(nat, OMYAC_SCALE);
		double nx, ny;
		centroid(nref.pixels, nref.w, nref.h, 0xff, nx, ny);

		OmyacResult omyac = renderOmyac(ref, defaultPasses());
		double ox, oy;
		centroid(omyac.pixels, OMYAC_HYBRID_W, OMYAC_HYBRID_H, 0xff, ox, oy);

		TS_ASSERT_DELTA(ox, nx, 1.0);
		TS_ASSERT_DELTA(oy, ny, 1.0);
	}
};
```

Implementer notes for this file:
- Verify the `DrawCommand`/`Point` field spellings against `roger_draw_command.h` and the fixture in `test/sci/roger/test_omyac_params.h` (which builds `kCmdPline` commands the same way) — copy the working incantation from there.
- `renderOmyac` fills null pixels at the end (`fillNullPixels`) — if `omyac.pixels` ends up with no 0xff left, weight the centroid by "non-background" instead: compute the background byte as the most common value and pass THAT as `zero`. Adjust the helper only if the naive version fails for this mechanical reason (mass ≈ whole image); do not touch the tolerance.
- If `test_omyac_default_pipeline_centered_vs_nearest_ref` fails with a genuine offset (centroids differ by more than 1.0 in a consistent direction): this is a REAL FINDING (the user's suspected rounding drift). Report status DONE_WITH_CONCERNS with the measured deltas. Leave the test failing? No — mark it with the measured values in the report, then lock CURRENT behavior by asserting the measured deltas (e.g. `TS_ASSERT_DELTA(ox - nx, <measured>, 0.2)`) with a `// KNOWN DRIFT — see report` comment, so the suite stays green while the drift is tracked openly. The controller/user decides whether to fix the pipeline.

- [ ] **Step 3: Register the new test header**

In `build_tests.ps1`, add `"test_shift_lock.h"` to `$RogerTestHeaders`, exactly like the existing entries.

- [ ] **Step 4: Run tests to verify they fail**

Run: `.\build_tests.ps1` — compile FAILURE (`diffMapRGBA` undeclared).

- [ ] **Step 5: Implement diff/SAD in `roger_studio_render.{h,cpp}`**

Header:

```cpp
// ── Studio v2: shift diagnosis (pure; unit-tested) ───────────────────────────
// Per-pixel |a-b| map, white-on-black: out = (m,m,m,255) with m = max channel
// delta of the first 3 bytes. All buffers w*h*4 bytes, same layout.
void diffMapRGBA(const byte *a, const byte *b, int w, int h, byte *out);

// Best global alignment of a against b over (dx,dy) in [-radius,+radius]^2,
// SAD over the first 3 bytes/pixel, normalized by overlap area. Result is the
// displacement applied to a that minimizes SAD: dx=+1 means b's content sits
// 1 px right of a's. Ties prefer smaller |dx|+|dy| (0,0 first). False if the
// image is too small (w or h <= 2*radius).
bool estimateOffsetSAD(const byte *a, const byte *b, int w, int h, int radius,
                       int &outDx, int &outDy);
```

Implementation:

```cpp
void diffMapRGBA(const byte *a, const byte *b, int w, int h, byte *out) {
	const int n = w * h;
	for (int i = 0; i < n; i++) {
		const byte *pa = a + i * 4, *pb = b + i * 4;
		int m = 0;
		for (int c = 0; c < 3; c++) {
			const int d = (int)pa[c] - (int)pb[c];
			m = MAX(m, d < 0 ? -d : d);
		}
		byte *po = out + i * 4;
		po[0] = po[1] = po[2] = (byte)m;
		po[3] = 255;
	}
}

bool estimateOffsetSAD(const byte *a, const byte *b, int w, int h, int radius,
                       int &outDx, int &outDy) {
	if (w <= 2 * radius || h <= 2 * radius)
		return false;
	double best = -1.0;
	int bestDx = 0, bestDy = 0, bestDist = 0;
	for (int dy = -radius; dy <= radius; dy++) {
		for (int dx = -radius; dx <= radius; dx++) {
			// a sampled at (x, y), b at (x + dx, y + dy), over the overlap.
			const int x0 = MAX(0, -dx), x1 = MIN(w, w - dx);
			const int y0 = MAX(0, -dy), y1 = MIN(h, h - dy);
			uint64 sad = 0;
			for (int y = y0; y < y1; y++) {
				const byte *ra = a + (y * w + x0) * 4;
				const byte *rb = b + ((y + dy) * w + (x0 + dx)) * 4;
				for (int x = x0; x < x1; x++, ra += 4, rb += 4)
					for (int c = 0; c < 3; c++) {
						const int d = (int)ra[c] - (int)rb[c];
						sad += (uint64)(d < 0 ? -d : d);
					}
			}
			const long overlap = (long)(x1 - x0) * (y1 - y0);
			if (overlap <= 0)
				continue;
			const double norm = (double)sad / (double)overlap;
			const int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
			if (best < 0 || norm < best ||
			    (norm == best && dist < bestDist)) {
				best = norm; bestDx = dx; bestDy = dy; bestDist = dist;
			}
		}
	}
	outDx = bestDx; outDy = bestDy;
	return true;
}
```

(The loop order dy-then-dx with the `dist < bestDist` tie-break gives the "(0,0) wins ties" contract because strictly-smaller distance is required to replace an equal-SAD candidate; (0,0) is evaluated before larger-|d| candidates of equal SAD only when radius ordering reaches it first — the explicit `dist` tie-break makes the contract hold regardless of visit order.)

- [ ] **Step 6: Run tests to verify they pass**

Run: `.\build_tests.ps1`
Expected: all suites pass — including the 2 new studio_render tests AND the 2 shift-lock tests (113 total). **Watch `test_omyac_default_pipeline_centered_vs_nearest_ref` closely** — see the escalation policy in Step 2's notes if it reveals a real drift.

- [ ] **Step 7: Commit**

```powershell
git add engines/sci/roger/roger_studio_render.h engines/sci/roger/roger_studio_render.cpp test/sci/roger/test_studio_render.h test/sci/roger/test_shift_lock.h build_tests.ps1
git commit -m "Roger studio v2: diff map + SAD offset estimator; shift-lock centroid tests"
```

---

### Task 4: `RogerAssetGen::generatePlateNearest` — the reference plate

**Files:**
- Modify: `engines/sci/roger/roger_asset_gen.h`
- Modify: `engines/sci/roger/roger_asset_gen.cpp`

**Interfaces:**
- Consumes: `parsePic`, `nativePreRender` (already used by `generatePlateWithIndex`), `scaleNearest`, file-local `blendToSurface` (roger_asset_gen.cpp — maps a doubled-nibble byte buffer to an RGBA surface; used at `generatePlateWithIndex`'s tail).
- Produces (Task 5 relies on this exact name):
  - `Graphics::Surface *generatePlateNearest(int id, uint32 &outMs);` — the native pre-render (`NativeRef.refPixel`, 320×190 doubled-nibble bytes) replicated ×6 (nearest) and blended to RGBA. Zero-shift by construction. Caller owns. **Never cached** (no cache read or write in any mode); returns nullptr in `kGenPrebuilt` and on any failure, like `generatePlate`.

- [ ] **Step 1: Add the declaration** to `roger_asset_gen.h`, after `generatePlateWithIndex`:

```cpp
	/**
	 * Reference plate for shift diagnosis: the native 320x190 pre-render
	 * (NativeRef.refPixel) replicated x6 nearest-neighbour — geometrically
	 * exact by construction (every native pixel -> exactly one 6x6 block).
	 * Studio-only; never cached. Caller owns (->free() then delete).
	 */
	Graphics::Surface *generatePlateNearest(int id, uint32 &outMs);
```

- [ ] **Step 2: Implement** in `roger_asset_gen.cpp`, after `generatePlateWithIndex` (mirror its resource-fetch preamble exactly; note `blendToSurface` is file-local above — check its exact signature there and pass the expanded buffer with `OMYAC_HYBRID_W/H`):

```cpp
Graphics::Surface *RogerAssetGen::generatePlateNearest(int id, uint32 &outMs) {
	outMs = 0;
	if (_mode == kGenPrebuilt)
		return nullptr;
#ifdef ENABLE_SCI
	if (!g_sci)
		return nullptr;
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan)
		return nullptr;
	Resource *res = resMan->findResource(ResourceId(kResourceTypePic, (uint16)id), false);
	if (!res || res->size() == 0)
		return nullptr;

	uint32 t0 = g_system->getMillis();
	Common::Array<DrawCommand> cmds = parsePic(res->data(), (uint32)res->size());
	NativeRef ref = nativePreRender(cmds);

	// refPixel is 320x190 doubled-nibble bytes (0xff init == EGA white, which
	// is also SCI0's screen-clear colour, so untouched pixels render correctly).
	IndexImage nat;
	nat.w = OMYAC_NATIVE_W;
	nat.h = OMYAC_NATIVE_H;
	nat.pixels = ref.refPixel;
	IndexImage big = scaleNearest(nat, OMYAC_SCALE);

	Graphics::Surface *plate = blendToSurface(big.pixels, OMYAC_HYBRID_W, OMYAC_HYBRID_H);
	outMs = g_system->getMillis() - t0;
	return plate;
#else
	(void)id;
	return nullptr;
#endif
}
```

If `blendToSurface`'s real signature differs (e.g. it takes a non-const reference and consumes the buffer), adapt the call — `big.pixels` is already a throwaway copy, so consumption is fine.

- [ ] **Step 3: Regression gate**

Run: `.\build_tests.ps1` — ALL suites still pass (no behavior change to existing methods; the new method is engine-gated and untestable headlessly).
Run: `.\build_and_run.ps1 -NoLaunch` — build succeeds.

- [ ] **Step 4: Commit**

```powershell
git add engines/sci/roger/roger_asset_gen.h engines/sci/roger/roger_asset_gen.cpp
git commit -m "Roger: generatePlateNearest - zero-shift nearest-ref plate for studio diagnosis"
```

---

### Task 5: Studio core rework — slots, scene state, renderSlot; strip v1 UI

**Files:**
- Modify: `engines/sci/roger/roger_studio.h` (full replacement of the private section)
- Modify: `engines/sci/roger/roger_studio.cpp` (major rework)

**Interfaces:**
- Consumes: `generatePlate`/`generatePlateNearest`/`nativeCelIndexImage`/`surfaceFromIndex`/`setOmyacParams`/`setEnhancePasses` (RogerAssetGen), `applyScalerVariant`/`scalerVariantFactor`/`scalerVariantName` (Task-2-era helpers), `studioDefaultsForGame` (Task 1), `omyacParamStamp`/`omyacPassStamp`, `defaultPasses`, `Roger::dumpSurfacePng`, `blendBlitFrom` (the compositor's alpha-blit incantation, already used in v1's combined mode).
- Produces (Tasks 6–8 add to this class; these exact members must exist):

New `roger_studio.h` private section (replaces everything from `enum Mode` down; public API unchanged):

```cpp
private:
	enum PlateMode { kPlateOmyac = 0, kPlateNearestRef };
	enum Display { kShowA = 0, kShowB, kShowSplit, kShowDiff };

	struct Slot {
		OmyacParams        params;
		Common::Array<int> passes;               // starts = defaultPasses()
		int                variant = kScaler6x;  // factor-6 only
		PlateMode          plateMode = kPlateOmyac;
		Graphics::Surface *render = nullptr;     // cached scene render (1920x1140 RGBA)
		bool               stale = true;
		uint32             renderMs = 0;
	};

	// Frame / input
	void handleEvent(const Common::Event &ev);
	void drawFrame();                // compose scene area + panel into _display, push
	void drawPanel();                // Task 6
	void dispatchWidget(uint32 id);  // Task 6
	void markDirty() { _dirty = true; }

	// Rendering
	void renderSlot(Slot &slot);     // plate (+ cel) -> slot.render
	Slot &activeSlot() { return _slots[_activeSlot]; }
	void invalidateScene() { _slots[0].stale = _slots[1].stale = true; markDirty(); }
	void invalidateActive() { activeSlot().stale = true; markDirty(); }
	void ensureFresh(Slot &slot) { if (slot.stale) renderSlot(slot); }
	Common::String slotStamp(const Slot &slot) const; // "default-ffflffaaaa-6x[-nref]"
	void exportShown();              // Task 8 extends for split/diff

	// Scene-area geometry (Task 7 fills the interaction)
	Common::Rect sceneArea() const;  // _display minus the panel strip
	void fitView();                  // zoom/pan so the plate fits sceneArea
	bool displayToNative(int mx, int my, int &nx, int &ny) const;

	static const int kPanelH = 560;  // overlay px (drawn 2x from a small surface)

	RogerAssetGen        _gen;       // kGenMemory, empty cache dir
	Graphics::ManagedSurface *_display = nullptr;

	bool  _dirty = true;
	bool  _quit = false;

	Slot _slots[2];
	int  _activeSlot = 0;            // 0 = A, 1 = B
	int  _displayMode = kShowA;      // Display

	// Shared scene state
	Common::Array<int> _picIds;  int _picIdx = 0;
	Common::Array<int> _viewIds; int _viewIdx = 0;
	int _loopNo = 0, _celNo = 0;
	int _celX = 160, _celY = 150;    // native coords, cel BOTTOM-CENTRE anchor
	bool _showView = true;

	// View transform (scene area only)
	float _viewScale = 1.0f;         // set by fitView()
	float _fitScale = 1.0f;
	int _panX = 0, _panY = 0;        // overlay px offset of plate origin in sceneArea
	bool _draggingCel = false;
	bool _panning = false;
	int _dragLastX = 0, _dragLastY = 0;
	Common::Rect _celScreenRect;     // last-drawn cel rect in _display coords (for drag hit)

	// Panel (Task 6)
	Common::Array<StudioWidget> _widgets;   // panel-local small coords
	int _selectedChip = -1;
	uint32 _hoverWid = 0;

	Common::String _status;
	Common::String _offsetReadout;   // Task 8: SAD readout line (Diff only)
```

**What is DELETED from v1** (be explicit — remove all of it): `enum Mode`, `_mode`, `rerender()`, `renderPicMode()`, `renderViewMode()`, `renderCombinedMode()`, `setCurrent()`, `exportCurrent()`, `_current/_previous/_baseline` + labels + `_showPrevious`/`_split`, `_showKeymap`, `drawHud()` and the keymap text, `kHudH`, `_params`/`_passes`/`_paramCursor`/`_passCursor` (replaced by slots), `_variant`/`_spriteX`/`_spriteY` (replaced by per-slot variant and `_celX/_celY`), `_zoomIdx`/`ZOOM_STEPS` (replaced by `_viewScale`), `_lastRenderMs`, ALL keyboard handling except Esc and E, and the entire mode-gated key dispatch.

- [ ] **Step 1: Rewrite `roger_studio.h`** with the private section above (keep the class comment, public ctor/dtor/`run()`, and the includes; add `#include "common/rect.h"` if not present — it is).

- [ ] **Step 2: Rework `roger_studio.cpp`**

Constructor — keep the v1 resource enumeration, then apply defaults:

```cpp
RogerStudio::RogerStudio(const Common::String &gameId)
	: _gen(gameId, "", kGenMemory) {
	_slots[0].passes = defaultPasses();
	_slots[1].passes = defaultPasses();
#ifdef ENABLE_SCI
	// ... existing pic/view enumeration into _picIds/_viewIds, unchanged ...
#endif
	const StudioDefaults d = studioDefaultsForGame(gameId);
	_celX = d.celX; _celY = d.celY;
	if (d.picId >= 0)
		for (uint i = 0; i < _picIds.size(); i++)
			if (_picIds[i] == d.picId) { _picIdx = (int)i; break; }
	if (d.viewId >= 0)
		for (uint i = 0; i < _viewIds.size(); i++)
			if (_viewIds[i] == d.viewId) { _viewIdx = (int)i; _loopNo = d.loopNo; _celNo = d.celNo; break; }
}
```

Destructor: free `_display` and both `_slots[i].render` (`->free(); delete`).

`renderSlot` (the single scene renderer — replaces all three v1 modes):

```cpp
void RogerStudio::renderSlot(Slot &slot) {
	slot.stale = false;
	if (slot.render) { slot.render->free(); delete slot.render; slot.render = nullptr; }
	if (_picIds.empty()) { _status = "no pic resources"; markDirty(); return; }

	_gen.setEnhancePasses(slot.passes);
	_gen.setOmyacParams(slot.params);
	uint32 ms = 0;
	Graphics::Surface *plate = (slot.plateMode == kPlateNearestRef)
		? _gen.generatePlateNearest(_picIds[_picIdx], ms)
		: _gen.generatePlate(_picIds[_picIdx], ms);
	if (!plate) {
		_status = Common::String::format("pic %d: generation FAILED", _picIds[_picIdx]);
		markDirty();
		return;
	}

	Graphics::ManagedSurface composed(plate->w, plate->h, plate->format);
	composed.blitFrom(*plate);
	plate->free(); delete plate;

	if (_showView && !_viewIds.empty()) {
		// Clamp loop/cel against real counts (same #ifdef ENABLE_SCI block as v1
		// renderViewMode used — keep it verbatim, it still lives in this file).
		IndexImage cel;
		byte clearKey = 0;
		if (_gen.nativeCelIndexImage(_viewIds[_viewIdx], _loopNo, _celNo, cel, clearKey)) {
			IndexImage scaled = applyScalerVariant(slot.variant, cel);
			Graphics::Surface *celSurf = _gen.surfaceFromIndex(scaled, clearKey);
			if (celSurf) {
				// Bottom-centre anchor at (_celX, _celY) native.
				const int dx = _celX * 6 - celSurf->w / 2;
				const int dy = _celY * 6 - celSurf->h;
				composed.blendBlitFrom(*celSurf,
					Common::Rect(0, 0, celSurf->w, celSurf->h),
					Common::Rect(dx, dy, dx + celSurf->w, dy + celSurf->h),
					Graphics::FLIP_NONE);
				celSurf->free(); delete celSurf;
			} else {
				_status = "cel render failed; plate only";
			}
		} else {
			_status = "cel extraction failed; plate only";
		}
	}

	slot.render = new Graphics::Surface();
	slot.render->copyFrom(composed.rawSurface());
	slot.renderMs = ms;
	markDirty();
}
```

(Move the v1 loop/cel clamp block — `GfxView *view = g_sci->_gfxCache->getView(...)` etc. — to the top of `renderSlot`, before `nativeCelIndexImage`. The `blendBlitFrom` incantation is v1's own combined-mode call; keep the same argument spellings. `Graphics::FLIP_NONE` may not need the namespace — match v1.)

`slotStamp`:

```cpp
Common::String RogerStudio::slotStamp(const Slot &slot) const {
	Common::String s = omyacParamStamp(slot.params) + "-" + omyacPassStamp(slot.passes);
	if (slot.plateMode == kPlateNearestRef)
		s += "-nref";
	return s;
}
```

`run()` — same loop as v1 (`showOverlay(false)`, allocate `_display`, poll/dirty/delay), but instead of `rerender()` call `fitView(); ensureFresh(_slots[0]);` before the loop.

`handleEvent` — this task's scope: `EVENT_QUIT`/`EVENT_RETURN_TO_LAUNCHER` → quit; `KEYDOWN` Esc → quit, `e` → `exportShown()`; everything else ignored for now (mouse arrives in Tasks 6–7).

`drawFrame` — this task's scope (Task 7 adds split/diff, Task 6 adds the panel): fill `_display` dark gray; `ensureFresh` the slot named by `_displayMode` (kShowA→slot 0, kShowB→slot 1; treat Split/Diff as A for now); blit `slot.render` into `sceneArea()` at `_panX/_panY` scaled by `_viewScale` (reuse v1's src/dst mapping math from its `drawFrame`, replacing `ZOOM_STEPS[_zoomIdx]` with `_viewScale`); record `_celScreenRect` (map `_celX/_celY` + cel dims through the same transform; empty rect when `!_showView`); call `drawPanel()` (stub this task: fill the panel strip black, draw `_status` with the FontMan small-then-2x pattern from v1's drawHud, including the warn-once null-font guard); push + `updateScreen()` as v1.

`sceneArea`/`fitView`/`displayToNative`:

```cpp
Common::Rect RogerStudio::sceneArea() const {
	return Common::Rect(0, 0, _display->w, _display->h - kPanelH);
}

void RogerStudio::fitView() {
	const Common::Rect area = sceneArea();
	const float sx = (float)area.width() / (float)OMYAC_HYBRID_W;
	const float sy = (float)area.height() / (float)OMYAC_HYBRID_H;
	_fitScale = MIN(sx, sy);
	_viewScale = _fitScale;
	_panX = (area.width() - (int)(OMYAC_HYBRID_W * _viewScale)) / 2;
	_panY = (area.height() - (int)(OMYAC_HYBRID_H * _viewScale)) / 2;
	markDirty();
}

bool RogerStudio::displayToNative(int mx, int my, int &nx, int &ny) const {
	const Common::Rect area = sceneArea();
	if (!area.contains((int16)mx, (int16)my))
		return false;
	const float px = (mx - area.left - _panX) / _viewScale; // plate coords
	const float py = (my - area.top - _panY) / _viewScale;
	nx = (int)(px / 6.0f); ny = (int)(py / 6.0f);
	if (nx < 0 || nx > 319 || ny < 0 || ny > 189)
		return false;
	return true;
}
```

`exportShown` — this task's minimal version (Task 8 extends): export the shown slot's render via `dumpSurfacePng` with name `studioSceneExportName(_picIds[_picIdx], _activeSlot == 0 ? 'A' : 'B', sanitize(slotStamp(...)))` — keep v1's sanitize loop and screenshotpath/dir-fallback logic verbatim (copy from v1 `exportCurrent` before deleting it). Actually export the slot selected by `_displayMode` (A or B; Split/Diff treated as A until Task 8).

- [ ] **Step 3: Build + smoke**

Run: `.\build_and_run.ps1 -NoLaunch` — build succeeds.
Run: `.\build_and_run.ps1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin` (NO `-Game`: SQ3 is the harness default and the defaults under test are SQ3's). Expected: exits on its own; a `studio-scene002-A-default-*.png` lands in the screenshotpath (pic 2 plate with the view-12 cel composited at 160,150). Read the PNG to eyeball: SQ3 room-2 plate with Roger's sprite standing lower-centre. Note: the current smoke script still presses F1 twice — harmless now (unbound); Task 8 cleans the script.
Run: `.\build_tests.ps1` — all suites pass (studio has no unit tests; this guards the helpers).

- [ ] **Step 4: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp
git commit -m "Roger studio v2: single-scene core - A/B slots, renderSlot, SQ3 defaults; v1 modes/keys removed"
```

---

### Task 6: Panel wiring — draw widgets, hover, click dispatch

**Files:**
- Modify: `engines/sci/roger/roger_studio.h` (only if a helper decl is needed)
- Modify: `engines/sci/roger/roger_studio.cpp`

**Interfaces:**
- Consumes: `buildStudioPanel`/`hitTestWidgets`/`StudioPanelState`/`StudioWidget`/wid kinds (Task 2), pass ops (Task 1), `omyacParamGet/Set/Desc/Count`, `scalerVariantFactor/Name`.
- Produces: working `drawPanel()` + `dispatchWidget(uint32)`; mouse clicks on the panel fully drive the studio.

- [ ] **Step 1: Implement `drawPanel()`**

Pattern (small surface at half resolution, 2× blit — v1's drawHud approach):

```cpp
void RogerStudio::drawPanel() {
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (!font) { /* keep v1's warn-once guard */ return; }
	const int smallW = _display->w / 2, smallH = kPanelH / 2;
	Graphics::ManagedSurface small(smallW, smallH, _display->format);
	const uint32 bg = _display->format.RGBToColor(0, 0, 0);
	const uint32 fg = _display->format.RGBToColor(220, 220, 220);
	const uint32 hi = _display->format.RGBToColor(255, 255, 0);
	const uint32 hov = _display->format.RGBToColor(90, 90, 140);
	small.fillRect(Common::Rect(smallW, smallH), bg);

	// Rebuild widgets from current state (cheap; keeps rects in lockstep with state).
	StudioPanelState st;
	const Slot &s = _slots[_activeSlot];
	st.picId = _picIds.empty() ? -1 : _picIds[_picIdx];
	st.viewId = _viewIds.empty() ? -1 : _viewIds[_viewIdx];
	st.loopNo = _loopNo; st.celNo = _celNo;
	st.variantName = scalerVariantName(s.variant);
	st.plateNearest = s.plateMode == kPlateNearestRef;
	st.showView = _showView;
	st.activeSlot = _activeSlot;
	st.displayMode = _displayMode;
	st.selectedChip = _selectedChip;
	st.passes = s.passes;
	for (int i = 0; i < omyacParamCount(); i++)
		st.paramValues.push_back(omyacParamGet(s.params, i));
	buildStudioPanel(Common::Rect(0, 0, smallW, smallH - kStudioRowH), st, _widgets);

	for (uint i = 0; i < _widgets.size(); i++) {
		const StudioWidget &wg = _widgets[i];
		if (wg.enabled) {
			if (wg.id == _hoverWid)
				small.fillRect(wg.rect, hov);
			small.frameRect(wg.rect, wg.on ? hi : fg);
		}
		font->drawString(&small, wg.label, wg.rect.left + 4, wg.rect.top + 2,
		                 wg.rect.width() - 6, wg.on ? hi : fg);
	}

	// Bottom line: render ms + status + (Task 8) offset readout.
	font->drawString(&small, Common::String::format("%ums  %s  %s",
		s.renderMs, _status.c_str(), _offsetReadout.c_str()),
		4, smallH - kStudioRowH + 4, smallW - 8, hi);

	const Common::Rect srcR(0, 0, smallW, smallH);
	const Common::Rect dstR(0, _display->h - kPanelH, _display->w, _display->h);
	_display->blitFrom(small.rawSurface(), srcR, dstR);
}
```

- [ ] **Step 2: Mouse → panel routing in `handleEvent`**

```cpp
	case Common::EVENT_MOUSEMOVE: {
		const int panelTop = _display->h - kPanelH;
		uint32 h = 0;
		if (ev.mouse.y >= panelTop)
			h = hitTestWidgets(_widgets, ev.mouse.x / 2, (ev.mouse.y - panelTop) / 2);
		if (h != _hoverWid) { _hoverWid = h; markDirty(); }
		// Task 7 adds cel-drag / pan handling for scene-area moves here.
		return;
	}
	case Common::EVENT_LBUTTONDOWN: {
		const int panelTop = _display->h - kPanelH;
		if (ev.mouse.y >= panelTop) {
			const uint32 id = hitTestWidgets(_widgets, ev.mouse.x / 2, (ev.mouse.y - panelTop) / 2);
			if (id != (uint32)kWidNone)
				dispatchWidget(id);
			return;
		}
		// Task 7 handles scene-area clicks.
		return;
	}
```

(Mouse↔small mapping: the panel small surface is `_display` at half scale, so divide by 2 after subtracting `panelTop`; x divides directly because the small surface spans the full width at half scale.)

- [ ] **Step 3: Implement `dispatchWidget`**

```cpp
void RogerStudio::dispatchWidget(uint32 id) {
	Slot &s = activeSlot();
	const int kind = widKind(id), idx = widIndex(id);
	switch (kind) {
	case kWidPicPrev: case kWidPicNext:
		if (_picIds.empty()) break;
		_picIdx = (_picIdx + (kind == kWidPicPrev ? (int)_picIds.size() - 1 : 1)) % (int)_picIds.size();
		invalidateScene(); break;
	case kWidViewPrev: case kWidViewNext:
		if (_viewIds.empty()) break;
		_viewIdx = (_viewIdx + (kind == kWidViewPrev ? (int)_viewIds.size() - 1 : 1)) % (int)_viewIds.size();
		_loopNo = _celNo = 0;
		invalidateScene(); break;
	case kWidLoopPrev: _loopNo = MAX(0, _loopNo - 1); _celNo = 0; invalidateScene(); break;
	case kWidLoopNext: _loopNo++; _celNo = 0; invalidateScene(); break; // clamped in renderSlot
	case kWidCelPrev: _celNo = MAX(0, _celNo - 1); invalidateScene(); break;
	case kWidCelNext: _celNo++; invalidateScene(); break;              // clamped in renderSlot
	case kWidVariantCycle:
		do { s.variant = (s.variant + 1) % kScalerCount; }
		while (scalerVariantFactor(s.variant) != 6);
		invalidateActive(); break;
	case kWidPlateMode:
		s.plateMode = (s.plateMode == kPlateOmyac) ? kPlateNearestRef : kPlateOmyac;
		invalidateActive(); break;
	case kWidShowView: _showView = !_showView; invalidateScene(); break;
	case kWidFit: fitView(); break;
	case kWidTabA: _activeSlot = 0; _selectedChip = -1; markDirty(); break;
	case kWidTabB: _activeSlot = 1; _selectedChip = -1; markDirty(); break;
	case kWidShowA: _displayMode = kShowA; markDirty(); break;
	case kWidShowB: _displayMode = kShowB; markDirty(); break;
	case kWidSplit: _displayMode = kShowSplit; markDirty(); break;
	case kWidDiff: _displayMode = kShowDiff; markDirty(); break;
	case kWidCopyAB: {
		Slot &b = _slots[1];
		b.params = _slots[0].params; b.passes = _slots[0].passes;
		b.variant = _slots[0].variant; b.plateMode = _slots[0].plateMode;
		b.stale = true;
		_status = "copied A settings to B";
		markDirty(); break;
	}
	case kWidExport: exportShown(); break;
	case kWidParamMinus:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) - omyacParamDesc(idx).step);
		invalidateActive(); break;
	case kWidParamPlus:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) + omyacParamDesc(idx).step);
		invalidateActive(); break;
	case kWidParamToggle:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) ? 0 : 1);
		invalidateActive(); break;
	case kWidChip: _selectedChip = idx; markDirty(); break;
	case kWidChipX: { int sel = idx; passRemoveAt(s.passes, sel); _selectedChip = sel; invalidateActive(); break; }
	case kWidChipLeft:  if (passMove(s.passes, _selectedChip, -1)) invalidateActive(); break;
	case kWidChipRight: if (passMove(s.passes, _selectedChip, +1)) invalidateActive(); break;
	case kWidChipAddF: passInsertAfter(s.passes, _selectedChip, 2); invalidateActive(); break;
	case kWidChipAddL: passInsertAfter(s.passes, _selectedChip, 1); invalidateActive(); break;
	case kWidChipAddA: passInsertAfter(s.passes, _selectedChip, 0); invalidateActive(); break;
	case kWidChipReset:
		s.passes = defaultPasses(); _selectedChip = -1; invalidateActive(); break;
	default: break;
	}
}
```

Note `invalidateScene()` (both slots — scene state changed) vs `invalidateActive()` (settings changed) vs bare `markDirty()` (display-only) — this is the staleness contract from the spec; keep it exact.

- [ ] **Step 4: Build + verify**

Run: `.\build_and_run.ps1 -NoLaunch` — build succeeds.
Run: `.\build_and_run.ps1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin` — still exits on its own, export PNG appears.
Run: `.\build_tests.ps1` — all pass.
Then a click-evidence run with a TEMPORARY `.rin` (uncommitted, outside `test/`). Computing a button's `.rin` click point: `.rin` coordinates are game space (320×200) mapped over the full window, so `gameX = overlayX * 320 / overlayW`, `gameY = overlayY * 200 / overlayH`, where `overlayX = smallX * 2`, `overlayY = (overlayH − 560) + smallY * 2` and (smallX, smallY) is the widget's rect centre from the layout constants (row 1 starts at y=2, `kStudioRowH` = 24 small px per row; recompute the x-advance from the labels exactly as `buildStudioPanel` does). Target the `plate: omyac` toggle: script = `wait 3500`, `click <x> <y>`, `wait 2500`, `key e`, `wait 800`, `key esc`, `quit`. The exported PNG must show the blocky nearest-ref plate (visibly different from the omyac export of the smoke run) — that proves panel clicks dispatch. Delete the temp script afterwards.

- [ ] **Step 5: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp
git commit -m "Roger studio v2: control panel - widget drawing, hover, full click dispatch"
```

---

### Task 7: Scene mouse interaction — click-to-place, drag, zoom, pan

**Files:**
- Modify: `engines/sci/roger/roger_studio.h` (new members: `_celW/_celH`, per-slot `plateCache`/`plateStale`, `invalidateCelOnly()`)
- Modify: `engines/sci/roger/roger_studio.cpp` (handleEvent + drawFrame + renderSlot plate cache)

**Interfaces:**
- Consumes: `displayToNative`/`sceneArea`/`fitView`/`_celScreenRect` (Task 5), `invalidateScene`.
- Produces: complete mouse interaction; `_celScreenRect` maintained by `drawFrame`.

- [ ] **Step 1: Scene-area mouse handling in `handleEvent`**

Extend the Task-6 cases (scene-area branches):

```cpp
	case Common::EVENT_LBUTTONDOWN:
		// ... panel branch from Task 6 first ...
		{
			int nx, ny;
			if (displayToNative(ev.mouse.x, ev.mouse.y, nx, ny)) {
				if (_showView) {
					_celX = CLIP(nx, 0, 319);
					_celY = CLIP(ny, 0, 189);
					_draggingCel = true;
					invalidateScene();
				}
			}
		}
		return;
	case Common::EVENT_LBUTTONUP:
		_draggingCel = false;
		return;
	case Common::EVENT_RBUTTONDOWN:
		_panning = true; _dragLastX = ev.mouse.x; _dragLastY = ev.mouse.y;
		return;
	case Common::EVENT_RBUTTONUP:
		_panning = false;
		return;
	case Common::EVENT_MOUSEMOVE:
		// ... panel hover from Task 6 first ...
		if (_draggingCel) {
			int nx, ny;
			if (displayToNative(ev.mouse.x, ev.mouse.y, nx, ny)) {
				if (nx != _celX || ny != _celY) {
					_celX = nx; _celY = ny;
					invalidateScene();
				}
			}
		} else if (_panning) {
			_panX += ev.mouse.x - _dragLastX;
			_panY += ev.mouse.y - _dragLastY;
			_dragLastX = ev.mouse.x; _dragLastY = ev.mouse.y;
			markDirty();
		}
		return;
	case Common::EVENT_WHEELUP:
	case Common::EVENT_WHEELDOWN: {
		const Common::Rect area = sceneArea();
		if (!area.contains((int16)ev.mouse.x, (int16)ev.mouse.y))
			return;
		const float oldScale = _viewScale;
		_viewScale = CLIP(_viewScale * (ev.type == Common::EVENT_WHEELUP ? 1.25f : 0.8f),
		                  _fitScale * 0.5f, 8.0f);
		// Keep the plate point under the cursor fixed.
		const float k = _viewScale / oldScale;
		_panX = (int)(ev.mouse.x - area.left - k * (ev.mouse.x - area.left - _panX));
		_panY = (int)(ev.mouse.y - area.top - k * (ev.mouse.y - area.top - _panY));
		markDirty();
		return;
	}
```

Design note (spec): click-to-place and drag are the SAME gesture — a click places the cel (bottom-centre at the click) and starting the drag from there moves it; a drag that started on the cel simply keeps moving it. That is why `LBUTTONDOWN` places immediately and sets `_draggingCel`. `invalidateScene()` on every drag step re-renders both slots at drag rate — the plate part is a full omyac regen (~hundreds of ms). **Fix that in the same step:** cache the plate per slot: add to `Slot` a `Graphics::Surface *plateCache = nullptr;` + `bool plateStale = true;`, split `renderSlot` so the plate is regenerated only when `plateStale` (set by `invalidateActive`, pic change, plate-mode change) while the cel recomposite happens on every `stale`. Concretely: `invalidateScene()` gains a parameter-free companion `invalidateCelOnly()` (sets `stale` on both slots but NOT `plateStale`) used by cel place/drag/loop/cel/view/showView changes; pic browsing and settings changes set both flags. `renderSlot` uses `plateCache` when `!plateStale` (blit from cache instead of `generatePlate*`). Free `plateCache` in the destructor and when regenerating.

- [ ] **Step 2: `drawFrame` records `_celScreenRect`**

In `drawFrame`, after blitting the shown render, compute the cel's display rect from `_celX/_celY` and the last cel dims (store `int _celW = 0, _celH = 0;` — set in `renderSlot` from `celSurf->w/h`; add the two members to the header) through the same plate→display transform used for the blit; store in `_celScreenRect`. (Task 7 doesn't strictly need it for hit-testing — placement is position-based — but the rect gives future hover affordance and debugging; keep it accurate.)

- [ ] **Step 3: Build + verify**

Run: `.\build_and_run.ps1 -NoLaunch`; `.\build_and_run.ps1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin`; `.\build_tests.ps1` — all green. Scripted evidence: TEMPORARY `.rin` — `wait 3000`, `capture before`, then `click` at two different scene points (overlay coords: any point in the upper half of the window maps into the scene area; e.g. click 160 60 — remember `.rin` coordinates are GAME space 320×200 mapped to the window, which lands inside the scene area), `wait 1500`, `capture after`, `key e`, `wait 800`, `key esc`, `quit`. Read before/after PNGs: the cel must be at two different positions. Delete the temp script.

- [ ] **Step 4: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp
git commit -m "Roger studio v2: scene mouse - click-to-place/drag cel, cursor zoom, pan; per-slot plate cache"
```

---

### Task 8: Split + Diff displays, SAD readout, export naming, smoke + docs

**Files:**
- Modify: `engines/sci/roger/roger_studio.h` (new members: `_diffSurf`, `_diffStale`; `ensureDiff()` decl)
- Modify: `engines/sci/roger/roger_studio.cpp`
- Modify: `test/sci/roger/scripts/studio-smoke.rin`
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: `diffMapRGBA`/`estimateOffsetSAD` (Task 3), `studioCompareExportName`/`studioSceneExportName` (Task 1), both slots' renders.
- Produces: complete v2 feature set.

- [ ] **Step 1: Split + Diff in `drawFrame`**

Replace the single-slot blit with a `_displayMode` dispatch:

- `kShowA`/`kShowB`: as Task 5 (ensureFresh that slot, blit).
- `kShowSplit`: `ensureFresh` both; left half of `sceneArea()` shows A, right half shows B, both at the shared `_viewScale/_panX/_panY` (each half clips its own blit — reuse v1's split-blit math, which is still in git history at v1's `drawFrame`; re-derive: per side, dst rect = side area offset by pan, src = inverse-mapped and clipped). Draw a 1 px white vLine between halves and the slot letter + `slotStamp` in the top-left of each half (small font, 2x — or plain `drawString` on `_display` with the big GUI font; either is fine, keep it readable).
- `kShowDiff`: `ensureFresh` both; if either render is missing → status + fall back to kShowA behavior. Both renders are always 1920×1140 (same pipeline) — assert dims equal, else status + bail. Build the diff ONCE per change: cache `Graphics::Surface *_diffSurf = nullptr; bool _diffStale = true;` (members; `_diffStale = true` wherever either slot is invalidated — put it inside `invalidateScene/invalidateActive/invalidateCelOnly` and the Copy dispatch); when stale, allocate/reuse a 1920×1140 surface, run `diffMapRGBA((const byte *)a->getPixels(), (const byte *)b->getPixels(), 1920, 1140, (byte *)_diffSurf->getPixels())`, then `estimateOffsetSAD(..., 3, dx, dy)` and set `_offsetReadout = Common::String::format("best align: dx=%+d dy=%+d overlay px (1/6 native)", dx, dy);`. Blit `_diffSurf` like a single view. Clear `_offsetReadout` when leaving kShowDiff (in the display-mode dispatch cases).

Free `_diffSurf` in the destructor.

- [ ] **Step 2: `exportShown` full version**

```cpp
void RogerStudio::exportShown() {
	const int picId = _picIds.empty() ? 0 : _picIds[_picIdx];
	Common::String name;
	Graphics::Surface *tmp = nullptr;        // composed export needing free
	const Graphics::Surface *src = nullptr;
	if (_displayMode == kShowSplit || _displayMode == kShowDiff) {
		ensureFresh(_slots[0]); ensureFresh(_slots[1]);
		if (!_slots[0].render || !_slots[1].render) { _status = "nothing to export"; markDirty(); return; }
		const Common::String sa = sanitize(slotStamp(_slots[0]));
		const Common::String sb = sanitize(slotStamp(_slots[1]));
		if (_displayMode == kShowDiff) {
			// _diffSurf is fresh whenever diff is displayed; rebuild if needed.
			name = studioCompareExportName(picId, true, sa, sb);
			src = _diffSurf;
		} else {
			// Full-res side-by-side compose (independent of window/zoom).
			const Graphics::Surface *a = _slots[0].render, *b = _slots[1].render;
			Graphics::ManagedSurface side(a->w + b->w, MAX(a->h, b->h), a->format);
			side.blitFrom(*a, Common::Point(0, 0));
			side.blitFrom(*b, Common::Point(a->w, 0));
			tmp = new Graphics::Surface();
			tmp->copyFrom(side.rawSurface());
			name = studioCompareExportName(picId, false, sa, sb);
			src = tmp;
		}
	} else {
		Slot &s = _slots[_displayMode == kShowB ? 1 : 0];
		ensureFresh(s);
		if (!s.render) { _status = "nothing to export"; markDirty(); return; }
		name = studioSceneExportName(picId, _displayMode == kShowB ? 'B' : 'A',
		                             sanitize(slotStamp(s)));
		src = s.render;
	}
	// ... v1's dir resolution + dumpSurfacePng + status lines, unchanged ...
	if (tmp) { tmp->free(); delete tmp; }
}
```

(`sanitize` = v1's keep-alnum-and-dash loop, extracted to a small file-local helper since it is now used in several branches. If diff export can be reached before the diff surface was ever built — e.g. via the E key — build it on demand: factor the Task-8 Step-1 "build diff" block into `ensureDiff()` and call it from both places.)

- [ ] **Step 3: Update the smoke script**

Replace `test/sci/roger/scripts/studio-smoke.rin` content:

```
# Roger Studio v2 smoke: SQ3 defaults render (pic 2 + view 12 cel on it),
# export via the automation E key, quit via Esc. No -Game flag: SQ3 is the
# harness default and the studio defaults under test are SQ3's.
wait 3500
key e
wait 800
key esc
wait 500
quit
```

- [ ] **Step 4: Update CLAUDE.md**

Replace the `-Studio` paragraph body with:

```
`-Studio` launches the **Roger Studio** tuning environment instead of a game
(`ROGER_STUDIO=1`, per-launch): a mouse-driven, single-scene tuner — the enhanced
plate with a view cel composited on it game-style (SQ3 defaults: pic 2, view 12
loop 1), two live A/B setting slots (params + passes + scaler variant + plate
mode each), Split and Diff comparison views with an automatic alignment readout,
click-to-place/drag cel, and stamped PNG export. Fully button-driven; Esc quits
and E exports (automation-only keys). Debug-only; never touches the generation
disk cache. Spec: `docs/superpowers/specs/2026-07-02-roger-studio-v2-ui-design.md`.
```

- [ ] **Step 5: Full verification**

1. `.\build_and_run.ps1 -NoLaunch` — build succeeds.
2. `.\build_and_run.ps1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin` — exits on its own; `studio-scene002-A-default-*.png` exists and shows the SQ3 room-2 plate with the cel.
3. Diff evidence (TEMPORARY `.rin`, uncommitted). Three panel clicks, coordinates derived exactly as in Task 6 Step 4 (game space = overlay × 320/overlayW, 200/overlayH; overlay = small×2 offset by panelTop = overlayH − 560). Leave slot A as omyac; click the `B` tab, then the `plate:` toggle (B becomes nearest-ref), then the `Diff` button — the diff is then omyac-vs-reference, the exact shift test. Script: `wait 3500`, `click <tabB>`, `wait 300`, `click <plateToggle>`, `wait 2500`, `click <diffBtn>`, `wait 2500`, `capture diffpanel`, `move 160 100`, `wait 400`, `key e`, `wait 800`, `key esc`, `quit`. Verify: the exported `studio-scene002-diff-*.png` is the white-on-black diff map (omyac A vs nearest-ref B); the `diffpanel` capture shows the panel with the `best align: dx=? dy=?` readout. **Report the dx/dy numbers in the task report — this is the user's shift question answered.**
4. `.\build_tests.ps1` — all suites green.
5. Inertness regression: `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1 -Script <existing committed game-mode rin if available>` or the Task-8-of-v1 method (short-timeout normal launch, check log has no studio marker, kill).

- [ ] **Step 6: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp test/sci/roger/scripts/studio-smoke.rin CLAUDE.md
git commit -m "Roger studio v2: split/diff displays with SAD offset readout, export naming, smoke + docs"
```

---

## Verification (whole feature)

1. `.\build_tests.ps1` — every suite green (incl. new `test_shift_lock.h`; expect ~113 tests).
2. `.\build_and_run.ps1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin` — exits on its own; `studio-scene002-A-*.png` shows pic-2 plate + view-12 cel.
3. Shift-diagnosis evidence: the Task-8 diff run's PNG + panel capture with the `best align: dx=? dy=?` readout — **report the numbers to the user**; they asked this exact question.
4. `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1` — normal launch unaffected.
5. User drive-through: click every panel section (spinners, params, chips, tabs, judge buttons), place/drag the cel, wheel-zoom/pan/Fit, export from each display mode.
