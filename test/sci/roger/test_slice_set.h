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
#include "sci/roger/slice_set.h"

// EGA priority-band helpers (the former SliceSet class was removed; see slice_set.h).
class TestSliceSet : public CxxTest::TestSuite {
public:
	void test_band_for_rgb_nearest_match() {
		// Near-white maps to band 15; near-black to band 0; pure-ish brown to band 6.
		TS_ASSERT_EQUALS(Sci::Roger::bandForRGB(250, 250, 250), 15);
		TS_ASSERT_EQUALS(Sci::Roger::bandForRGB(5, 5, 5), 0);
		TS_ASSERT_EQUALS(Sci::Roger::bandForRGB(0xaa, 0x55, 0x00), 6); // brown
	}
};
