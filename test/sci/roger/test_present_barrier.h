/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_coords.h"
#include "common/rect.h"

using namespace Sci;

class PresentBarrierExtentTestSuite : public CxxTest::TestSuite {
public:
	// gameRect chosen so 1 native px = 9x9 overlay px exactly (320*9=2880, 200*9=1800).
	Common::Rect gr() const { return Common::Rect(0, 0, 2880, 1800); }

	void test_paint_extent_covers_shipped_overdraw_math() {
		const Common::Rect nr(10, 10, 20, 20);
		Common::Rect grown = nr;
		grown.grow(2);
		Common::Rect expected = Roger::sciRectToDest(grown, gr());
		expected.grow(2);
		TS_ASSERT_EQUALS(Roger::uiPaintExtent(nr, gr()), expected);
	}

	void test_paint_extent_strictly_contains_vacated_extent() {
		const Common::Rect nr(50, 40, 90, 60);
		const Common::Rect paint = Roger::uiPaintExtent(nr, gr());
		const Common::Rect vac = Roger::uiVacatedExtent(nr, gr());
		TS_ASSERT(paint.contains(vac));
		TS_ASSERT(paint.width() > vac.width());
	}

	void test_vacated_extent_is_exact_plus_ttf_pad() {
		const Common::Rect nr(50, 40, 90, 60);
		Common::Rect expected = Roger::sciRectToDest(nr, gr());
		expected.grow(2);
		TS_ASSERT_EQUALS(Roger::uiVacatedExtent(nr, gr()), expected);
	}
};
