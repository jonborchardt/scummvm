// test/sci/roger/test_compare_mode.h
#include <cxxtest/TestSuite.h>
#include "graphics/surface.h"
#include "sci/roger/overlay/roger_compositor.h"
using namespace Sci::Roger;

class RogerCompareModeTestSuite : public CxxTest::TestSuite {
public:
	void test_mode_cycles_enhanced_original_sidebyside() {
		TS_ASSERT_EQUALS(nextDisplayMode(kModeEnhanced), kModeOriginal);
		TS_ASSERT_EQUALS(nextDisplayMode(kModeOriginal), kModeSideBySide);
		TS_ASSERT_EQUALS(nextDisplayMode(kModeSideBySide), kModeEnhanced);
	}

	void test_panel_rects_split_and_letterbox() {
		Common::Rect left, right;
		comparePanelRects(2862, 1986, left, right);
		// Each panel occupies its own half horizontally (no overlap, within bounds).
		TS_ASSERT(left.right <= 1431);
		TS_ASSERT(right.left >= 1431);
		TS_ASSERT(right.right <= 2862);
		// 8:5 aspect preserved (within 1px rounding).
		TS_ASSERT_DELTA((double)left.width() / left.height(), 8.0 / 5.0, 0.01);
		TS_ASSERT_DELTA((double)right.width() / right.height(), 8.0 / 5.0, 0.01);
		// Half is 1431 wide; an 8:5 frame is width-limited there -> ~894 tall, vertically centered.
		TS_ASSERT_EQUALS(left.width(), 1431);
		TS_ASSERT(left.top > 0); // letterbox bars top and bottom
		TS_ASSERT(left.top + left.height() <= 1986);
	}

	void test_scale_blit_nearest_corners() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface src; src.create(2, 2, rgba);
		src.setPixel(0, 0, rgba.ARGBToColor(255, 255, 0, 0)); // red TL
		src.setPixel(1, 0, rgba.ARGBToColor(255, 0, 255, 0)); // green TR
		src.setPixel(0, 1, rgba.ARGBToColor(255, 0, 0, 255)); // blue BL
		src.setPixel(1, 1, rgba.ARGBToColor(255, 255, 255, 0)); // yellow BR
		Graphics::Surface dst; dst.create(4, 4, rgba);
		scaleBlitNearest(dst, Common::Rect(0, 0, 4, 4), src);
		TS_ASSERT_EQUALS(dst.getPixel(0, 0), rgba.ARGBToColor(255, 255, 0, 0));   // TL red
		TS_ASSERT_EQUALS(dst.getPixel(3, 0), rgba.ARGBToColor(255, 0, 255, 0));   // TR green
		TS_ASSERT_EQUALS(dst.getPixel(0, 3), rgba.ARGBToColor(255, 0, 0, 255));   // BL blue
		TS_ASSERT_EQUALS(dst.getPixel(3, 3), rgba.ARGBToColor(255, 255, 255, 0)); // BR yellow
		src.free(); dst.free();
	}

	void test_scaleBlitNearest_srcRect_subregion() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface src;
		src.create(8, 4, rgba);
		const uint32 red  = rgba.ARGBToColor(255, 255, 0, 0);
		const uint32 blue = rgba.ARGBToColor(255, 0, 0, 255);
		for (int y = 0; y < 4; y++)
			for (int x = 0; x < 8; x++)
				src.setPixel(x, y, x < 4 ? red : blue);
		Graphics::Surface dst;
		dst.create(4, 4, rgba);
		dst.fillRect(Common::Rect(0, 0, 4, 4), rgba.ARGBToColor(255, 0, 0, 0));
		// Blit only the right (blue) half: every dest pixel must be blue.
		scaleBlitNearest(dst, Common::Rect(0, 0, 4, 4), src, Common::Rect(4, 0, 8, 4));
		TS_ASSERT_EQUALS(dst.getPixel(0, 0), blue);
		TS_ASSERT_EQUALS(dst.getPixel(3, 0), blue);
		TS_ASSERT_EQUALS(dst.getPixel(0, 3), blue);
		TS_ASSERT_EQUALS(dst.getPixel(3, 3), blue);
		src.free();
		dst.free();
	}

	void test_scaleBlitNearest_srcRect_full_matches_legacy() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface src;
		src.create(6, 2, rgba);
		for (int y = 0; y < 2; y++)
			for (int x = 0; x < 6; x++)
				src.setPixel(x, y, rgba.ARGBToColor(255, (byte)(x * 40), (byte)(y * 100), 7));
		Graphics::Surface a, b;
		a.create(9, 4, rgba); b.create(9, 4, rgba);
		scaleBlitNearest(a, Common::Rect(0, 0, 9, 4), src);
		scaleBlitNearest(b, Common::Rect(0, 0, 9, 4), src, Common::Rect(0, 0, 6, 2));
		for (int y = 0; y < 4; y++)
			for (int x = 0; x < 9; x++)
				TS_ASSERT_EQUALS(a.getPixel(x, y), b.getPixel(x, y));
		src.free(); a.free(); b.free();
	}
};
