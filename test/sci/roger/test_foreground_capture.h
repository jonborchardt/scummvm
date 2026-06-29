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
#include "common/rect.h"
#include "common/array.h"
#include "engines/sci/roger/roger_compositor.h"

class ForegroundCaptureTestSuite : public CxxTest::TestSuite {
public:
	void test_keeps_non_overlapping_and_drops_overlapping() {
		Common::Array<Common::Rect> captured;
		captured.push_back(Common::Rect(204, 115, 294, 127)); // a stat label (no cast nearby)
		captured.push_back(Common::Rect(10, 20, 40, 60));     // overlaps the live ego below

		Common::Array<Common::Rect> live;
		live.push_back(Common::Rect(8, 18, 50, 70));          // live ego cel rect

		Common::Array<Common::Rect> out;
		Roger::filterForegroundCaptureRegions(captured, live, out);

		TS_ASSERT_EQUALS(out.size(), 1u);
		TS_ASSERT(out[0] == Common::Rect(204, 115, 294, 127));
	}

	void test_no_live_cast_keeps_all() {
		Common::Array<Common::Rect> captured;
		captured.push_back(Common::Rect(0, 0, 10, 10));
		captured.push_back(Common::Rect(20, 20, 30, 30));
		Common::Array<Common::Rect> live;   // empty: static menu screen
		Common::Array<Common::Rect> out;
		Roger::filterForegroundCaptureRegions(captured, live, out);
		TS_ASSERT_EQUALS(out.size(), 2u);
	}

	void test_empty_captured_yields_empty() {
		Common::Array<Common::Rect> captured, live, out;
		live.push_back(Common::Rect(0, 0, 5, 5));
		Roger::filterForegroundCaptureRegions(captured, live, out);
		TS_ASSERT_EQUALS(out.size(), 0u);
	}
};
