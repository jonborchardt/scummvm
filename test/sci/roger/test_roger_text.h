/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_text.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "common/array.h"

using namespace Sci::Roger;

class TestRogerText : public CxxTest::TestSuite {
public:
	void test_fit_font_index_picks_largest_that_fits() {
		const Graphics::Font *small = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
		const Graphics::Font *big   = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		TS_ASSERT(small != nullptr);
		TS_ASSERT(big != nullptr);
		Common::Array<const Graphics::Font *> fonts;
		fonts.push_back(small);
		fonts.push_back(big);

		// A box big enough for the big font -> index 1.
		int wideW = big->getStringWidth("OK") + 50;
		int tallH = big->getFontHeight() + 10;
		TS_ASSERT_EQUALS(fitFontIndex(fonts, "OK", wideW, tallH), 1);

		// A box too short for the big font -> falls back to the smaller (index 0).
		// One pixel under the big font's height guarantees it cannot fit, whether or
		// not the two built-in fonts happen to share a height on this build.
		int shortH = big->getFontHeight() - 1;
		TS_ASSERT_EQUALS(fitFontIndex(fonts, "OK", wideW, shortH), 0);

		// Nothing fits a 1px box -> falls back to smallest (index 0), never -1 here.
		TS_ASSERT_EQUALS(fitFontIndex(fonts, "OK", 1, 1), 0);
	}

	void test_fit_font_index_empty_is_minus_one() {
		Common::Array<const Graphics::Font *> fonts;
		TS_ASSERT_EQUALS(fitFontIndex(fonts, "x", 100, 100), -1);
	}

	void test_edit_text_top_aligned_starts_near_top() {
		// firstLineTop(top, boxH, lines, lineH, vAlignTop)
		TS_ASSERT_EQUALS(firstLineTop(0, 100, 1, 20, /*top*/true), 0);
		TS_ASSERT_EQUALS(firstLineTop(0, 100, 1, 20, /*centre*/false), 40);
		// Block taller than the box clamps to the top in either mode.
		TS_ASSERT_EQUALS(firstLineTop(10, 20, 3, 20, /*centre*/false), 10);
	}

	void test_fit_by_height_and_width_respects_both_caps() {
		const Graphics::Font *small = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
		const Graphics::Font *big   = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		TS_ASSERT(small != nullptr);
		TS_ASSERT(big != nullptr);
		Common::Array<const Graphics::Font *> fonts;
		fonts.push_back(small);
		fonts.push_back(big);

		// Tall + wide enough for the big font -> index 1.
		int tallH = big->getFontHeight() + 10;
		int wideW = big->getStringWidth("OK") + 50;
		TS_ASSERT_EQUALS(fitFontIndexByHeightAndWidth(fonts, "OK", tallH, wideW), 1);

		// Height OK but width too narrow for the big font -> falls back to small (0).
		int narrowW = small->getStringWidth("OK"); // big is at least as wide
		TS_ASSERT_EQUALS(fitFontIndexByHeightAndWidth(fonts, "OK", tallH, narrowW), 0);

		// maxW <= 0 means width unbounded -> height alone decides (big fits) -> 1.
		TS_ASSERT_EQUALS(fitFontIndexByHeightAndWidth(fonts, "OK", tallH, 0), 1);

		// Nothing fits a 1px-tall box -> smallest (0), never -1 here.
		TS_ASSERT_EQUALS(fitFontIndexByHeightAndWidth(fonts, "OK", 1, wideW), 0);
	}

	void test_fit_by_height_and_width_empty_is_minus_one() {
		Common::Array<const Graphics::Font *> fonts;
		TS_ASSERT_EQUALS(fitFontIndexByHeightAndWidth(fonts, "x", 100, 100), -1);
	}

	void test_roger_target_px_scales_native_height_by_overlay_and_user_pct() {
		// 9px native font, overlay game-area height 2000 (10x of 200), 100% user scale -> 90.
		TS_ASSERT_EQUALS(rogerTargetPx(9, 2000, 100), 90);
		// 150% user scale -> 135.
		TS_ASSERT_EQUALS(rogerTargetPx(9, 2000, 150), 135);
		// Unknown native height -> 0 (caller falls back to role/box).
		TS_ASSERT_EQUALS(rogerTargetPx(0, 2000, 100), 0);
	}

