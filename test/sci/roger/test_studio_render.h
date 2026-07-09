#include <cxxtest/TestSuite.h>
#include "sci/roger/utils/studio/roger_studio_render.h"
#include "sci/roger/gen/roger_passes.h" // omyacPassStamp + pass-edit ops (moved there)
#include "sci/roger/gen/roger_view_scaler.h"
using namespace Sci::Roger;

class RogerStudioRenderTestSuite : public CxxTest::TestSuite {
public:
	void test_param_registry_roundtrip_and_clamp() {
		TS_ASSERT_EQUALS(omyacParamCount(), 7);
		OmyacParams p;
		// index 0 = minVotesLine (default 1)
		TS_ASSERT_EQUALS(omyacParamGet(p, 0), 1);
		omyacParamSet(p, 0, 5);
		TS_ASSERT_EQUALS(p.minVotesLine, 5);
		omyacParamSet(p, 0, 999); // clamps to maxV
		TS_ASSERT_EQUALS(omyacParamGet(p, 0), omyacParamDesc(0).maxV);
		omyacParamSet(p, 0, -5); // clamps to minV
		TS_ASSERT_EQUALS(omyacParamGet(p, 0), omyacParamDesc(0).minV);
		// bool param roundtrip (index 4 = isolatedPixelPass, default 1)
		TS_ASSERT(omyacParamDesc(4).isBool);
		TS_ASSERT_EQUALS(omyacParamGet(p, 4), 1);
		omyacParamSet(p, 4, 0);
		TS_ASSERT(!p.isolatedPixelPass);
	}

	void test_every_registry_index_maps_to_a_distinct_field() {
		// Setting each index to a non-default value must flip isDefault().
		for (int i = 0; i < omyacParamCount(); i++) {
			OmyacParams p;
			const OmyacParamDesc d = omyacParamDesc(i);
			const int def = omyacParamGet(p, i);
			const int other = (def == d.minV) ? d.maxV : d.minV;
			omyacParamSet(p, i, other);
			TS_ASSERT(!p.isDefault());
			TS_ASSERT_EQUALS(omyacParamGet(p, i), other);
		}
	}

	void test_pass_stamp() {
		Common::Array<int> passes;
		TS_ASSERT_EQUALS(omyacPassStamp(passes), Common::String("none"));
		passes.push_back(2); passes.push_back(2); passes.push_back(1); passes.push_back(0);
		TS_ASSERT_EQUALS(omyacPassStamp(passes), Common::String("ffla"));
	}

	void test_param_stamp() {
		OmyacParams p;
		TS_ASSERT_EQUALS(omyacParamStamp(p), Common::String("default"));
		p.minVotesLine = 3;
		p.isolatedPixelPass = false;
		Common::String s = omyacParamStamp(p);
		TS_ASSERT(s.contains("mvl3"));
		TS_ASSERT(s.contains("iso0"));
		TS_ASSERT(!s.contains("mvf")); // still-default fields omitted
	}

	void test_export_name() {
		TS_ASSERT_EQUALS(studioExportName("pic", 2, "default-ffflffaaaa"),
		                 Common::String("studio-pic002-default-ffflffaaaa.png"));
		TS_ASSERT_EQUALS(studioExportName("view", 300, "l2c0-scale6x"),
		                 Common::String("studio-view300-l2c0-scale6x.png"));
	}

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

	void test_passes_equal() {
		Common::Array<int> a, b;
		TS_ASSERT(passesEqual(a, b));        // both empty
		a.push_back(2);
		TS_ASSERT(!passesEqual(a, b));       // size differs
		b.push_back(2);
		TS_ASSERT(passesEqual(a, b));
		a.push_back(1); b.push_back(0);
		TS_ASSERT(!passesEqual(a, b));       // element differs
	}

	void test_v2_export_names() {
		TS_ASSERT_EQUALS(studioSceneExportName(2, 'A', "default-ffflffaaaa"),
		                 Common::String("studio-scene002-A-default-ffflffaaaa.png"));
		TS_ASSERT_EQUALS(studioCompareExportName(2, false, "default", "mvl3"),
		                 Common::String("studio-scene002-AB-default-vs-mvl3.png"));
		TS_ASSERT_EQUALS(studioCompareExportName(2, true, "default", "nref"),
		                 Common::String("studio-scene002-diff-default-vs-nref.png"));
	}

