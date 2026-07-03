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
};
