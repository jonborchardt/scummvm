#include <cxxtest/TestSuite.h>
#include "common/util.h"
#include "engines/sci/roger/utils/tunepanel/roger_tune_panel.h"
#include "engines/sci/roger/gen/roger_view_scaler.h"
#include "engines/sci/roger/gen/roger_passes.h"

using namespace Sci::Roger;

class TunePanelTestSuite : public CxxTest::TestSuite {
public:
	void test_passes_equal_and_pending() {
		Common::Array<int> a, b;
		TS_ASSERT(tunePassesEqual(a, b));
		a.push_back(2);
		TS_ASSERT(!tunePassesEqual(a, b));
		b.push_back(2);
		TS_ASSERT(tunePassesEqual(a, b));
		b[0] = 1;
		TS_ASSERT(!tunePassesEqual(a, b));

		// pending = staged sequence differs from what is applied.
		TunePanelState st;
		TS_ASSERT(!tunePending(st));
		st.stagedPasses.push_back(0);
		TS_ASSERT(tunePending(st));
		st.appliedPasses.push_back(0);
		TS_ASSERT(!tunePending(st));
	}

	// View-enhance modes: every registered scaler plus the synthetic nearest,
	// which is always the last slot.
	void test_view_modes() {
		TS_ASSERT_EQUALS(tuneViewModeCount(), viewScalerCount() + 1);
		TS_ASSERT(!tuneViewModeIsNearest(0));                    // registry entry 0 (6x)
		TS_ASSERT(tuneViewModeIsNearest(viewScalerCount()));     // last slot = nearest
	}

	// Pic-enhance mode list: seeded from the goodPassPattern registry (once);
	// selectOrAdd points at an existing match, or appends a novel sequence.
	void test_pic_modes_seed_and_select() {
		TunePanelState st;
		tuneSeedPicModes(st);
		TS_ASSERT_EQUALS((int)st.picModes.size(), goodPassPatternCount());
		// Seeding is idempotent (session-added modes survive a reopen).
		tuneSeedPicModes(st);
		TS_ASSERT_EQUALS((int)st.picModes.size(), goodPassPatternCount());

		// Selecting an existing registry pattern points picModeSel at it.
		Common::Array<int> reg1 = parsePassString(goodPassPattern(1).compact);
		tuneSelectOrAddMode(st, reg1);
		TS_ASSERT_EQUALS(st.picModeSel, 1);
		TS_ASSERT_EQUALS((int)st.picModes.size(), goodPassPatternCount());

		// A novel sequence is appended and selected.
		Common::Array<int> novel;
		novel.push_back(2); novel.push_back(2); novel.push_back(1); // ffl
		tuneSelectOrAddMode(st, novel);
		TS_ASSERT_EQUALS((int)st.picModes.size(), goodPassPatternCount() + 1);
		TS_ASSERT_EQUALS(st.picModeSel, goodPassPatternCount());
		TS_ASSERT(tunePassesEqual(st.picModes[st.picModeSel], novel));
	}

