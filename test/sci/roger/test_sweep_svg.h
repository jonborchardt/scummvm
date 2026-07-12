#include <cxxtest/TestSuite.h>
#include <cstring>
#include "sci/roger/utils/studio/roger_sweep_svg.h"
#include "graphics/surface.h"
#include "graphics/pixelformat.h"
using namespace Sci::Roger;

// Fake PNG payloads: layer 1 never inspects the bytes, so any bytes do.
// base64("PNGA") == "UE5HQQ==", base64("PNGB") == "UE5HQg==".
static const byte kPngA[] = { 'P', 'N', 'G', 'A' };
static const byte kPngB[] = { 'P', 'N', 'G', 'B' };

class RogerSweepSvgTestSuite : public CxxTest::TestSuite {
	Common::String buildIt(bool animated, int w = 1920, int h = 1140) {
		return buildSweepSvgFromPngData(kPngA, 4, kPngB, 4, w, h, animated);
	}

public:
	void test_document_structure_and_ids() {
		const Common::String s = buildIt(false);
		TS_ASSERT(s.hasPrefix("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"));
		TS_ASSERT(s.contains("viewBox=\"0 0 1920 1140\""));
		TS_ASSERT(s.contains("id=\"picSweepReveal\""));
		TS_ASSERT(s.contains("id=\"picSweepRevealRect\""));
		TS_ASSERT(s.contains("id=\"picSweepHandle\""));
		TS_ASSERT(s.contains("</svg>"));
	}

	void test_both_images_embedded_left_clipped_right_full() {
		const Common::String s = buildIt(false);
		// Right (B) image is the full-frame background and must come FIRST;
		// left (A) is inside the clip group.
		const char *right = strstr(s.c_str(), "data:image/png;base64,UE5HQg==");
		const char *left = strstr(s.c_str(), "data:image/png;base64,UE5HQQ==");
		const char *clip = strstr(s.c_str(), "clip-path=\"url(#picSweepReveal)\"");
		TS_ASSERT(right != nullptr);
		TS_ASSERT(left != nullptr);
		TS_ASSERT(clip != nullptr);
		TS_ASSERT(right < clip);
		TS_ASSERT(clip < left);
	}

	void test_labels_block_after_images_script_last() {
		const Common::String s = buildIt(false);
		const char *left = strstr(s.c_str(), "UE5HQQ==");
		const char *marker = strstr(s.c_str(), "EDIT LABELS HERE");
		const char *script = strstr(s.c_str(), "<script>");
		TS_ASSERT(marker != nullptr);
		TS_ASSERT(script != nullptr);
		TS_ASSERT(left < marker);
		TS_ASSERT(marker < script);
		// Placeholder label texts present.
		TS_ASSERT(s.contains(">A</text>"));
		TS_ASSERT(s.contains(">B</text>"));
	}

	void test_animated_variant_has_smil_interactive_does_not() {
		const Common::String anim = buildIt(true);
		const Common::String inter = buildIt(false);
		TS_ASSERT(anim.contains("<animate "));
		TS_ASSERT(anim.contains("<animateTransform "));
		TS_ASSERT(anim.contains("calcMode=\"spline\""));
		TS_ASSERT(anim.contains("keySplines=\"0.42 0 0.58 1;0.42 0 0.58 1\""));
		TS_ASSERT(!inter.contains("<animate"));
		// Animated starts at SWEEP_MIN (120 at 1920 wide); interactive at W/2.
		TS_ASSERT(anim.contains("width=\"120\""));
		TS_ASSERT(anim.contains("values=\"120;1800;120\""));
		TS_ASSERT(inter.contains("width=\"960\""));
	}

	void test_script_block_is_xml_safe() {
		const Common::String s = buildIt(false);
		const char *open = strstr(s.c_str(), "<script>");
		const char *close = strstr(s.c_str(), "</script>");
		TS_ASSERT(open != nullptr);
		TS_ASSERT(close != nullptr);
		for (const char *p = open + 8; p < close; p++) {
			TS_ASSERT(*p != '<');
			TS_ASSERT(*p != '>');
			TS_ASSERT(*p != '&');
		}
		// The script's essential hooks survived the port.
		TS_ASSERT(s.contains("picSweepRevealRect"));
		TS_ASSERT(s.contains("stopAutoAnim"));
		TS_ASSERT(s.contains("pointerdown"));
	}

	void test_output_is_pure_ascii() {
		const Common::String s = buildIt(true);
		for (uint i = 0; i < s.size(); i++) {
			TS_ASSERT((byte)s[i] < 0x80);
		}
	}

