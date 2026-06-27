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

	void test_draw_px_clips_to_rect_bottom() {
		// Build a renderer using the bitmap fallback (no game files needed).
		Common::Array<int> sizes;
		RogerTextRenderer tr("", sizes);
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
