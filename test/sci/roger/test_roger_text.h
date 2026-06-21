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
};
