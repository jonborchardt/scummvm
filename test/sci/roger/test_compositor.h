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
#include "sci/roger/png_loader.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "common/array.h"
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

	void test_plate_then_sprite_with_priority_occlusion() {
		// 8x8 gray plate (64,64,64). One green sprite of priority 1 covering the whole
		// picture, supplied via celOverride (the cel source after spritesheet loading
		// was removed — cels now come from the generator or a pre-rendered override).
		// An 8x8 priority map: left half band 0 (<= sprite -> sprite shows), right half
		// band 15 (> sprite -> occluded, the plate's pixels are restored). picture ==
		// 8x8, no menu offset.
		Graphics::Surface *plate = Sci::Roger::loadSurfaceRGBA(
			Common::String(FIXTURE_DIR) + "/plate_8x8.png");
		TS_ASSERT(plate != nullptr);

		Graphics::Surface cel; // green sprite content, supplied as celOverride
		cel.create(8, 8, plate->format);
		cel.fillRect(Common::Rect(0, 0, 8, 8), plate->format.ARGBToColor(255, 0, 255, 0));

		// Priority map: byte per pixel; cols 0..3 = band 0, cols 4..7 = band 15.
		Common::Array<byte> prio;
		prio.resize(8 * 8);
		for (int y = 0; y < 8; y++)
			for (int x = 0; x < 8; x++)
				prio[y * 8 + x] = (x < 4) ? 0 : 15;

		Sci::Roger::RogerCompositor comp;
		comp.setRoom(plate, nullptr);            // no ViewCache -> uses celOverride
		comp.setPicture(8, 8, 0);
		comp.setPriorityMask(prio.begin(), 8, 8);

		Sci::Roger::Sprite spr;
		spr.viewId = 900; spr.loopNo = 0; spr.celNo = 1;
		spr.priority = 1; spr.mirror = false;
		spr.celRect = Common::Rect(0, 0, 8, 8);  // picture-space -> full 8x8 dest
		spr.celOverride = &cel;

		Common::Array<Sci::Roger::Sprite> list;
		list.push_back(spr);

		Graphics::ManagedSurface dest(8, 8, plate->format);
		comp.renderScene(dest, list);

		uint8 a, r, g, b;
		// Left half (x<4): priority 0 <= sprite priority 1 -> sprite (green) shows.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(1, 4), a, r, g, b);
		TS_ASSERT_EQUALS(g, 255);
		TS_ASSERT_EQUALS(r, 0);
		// Right half (x>=4): priority 15 > 1 -> occluded, plate gray (64) restored.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(6, 1), a, r, g, b);
		TS_ASSERT_EQUALS(r, 64); TS_ASSERT_EQUALS(g, 64); TS_ASSERT_EQUALS(b, 64);

		cel.free();
		plate->free(); delete plate;
	}

	void test_splat_matches_background_scaler_under_scaling() {
		// REGRESSION for the "splatted pixels are off by a few px" bug. When the plate
		// is scaled into the game rect (plate wider than picRect), the occlusion
		// punch-back must sample the plate with the SAME integer scaler that blitFrom
		// used to draw the background — otherwise the restored foreground pixels drift
		// from the background. Here: a 10x2 gradient plate scaled into a 6x2 rect, a
		// sprite covering it all, priority everywhere > sprite -> splat everywhere.
		// The result must be pixel-identical to blitFrom(plate -> 6x2).
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);

		Graphics::Surface plate;
		plate.create(10, 2, rgba);
		for (int y = 0; y < 2; y++)
			for (int x = 0; x < 10; x++)
				plate.setPixel(x, y, rgba.ARGBToColor(255, (byte)(x * 25), 0, 0)); // horiz gradient

		Graphics::Surface cel;   // sprite content is irrelevant (fully splatted)
		cel.create(6, 2, rgba);
		cel.fillRect(Common::Rect(0, 0, 6, 2), rgba.ARGBToColor(255, 0, 255, 0));

		Common::Array<byte> prio;
		prio.resize(6 * 2);
		for (uint i = 0; i < prio.size(); i++)
			prio[i] = 15;  // everywhere foreground -> always occlude

		Sci::Roger::Sprite spr;
		spr.viewId = -1; spr.loopNo = 0; spr.celNo = 0;
		spr.priority = 0; spr.mirror = false;
		spr.celRect = Common::Rect(0, 0, 6, 2);
		spr.celOverride = &cel;
		Common::Array<Sci::Roger::Sprite> list;
		list.push_back(spr);

		Sci::Roger::RogerCompositor comp;
		comp.setRoom(&plate, nullptr);       // no ViewCache -> uses celOverride
		comp.setPicture(6, 2, 0);
		comp.setPriorityMask(prio.begin(), 6, 2);

		Graphics::ManagedSurface dest(6, 2, rgba);
		comp.renderScene(dest, list);

		// Reference: the background plate as ScummVM's own scaler draws it.
		Graphics::ManagedSurface ref(6, 2, rgba);
		ref.blitFrom(plate, Common::Rect(0, 0, 10, 2), Common::Rect(0, 0, 6, 2));

		for (int y = 0; y < 2; y++)
			for (int x = 0; x < 6; x++)
				TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(x, y), ref.surfacePtr()->getPixel(x, y));

		plate.free();
		cel.free();
	}

	void test_sprite_history_survives_ui_only_present() {
		// Regression: a UI-only present (presentWithUi — cursor move / dialog, with NO
		// renderScene) must not discard the previous sprite position. If sprite rects were
		// rolled at present granularity, a present that lacks them (UI-only) clobbers the
		// history, so when the sprite next MOVES its old position is never repainted ->
		// "shadow of old animation frames." Sprite rects are tracked at renderScene
		// granularity (rolled in renderScene) and unioned by presentToOverlay, so the
		// vacated spot is always pushed. We drive the g_system-free seams (renderScene fills
		// the scene set; rollPresentDirty simulates a present's UI-granularity roll;
		// dirtyUnion is what the dirty present pushes) so no overlay backend is needed.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);

		Graphics::Surface plate;
		plate.create(64, 64, rgba);
		plate.fillRect(Common::Rect(0, 0, 64, 64), rgba.ARGBToColor(255, 64, 64, 64));

		Graphics::Surface cel; // green sprite content via celOverride (no ViewCache)
		cel.create(8, 8, rgba);
		cel.fillRect(Common::Rect(0, 0, 8, 8), rgba.ARGBToColor(255, 0, 255, 0));

		Sci::Roger::RogerCompositor comp;
		comp.setRoom(&plate, nullptr);
		comp.setPicture(64, 64, 0);   // PIC == picRect == surface, so dst == celRect
		comp.setDirtyPresent(true);

		const Common::Rect gameRect(0, 0, 64, 64);
		const Common::Rect bounds(0, 0, 64, 64);
		Graphics::ManagedSurface dest(64, 64, rgba);

		Sci::Roger::Sprite spr;
		spr.viewId = 900; spr.loopNo = 0; spr.celNo = 1;
		spr.priority = 1; spr.mirror = false;
		spr.celOverride = &cel;
		Common::Array<Sci::Roger::Sprite> list;

		// Frame 1: sprite at P=(4,4,12,12). A present follows (its UI-granularity roll).
		spr.celRect = Common::Rect(4, 4, 12, 12);
		list.clear(); list.push_back(spr);
		comp.renderScene(dest, list, gameRect);
		comp.rollPresentDirty();

		// Frame 2: a UI-only present (no renderScene) — e.g. presentWithUi on a mouse move.
		// Only a cursor rect is added; the sprite is static (still at P). The present rolls.
		comp.addDirtyRect(Common::Rect(50, 4, 58, 12)); // "cursor"
		comp.rollPresentDirty();

		// Frame 3: the animation advances — the sprite moves to Q=(40,40,52,52).
		spr.celRect = Common::Rect(40, 40, 52, 52);
		list.clear(); list.push_back(spr);
		comp.renderScene(dest, list, gameRect);

		// What the dirty present would push this frame. The vacated OLD position P must be
		// inside some region so its background repaints (no shadow). Center of P is (8,8);
		// Q=(40,40,52,52) and the cursor=(50,4,58,12) do not contain it.
		Common::Array<Common::Rect> push;
		comp.dirtyUnion(bounds, push);
		bool pCovered = false;
		for (uint i = 0; i < push.size(); i++)
			if (push[i].contains(8, 8))
				pCovered = true;
		TS_ASSERT(pCovered);

		cel.free();
		plate.free();
	}

	void test_coalesce_clamps_drops_and_merges() {
		Common::Array<Common::Rect> in, out;
		const Common::Rect bounds(0, 0, 100, 100);
		// (a) out-of-bounds rect is clipped to bounds
		in.push_back(Common::Rect(-10, -10, 20, 20));
		// (b) fully-outside rect is dropped
		in.push_back(Common::Rect(200, 200, 300, 300));
		// (c) two overlapping rects merge into their union
		in.push_back(Common::Rect(50, 50, 70, 70));
		in.push_back(Common::Rect(60, 60, 80, 80));
		Sci::Roger::coalesceDirtyRects(in, bounds, out);
		// Expect: clipped (0,0,20,20) and merged (50,50,80,80) — the outside one gone.
		TS_ASSERT_EQUALS(out.size(), (uint)2);
		bool hasClip = false, hasMerge = false;
		for (uint i = 0; i < out.size(); i++) {
			if (out[i] == Common::Rect(0, 0, 20, 20)) hasClip = true;
			if (out[i] == Common::Rect(50, 50, 80, 80)) hasMerge = true;
			// every output rect is within bounds
			TS_ASSERT(out[i].left >= 0 && out[i].top >= 0 && out[i].right <= 100 && out[i].bottom <= 100);
		}
		TS_ASSERT(hasClip);
		TS_ASSERT(hasMerge);
	}

	void test_cursor_only_dirty_covers_old_and_new_rects() {
		// Simulates the cursor-only fast path: two addDirtyRect calls (old + new cursor pos).
		// dirtyUnion must cover both rects and must NOT blow up to a full-surface rect.
		Sci::Roger::RogerCompositor comp;
		comp.setDirtyPresent(true);

		const Common::Rect oldCursor(100, 200, 180, 280); // 80x80 cursor at (100,200)
		const Common::Rect newCursor(120, 210, 200, 290); // moved 20px right, 10px down
		const Common::Rect bounds(0, 0, 2862, 1986);

		comp.addDirtyRect(oldCursor);
		comp.addDirtyRect(newCursor);

		Common::Array<Common::Rect> result;
		comp.dirtyUnion(bounds, result);

		TS_ASSERT(!result.empty());
		bool coversOld = false, coversNew = false;
		for (uint i = 0; i < result.size(); i++) {
			if (result[i].contains(oldCursor)) coversOld = true;
			if (result[i].contains(newCursor)) coversNew = true;
		}
		TS_ASSERT(coversOld);
		TS_ASSERT(coversNew);
		// Sanity: the dirty area must be much smaller than the full surface.
		int totalArea = 0;
		for (uint i = 0; i < result.size(); i++)
			totalArea += result[i].width() * result[i].height();
		TS_ASSERT_LESS_THAN(totalArea, bounds.width() * bounds.height() / 10);
	}
};
