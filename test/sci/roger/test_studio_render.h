#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_studio_render.h"
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

	void test_scaler_variants_factor_and_dims() {
		IndexImage img;
		img.w = 4; img.h = 3;
		img.pixels.resize(12, 7);
		for (int v = 0; v < kScalerCount; v++) {
			const int f = scalerVariantFactor(v);
			IndexImage out = applyScalerVariant(v, img);
			TS_ASSERT_EQUALS(out.w, img.w * f);
			TS_ASSERT_EQUALS(out.h, img.h * f);
			TS_ASSERT_EQUALS(out.pixels.size(), (uint)(out.w * out.h));
			TS_ASSERT(scalerVariantName(v) != nullptr);
		}
		// A solid image stays solid through every variant.
		IndexImage out6 = applyScalerVariant(kScaler6x, img);
		for (uint i = 0; i < out6.pixels.size(); i++)
			TS_ASSERT_EQUALS(out6.pixels[i], (byte)7);
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

	static StudioPanelState samplePanelState() {
		StudioPanelState st;
		st.picId = 2; st.viewId = 12; st.loopNo = 1; st.celNo = 0;
		st.celX = 160; st.celY = 150;
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
			kWidChipAddF, kWidChipAddL, kWidChipAddA, kWidChipReset,
			kWidChipClear };
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

	void test_panel_layout_non_zero_origin() {
		// Regression: PanelCursor.newRow() must respect panel.left, not hardcode 4.
		// Test panel with non-zero left and top origins.
		const Common::Rect panel(20, 10, 1420, 290);
		Common::Array<StudioWidget> w;
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

	// ── New tests for polish wave ──────────────────────────────────────────────

	void test_chip_caret_with_selection() {
		// selectedChip=1, 3 passes: caret must appear immediately after chip 1's
		// x-button (i.e. left >= that button's right) and before chip 2's rect.
		const Common::Rect panel(0, 0, 1400, 280);
		Common::Array<StudioWidget> w;
		buildStudioPanel(panel, samplePanelState(), w);

		// Find chip 1's x-button rect.
		int xBtnRight = -1;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kWidChipX && widIndex(w[i].id) == 1)
				xBtnRight = w[i].rect.right;
		}
		TS_ASSERT(xBtnRight >= 0); // chip 1 x-button must exist

		// Find chip 2's rect left.
		int chip2Left = 99999;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kWidChip && widIndex(w[i].id) == 2)
				chip2Left = w[i].rect.left;
		}
		TS_ASSERT(chip2Left < 99999); // chip 2 must exist

		// Caret: exactly one kWidNone widget with label "^" in the chip row.
		int caretCount = 0;
		int caretLeft = -1;
		for (uint i = 0; i < w.size(); i++) {
			if (w[i].id == (uint32)kWidNone && w[i].label == "^") {
				caretCount++;
				caretLeft = w[i].rect.left;
			}
		}
		TS_ASSERT_EQUALS(caretCount, 1);
		// Caret sits between x-button and next chip.
		TS_ASSERT(caretLeft >= xBtnRight);
		TS_ASSERT(caretLeft < chip2Left);
		// Caret must be inside the panel.
		for (uint i = 0; i < w.size(); i++) {
			if (w[i].id == (uint32)kWidNone && w[i].label == "^") {
				TS_ASSERT(w[i].rect.left >= panel.left && w[i].rect.right <= panel.right);
				TS_ASSERT(w[i].rect.top >= panel.top && w[i].rect.bottom <= panel.bottom);
			}
		}
	}

	void test_chip_caret_no_selection() {
		// selectedChip = -1: caret sits after the last chip's x-button.
		const Common::Rect panel(0, 0, 1400, 280);
		StudioPanelState st = samplePanelState();
		st.selectedChip = -1; // no selection
		Common::Array<StudioWidget> w;
		buildStudioPanel(panel, st, w);

		// Find the last chip x-button (index 2).
		int lastXRight = -1;
		for (uint i = 0; i < w.size(); i++) {
			if (widKind(w[i].id) == kWidChipX && widIndex(w[i].id) == (int)st.passes.size() - 1)
				lastXRight = w[i].rect.right;
		}
		TS_ASSERT(lastXRight >= 0);

		int caretCount = 0;
		int caretLeft = -1;
		for (uint i = 0; i < w.size(); i++) {
			if (w[i].id == (uint32)kWidNone && w[i].label == "^") {
				caretCount++;
				caretLeft = w[i].rect.left;
			}
		}
		TS_ASSERT_EQUALS(caretCount, 1);
		TS_ASSERT(caretLeft >= lastXRight);
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
		Common::Array<StudioWidget> w;
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
};
