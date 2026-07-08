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
#include "sci/roger/overlay/roger_coords.h"
#include "sci/roger/overlay/roger_compositor.h"
#include "sci/roger/overlay/roger_ui_layer.h"
#include "graphics/managed_surface.h"
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

class RegionExpansionTestSuite : public CxxTest::TestSuite {
	Roger::UiElement mkWindow(int l, int t, int r, int b, uint32 tok) {
		Roger::UiElement e;
		e.type = Roger::kUiWindow; e.nativeRect = Common::Rect(l, t, r, b);
		e.backColor = 15; e.penColor = 0; e.hasFrame = true; e.token = tok;
		return e;
	}
public:
	Common::Rect gr() const { return Common::Rect(0, 0, 2880, 1800); }
	Common::Rect bounds() const { return Common::Rect(0, 0, 2880, 1800); }

	void test_no_intersection_returns_inputs() {
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(200, 100, 250, 130, 0x40000001u)); // far from region
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(0, 0, 90, 90)); // native (10,10) area in overlay px
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 0u);
		TS_ASSERT_EQUALS(regions.size(), 1u);
	}

	void test_intersecting_element_expands_region_to_its_extent() {
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(5, 5, 40, 40, 0x40000001u));
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(80, 80, 100, 100)); // overlaps the window's overlay extent
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 1u);
		const Common::Rect want = Roger::uiPaintExtent(Common::Rect(5, 5, 40, 40), gr());
		bool covered = false;
		for (uint i = 0; i < regions.size(); i++)
			if (regions[i].contains(want)) covered = true;
		TS_ASSERT(covered);
	}

	void test_token_group_closure_pulls_in_far_member() {
		// Window A (token T) intersects the region; text B shares T but sits far away.
		// Closure must include B and grow the regions over B's extent too, so the
		// window-border content-union logic sees the whole group.
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(5, 5, 40, 40, 0x40000007u));
		Roger::UiElement b = mkWindow(200, 150, 240, 170, 0x40000007u);
		b.type = Roger::kUiText; b.text = "hi";
		elems.push_back(b);
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(80, 80, 100, 100));
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 2u);
		const Common::Rect wantB = Roger::uiPaintExtent(Common::Rect(200, 150, 240, 170), gr());
		bool covered = false;
		for (uint i = 0; i < regions.size(); i++)
			if (regions[i].contains(wantB)) covered = true;
		TS_ASSERT(covered);
	}

	void test_chain_expansion_reaches_fixpoint() {
		// A intersects the region; B intersects only A's extent; C intersects only B's.
		// All three must be selected.
		Common::Array<Roger::UiElement> elems;
		elems.push_back(mkWindow(10, 10, 30, 30, 0x40000001u));
		elems.push_back(mkWindow(31, 10, 50, 30, 0x40000002u));
		elems.push_back(mkWindow(51, 10, 70, 30, 0x40000003u));
		Common::Array<Common::Rect> regions;
		regions.push_back(Roger::uiPaintExtent(Common::Rect(10, 10, 12, 12), gr()));
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(regions, elems, gr(), bounds(), idx);
		TS_ASSERT_EQUALS(idx.size(), 3u);
	}
};

class PatchCompositeTestSuite : public CxxTest::TestSuite {
public:
	// patch == full recompose inside the expanded regions; untouched outside.
	void test_patch_matches_full_recompose_inside_and_preserves_outside() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		const int W = 320, H = 200; // small overlay for the test
		Graphics::ManagedSurface sceneNoUi(W, H, rgba);
		sceneNoUi.clear(rgba.ARGBToColor(255, 0, 0, 80)); // dark blue "scene"
		byte pal[256 * 3];
		for (int i = 0; i < 256; i++) { pal[i * 3] = (byte)i; pal[i * 3 + 1] = (byte)i; pal[i * 3 + 2] = (byte)i; }
		const Common::Rect gameRect(0, 0, W, H); // 1:1 native->overlay for simplicity

		Common::Array<Roger::UiElement> elems;
		Roger::UiElement w;
		w.type = Roger::kUiWindow; w.nativeRect = Common::Rect(40, 40, 120, 90);
		w.backColor = 15; w.penColor = 0; w.hasFrame = true; w.token = 0x40000001u;
		elems.push_back(w);

		Roger::RogerCompositor comp;

		// Reference: full recompose.
		Graphics::ManagedSurface full(W, H, rgba);
		full.copyFrom(sceneNoUi);
		comp.renderUiLayer(full, elems, pal, gameRect, nullptr, nullptr);

		// Composite under test: stale sentinel everywhere, then patch one region
		// overlapping the window.
		Graphics::ManagedSurface patched(W, H, rgba);
		patched.clear(rgba.ARGBToColor(255, 255, 0, 0)); // red sentinel = "stale"
		Common::Array<Common::Rect> regions;
		regions.push_back(Common::Rect(30, 30, 70, 70));
		comp.patchCompositeRegions(patched, sceneNoUi, elems, regions, pal, gameRect, nullptr, nullptr);

		// Expanded regions cover the window's full extent; inside them the patch
		// must equal the full recompose.
		Common::Array<Common::Rect> expanded;
		expanded.push_back(Common::Rect(30, 30, 70, 70));
		Common::Array<uint> idx;
		Roger::expandRegionsToElements(expanded, elems, gameRect, Common::Rect(0, 0, W, H), idx);
		for (uint r = 0; r < expanded.size(); r++)
			for (int y = expanded[r].top; y < expanded[r].bottom; y++)
				for (int x = expanded[r].left; x < expanded[r].right; x++)
					TS_ASSERT_EQUALS(patched.surfacePtr()->getPixel(x, y), full.surfacePtr()->getPixel(x, y));

		// A pixel far outside every expanded region keeps the sentinel.
		TS_ASSERT_EQUALS(patched.surfacePtr()->getPixel(300, 190), rgba.ARGBToColor(255, 255, 0, 0));
	}
};