	void test_shared_group_scale_shrinks_group_proportionally() {
		// One element only fits at 90% of its ideal; its sibling fits fully.
		// Both must shrink by the group's worst ratio, preserving the 2:1
		// native size relationship (100:50 -> 90:45), never unifying to one size.
		Common::Array<TextSizeFit> items;
		TextSizeFit a; a.group = 1; a.idealPx = 100; a.fitPx = 90;
		TextSizeFit b; b.group = 1; b.idealPx = 50;  b.fitPx = 50;
		items.push_back(a); items.push_back(b);
		applySharedGroupScale(items);
		TS_ASSERT_EQUALS(items[0].fitPx, 90);
		TS_ASSERT_EQUALS(items[1].fitPx, 45);
	}

	void test_shared_group_scale_groups_are_independent() {
		// A squeezed element in group 1 must not shrink group 2.
		Common::Array<TextSizeFit> items;
		TextSizeFit a; a.group = 1; a.idealPx = 100; a.fitPx = 50;
		TextSizeFit b; b.group = 2; b.idealPx = 100; b.fitPx = 100;
		items.push_back(a); items.push_back(b);
		applySharedGroupScale(items);
		TS_ASSERT_EQUALS(items[0].fitPx, 50);
		TS_ASSERT_EQUALS(items[1].fitPx, 100);
	}

	void test_shared_group_scale_takes_group_minimum() {
		Common::Array<TextSizeFit> items;
		TextSizeFit a; a.group = 7; a.idealPx = 60; a.fitPx = 60;  // fits fully
		TextSizeFit b; b.group = 7; b.idealPx = 60; b.fitPx = 30;  // worst: 50%
		TextSizeFit c; c.group = 7; c.idealPx = 60; c.fitPx = 45;  // 75%
		items.push_back(a); items.push_back(b); items.push_back(c);
		applySharedGroupScale(items);
		TS_ASSERT_EQUALS(items[0].fitPx, 30);
		TS_ASSERT_EQUALS(items[1].fitPx, 30);
		TS_ASSERT_EQUALS(items[2].fitPx, 30);
	}

	void test_shared_group_scale_ignores_non_text_entries() {
		// idealPx == 0 marks a non-text element: it must neither poison the
		// group ratio nor be rescaled itself.
		Common::Array<TextSizeFit> items;
		TextSizeFit a; a.group = 1; a.idealPx = 0;   a.fitPx = 0;
		TextSizeFit b; b.group = 1; b.idealPx = 100; b.fitPx = 80;
		items.push_back(a); items.push_back(b);
		applySharedGroupScale(items);
		TS_ASSERT_EQUALS(items[0].fitPx, 0);
		TS_ASSERT_EQUALS(items[1].fitPx, 80);
	}

	void test_shared_group_scale_never_grows_an_element() {
		// A fit reported above ideal (defensive) is treated as ratio 1.
		Common::Array<TextSizeFit> items;
		TextSizeFit a; a.group = 3; a.idealPx = 50; a.fitPx = 80;
		items.push_back(a);
		applySharedGroupScale(items);
		TS_ASSERT_EQUALS(items[0].fitPx, 50);
	}

	void test_optical_block_top_centres_ink_not_cell() {
		// Single line, cell 24 with ink rows 10..20 (TTF-style top leading):
		// centring the INK in a 100-tall box puts the ink top at (100-10)/2 = 45,
		// so the cell top the draw call needs is 45 - 10 = 35. Cell centring
		// would have said (100-24)/2 = 38 — visibly low.
		TS_ASSERT_EQUALS(opticalBlockTop(0, 100, 1, 24, 10, 20), 35);
		// Ink that fills the cell exactly == the old cell centring.
		TS_ASSERT_EQUALS(opticalBlockTop(0, 100, 1, 20, 0, 20), 40);
		// Two lines, lh 20, per-line ink 4..16: block ink spans 4..36 (h=32) ->
		// ink top at (100-32)/2 = 34 -> cell top = 30.
		TS_ASSERT_EQUALS(opticalBlockTop(0, 100, 2, 20, 4, 16), 30);
		// Degenerate ink (empty bbox) falls back to cell centring.
		TS_ASSERT_EQUALS(opticalBlockTop(0, 100, 1, 20, 5, 5),
		                 firstLineTop(0, 100, 1, 20, false));
		// Non-zero box top offsets the result.
		TS_ASSERT_EQUALS(opticalBlockTop(10, 100, 1, 24, 10, 20), 45);
	}