	// Pic-enhance nearest: the cycle is the mode list + one trailing nearest
	// slot; selecting it flips tunePicModeIsNearest and labels the row.
	void test_pic_mode_nearest_slot() {
		TunePanelState st;
		tuneSeedPicModes(st);
		TS_ASSERT_EQUALS(tunePicModeCount(st), (int)st.picModes.size() + 1);
		TS_ASSERT(!tunePicModeIsNearest(st)); // sel 0 = a pass mode
		st.picModeSel = (int)st.picModes.size();
		TS_ASSERT(tunePicModeIsNearest(st));

		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		bool found = false;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) != kTunePicEnhance)
				continue;
			found = true;
			TS_ASSERT(w[i].label.contains("nearest"));
		}
		TS_ASSERT(found);
	}

	// Layout invariants: every widget inside the panel rect; the three toggle
	// rows and the build row (+f +l +a clear add) are present; one chip per
	// staged pass, drawn non-interactive.
	void test_layout_invariants() {
		TunePanelState st;
		tuneSeedPicModes(st);
		st.stagedPasses.push_back(2);
		st.stagedPasses.push_back(1);
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		const Common::Rect p = tunePanelRect();
		int chips = 0;
		bool haveLog = false, haveView = false, havePic = false;
		bool haveAdd = false, haveClear = false, haveClose = false;
		bool haveF = false, haveL = false, haveA = false;
		bool chipsInert = true;
		for (uint i = 0; i < w.size(); i++) {
			TS_ASSERT(p.contains(w[i].rect.left, w[i].rect.top));
			TS_ASSERT(w[i].rect.right <= p.right && w[i].rect.bottom <= p.bottom);
			switch (widKind(w[i].id)) {
			case kTuneDebugLog:   haveLog = true; break;
			case kTuneViewEnhance: haveView = true; break;
			case kTunePicEnhance:  havePic = true; break;
			case kTuneChip: chips++; if (w[i].enabled) chipsInert = false; break;
			case kTuneChipAddF: haveF = true; break;
			case kTuneChipAddL: haveL = true; break;
			case kTuneChipAddA: haveA = true; break;
			case kTuneClear: haveClear = true; break;
			case kTuneAdd: haveAdd = true; TS_ASSERT(w[i].on); break; // pending (staged != applied)
			case kTuneClose: haveClose = true; break;
			default: break;
			}
		}
		TS_ASSERT_EQUALS(chips, 2);
		TS_ASSERT(chipsInert); // chips are display-only
		TS_ASSERT(haveLog && haveView && havePic);
		TS_ASSERT(haveF && haveL && haveA && haveClear && haveAdd && haveClose);

		// No pending edits -> Add not highlighted.
		st.appliedPasses = st.stagedPasses;
		buildTunePanel(st, w);
		for (uint i = 0; i < w.size(); i++)
			if (widKind(w[i].id) == kTuneAdd)
				TS_ASSERT(!w[i].on);
	}

	// View/pic toggle labels reflect state; log row lights only while on.
	void test_toggle_labels() {
		TunePanelState st;
		tuneSeedPicModes(st);
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kTuneViewEnhance)
				TS_ASSERT(w[i].label.contains(viewScaler(0).label)); // viewMode 0
			if (widKind(w[i].id) == kTunePicEnhance)
				TS_ASSERT(w[i].label.contains("pic enhance"));
			if (widKind(w[i].id) == kTuneDebugLog) {
				TS_ASSERT(w[i].label.contains("off"));
				TS_ASSERT(!w[i].on);
			}
		}

		st.viewMode = viewScalerCount(); // nearest
		st.debugLog = true;
		buildTunePanel(st, w);
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kTuneViewEnhance)
				TS_ASSERT(w[i].label.contains("nearest"));
			if (widKind(w[i].id) == kTuneDebugLog) {
				TS_ASSERT(w[i].label.contains("on"));
				TS_ASSERT(w[i].on);
			}
		}
	}

	// Geometry lock for the .rin verification script: panel rect and the fixed
	// toggle-row / bottom-anchored build-row centers must not drift or scripted
	// clicks miss.
	void test_script_geometry_lock() {
		TS_ASSERT_EQUALS(tunePanelRect().left, 228);
		TS_ASSERT_EQUALS(tunePanelRect().top, 12);
		TS_ASSERT_EQUALS(tunePanelRect().right, 318);
		TS_ASSERT_EQUALS(tunePanelRect().bottom, 196);
		TunePanelState st;
		tuneSeedPicModes(st);
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		// Top toggle rows (fixed: they flow from the title row, no variable rows above).
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 273, 31)), (int)kTuneDebugLog);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 273, 42)), (int)kTuneViewEnhance);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 273, 53)), (int)kTunePicEnhance);
		// Bottom-anchored build row (+f +l +a clear add) at y=177.
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 236, 177)), (int)kTuneChipAddF);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 249, 177)), (int)kTuneChipAddL);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 262, 177)), (int)kTuneChipAddA);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 280, 177)), (int)kTuneClear);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 304, 177)), (int)kTuneAdd);
	}

	void test_status_line() {
		TunePanelState st;
		st.stagedPasses.push_back(2);
		st.stagedPasses.push_back(0);
		st.appliedPasses = st.stagedPasses;
		st.lastGenMs = 812;
		TS_ASSERT(tuneStatusLine(st).contains("fa"));
		TS_ASSERT(tuneStatusLine(st).contains("812"));
		TS_ASSERT(!tuneStatusLine(st).contains("*"));
		st.stagedPasses.push_back(1);
		TS_ASSERT(tuneStatusLine(st).contains("*"));
	}

	// Side toggle: the side button exists on both sides with a label pointing at
	// the side the panel will move TO; the left-side layout mirrors every widget
	// into tunePanelRect(true). The RIGHT-side default stays locked above.
	void test_side_toggle_layout() {
		TunePanelState st;
		tuneSeedPicModes(st);
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		bool sideFound = false;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) != kTuneSide)
				continue;
			sideFound = true;
			TS_ASSERT_EQUALS(w[i].label, Common::String("<")); // docked right: moves left
		}
		TS_ASSERT(sideFound);

		st.leftSide = true;
		buildTunePanel(st, w);
		const Common::Rect p = tunePanelRect(true);
		TS_ASSERT_EQUALS(p.left, 2);
		TS_ASSERT_EQUALS(p.right, 92);
		sideFound = false;
		for (uint i = 0; i < w.size(); i++) {
			TS_ASSERT(w[i].rect.left >= p.left && w[i].rect.right <= p.right);
			TS_ASSERT(w[i].rect.top >= p.top && w[i].rect.bottom <= p.bottom);
			if (widKind(w[i].id) == kTuneSide) {
				sideFound = true;
				TS_ASSERT_EQUALS(w[i].label, Common::String(">")); // docked left: moves right
			}
		}
		TS_ASSERT(sideFound);
		// Mirrored hit-test spot check: Add lands at the right end of the build row.
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 78, 177)), (int)kTuneAdd);
	}
};
