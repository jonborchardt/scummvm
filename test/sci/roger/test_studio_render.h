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
};