	void test_geometry_scales_to_width() {
		// At 320 wide everything is /6 of the 1920 reference, min 1.
		const Common::String s = buildIt(true, 320, 190);
		TS_ASSERT(s.contains("viewBox=\"0 0 320 190\""));
		TS_ASSERT(s.contains("values=\"20;300;20\""));  // SWEEP_MIN/MAX scaled
		TS_ASSERT(s.contains("r=\"6\""));               // circle r 36/6
		TS_ASSERT(s.contains("stroke-width=\"1\""));    // line stroke 6/6
	}

	void test_failure_paths_return_empty() {
		TS_ASSERT(buildSweepSvgFromPngData(nullptr, 0, kPngB, 4, 10, 10, false).empty());
		TS_ASSERT(buildSweepSvgFromPngData(kPngA, 4, kPngB, 0, 10, 10, false).empty());
		TS_ASSERT(buildSweepSvgFromPngData(kPngA, 4, kPngB, 4, 0, 10, false).empty());
	}

	// w x h RGBA surface with pixel (x,y) = (x, y, 0, 255) for sampling checks.
	static Graphics::Surface *makePattern(int w, int h) {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *s = new Graphics::Surface();
		s->create(w, h, rgba);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				*(uint32 *)s->getBasePtr(x, y) = rgba.ARGBToColor(255, x, y, 0);
			}
		}
		return s;
	}

	void test_downscale_nearest_samples_top_left() {
		Graphics::Surface *src = makePattern(6, 6);
		Graphics::Surface *out = downscaleNearest(*src, 3);
		TS_ASSERT(out != nullptr);
		TS_ASSERT_EQUALS(out->w, 2);
		TS_ASSERT_EQUALS(out->h, 2);
		// Block top-left samples: (0,0), (3,0), (0,3), (3,3).
		uint8 a, r, g, b;
		out->format.colorToARGB(*(const uint32 *)out->getBasePtr(1, 1), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 3);
		TS_ASSERT_EQUALS((int)g, 3);
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_downscale_divisor_one_is_identity_copy() {
		Graphics::Surface *src = makePattern(4, 4);
		Graphics::Surface *out = downscaleNearest(*src, 1);
		TS_ASSERT(out != nullptr);
		TS_ASSERT_EQUALS(out->w, 4);
		TS_ASSERT_EQUALS(out->h, 4);
		TS_ASSERT_EQUALS(*(const uint32 *)out->getBasePtr(2, 3),
		                 *(const uint32 *)src->getBasePtr(2, 3));
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_build_sweep_svg_from_surfaces() {
		Graphics::Surface *a = makePattern(12, 12);
		Graphics::Surface *b = makePattern(12, 12);
		const Common::String svg = buildSweepSvg(*a, *b, 2, false);
		TS_ASSERT(!svg.empty());
		// Downscaled dims drive the viewBox.
		TS_ASSERT(svg.contains("viewBox=\"0 0 6 6\""));
		// Real PNG bytes now: the base64 of the PNG signature 0x89 P N G is "iVBORw".
		TS_ASSERT(svg.contains("data:image/png;base64,iVBORw"));
		a->free(); delete a;
		b->free(); delete b;
	}

	void test_build_sweep_svg_rejects_mismatched_sizes() {
		Graphics::Surface *a = makePattern(12, 12);
		Graphics::Surface *b = makePattern(6, 6);
		TS_ASSERT(buildSweepSvg(*a, *b, 1, false).empty());
		a->free(); delete a;
		b->free(); delete b;
	}

	static Graphics::Surface *makeBlank(int w, int h) {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *s = new Graphics::Surface();
		s->create(w, h, rgba);
		return s;
	}

	static void putPx(Graphics::Surface *s, int x, int y, int a, int r, int g, int b) {
		*(uint32 *)s->getBasePtr(x, y) = s->format.ARGBToColor(a, r, g, b);
	}

	static void getPx(const Graphics::Surface *s, int x, int y,
	                  uint8 &a, uint8 &r, uint8 &g, uint8 &b) {
		s->format.colorToARGB(*(const uint32 *)s->getBasePtr(x, y), a, r, g, b);
	}

	void test_area_resample_preserves_replicated_pixels() {
		// A 2x2 base pattern replicated 6x (12x12), resampled to 4x4 (/3),
		// must come out as the base pattern replicated 2x: every 3x3 sample
		// block is uniform, so area averaging cannot blur pixel art.
		const int base[2][2][3] = {
			{ { 255, 0, 0 }, { 0, 200, 34 } },
			{ { 12, 34, 56 }, { 240, 240, 240 } },
		};
		Graphics::Surface *src = makeBlank(12, 12);
		for (int y = 0; y < 12; y++) {
			for (int x = 0; x < 12; x++) {
				const int *c = base[y / 6][x / 6];
				putPx(src, x, y, 255, c[0], c[1], c[2]);
			}
		}
		Graphics::Surface *out = areaResample(*src, 4, 4);
		TS_ASSERT(out != nullptr);
		TS_ASSERT_EQUALS(out->w, 4);
		TS_ASSERT_EQUALS(out->h, 4);
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				const int *c = base[y / 2][x / 2];
				uint8 a, r, g, b;
				getPx(out, x, y, a, r, g, b);
				TS_ASSERT_EQUALS((int)a, 255);
				TS_ASSERT((int)r >= c[0] - 1 && (int)r <= c[0] + 1);
				TS_ASSERT((int)g >= c[1] - 1 && (int)g <= c[1] + 1);
				TS_ASSERT((int)b >= c[2] - 1 && (int)b <= c[2] + 1);
			}
		}
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_area_resample_averages_in_linear_light() {
		// black + white averaged in linear light is 0.5 linear = ~188 sRGB,
		// NOT the naive byte midpoint 128.
		Graphics::Surface *src = makeBlank(2, 1);
		putPx(src, 0, 0, 255, 0, 0, 0);
		putPx(src, 1, 0, 255, 255, 255, 255);
		Graphics::Surface *out = areaResample(*src, 1, 1);
		TS_ASSERT(out != nullptr);
		uint8 a, r, g, b;
		getPx(out, 0, 0, a, r, g, b);
		TS_ASSERT((int)r >= 186 && (int)r <= 190);
		TS_ASSERT_EQUALS((int)r, (int)g);
		TS_ASSERT_EQUALS((int)g, (int)b);
		TS_ASSERT_EQUALS((int)a, 255);
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_area_resample_premultiplies_alpha() {
		// Fully transparent red + opaque green: the red must not bleed into
		// the average (premultiplied), and alpha averages to ~half.
		Graphics::Surface *src = makeBlank(2, 1);
		putPx(src, 0, 0, 0, 255, 0, 0);
		putPx(src, 1, 0, 255, 0, 255, 0);
		Graphics::Surface *out = areaResample(*src, 1, 1);
		TS_ASSERT(out != nullptr);
		uint8 a, r, g, b;
		getPx(out, 0, 0, a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);
		TS_ASSERT_EQUALS((int)g, 255);
		TS_ASSERT((int)a >= 127 && (int)a <= 128);
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_area_resample_fractional_overlap() {
		// 3 -> 2: dst0 covers src [0,1.5) = white + half white -> white;
		// dst1 covers [1.5,3) = half white + black -> linear 1/3 -> sRGB ~156.
		Graphics::Surface *src = makeBlank(3, 1);
		putPx(src, 0, 0, 255, 255, 255, 255);
		putPx(src, 1, 0, 255, 255, 255, 255);
		putPx(src, 2, 0, 255, 0, 0, 0);
		Graphics::Surface *out = areaResample(*src, 2, 1);
		TS_ASSERT(out != nullptr);
		uint8 a, r, g, b;
		getPx(out, 0, 0, a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255);
		getPx(out, 1, 0, a, r, g, b);
		TS_ASSERT((int)r >= 154 && (int)r <= 158);
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_area_resample_identity_roundtrip() {
		// Same-size resample = per-pixel sRGB->linear->sRGB round trip;
		// each channel must survive within 1.
		Graphics::Surface *src = makePattern(4, 4);
		Graphics::Surface *out = areaResample(*src, 4, 4);
		TS_ASSERT(out != nullptr);
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				uint8 sa, sr, sg, sb, oa, orr, og, ob;
				getPx(src, x, y, sa, sr, sg, sb);
				getPx(out, x, y, oa, orr, og, ob);
				TS_ASSERT_EQUALS((int)oa, (int)sa);
				TS_ASSERT((int)orr >= (int)sr - 1 && (int)orr <= (int)sr + 1);
				TS_ASSERT((int)og >= (int)sg - 1 && (int)og <= (int)sg + 1);
				TS_ASSERT((int)ob >= (int)sb - 1 && (int)ob <= (int)sb + 1);
			}
		}
		out->free(); delete out;
		src->free(); delete src;
	}

	void test_area_resample_rejects_invalid_sizes() {
		Graphics::Surface *src = makePattern(4, 4);
		TS_ASSERT(areaResample(*src, 8, 4) == nullptr);   // upscale refused
		TS_ASSERT(areaResample(*src, 4, 8) == nullptr);
		TS_ASSERT(areaResample(*src, 0, 4) == nullptr);
		TS_ASSERT(areaResample(*src, 4, 0) == nullptr);
		src->free(); delete src;
	}
};
