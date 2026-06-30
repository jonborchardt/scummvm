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
#include "common/array.h"
#include "common/rect.h"
#include "engines/sci/roger/roger_compositor.h"

using namespace Sci;
using namespace Sci::Roger;

class CharScreenFidelityTestSuite : public CxxTest::TestSuite {
public:
	void test_coverage_fraction_full_and_partial() {
		// inner fully inside outer -> 100
		TS_ASSERT_EQUALS(Roger::rectCoverageFraction(Common::Rect(10,10,20,20), Common::Rect(0,0,100,100)), 100);
		// no overlap -> 0
		TS_ASSERT_EQUALS(Roger::rectCoverageFraction(Common::Rect(10,10,20,20), Common::Rect(50,50,60,60)), 0);
		// half covered (left half of a 10x10 inner) -> 50
		TS_ASSERT_EQUALS(Roger::rectCoverageFraction(Common::Rect(0,0,10,10), Common::Rect(0,0,5,10)), 50);
	}
	void test_covered_filter_keeps_lightly_overlapped_graphic() {
		// A graphic region only clipped at its edge by a wide text rect must be KEPT.
		Common::Array<Common::Rect> captured, exclude, out;
		captured.push_back(Common::Rect(26, 42, 63, 102));   // portrait graphic
		exclude.push_back(Common::Rect(20, 95, 315, 151));   // wide multi-line text rect, clips only the bottom edge
		Roger::filterForegroundCaptureRegionsCovered(captured, exclude, 80, out);
		TS_ASSERT_EQUALS(out.size(), 1u);                    // kept (only ~12% covered)
	}
	void test_covered_filter_drops_text_region() {
		// A region a text rect substantially covers must be DROPPED (no blocky-under-crisp).
		Common::Array<Common::Rect> captured, exclude, out;
		captured.push_back(Common::Rect(170, 45, 192, 57));  // a stat-value cell
		exclude.push_back(Common::Rect(168, 44, 194, 58));   // generic text rect covering it
		Roger::filterForegroundCaptureRegionsCovered(captured, exclude, 80, out);
		TS_ASSERT_EQUALS(out.size(), 0u);                    // dropped
	}
};
