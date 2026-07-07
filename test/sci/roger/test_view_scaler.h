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
	// Registry shape: at least one module; each module's declared factor
	// matches measured output dimensions; ids unique + filename-safe; labels
	// non-empty; id lookup round-trips.
	void test_registry_shape() {
		IndexImage in = synthImgVS(4, 3);
		TS_ASSERT(viewScalerCount() >= 1);
		for (int i = 0; i < viewScalerCount(); i++) {
			const ViewScaler &s = viewScaler(i);
			IndexImage out = applyViewScaler(i, in, 0xFF);
			TS_ASSERT_EQUALS(out.w, in.w * s.factor);
			TS_ASSERT_EQUALS(out.h, in.h * s.factor);
			TS_ASSERT(s.id && *s.id);
			TS_ASSERT(s.label && *s.label);
			for (const char *c = s.id; *c; c++)
				TS_ASSERT((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-');
			for (int j = 0; j < i; j++)
				TS_ASSERT(strcmp(viewScaler(j).id, s.id) != 0);
			TS_ASSERT_EQUALS(viewScalerIndexById(s.id), i);
		}
		TS_ASSERT_EQUALS(viewScalerIndexById("no-such-id"), -1);
		TS_ASSERT_EQUALS(viewScalerIndexById(nullptr), -1);
	}

	// Entry 0 is THE shipping module: id "s2-s3", factor 6, byte-identical to
	// scale6x() both directly and through the 6x-grid helper. This is the lock
	// that keeps the registry honest about the shipping path (and the proof
	// that this refactor did not change cel output — kTransformVersion holds).
	void test_shipping_module_is_scale6x() {
		IndexImage in = synthImgVS(8, 7);
		TS_ASSERT_EQUALS(strcmp(viewScaler(0).id, "s2-s3"), 0);
		TS_ASSERT_EQUALS(viewScaler(0).factor, 6);
		TS_ASSERT(sameImageVS(applyViewScaler(0, in, 0xFF), scale6x(in)));
		TS_ASSERT(sameImageVS(applyViewScalerTo6x(0, in, 0xFF), scale6x(in)));
	}

	// Index clamping: out-of-range indices resolve to a valid module.
	void test_index_clamps() {
		const ViewScaler &lo = viewScaler(-5);
		const ViewScaler &hi = viewScaler(9999);
		TS_ASSERT_EQUALS(strcmp(lo.id, viewScaler(0).id), 0);
		TS_ASSERT_EQUALS(strcmp(hi.id, viewScaler(viewScalerCount() - 1).id), 0);
	}

	// Every module lands on the 6x plate grid through applyViewScalerTo6x
	// (non-6x factors resample exact-rationally; 6x passes through).
	void test_to_6x_grid_dims() {
		IndexImage in = synthImgVS(9, 5);
		for (int i = 0; i < viewScalerCount(); i++) {
			IndexImage out = applyViewScalerTo6x(i, in, 0xFF);
			TS_ASSERT_EQUALS(out.w, in.w * 6);
			TS_ASSERT_EQUALS(out.h, in.h * 6);
		}
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
};
