#include <cxxtest/TestSuite.h>
#include "common/util.h"
#include "engines/sci/roger/roger_view_scaler.h"
#include "engines/sci/roger/roger_scale.h"

using namespace Sci::Roger;

namespace {

IndexImage synthImgVS(int w, int h) {
	IndexImage img;
	img.w = w; img.h = h;
	img.pixels.resize((size_t)w * h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			img.pixels[(size_t)y * w + x] = (byte)((x * 3 + y * 5) % 16);
	return img;
}

bool sameImageVS(const IndexImage &a, const IndexImage &b) {
	if (a.w != b.w || a.h != b.h)
		return false;
	for (uint i = 0; i < a.pixels.size(); i++)
		if (a.pixels[i] != b.pixels[i])
			return false;
	return true;
}

} // anonymous namespace

class ViewScalerTestSuite : public CxxTest::TestSuite {
public:
	// Every preset: declared factor matches measured output dimensions,
	// id is unique and filename-safe, label is non-empty.
	void test_presets_factor_ids_labels() {
		IndexImage in = synthImgVS(4, 3);
		TS_ASSERT(viewScalerPresetCount() >= 8);
		for (int i = 0; i < viewScalerPresetCount(); i++) {
			const ViewScalerPreset &p = viewScalerPreset(i);
			const int f = viewScalerPresetFactor(i);
			IndexImage out = applyViewScalerPreset(i, in, 0xFF);
			TS_ASSERT_EQUALS(out.w, in.w * f);
			TS_ASSERT_EQUALS(out.h, in.h * f);
			TS_ASSERT(p.id && *p.id);
			TS_ASSERT(p.label && *p.label);
			for (const char *c = p.id; *c; c++)
				TS_ASSERT((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-');
			for (int j = 0; j < i; j++)
				TS_ASSERT(strcmp(viewScalerPreset(j).id, p.id) != 0);
			// Round-trip through the id lookup.
			TS_ASSERT_EQUALS(viewScalerPresetIndexById(p.id), i);
		}
		TS_ASSERT_EQUALS(viewScalerPresetIndexById("no-such-id"), -1);
	}

	// Preset 0 ("s2-s3") is byte-identical to the shipping scale6x().
	// This is the lock that keeps the registry honest about "current".
	void test_preset0_matches_shipping_scale6x() {
		IndexImage in = synthImgVS(8, 7);
		TS_ASSERT_EQUALS(strcmp(viewScalerPreset(0).id, "s2-s3"), 0);
		TS_ASSERT_EQUALS(viewScalerPresetFactor(0), 6);
		TS_ASSERT(sameImageVS(applyViewScalerPreset(0, in, 0xFF), scale6x(in)));
	}

	// The requested comparison pipelines all exist with the right factors.
	void test_requested_pipelines_present() {
		struct { const char *id; int factor; } want[] = {
			{ "s2-s3", 6 }, { "s3-s2", 6 }, { "s3-mx", 6 }, { "mx-s3", 6 },
			{ "mx-mx-mx", 8 }, { "mx-s2-s2", 8 }, { "s3-s3", 9 }, { "n2-n3", 6 },
		};
		for (uint i = 0; i < ARRAYSIZE(want); i++) {
			const int idx = viewScalerPresetIndexById(want[i].id);
			TS_ASSERT(idx >= 0);
			TS_ASSERT_EQUALS(viewScalerPresetFactor(idx), want[i].factor);
		}
	}

	// Kernel primitives: factors, codes, and dispatch behave.
	void test_kernels() {
		IndexImage in = synthImgVS(3, 3);
		for (int k = 0; k < kKernCount; k++) {
			const int f = kernelFactor(k);
			TS_ASSERT(f == 2 || f == 3);
			TS_ASSERT(kernelCode(k) && strlen(kernelCode(k)) == 2);
			IndexImage out = applyKernel(k, in, 0xFF);
			TS_ASSERT_EQUALS(out.w, in.w * f);
			TS_ASSERT_EQUALS(out.h, in.h * f);
		}
		TS_ASSERT(sameImageVS(applyKernel(kKernScale2x, in, 0xFF), scale2x(in)));
		TS_ASSERT(sameImageVS(applyKernel(kKernNearest3, in, 0xFF), scaleNearest(in, 3)));
	}

	// resampleNearestExact: identity at equal dims; exact rational sampling
	// out(x,y) == in(x*inW/outW, y*inH/outH); integer upscale == scaleNearest.
	void test_resample_nearest_exact() {
		IndexImage in = synthImgVS(8, 8);
		TS_ASSERT(sameImageVS(resampleNearestExact(in, 8, 8), in));
		IndexImage down = resampleNearestExact(in, 6, 6);
		TS_ASSERT_EQUALS(down.w, 6);
		for (int y = 0; y < 6; y++)
			for (int x = 0; x < 6; x++)
				TS_ASSERT_EQUALS(down.pixels[(size_t)y * 6 + x],
				                 in.pixels[(size_t)(y * 8 / 6) * 8 + (x * 8 / 6)]);
		TS_ASSERT(sameImageVS(resampleNearestExact(in, 16, 16), scaleNearest(in, 2)));
	}

	// Preset clamping: out-of-range indices resolve to a valid preset.
	void test_preset_clamps() {
		const ViewScalerPreset &lo = viewScalerPreset(-5);
		const ViewScalerPreset &hi = viewScalerPreset(9999);
		TS_ASSERT_EQUALS(strcmp(lo.id, viewScalerPreset(0).id), 0);
		TS_ASSERT_EQUALS(strcmp(hi.id, viewScalerPreset(viewScalerPresetCount() - 1).id), 0);
	}
};
