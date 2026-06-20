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
#include "sci/roger/roger_compositor.h"
#include "sci/roger/view_cache.h"
#include "sci/roger/slice_set.h"
#include "sci/roger/png_loader.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "../../system/null_osystem.h"

// FIXTURE_DIR must be defined on the compiler command line, e.g.:
//   -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"
// The Makefile (and build_tests.ps1 for MSVC) sets this automatically.

class TestCompositor : public CxxTest::TestSuite {
public:
	// RogerCompositor reads files via ViewCache/SliceSet which dereference
	// the global OSystem (g_system). Install the null backend for each test.
	void setUp() {
		Common::install_null_g_system();
	}

	void tearDown() {
		Common::uninstall_null_g_system();
	}

	void test_plate_then_sprite_then_slice_occludes() {
		// 8x8 gray plate; slice = white 4x4 at (4,0) band 15.
		Graphics::Surface *plate = Sci::Roger::loadSurfaceRGBA(
			Common::String(FIXTURE_DIR) + "/plate_8x8.png");
		TS_ASSERT(plate != nullptr);
		Sci::Roger::SliceSet slices(Common::String(FIXTURE_DIR), "slice_manifest.json");
		TS_ASSERT(slices.load());
		Sci::Roger::ViewCache views(Common::String(FIXTURE_DIR));

		Sci::Roger::RogerCompositor comp;
		comp.setRoom(plate, &slices, &views);

		// One sprite: view 900 loop 0 cel 1 (green), priority band 1,
		// celRect chosen so 6x mapping lands across x 0..8 in an 8x8 dest.
		// Use dest 8x8 so overlay scale = 8/320; place celRect to cover (0..8,0..8).
		Sci::Roger::Sprite spr;
		spr.viewId = 900; spr.loopNo = 0; spr.celNo = 1;
		spr.priority = 1; spr.mirror = false;
		spr.celRect = Common::Rect(0, 0, 320, 200);  // -> full 8x8 dest

		Common::Array<Sci::Roger::Sprite> list;
		list.push_back(spr);

		Graphics::ManagedSurface dest(8, 8, plate->format);
		comp.renderScene(dest, list);

		// Left half (x<4): sprite (green) shows over plate.
		uint8 a, r, g, b;
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(1, 4), a, r, g, b);
		TS_ASSERT_EQUALS(g, 255);
		// Right half (x>=4): band-15 white slice occludes the band-1 sprite.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(6, 1), a, r, g, b);
		TS_ASSERT_EQUALS(r, 255); TS_ASSERT_EQUALS(g, 255); TS_ASSERT_EQUALS(b, 255);

		plate->free(); delete plate;
	}
};
