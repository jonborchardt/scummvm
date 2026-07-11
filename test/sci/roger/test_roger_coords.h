#include <cxxtest/TestSuite.h>
#include "sci/roger/overlay/roger_coords.h"
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

	// --- Stretch-mode-aware game rect (mirrors backend populateDisplayAreaDrawRect) ---

	void test_game_rect_stretch_mode_fills_window() {
		// "Stretch to window" ignores aspect: the game rect IS the window.
		Common::Rect r = Sci::Roger::computeGameRect(1000, 300, false, Sci::Roger::kStretchStretch);
		TS_ASSERT_EQUALS(r.left, 0);
		TS_ASSERT_EQUALS(r.top, 0);
		TS_ASSERT_EQUALS(r.width(), 1000);
		TS_ASSERT_EQUALS(r.height(), 300);
	}

	void test_game_rect_fit_force_aspect_ignores_correction_flag() {
		// "Fit to window (4:3)" forces 4:3 even with aspect correction off.
		// Backend frac math: width = fracToInt(1200 * (intToFrac(4)/3)) = 1599.
		Common::Rect r = Sci::Roger::computeGameRect(2000, 1200, false, Sci::Roger::kStretchFitForceAspect);
		TS_ASSERT_EQUALS(r.height(), 1200);
		TS_ASSERT_EQUALS(r.width(), 1599);
		TS_ASSERT_EQUALS(r.left, 200); // (2000-1599)/2
	}

	void test_game_rect_center_mode_is_unscaled() {
		// "Center" shows the game surface at scaler size (320x200 at 1x), centered.
		Common::Rect r = Sci::Roger::computeGameRect(1000, 1000, false, Sci::Roger::kStretchCenter, 1);
		TS_ASSERT_EQUALS(r.width(), 320);
		TS_ASSERT_EQUALS(r.height(), 200);
		TS_ASSERT_EQUALS(r.left, 340);
		TS_ASSERT_EQUALS(r.top, 400);
	}

	void test_game_rect_center_mode_uses_render_scale() {
		// A 3x software scaler triples the centered surface.
		Common::Rect r = Sci::Roger::computeGameRect(1000, 1000, false, Sci::Roger::kStretchCenter, 3);
		TS_ASSERT_EQUALS(r.width(), 960);
		TS_ASSERT_EQUALS(r.height(), 600);
		TS_ASSERT_EQUALS(r.left, 20);
		TS_ASSERT_EQUALS(r.top, 200);
	}

	void test_game_rect_integral_mode_snaps_to_whole_multiples() {
		// "Pixel-perfect": largest integer multiple fitting the window (3x here).
		Common::Rect r = Sci::Roger::computeGameRect(1000, 1000, false, Sci::Roger::kStretchIntegral, 1);
		TS_ASSERT_EQUALS(r.width(), 960);
		TS_ASSERT_EQUALS(r.height(), 600);
	}

	// --- Canonical edge mapping: rect edges must agree with the nearest sampler ---
	//
	// Every Roger scaler samples source row `dy * srcN / dstD` (top-left rational).
	// The dest edge consistent with that sampling is ceil(v * dstD / srcN): dest
	// pixels [0, edge) sample exactly source rows [0, v). Flooring the edge instead
	// leaves the last dest pixel of a source row outside its rect -- the 1px-short
	// status bar.

	// Dest pixels whose top-left-rational sample lands before source row r.
	static int pixelsBeforeSourceRow(int r, int srcN, int dstD) {
		int count = 0;
		for (int dy = 0; dy < dstD; dy++) {
			if (dy * srcN / dstD < r) {
				count++;
			}
		}
		return count;
	}

	void test_rect_dest_edges_match_nearest_sampler() {
		const int heights[] = { 499, 998, 1975, 1986, 333, 200 };
		for (uint hi = 0; hi < ARRAYSIZE(heights); hi++) {
			const int gh = heights[hi];
			Common::Rect game(0, 0, 1600, gh);
			for (int r = 1; r < 200; r++) {
				const int edge = pixelsBeforeSourceRow(r, 200, gh);
				Common::Rect below = Sci::Roger::sciRectToDest(Common::Rect(0, 0, 320, r), game);
				Common::Rect above = Sci::Roger::sciRectToDest(Common::Rect(0, r, 320, 200), game);
				TS_ASSERT_EQUALS(below.bottom, edge);
				TS_ASSERT_EQUALS(above.top, edge);   // shared edge: adjacent rects abut
			}
		}
		// Same rule on the x axis at a non-integral width.
		Common::Rect gameX(0, 0, 799, 499);
		for (int x = 1; x < 320; x++) {
			const int edge = pixelsBeforeSourceRow(x, 320, 799);
			Common::Rect left = Sci::Roger::sciRectToDest(Common::Rect(0, 0, x, 200), gameX);
			TS_ASSERT_EQUALS(left.right, edge);
		}
	}

	void test_status_bar_edges_at_measured_sbs_scale() {
		// Measured (roger-300-bar capture, SBS panel 799x499): the nearest
		// scaler rendered the native white bar (rows 0-8) 23px tall and started the
		// scene at row 25. The banner rect and the picture rect must land on those
		// same edges -- floor gave 22 and 24 (bar 1px short, underline 1px thick).
		Common::Rect game(0, 738, 799, 738 + 499);
		Common::Rect bar = Sci::Roger::sciRectToDest(Common::Rect(0, 0, 320, 9), game);
		TS_ASSERT_EQUALS(bar.top, game.top);
		TS_ASSERT_EQUALS(bar.bottom - game.top, 23);      // ceil(9*499/200)
		Common::Rect pic = Sci::Roger::computePictureRect(game, 10, 200);
		TS_ASSERT_EQUALS(pic.top - game.top, 25);         // ceil(10*499/200)
		// The picture rect's strip edge is the SAME mapping sciRectToDest uses.
		Common::Rect strip = Sci::Roger::sciRectToDest(Common::Rect(0, 0, 320, 10), game);
		TS_ASSERT_EQUALS(strip.bottom, pic.top);
	}

	void test_rect_dest_negative_edges_round_toward_ceiling() {
		// Grown UI rects go off-screen (nr.grow(2)); ceil holds for negative edges
		// too (C++ toward-zero truncation IS ceil there).
		Common::Rect game(100, 50, 100 + 799, 50 + 499);
		Common::Rect d = Sci::Roger::sciRectToDest(Common::Rect(-2, -2, 322, 202), game);
		TS_ASSERT_EQUALS(d.left, 100 - 4);   // ceil(-2*799/320) = ceil(-4.99) = -4
		TS_ASSERT_EQUALS(d.top, 50 - 4);     // ceil(-2*499/200) = ceil(-4.99) = -4
	}

	void test_status_strip_remainder_is_menu_line() {
		// SCI0 pushes the bar as rows [0,9); the reserved strip is 10 rows. The
		// remainder is GfxPorts::_menuLine (the black underline row), which Roger
		// draws itself so the whole strip is overlay-owned.
		Common::Rect line = Sci::Roger::statusStripRemainder(Common::Rect(0, 0, 320, 9), 10);
		TS_ASSERT_EQUALS(line.left, 0);
		TS_ASSERT_EQUALS(line.top, 9);
		TS_ASSERT_EQUALS(line.right, 320);
		TS_ASSERT_EQUALS(line.bottom, 10);
		TS_ASSERT(Sci::Roger::statusStripRemainder(Common::Rect(0, 0, 320, 10), 10).isEmpty());
	}

	void test_game_rect_default_args_are_fit() {
		// The 3-arg form must stay the aspect-true letterboxed fit.
		Common::Rect def = Sci::Roger::computeGameRect(1600, 1600, false);
		Common::Rect fit = Sci::Roger::computeGameRect(1600, 1600, false, Sci::Roger::kStretchFit, 1);
		TS_ASSERT_EQUALS(def.left, fit.left);
		TS_ASSERT_EQUALS(def.top, fit.top);
		TS_ASSERT_EQUALS(def.right, fit.right);
		TS_ASSERT_EQUALS(def.bottom, fit.bottom);
		TS_ASSERT_EQUALS(def.width(), 1600);
		TS_ASSERT_EQUALS(def.height(), 1000);
	}

	// --- Cursor on-screen footprint: native game-px size, not surface px ---

	void test_cursor_rect_is_native_footprint_at_2x() {
		// A 16x16 game-px cursor at exact 2x scale occupies exactly 32x32 overlay px,
		// regardless of how large the enhanced cursor surface is.
		Common::Rect game(0, 0, 640, 400);
		Common::Rect r = Sci::Roger::cursorOverlayRect(Common::Point(100, 100), game,
		                                               Common::Point(16, 16), Common::Point(8, 8));
		TS_ASSERT_EQUALS(r.width(), 32);
		TS_ASSERT_EQUALS(r.height(), 32);
		// Centered hotspot: game rect (92,92)-(108,108) -> overlay (184,184)-(216,216).
		TS_ASSERT_EQUALS(r.left, 184);
		TS_ASSERT_EQUALS(r.top, 184);
	}

	void test_cursor_rect_tracks_game_rect_offset_and_scale() {
		// Letterboxed game rect: the cursor rect lands inside it at its scale.
		// Width 799: a 16 game-px cursor spans ceil-mapped edges, ~40px (2.497x).
		Common::Rect game(0, 738, 799, 738 + 499);
		Common::Rect r = Sci::Roger::cursorOverlayRect(Common::Point(0, 0), game,
		                                               Common::Point(16, 16), Common::Point(0, 0));
		TS_ASSERT_EQUALS(r.left, game.left);
		TS_ASSERT_EQUALS(r.top, game.top);
		TS_ASSERT_EQUALS(r.right, game.left + 40);  // ceil(16*799/320)
		TS_ASSERT_EQUALS(r.bottom, game.top + 40);  // ceil(16*499/200)
	}

	void test_cursor_rect_view_cel_uses_cel_dims() {
		// View-cel cursors keep their cel's native dims (not 16x16).
		Common::Rect game(0, 0, 640, 400);
		Common::Rect r = Sci::Roger::cursorOverlayRect(Common::Point(50, 50), game,
		                                               Common::Point(12, 20), Common::Point(6, 19));
		TS_ASSERT_EQUALS(r.width(), 24);   // 12 * 2x
		TS_ASSERT_EQUALS(r.height(), 40);  // 20 * 2x
		// Hotspot (6,19): rect starts at game (44,31) -> overlay (88,62).
		TS_ASSERT_EQUALS(r.left, 88);
		TS_ASSERT_EQUALS(r.top, 62);
	}
};
