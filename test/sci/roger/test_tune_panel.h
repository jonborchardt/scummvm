#include <cxxtest/TestSuite.h>
#include "common/util.h"
#include "engines/sci/roger/utils/tunepanel/roger_tune_panel.h"
#include "engines/sci/roger/roger_view_scaler.h"
#include "engines/sci/roger/roger_passes.h"

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

		TunePanelState st;
		TS_ASSERT(!tunePending(st));
		st.stagedPasses.push_back(0);
		TS_ASSERT(tunePending(st));
		st.appliedPasses.push_back(0);
		TS_ASSERT(!tunePending(st));
	}

	// Layout invariants: every widget inside the panel rect, one row per
	// variant preset, one chip per staged pass, ops/apply rows present,
	// chip-op enablement follows selection.
	void test_layout_invariants() {
		TunePanelState st;
		st.stagedPasses.push_back(2);
		st.stagedPasses.push_back(1);
		st.selectedChip = 1;
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		const Common::Rect p = tunePanelRect();
		int variantRows = 0, chips = 0;
		bool haveApply = false, haveClose = false, xEnabled = false;
		for (uint i = 0; i < w.size(); i++) {
			TS_ASSERT(p.contains(w[i].rect.left, w[i].rect.top));
			TS_ASSERT(w[i].rect.right <= p.right && w[i].rect.bottom <= p.bottom);
			switch (widKind(w[i].id)) {
			case kTuneVariantRow: variantRows++; break;
			case kTuneChip: chips++; break;
			case kTuneApply: haveApply = true; TS_ASSERT(w[i].on); break; // pending
			case kTuneClose: haveClose = true; break;
			case kTuneChipX: xEnabled = w[i].enabled; break;
			default: break;
			}
		}
		TS_ASSERT_EQUALS(variantRows, viewScalerCount());
		TS_ASSERT_EQUALS(chips, 2);
		TS_ASSERT(haveApply && haveClose && xEnabled);

		// No selection -> chip ops disabled; no pending -> apply not highlighted.
		st.selectedChip = -1;
		st.appliedPasses = st.stagedPasses;
		buildTunePanel(st, w);
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kTuneChipX || widKind(w[i].id) == kTuneChipLeft ||
			    widKind(w[i].id) == kTuneChipRight)
				TS_ASSERT(!w[i].enabled);
			if (widKind(w[i].id) == kTuneApply)
				TS_ASSERT(!w[i].on);
		}
	}

	// The single registered module's row is marked on for variant 0;
	// hit-testing the row center returns that row.
	void test_variant_rows_and_hittest() {
		TunePanelState st;
		st.variant = 0;
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		int rows = 0;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) != kTuneVariantRow)
				continue;
			rows++;
			TS_ASSERT_EQUALS(w[i].on, widIndex(w[i].id) == 0);
			const int cx = (w[i].rect.left + w[i].rect.right) / 2;
			const int cy = (w[i].rect.top + w[i].rect.bottom) / 2;
			TS_ASSERT_EQUALS(hitTestWidgets(w, cx, cy), w[i].id);
		}
		TS_ASSERT_EQUALS(rows, viewScalerCount());
	}

	// Geometry lock for the .rin verification script: the panel rect and the
	// bottom-anchored rows must not drift or scripted clicks miss.
	void test_script_geometry_lock() {
		TS_ASSERT_EQUALS(tunePanelRect().left, 228);
		TS_ASSERT_EQUALS(tunePanelRect().top, 12);
		TS_ASSERT_EQUALS(tunePanelRect().right, 318);
		TS_ASSERT_EQUALS(tunePanelRect().bottom, 196);
		TunePanelState st;
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		// Clicks used by test/sci/roger/scripts/tune-panel-smoke.rin:
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 273, 31)), (int)kTuneVariantRow);
		TS_ASSERT_EQUALS(widIndex(hitTestWidgets(w, 273, 31)), 0);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 244, 177)), (int)kTuneClear);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 274, 177)), (int)kTuneReset);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 303, 177)), (int)kTuneApply);
		// Preset row coordinates flow below the variant rows, so (unlike the
		// bottom-anchored rows) they move if a second scaler module registers —
		// same caveat as the chip strip, documented in the smoke script.
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 273, 44)), (int)kTunePreset);
		TS_ASSERT_EQUALS(widIndex(hitTestWidgets(w, 273, 44)), 0);
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 273, 55)), (int)kTunePreset);
		TS_ASSERT_EQUALS(widIndex(hitTestWidgets(w, 273, 55)), 1);
	}

	// Known-good preset rows: one per goodPassPattern() registry entry, labeled
	// by its compact string, lit only while the staged list matches, and the
	// one-click swap contract (registry order preserved, rows sit between the
	// variant rows and the ops row).
	void test_preset_rows() {
		TunePanelState st;
		Common::Array<PanelWidget> w;
		buildTunePanel(st, w);
		int presets = 0;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) != kTunePreset)
				continue;
			const int idx = widIndex(w[i].id);
			TS_ASSERT_EQUALS(presets, idx); // registry order, best-first
			presets++;
			TS_ASSERT_EQUALS(w[i].label, Common::String(goodPassPattern(idx).compact));
			TS_ASSERT(!w[i].on); // staged empty: no preset matches
			const int cx = (w[i].rect.left + w[i].rect.right) / 2;
			const int cy = (w[i].rect.top + w[i].rect.bottom) / 2;
			TS_ASSERT_EQUALS(hitTestWidgets(w, cx, cy), w[i].id);
		}
		TS_ASSERT_EQUALS(presets, goodPassPatternCount());

		// Staging a registry pattern lights exactly that row.
		st.stagedPasses = parsePassString(goodPassPattern(0).compact);
		buildTunePanel(st, w);
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) != kTunePreset)
				continue;
			TS_ASSERT_EQUALS(w[i].on, widIndex(w[i].id) == 0);
		}
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

	// Side toggle: the side button exists on both sides with a label pointing
	// at the side the panel will move TO, the left-side layout mirrors every
	// widget into tunePanelRect(true), and hit-testing follows the mirror.
	// The RIGHT-side default stays locked by test_script_geometry_lock.
	void test_side_toggle_layout() {
		TunePanelState st;
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
		// Mirrored hit-test spot check: apply row lands at (64..90, 172..182).
		TS_ASSERT_EQUALS(widKind(hitTestWidgets(w, 77, 177)), (int)kTuneApply);
	}
};
