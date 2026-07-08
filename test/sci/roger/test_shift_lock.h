#include <cxxtest/TestSuite.h>
#include "sci/roger/gen/roger_scale.h"
#include "sci/roger/gen/roger_pic_native.h"
#include "sci/roger/gen/roger_omyac.h"
#include "sci/roger/utils/studio/roger_studio_render.h"
using namespace Sci::Roger;

// Locks against sub-pixel drift ("shifting") in the upscalers. If one of these
// fails after a pipeline change, the pipeline gained a systematic dx/dy bias â€”
// exactly the class of bug the studio's Diff/offset tools diagnose visually.
class RogerShiftLockTestSuite : public CxxTest::TestSuite {
	// Centroid of non-background mass, in output pixels.
	// zero = the background byte to skip.
	static void centroid(const Common::Array<byte> &px, int w, int h, byte zero,
	                     double &cx, double &cy) {
		double sx = 0, sy = 0; long n = 0;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				if (px[y * w + x] != zero) { sx += x; sy += y; n++; }
		cx = n ? sx / n : 0; cy = n ? sy / n : 0;
	}

	// Most-common byte value in the array â€” used as the background sentinel when
	// renderOmyac's fillNullPixels replaces every 0xff with a fill color so that
	// the naive 0xff-background centroid would cover the whole image.
	static byte mostCommonByte(const Common::Array<byte> &px) {
		int freq[256] = {};
		for (uint i = 0; i < px.size(); i++) freq[(uint8)px[i]]++;
		byte best = 0;
		for (int v = 1; v < 256; v++)
			if (freq[v] > freq[(int)best]) best = (byte)v;
		return best;
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
	// widen the tolerance â€” the failure IS the diagnosis of a real drift.
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

		// Detect whether fillNullPixels replaced all 0xff values; if so, use the
		// most-common byte as the background sentinel so the centroid is correct.
		bool hasBackground = false;
		for (uint i = 0; i < omyac.pixels.size(); i++)
			if (omyac.pixels[i] == 0xff) { hasBackground = true; break; }
		byte bgByte = hasBackground ? (byte)0xff : mostCommonByte(omyac.pixels);

		double ox, oy;
		centroid(omyac.pixels, OMYAC_HYBRID_W, OMYAC_HYBRID_H, bgByte, ox, oy);

		TS_ASSERT_DELTA(ox, nx, 1.0);
		TS_ASSERT_DELTA(oy, ny, 1.0);
	}
};