	static StudioPanelState samplePanelState() {
		StudioPanelState st;
		st.picId = 2; st.viewId = 12; st.loopNo = 1; st.celNo = 0;
		st.celX = 160; st.celY = 150;
		st.viewEnhanceLabel = "6x (s2>s3)";
		st.picEnhanceLabel = "ffl";
		st.showView = true;
		st.activeSlot = 0;
		st.displayMode = 0;
		st.showBackfill = true;
		st.showGrid = false;
		st.addPending = true;
		st.buildPasses.push_back(2); st.buildPasses.push_back(2); st.buildPasses.push_back(1);
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
		Common::Array<PanelWidget> w;
		buildStudioPanel(panel, samplePanelState(), w);
		TS_ASSERT(!w.empty());
		// Every expected clickable kind is present at least once.
		static const int MUST[] = {
			kWidPicPrev, kWidPicNext, kWidViewPrev, kWidViewNext,
			kWidLoopPrev, kWidLoopNext, kWidCelPrev, kWidCelNext,
			kWidViewEnhance, kWidPicEnhance, kWidShowView, kWidFit,
			kWidTabA, kWidTabB, kWidShowA, kWidShowB, kWidSplit, kWidDiff,
			kWidCopyAB, kWidExport,
			kWidChipAddF, kWidChipAddL, kWidChipAddA,
			kWidChipClear, kWidChipAdd, kWidShowBackfill, kWidShowGrid };
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
		// Build chips are DISPLAY-ONLY (kWidNone texts, one per built pass:
		// buildPasses ffl -> two "f" texts + one "l"); "add" highlights while
		// pending; the toggle rows carry their labels.
		int chipF = 0, chipL = 0;
		bool addOn = false, viewLbl = false, picLbl = false;
		for (uint i = 0; i < w.size(); i++) {
			if (w[i].id == (uint32)kWidNone && w[i].label == "f") chipF++;
			if (w[i].id == (uint32)kWidNone && w[i].label == "l") chipL++;
			if (widKind(w[i].id) == kWidChipAdd) addOn = w[i].on;
			if (widKind(w[i].id) == kWidViewEnhance && w[i].label.contains("6x (s2>s3)")) viewLbl = true;
			if (widKind(w[i].id) == kWidPicEnhance && w[i].label.contains("ffl")) picLbl = true;
		}
		TS_ASSERT_EQUALS(chipF, 2);
		TS_ASSERT_EQUALS(chipL, 1);
		TS_ASSERT(addOn); // samplePanelState sets addPending
		TS_ASSERT(viewLbl && picLbl);
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
		Common::Array<PanelWidget> w;
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

	void test_panel_layout_non_zero_origin() {
		// Regression: PanelCursor.newRow() must respect panel.left, not hardcode 4.
		// Test panel with non-zero left and top origins.
		const Common::Rect panel(20, 10, 1420, 290);
		Common::Array<PanelWidget> w;
		buildStudioPanel(panel, samplePanelState(), w);
		TS_ASSERT(!w.empty());
		// Every widget rect must be fully inside the panel.
		for (uint i = 0; i < w.size(); i++) {
			const Common::Rect &r = w[i].rect;
			TS_ASSERT(r.left >= panel.left);
			TS_ASSERT(r.top >= panel.top);
			TS_ASSERT(r.right <= panel.right);
			TS_ASSERT(r.bottom <= panel.bottom);
		}
	}

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

	void test_sad_tie_breaks() {
		// Uniform images: all pixels same color -> all offsets have SAD 0,
		// so the function must apply the full tie-break contract and return (0,0)
		// (smaller |dx|+|dy| wins, with sub-breaks on |dy| then |dx|).
		const int W = 10, H = 10;
		Common::Array<byte> uniform, uniform2;
		uniform.resize(W * H * 4);
		uniform2.resize(W * H * 4);
		for (int i = 0; i < W * H; i++) {
			uniform[i * 4 + 0] = 128;     // red
			uniform[i * 4 + 1] = 64;      // green
			uniform[i * 4 + 2] = 32;      // blue
			uniform[i * 4 + 3] = 255;     // alpha
			uniform2[i * 4 + 0] = 128;
			uniform2[i * 4 + 1] = 64;
			uniform2[i * 4 + 2] = 32;
			uniform2[i * 4 + 3] = 255;
		}
		// With uniform images, every offset (dx,dy) has SAD=0 and all ties go through.
		// The function must return (0,0) as it has the smallest distance.
		int dx = -99, dy = -99;
		TS_ASSERT(estimateOffsetSAD(uniform.begin(), uniform2.begin(), W, H, 2, dx, dy));
		TS_ASSERT_EQUALS(dx, 0);
		TS_ASSERT_EQUALS(dy, 0);
	}

	// â”€â”€ New tests for polish wave â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

	// Empty builder: the "(none - wireframe)" placeholder shows and no chip
	// texts are emitted; add stays clickable.
	void test_build_row_empty() {
		const Common::Rect panel(0, 0, 1400, 280);
		StudioPanelState st = samplePanelState();
		st.buildPasses.clear();
		st.addPending = false;
		Common::Array<PanelWidget> w;
		buildStudioPanel(panel, st, w);
		bool placeholder = false, addFound = false, addOn = true;
		for (uint i = 0; i < w.size(); i++) {
			if (w[i].id == (uint32)kWidNone && w[i].label.contains("wireframe"))
				placeholder = true;
			if (widKind(w[i].id) == kWidChipAdd) { addFound = true; addOn = w[i].on; }
		}
		TS_ASSERT(placeholder);
		TS_ASSERT(addFound);
		TS_ASSERT(!addOn); // nothing pending
	}

	void test_all_params_have_nonempty_help() {
		for (int i = 0; i < omyacParamCount(); i++) {
			const OmyacParamDesc d = omyacParamDesc(i);
			TS_ASSERT(d.help != nullptr);
			TS_ASSERT(*d.help != '\0');
		}
	}

	void test_cel_coords_label_in_panel() {
		// With celX=160, celY=150 the panel must contain a kWidNone widget
		// whose label is "@(160,150)".
		const Common::Rect panel(0, 0, 1400, 280);
		StudioPanelState st = samplePanelState(); // already has celX=160, celY=150
		Common::Array<PanelWidget> w;
		buildStudioPanel(panel, st, w);

		bool found = false;
		for (uint i = 0; i < w.size(); i++) {
			if (w[i].id == (uint32)kWidNone && w[i].label == "@(160,150)") {
				found = true;
				// Must be inside the panel.
				TS_ASSERT(w[i].rect.left >= panel.left && w[i].rect.right <= panel.right);
				TS_ASSERT(w[i].rect.top >= panel.top && w[i].rect.bottom <= panel.bottom);
				break;
			}
		}
		TS_ASSERT(found);
	}

	// â”€â”€ Grid + animation helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

	// 2x3 row-major tiling: 6 tiles inside the area, no overlaps, gutters.
	// gridTileRect is a pure geometry helper covering all 6 positions regardless
	// of how many registry modules are active.
	void test_grid_tile_rects() {
		const Common::Rect area(0, 0, 900, 400);
		Common::Rect r[6];
		for (int i = 0; i < 6; i++) {
			r[i] = gridTileRect(area, i);
			TS_ASSERT(!r[i].isEmpty());
			TS_ASSERT(area.contains(r[i]));
		}
		// Row-major: tiles 0..2 on the top row, 3..5 below.
		TS_ASSERT_EQUALS(r[0].top, r[1].top);
		TS_ASSERT_EQUALS(r[1].top, r[2].top);
		TS_ASSERT_EQUALS(r[3].top, r[4].top);
		TS_ASSERT(r[3].top >= r[0].bottom);
		TS_ASSERT(r[1].left >= r[0].right);
		TS_ASSERT(r[2].left >= r[1].right);
		// No pairwise overlap.
		for (int i = 0; i < 6; i++)
			for (int j = i + 1; j < 6; j++) {
				Common::Rect a = r[i];
				a.clip(r[j]);
				TS_ASSERT(a.isEmpty());
			}
	}

	// Grid tiles map 1:1 onto the view-enhance modes (registry scalers +
	// trailing nearest, max 6); tiles past the modes resolve to -1. Tile 0 is
	// the shipping 6x module; the last in-range tile is nearest.
	void test_grid_preset_slots() {
		TS_ASSERT_EQUALS(gridTileCount(), MIN(viewEnhanceModeCount(), 6));
		for (int i = 0; i < gridTileCount(); i++)
			TS_ASSERT_EQUALS(gridPresetSlot(i), i);
		TS_ASSERT_EQUALS(gridPresetSlot(gridTileCount()), -1);
		TS_ASSERT_EQUALS(gridPresetSlot(-1), -1);
		TS_ASSERT_EQUALS(strcmp(viewScaler(gridPresetSlot(0)).id, "s2-s3"), 0);
		TS_ASSERT_EQUALS(viewScaler(gridPresetSlot(0)).factor, 6);
		// With one registered scaler the grid finally shows a real comparison:
		// tile 1 = nearest.
		TS_ASSERT(viewEnhanceModeIsNearest(gridPresetSlot(gridTileCount() - 1)));
	}

	// Speed table + clamped stepping.
	void test_anim_speed() {
		TS_ASSERT_EQUALS(animSpeedMs(2), 150);
		TS_ASSERT_EQUALS(animSpeedMs(0), 300);
		TS_ASSERT_EQUALS(animSpeedMs(4), 66);
		TS_ASSERT_EQUALS(animSpeedMs(-3), 300);  // clamped
		TS_ASSERT_EQUALS(animSpeedMs(99), 66);   // clamped
		TS_ASSERT_EQUALS(animSpeedStep(2, +1), 3);
		TS_ASSERT_EQUALS(animSpeedStep(4, +1), 4); // clamped
		TS_ASSERT_EQUALS(animSpeedStep(0, -1), 0); // clamped
	}

	// Native SCI anchor formula (GfxView::getCelRect):
	//   left = ax + dx - (w >> 1); bottom = ay + dy + 1; top = bottom - h.
	void test_cel_anchor_rect() {
		const Common::Rect r = celAnchorRect(5, 8, 1, -2, 100, 50);
		TS_ASSERT_EQUALS(r.left, 100 + 1 - 2);   // 99
		TS_ASSERT_EQUALS(r.right, 99 + 5);
		TS_ASSERT_EQUALS(r.bottom, 50 - 2 + 1);  // 49
		TS_ASSERT_EQUALS(r.top, 49 - 8);
		TS_ASSERT_EQUALS(r.width(), 5);
		TS_ASSERT_EQUALS(r.height(), 8);
	}

	// Panel exposes the new widgets.
	void test_panel_has_grid_and_anim_widgets() {
		StudioPanelState st;
		st.picId = 1; st.viewId = 1; st.loopNo = 0; st.celNo = 0;
		st.celX = 10; st.celY = 10;
		st.viewEnhanceLabel = "6x (s2>s3)";
		st.picEnhanceLabel = "ffl";
		st.showView = true;
		st.activeSlot = 0; st.displayMode = 4;
		st.showBackfill = false; st.showGrid = false;
		st.animPlaying = true; st.animMs = 150;
		Common::Array<PanelWidget> w;
		buildStudioPanel(Common::Rect(0, 0, 1400, 500), st, w);
		bool haveGrid = false, havePlay = false, haveSlower = false, haveFaster = false;
		for (uint i = 0; i < w.size(); i++) {
			const int k = widKind(w[i].id);
			if (k == kWidGrid6) { haveGrid = true; TS_ASSERT(w[i].on); } // displayMode 4
			if (k == kWidAnimPlay) { havePlay = true; TS_ASSERT(w[i].on); }
			if (k == kWidAnimSlower) haveSlower = true;
			if (k == kWidAnimFaster) haveFaster = true;
		}
		TS_ASSERT(haveGrid);
		TS_ASSERT(havePlay);
		TS_ASSERT(haveSlower);
		TS_ASSERT(haveFaster);
	}
};
