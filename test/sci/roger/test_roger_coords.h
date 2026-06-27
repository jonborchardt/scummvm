#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_coords.h"
#include "common/rect.h"

class TestRogerCoords : public CxxTest::TestSuite {
public:
	void test_cel_rect_scales_6x() {
		Common::Rect in(10, 20, 30, 40);          // 320x200 space
		Common::Rect out = Sci::Roger::sciCelRectToOverlay(in, 1920, 1200);
		TS_ASSERT_EQUALS(out.left, 60);           // 10 * (1920/320)
		TS_ASSERT_EQUALS(out.top, 120);           // 20 * (1200/200)
		TS_ASSERT_EQUALS(out.right, 180);         // 30 * 6
		TS_ASSERT_EQUALS(out.bottom, 240);        // 40 * 6
	}
	void test_priority_band_above_horizon_is_zero() {
		TS_ASSERT_EQUALS(Sci::Roger::sciPriorityBand(10), 0);
	}
	void test_priority_band_increases_with_y() {
		int top = Sci::Roger::sciPriorityBand(45);
		int bottom = Sci::Roger::sciPriorityBand(185);
		TS_ASSERT(bottom > top);
		TS_ASSERT(top >= 1);
		TS_ASSERT(bottom <= 14);
	}

	// --- H4 geometry: game rect + picture sub-rect ---

	void test_fit_centered_letterbox_tall_window() {
		// A 320x200 (1.6) box in a tall 1000x1000 window: width-limited, centered vertically.
		Common::Rect r = Sci::Roger::fitCentered(320, 200, 1000, 1000);
		TS_ASSERT_EQUALS(r.width(), 1000);
		TS_ASSERT_EQUALS(r.height(), 625);          // 1000 * 200/320
		TS_ASSERT_EQUALS(r.top, 187);               // (1000-625)/2
		TS_ASSERT_EQUALS(r.left, 0);
	}

	void test_fit_centered_pillarbox_wide_window() {
		// Same box in a wide 1000x300 window: height-limited, centered horizontally.
		Common::Rect r = Sci::Roger::fitCentered(320, 200, 1000, 300);
		TS_ASSERT_EQUALS(r.height(), 300);
		TS_ASSERT_EQUALS(r.width(), 480);           // 300 * 320/200
		TS_ASSERT_EQUALS(r.left, 260);              // (1000-480)/2
	}

	void test_game_rect_aspect_correction_changes_shape() {
		// 4:3 (corrected) is taller/narrower than native 1.6 for the same window.
		Common::Rect plain = Sci::Roger::computeGameRect(1600, 1600, false); // 320x200
		Common::Rect corr  = Sci::Roger::computeGameRect(1600, 1600, true);  // 320x240
		TS_ASSERT_EQUALS(plain.width(), 1600);
		TS_ASSERT_EQUALS(plain.height(), 1000);     // 1600*200/320
		TS_ASSERT_EQUALS(corr.width(), 1600);
		TS_ASSERT_EQUALS(corr.height(), 1200);      // 1600*240/320 -> taller
		TS_ASSERT(corr.height() > plain.height());
	}

	void test_picture_rect_reserves_status_bar_strip() {
		// 320x200 game rect of height 1000: a 10-row status bar = 50px strip at top.
		Common::Rect game(0, 0, 1600, 1000);
		Common::Rect pic = Sci::Roger::computePictureRect(game, 10, 200);
		TS_ASSERT_EQUALS(pic.top, 50);              // 1000 * 10/200
		TS_ASSERT_EQUALS(pic.bottom, 1000);
		TS_ASSERT_EQUALS(pic.height(), 950);        // 190/200 of the game rect
		TS_ASSERT_EQUALS(pic.left, 0);
		TS_ASSERT_EQUALS(pic.right, 1600);
	}

	void test_picture_rect_zero_status_bar_is_full_game_rect() {
		Common::Rect game(5, 7, 100, 200);
		Common::Rect pic = Sci::Roger::computePictureRect(game, 0, 200);
		TS_ASSERT_EQUALS(pic.top, game.top);
		TS_ASSERT_EQUALS(pic.bottom, game.bottom);
	}
};
