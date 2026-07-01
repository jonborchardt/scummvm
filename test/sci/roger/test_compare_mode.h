// test/sci/roger/test_compare_mode.h
#include <cxxtest/TestSuite.h>
#include "graphics/surface.h"
#include "sci/roger/roger_compositor.h"
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

	void test_remap_left_panel_center_maps_to_game_center() {
		// A point at the visual center of the LEFT frame should map to game (160,100).
		Common::Rect left, right;
		comparePanelRects(2862, 1986, left, right);
		// Convert the left frame's center (overlay px) back to a full-window game coord:
		const int cx = (left.left + left.right) / 2;
		const int cy = (left.top + left.bottom) / 2;
		Common::Point winGame((int16)(cx * 320 / 2862), (int16)(cy * 200 / 1986));
		bool onLeft = false; Common::Point out;
		const bool inside = remapCompareMouse(winGame, 2862, 1986, onLeft, out);
		TS_ASSERT(inside);
		TS_ASSERT(onLeft);
		TS_ASSERT_DELTA(out.x, 160, 3);
		TS_ASSERT_DELTA(out.y, 100, 3);
	}

	void test_remap_right_panel_detected() {
		Common::Rect left, right;
		comparePanelRects(2862, 1986, left, right);
		const int cx = (right.left + right.right) / 2;
		const int cy = (right.top + right.bottom) / 2;
		Common::Point winGame((int16)(cx * 320 / 2862), (int16)(cy * 200 / 1986));
		bool onLeft = true; Common::Point out;
		const bool inside = remapCompareMouse(winGame, 2862, 1986, onLeft, out);
		TS_ASSERT(inside);
		TS_ASSERT(!onLeft);
		TS_ASSERT_DELTA(out.x, 160, 3);
		TS_ASSERT_DELTA(out.y, 100, 3);
	}
};