	void test_text_scale_group_unifies_generic_text_across_ports() {
		// Generic text (0x6 namespace) is tokened by the CURRENT port at draw time —
		// the QFG1 char sheet draws labels under the window port (id 3) and stat
		// redraws under the picture port (id 2). Same screen, same font -> same group,
		// or redrawn values change size relative to their labels.
		TS_ASSERT_EQUALS(textScaleGroup(0x60000003u, false, false, 0),
		                 textScaleGroup(0x60000002u, false, false, 5));
		// Controls keep their per-window grouping.
		TS_ASSERT_EQUALS(textScaleGroup(0x40000003u, false, false, 1),
		                 textScaleGroup(0x40000003u, false, false, 2));
		TS_ASSERT_DIFFERS(textScaleGroup(0x40000003u, false, false, 1),
		                  textScaleGroup(0x40000004u, false, false, 1));
	}

	void test_text_scale_group_multiline_is_singleton() {
		// A multi-line (wrap-fit) element shrinks to fit its own box; that squeeze is
		// local and must never drag sibling single-line text down. Each multi-line
		// element gets a group of its own (keyed by element index).
		TS_ASSERT_DIFFERS(textScaleGroup(0x60000002u, false, true, 3),
		                  textScaleGroup(0x60000002u, false, false, 4));
		TS_ASSERT_DIFFERS(textScaleGroup(0x60000002u, false, true, 3),
		                  textScaleGroup(0x60000002u, false, true, 4));
	}

	void test_text_scale_group_separates_fonts() {
		// Body and alt/header fonts are sized by different renderers -> never one group.
		TS_ASSERT_DIFFERS(textScaleGroup(0x60000002u, false, false, 0),
		                  textScaleGroup(0x60000002u, true, false, 0));
	}

	void test_scaled_ideal_px_applies_global_multiplier() {
		RogerTextRenderer tr("");
		TS_ASSERT(tr.ok());
		tr.setGlobalScale(150);
		TS_ASSERT_EQUALS(tr.scaledIdealPx(40), 60);
		tr.setGlobalScale(100);
		TS_ASSERT_EQUALS(tr.scaledIdealPx(40), 40);
	}

	void test_fit_px_caps_to_ideal_and_box_height() {
		RogerTextRenderer tr("");
		TS_ASSERT(tr.ok());
		// Roomy box, no width cap: the element fits at its ideal size.
		TS_ASSERT_EQUALS(tr.fitPx("OK", 500, 300, 40, 0), 40);
		// Box shorter than the ideal: capped to the box height.
		TS_ASSERT_EQUALS(tr.fitPx("OK", 500, 20, 40, 0), 20);
	}

	void test_fit_px_width_cap_shrinks_below_ideal() {
		RogerTextRenderer tr("");
		TS_ASSERT(tr.ok());
		// A generous width cap leaves the ideal size untouched...
		TS_ASSERT_EQUALS(tr.fitPx("Introduction", 500, 300, 40, 10000), 40);
		// ...a tiny one forces the size down (but never to zero).
		const int narrow = tr.fitPx("Introduction", 500, 300, 40, 8);
		TS_ASSERT_LESS_THAN(narrow, 40);
		TS_ASSERT_LESS_THAN_EQUALS(1, narrow);
	}

