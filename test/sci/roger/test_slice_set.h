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
#include "../../system/null_osystem.h"

// FIXTURE_DIR must be defined on the compiler command line, e.g.:
//   -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"
// The Makefile (and build_tests.ps1 for MSVC) sets this automatically.

class TestSliceSet : public CxxTest::TestSuite {
public:
	// SliceSet::load() reads files via Common::FSNode, which dereferences
	// the global OSystem (g_system). Install the null backend for each test.
	void setUp() {
		Common::install_null_g_system();
	}

	void tearDown() {
		Common::uninstall_null_g_system();
	}

	void test_band_for_white_is_15() {
		TS_ASSERT_EQUALS(Sci::Roger::bandForColor("#ffffff"), 15);
	}

	void test_band_for_black_is_0() {
		TS_ASSERT_EQUALS(Sci::Roger::bandForColor("#000000"), 0);
	}

	void test_manifest_loads_one_piece() {
		// fixtures/slice_manifest.json references color_ffffff.png at (4,0)
		Sci::Roger::SliceSet set(Common::String(FIXTURE_DIR), "slice_manifest.json");
		TS_ASSERT(set.load());
		TS_ASSERT_EQUALS(set.pieces().size(), 1u);
		TS_ASSERT_EQUALS(set.pieces()[0].x, 4);
		TS_ASSERT_EQUALS(set.pieces()[0].band, 15);  // white
		TS_ASSERT(set.pieces()[0].surface != nullptr);
	}
};