	void test_fit_px_width_capped_still_fits_box_height() {
		// The control-hook copy of a dialog carries a single-line width cap (nTextW)
		// that is huge and never binds, so the width arm leaves the size at ideal.
		// A long paragraph re-wraps (at the box width) to more lines than fit the box
		// height, so the height arm MUST shrink it below ideal — otherwise the text
		// overflows and clips the box bottom (QFG1 room 320 "look" overflow bug).
		RogerTextRenderer tr("");
		TS_ASSERT(tr.ok());
		const char *para =
			"This appears to be a Market street.  In the corner is a fruit and "
			"vegetable stand, or Farmer's Mart.  Next to it is a dry goods store, "
			"and next to that is a house.  Across the street you see the back of "
			"the barber's shop and the Sheriff's office.";
		// Short box (30 px) with a huge single-line width cap: the width arm never
		// binds, but the wrapped block must be shrunk to fit 30 px of height.
		const int fit = tr.fitPx(para, 300, 30, 40, 100000);
		TS_ASSERT_LESS_THAN(fit, 40);   // shrank below ideal (would overflow otherwise)
		TS_ASSERT_LESS_THAN_EQUALS(1, fit);
		// A taller box needs less (or no) shrink: the height arm is monotone in boxH.
		const int fitTall = tr.fitPx(para, 300, 200, 40, 100000);
		TS_ASSERT_LESS_THAN_EQUALS(fit, fitTall);
		// Prove the fitted size does not overflow the box: drawing at the box height
		// leaves no text pixels below the rect (the same clip check drawPx guarantees).
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dst(300, 60, rgba);
		const uint32 black = rgba.ARGBToColor(255, 0, 0, 0);
		const uint32 white = rgba.ARGBToColor(255, 255, 255, 255);
		dst.fillRect(Common::Rect(0, 0, 300, 60), black);
		// targetPx 40 + a huge width cap -> exercises the same fit path as in-game.
		tr.drawPx(dst, para, Common::Rect(0, 0, 300, 30), white, 0, 40,
		          false, nullptr, 100000);
		for (int y = 30; y < 60; y++) {
			for (int x = 0; x < 300; x++) {
				uint8 a, r, g, b;
				dst.surfacePtr()->format.colorToARGB(dst.surfacePtr()->getPixel(x, y), a, r, g, b);
				if (r != 0 || g != 0 || b != 0) {
					TS_FAIL("width-capped long text overflowed the box bottom");
					return;
				}
			}
		}
	}

	void test_fit_px_width_capped_short_text_stays_at_ideal() {
		// A short single-line control that already fits both its width cap and the
		// box height must NOT be shrunk — the height arm is a no-op here (one wrapped
		// line). Pins "no needless shrink" for the short dialogs that render correctly.
		RogerTextRenderer tr("");
		TS_ASSERT(tr.ok());
		TS_ASSERT_EQUALS(tr.fitPx("Look here", 500, 300, 40, 100000), 40);
	}

	void test_draw_px_clips_to_rect_bottom() {
		// Build a renderer using the bitmap fallback (no game files needed).
		RogerTextRenderer tr("");
		TS_ASSERT(tr.ok());

		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dst(200, 100, rgba);
		const uint32 black = rgba.ARGBToColor(255, 0,   0,   0);
		const uint32 white = rgba.ARGBToColor(255, 255, 255, 255);
		dst.fillRect(Common::Rect(0, 0, 200, 100), black);

		// Long text into only the top 30 rows. With word-wrap this produces more
		// lines than fit in 30 px, so without the clip fix some rows below 30 turn white.
		Common::Rect textRect(0, 0, 200, 30);
		tr.drawPx(dst,
		          "word word word word word word word word word word word word word word word",
		          textRect, white, 0 /*left*/, 0 /*targetPx=fill box*/);

		// No pixel in rows 30..99 should be white (the text color).
		for (int y = 30; y < 100; y++) {
			for (int x = 0; x < 200; x++) {
				uint8 a, r, g, b;
				dst.surfacePtr()->format.colorToARGB(dst.surfacePtr()->getPixel(x, y), a, r, g, b);
				if (r != 0 || g != 0 || b != 0) {
					TS_FAIL("drawPx rendered text past rect.bottom");
					return;
				}
			}
		}
	}
};
