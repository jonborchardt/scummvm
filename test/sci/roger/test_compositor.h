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
#include "sci/roger/overlay/roger_compositor.h"
#include "sci/roger/overlay/view_cache.h"
#include "sci/roger/png_loader.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "common/array.h"
#include "../../system/null_osystem.h"

// FIXTURE_DIR must be defined on the compiler command line, e.g.:
//   -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"
// The Makefile (and build_tests.ps1 for MSVC) sets this automatically.

class TestCompositor : public CxxTest::TestSuite {
	static int iabs(int v) { return v < 0 ? -v : v; }
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
		// was removed â€” cels now come from the generator or a pre-rendered override).
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

	void test_plate_and_cel_content_align_under_scaling() {
		// ROOT-CAUSE REGRESSION for the "6x view doesn't line up with the omyac plate"
		// bug (SQ3 pic 1 pod door showing cyan). ManagedSurface::blitFrom scales with a
		// TRUNCATED 8.8 fixed-point step (scaleX = 256*srcW/dstW), so plate content
		// drifted right/down by up to ~12 overlay px across the screen, while sprite
		// dest rects use exact rational math. The compositor must draw the plate with
		// an EXACT nearest scaler so a cel and the plate content for the same native
		// rect land at the same overlay position.
		//
		// Plate: black 1920x1140 (6x of 320x190) with a white 1-native-px vertical line
		// at native x=300 and a white horizontal line at native y=180 (far right/bottom,
		// where the truncation drift is largest). Render A: plate alone. Render B: black
		// plate + white line CELS at the same native rects. The white bands of A and B
		// must coincide within 1 px (pre-fix, A's vertical line sat ~11 px right of B's).
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		const int SRCW = 1920, SRCH = 1140; // 6x of 320x190
		const int DW = 2862, DH = 1699;     // realistic overlay picture rect
		const uint32 white = rgba.ARGBToColor(255, 255, 255, 255);
		const uint32 black = rgba.ARGBToColor(255, 0, 0, 0);
		const int LX = 300, LY = 180;       // native line positions

		Graphics::Surface plate;
		plate.create(SRCW, SRCH, rgba);
		plate.fillRect(Common::Rect(0, 0, SRCW, SRCH), black);
		plate.fillRect(Common::Rect(LX * 6, 0, LX * 6 + 6, SRCH), white);
		plate.fillRect(Common::Rect(0, LY * 6, SRCW, LY * 6 + 6), white);

		Graphics::Surface blackPlate;
		blackPlate.create(SRCW, SRCH, rgba);
		blackPlate.fillRect(Common::Rect(0, 0, SRCW, SRCH), black);

		// Render A: the patterned plate, no sprites.
		Sci::Roger::RogerCompositor compA;
		compA.setRoom(&plate, nullptr);
		compA.setPicture(320, 190, 0);
		Graphics::ManagedSurface destA(DW, DH, rgba);
		Common::Array<Sci::Roger::Sprite> none;
		compA.renderScene(destA, none);

		// Render B: black plate + the same lines as sprite cels (exact 6x content).
		Graphics::Surface vCel, hCel;
		vCel.create(6, SRCH, rgba);
		vCel.fillRect(Common::Rect(0, 0, 6, SRCH), white);
		hCel.create(SRCW, 6, rgba);
		hCel.fillRect(Common::Rect(0, 0, SRCW, 6), white);

		Sci::Roger::Sprite v, hs;
		v.viewId = -1; v.loopNo = 0; v.celNo = 0; v.priority = 1; v.mirror = false;
		v.celRect = Common::Rect(LX, 0, LX + 1, 190);
		v.celOverride = &vCel;
		hs = v;
		hs.celRect = Common::Rect(0, LY, 320, LY + 1);
		hs.celOverride = &hCel;
		Common::Array<Sci::Roger::Sprite> list;
		list.push_back(v);
		list.push_back(hs);

		Sci::Roger::RogerCompositor compB;
		compB.setRoom(&blackPlate, nullptr);
		compB.setPicture(320, 190, 0);
		Graphics::ManagedSurface destB(DW, DH, rgba);
		compB.renderScene(destB, list);

		// White vertical-band extents on a probe row away from the horizontal line.
		int aL = -1, aR = -1, bL = -1, bR = -1;
		const int probeY = DH / 4;
		for (int x = 0; x < DW; x++) {
			if (destA.surfacePtr()->getPixel(x, probeY) == white) {
				if (aL < 0) aL = x;
				aR = x;
			}
			if (destB.surfacePtr()->getPixel(x, probeY) == white) {
				if (bL < 0) bL = x;
				bR = x;
			}
		}
		TS_ASSERT(aL >= 0); TS_ASSERT(bL >= 0);
		TS_ASSERT_LESS_THAN_EQUALS(iabs(aL - bL), 1);
		TS_ASSERT_LESS_THAN_EQUALS(iabs(aR - bR), 1);

		// White horizontal-band extents on a probe column away from the vertical line.
		int aT = -1, aB = -1, bT = -1, bB = -1;
		const int probeX = DW / 4;
		for (int y = 0; y < DH; y++) {
			if (destA.surfacePtr()->getPixel(probeX, y) == white) {
				if (aT < 0) aT = y;
				aB = y;
			}
			if (destB.surfacePtr()->getPixel(probeX, y) == white) {
				if (bT < 0) bT = y;
				bB = y;
			}
		}
		TS_ASSERT(aT >= 0); TS_ASSERT(bT >= 0);
		TS_ASSERT_LESS_THAN_EQUALS(iabs(aT - bT), 1);
		TS_ASSERT_LESS_THAN_EQUALS(iabs(aB - bB), 1);

		vCel.free();
		hCel.free();
		blackPlate.free();
		plate.free();
	}

	void test_cover_grow_sprite_drawn_larger() {
		// Game view cels (coverGrow) are drawn kCelCoverPx (3) larger on every
		// side â€” capped at 1/8 of the dest size so tiny cels barely grow â€” so the
		// cel's opaque content covers the plate's smoothed boundary fringe (the
		// SQ3 pod-door cyan-seam class). coverGrow=false sprites (Feeder B
		// stamps, exact-geometry tests) stay geometrically exact.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		const uint32 green = rgba.ARGBToColor(255, 0, 255, 0);

		Graphics::Surface plate;
		plate.create(64, 64, rgba);
		plate.fillRect(Common::Rect(0, 0, 64, 64), rgba.ARGBToColor(255, 64, 64, 64));
		Graphics::Surface cel;
		cel.create(32, 32, rgba);
		cel.fillRect(Common::Rect(0, 0, 32, 32), green);

		Sci::Roger::Sprite spr;
		spr.viewId = -1; spr.loopNo = 0; spr.celNo = 0;
		spr.priority = 1; spr.mirror = false;
		spr.celRect = Common::Rect(16, 16, 48, 48); // PIC == surface -> dst == celRect
		spr.celOverride = &cel;
		Common::Array<Sci::Roger::Sprite> list;

		const Common::Rect gameRect(0, 0, 64, 64);

		// coverGrow off: exact rect, nothing outside it.
		Sci::Roger::RogerCompositor compA;
		compA.setRoom(&plate, nullptr);
		compA.setPicture(64, 64, 0);
		Graphics::ManagedSurface destA(64, 64, rgba);
		list.push_back(spr);
		compA.renderScene(destA, list, gameRect);
		TS_ASSERT_EQUALS(destA.surfacePtr()->getPixel(16, 32), green);
		TS_ASSERT_DIFFERS(destA.surfacePtr()->getPixel(15, 32), green);

		// coverGrow cel (32 px dest -> cap 32/8=4 >= 3): grown by 3 px each side.
		Sci::Roger::RogerCompositor compB;
		compB.setRoom(&plate, nullptr);
		compB.setPicture(64, 64, 0);
		Graphics::ManagedSurface destB(64, 64, rgba);
		spr.coverGrow = true;
		list.clear(); list.push_back(spr);
		compB.renderScene(destB, list, gameRect);
		TS_ASSERT_EQUALS(destB.surfacePtr()->getPixel(13, 32), green);  // grown left edge
		TS_ASSERT_EQUALS(destB.surfacePtr()->getPixel(50, 32), green);  // grown right edge
		TS_ASSERT_DIFFERS(destB.surfacePtr()->getPixel(11, 32), green); // but only by 3 px

		// Tiny cel (8 px dest -> cap 1): grows 1 px, not 3.
		Graphics::Surface tiny;
		tiny.create(8, 8, rgba);
		tiny.fillRect(Common::Rect(0, 0, 8, 8), green);
		Sci::Roger::Sprite tspr = spr;
		tspr.celRect = Common::Rect(28, 28, 36, 36);
		tspr.celOverride = &tiny;
		Sci::Roger::RogerCompositor compC;
		compC.setRoom(&plate, nullptr);
		compC.setPicture(64, 64, 0);
		Graphics::ManagedSurface destC(64, 64, rgba);
		list.clear(); list.push_back(tspr);
		compC.renderScene(destC, list, gameRect);
		TS_ASSERT_EQUALS(destC.surfacePtr()->getPixel(27, 32), green);  // 1 px growth
		TS_ASSERT_DIFFERS(destC.surfacePtr()->getPixel(25, 32), green); // not 3

		tiny.free();
		cel.free();
		plate.free();
	}

	void test_sprite_leaving_screen_is_cropped_not_squished() {
		// REGRESSION for the "views squish as they exit the screen" bug: the
		// coverGrow block clipped the sprite's DEST rect to picRect, so a sprite
		// partially off-screen had its whole off-screen extent amputated from the
		// rect while the full cel was still scaled into what remained â€” visible
		// compression at every screen edge. The dest rect must keep its off-screen
		// extent; the BLIT is what clips (crop), sampling the source against the
		// full rect, matching native SCI's port clipping.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		const uint32 red  = rgba.ARGBToColor(255, 255, 0, 0);
		const uint32 blue = rgba.ARGBToColor(255, 0, 0, 255);
		const uint32 gray = rgba.ARGBToColor(255, 64, 64, 64);

		Graphics::Surface plate;
		plate.create(64, 64, rgba);
		plate.fillRect(Common::Rect(0, 0, 64, 64), gray);
		// Cel: left half red, right half blue.
		Graphics::Surface cel;
		cel.create(32, 32, rgba);
		cel.fillRect(Common::Rect(0, 0, 16, 32), red);
		cel.fillRect(Common::Rect(16, 0, 32, 32), blue);

		// Half off the left edge: celRect (-16,16)-(16,48). PIC == surface -> 1:1.
		// Only the cel's RIGHT (blue) half is on-screen; red must never appear.
		Sci::Roger::Sprite spr;
		spr.viewId = -1; spr.loopNo = 0; spr.celNo = 0;
		spr.priority = 1; spr.mirror = false;
		spr.celRect = Common::Rect(-16, 16, 16, 48);
		spr.celOverride = &cel;
		spr.coverGrow = true; // the game-cel path (grow fed the clipping bug)

		Sci::Roger::RogerCompositor comp;
		comp.setRoom(&plate, nullptr);
		comp.setPicture(64, 64, 0);
		Graphics::ManagedSurface dest(64, 64, rgba);
		Common::Array<Sci::Roger::Sprite> list;
		list.push_back(spr);
		comp.renderScene(dest, list, Common::Rect(0, 0, 64, 64));

		// Crop: the screen-left column shows the cel's blue half (squish showed red).
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(0, 32), blue);
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(9, 32), blue);
		// No red anywhere on the sprite's row.
		for (int x = 0; x < 64; x++)
			TS_ASSERT_DIFFERS(dest.surfacePtr()->getPixel(x, 32), red);
		// Past the (grown) dest rect the plate shows through.
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(24, 32), gray);

		cel.free();
		plate.free();
	}

	void test_blend_blit_1to1_off_right_edge_paints_onscreen_part() {
		// REGRESSION for the "cursor vanishes at the right screen edge" bug: the
		// composited cursor used ManagedSurface::blendBlitFrom, whose right/bottom
		// clip computes the source crop against the SOURCE size instead of the dest
		// surface — a dst rect hanging off the right edge emptied the src rect and
		// the whole cursor silently vanished. Cursor draws go through
		// blendScaleBlitNearest (1:1 when dst == src size): the on-screen columns
		// must be painted, the overhang cropped, never squished.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		const uint32 red  = rgba.ARGBToColor(255, 255, 0, 0);
		const uint32 blue = rgba.ARGBToColor(255, 0, 0, 255);
		const uint32 gray = rgba.ARGBToColor(255, 64, 64, 64);

		// Cursor-shaped source: left half red, right half blue, 32x32.
		Graphics::Surface cur;
		cur.create(32, 32, rgba);
		cur.fillRect(Common::Rect(0, 0, 16, 32), red);
		cur.fillRect(Common::Rect(16, 0, 32, 32), blue);

		Graphics::ManagedSurface dest(64, 64, rgba);
		dest.fillRect(Common::Rect(0, 0, 64, 64), gray);

		// 1:1 dst, half off the right edge: (48,16)-(80,48). Only the source's
		// LEFT (red) half is on-screen.
		Sci::Roger::blendScaleBlitNearest(dest, cur, Common::Rect(48, 16, 80, 48), false);

		// The on-screen columns show the cursor's left half up to the last pixel.
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(48, 32), red);
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(63, 32), red); // very edge painted
		// No squish: blue (the off-screen half) never appears.
		for (int x = 0; x < 64; x++)
			TS_ASSERT_DIFFERS(dest.surfacePtr()->getPixel(x, 32), blue);
		// Outside the dst rect the background is untouched.
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(47, 32), gray);

		// Bottom edge, same rule: (16,48)-(48,80) — rows 48..63 painted.
		Sci::Roger::blendScaleBlitNearest(dest, cur, Common::Rect(16, 48, 48, 80), false);
		TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(16, 63), red); // bottom row painted

		cur.free();
	}

	void test_splat_matches_background_scaler_under_scaling() {
		// REGRESSION for the "splatted pixels are off by a few px" bug. When the plate
		// is scaled into the game rect (plate wider than picRect), the occlusion
		// punch-back must sample the plate with the SAME scaler the compositor used to
		// draw the background â€” otherwise the restored foreground pixels drift from
		// the background. Here: a 10x2 gradient plate scaled into a 6x2 rect, a
		// sprite covering it all, priority everywhere > sprite -> splat everywhere.
		// The result must be pixel-identical to the exact nearest scale (plate -> 6x2).
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

		// Reference: the background plate as the compositor's exact scaler draws it.
		Graphics::ManagedSurface ref(6, 2, rgba);
		Sci::Roger::scaleBlitNearest(*ref.surfacePtr(), Common::Rect(0, 0, 6, 2), plate);

		for (int y = 0; y < 2; y++)
			for (int x = 0; x < 6; x++)
				TS_ASSERT_EQUALS(dest.surfacePtr()->getPixel(x, y), ref.surfacePtr()->getPixel(x, y));

		plate.free();
		cel.free();
	}

	void test_sprite_history_survives_ui_only_present() {
		// Regression: a UI-only present (presentWithUi â€” cursor move / dialog, with NO
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

		// Frame 2: a UI-only present (no renderScene) â€” e.g. presentWithUi on a mouse move.
		// Only a cursor rect is added; the sprite is static (still at P). The present rolls.
		comp.addDirtyRect(Common::Rect(50, 4, 58, 12)); // "cursor"
		comp.rollPresentDirty();

		// Frame 3: the animation advances â€” the sprite moves to Q=(40,40,52,52).
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

	void test_reset_for_room_change_forces_full_first_frame() {
		// REGRESSION for the QFG1 fresh-start town breakage. A room entered via a real
		// transition pre-warms the static-bg cache (claimTransition -> composeRoomScene), so
		// the first post-transition renderFrame would otherwise take the BOUNDED-seed path
		// (lastSceneWasFull()==false) using dirty-rect history from the PREVIOUS room.
		// resetForRoomChange() must force the next renderScene back to a FULL seed and drop
		// the stale dirty history, matching the clean first frame a save-restore/instant-cut
		// entry gets for free. We drive the g_system-free seams (renderScene + accessors).
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);

		Graphics::Surface plate;
		plate.create(64, 64, rgba);
		plate.fillRect(Common::Rect(0, 0, 64, 64), rgba.ARGBToColor(255, 64, 64, 64));

		Graphics::Surface cel; // green sprite via celOverride (no ViewCache)
		cel.create(8, 8, rgba);
		cel.fillRect(Common::Rect(0, 0, 8, 8), rgba.ARGBToColor(255, 0, 255, 0));

		Sci::Roger::RogerCompositor comp;
		comp.setRoom(&plate, nullptr);
		comp.setPicture(64, 64, 0);
		comp.setDirtyPresent(true);

		const Common::Rect gameRect(0, 0, 64, 64);
		Graphics::ManagedSurface dest(64, 64, rgba);

		Sci::Roger::Sprite spr;
		spr.viewId = 900; spr.loopNo = 0; spr.celNo = 1;
		spr.priority = 1; spr.mirror = false; spr.celOverride = &cel;
		Common::Array<Sci::Roger::Sprite> list;

		// Frame 1: first render of the room -> bg cache built from scratch -> FULL seed.
		spr.celRect = Common::Rect(4, 4, 12, 12);
		list.clear(); list.push_back(spr);
		comp.renderScene(dest, list, gameRect);
		TS_ASSERT(comp.lastSceneWasFull());

		// Frame 2: cache now warm, same geometry -> BOUNDED seed (the pre-warmed state that,
		// after a transition, would run against the previous room's stale dirty history).
		spr.celRect = Common::Rect(20, 20, 28, 28);
		list.clear(); list.push_back(spr);
		comp.renderScene(dest, list, gameRect);
		TS_ASSERT(!comp.lastSceneWasFull());

		// Room change via transition: reset. Dirty history must be dropped immediately.
		comp.resetForRoomChange();
		TS_ASSERT(comp.lastSeedUnion().empty());

		// Frame 3: first frame of the "new" room -> FULL seed again despite the warm cache.
		spr.celRect = Common::Rect(40, 40, 48, 48);
		list.clear(); list.push_back(spr);
		comp.renderScene(dest, list, gameRect);
		TS_ASSERT(comp.lastSceneWasFull());

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
		// Expect: clipped (0,0,20,20) and merged (50,50,80,80) â€” the outside one gone.
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

	void test_merge_sprites_by_priority_orders_and_is_stable() {
		using namespace Sci::Roger;
		auto mk = [](int view, int prio) {
			Sprite s; s.viewId = view; s.loopNo = 0; s.celNo = 0;
			s.priority = prio; s.mirror = false; s.celOverride = nullptr;
			s.celRect = Common::Rect(0, 0, 1, 1);
			return s;
		};
		Common::Array<Sprite> animate, statics, out;
		// animate: priorities 2 then 5; static: priority 5 then 0.
		animate.push_back(mk(10, 2));
		animate.push_back(mk(11, 5));
		statics.push_back(mk(20, 5));
		statics.push_back(mk(21, 0));

		mergeSpritesByPriority(animate, statics, out);

		TS_ASSERT_EQUALS(out.size(), (uint)4);
		// Ascending priority: 0,2,5,5.
		TS_ASSERT_EQUALS(out[0].viewId, 21); // prio 0
		TS_ASSERT_EQUALS(out[1].viewId, 10); // prio 2
		// Two prio-5 entries: static (20) keeps its place BEFORE animate (11) â€” stable,
		// and static was appended first.
		TS_ASSERT_EQUALS(out[2].viewId, 20); // prio 5, static
		TS_ASSERT_EQUALS(out[3].viewId, 11); // prio 5, animate
	}

	void test_map_native_rect_to_overlay() {
		using namespace Sci::Roger;
		// picRect = overlay (100,50)-(740,530) i.e. 640x480; picture-window 320x190;
		// picScreenTop = 10 (SCI0 menu bar). A native rect at screen (0,10)-(320,200)
		// (the full picture window) maps to the whole picRect.
		Common::Rect picRect(100, 50, 740, 530);
		Common::Rect full = mapNativeRectToOverlay(Common::Rect(0, 10, 320, 200),
		                                            picRect, 320, 190, 10);
		TS_ASSERT_EQUALS(full.left, 100);
		TS_ASSERT_EQUALS(full.top, 50);
		TS_ASSERT_EQUALS(full.right, 740);
		TS_ASSERT_EQUALS(full.bottom, 530);

		// A native rect covering the left half, top half of the picture window:
		// screen (0,10)-(160,105) -> local (0,0)-(160,95) -> overlay (100,50)-(420,290).
		Common::Rect half = mapNativeRectToOverlay(Common::Rect(0, 10, 160, 105),
		                                            picRect, 320, 190, 10);
		TS_ASSERT_EQUALS(half.left, 100);
		TS_ASSERT_EQUALS(half.top, 50);
		TS_ASSERT_EQUALS(half.right, 420);
		TS_ASSERT_EQUALS(half.bottom, 290);
	}

	void test_map_native_rect_edges_match_nearest_sampler() {
		using namespace Sci::Roger;
		// Non-integral scale (320x190 picture -> 799x474 overlay): every mapped edge
		// must be ceil(v * dst / src) — the boundary consistent with the top-left
		// rational sampling all Roger nearest scalers use — so Feeder-B stamps land
		// exactly on the pixels that show their native rows.
		Common::Rect picRect(0, 25, 799, 25 + 474);
		for (int r = 1; r < 190; r++) {
			int edge = 0;
			for (int dy = 0; dy < 474; dy++)
				if (dy * 190 / 474 < r)
					edge++;
			Common::Rect m = mapNativeRectToOverlay(Common::Rect(0, 10, 320, (int16)(10 + r)),
			                                        picRect, 320, 190, 10);
			TS_ASSERT_EQUALS(m.bottom - picRect.top, edge);
		}
	}

	void test_upscale_native_region_nearest() {
		using namespace Sci::Roger;
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		// 2x1 native source: index 1 (red), index 2 (green). Pitch = 2.
		byte visual[2] = {1, 2};
		byte palette[256 * 3];
		for (int i = 0; i < 256 * 3; i++) palette[i] = 0;
		palette[1 * 3 + 0] = 255; // index 1 -> red
		palette[2 * 3 + 1] = 255; // index 2 -> green

		Graphics::Surface dest;
		dest.create(4, 1, rgba); // 2x horizontal upscale
		upscaleNativeRegionNearest(dest, Common::Rect(0, 0, 4, 1),
		                           visual, 2, Common::Rect(0, 0, 2, 1), palette);

		uint8 a, r, g, b;
		dest.format.colorToARGB(dest.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(r, 255); TS_ASSERT_EQUALS(g, 0);   // left half = index 1 (red)
		dest.format.colorToARGB(dest.getPixel(3, 0), a, r, g, b);
		TS_ASSERT_EQUALS(g, 255); TS_ASSERT_EQUALS(r, 0);   // right half = index 2 (green)
		TS_ASSERT_EQUALS(a, 255);                            // opaque
		dest.free();
	}

	void test_extract_changed_boxes() {
		using namespace Sci::Roger;
		const int w = 8, h = 8;
		byte prev[64], cur[64];
		for (int i = 0; i < 64; i++) { prev[i] = 0; cur[i] = 0; }
		// Change one pixel at (2,2) and an adjacent (3,2) -> one box.
		cur[2 * w + 2] = 5; cur[2 * w + 3] = 5;
		// A far-apart change at (6,6) -> a second box.
		cur[6 * w + 6] = 7;

		Common::Array<Common::Rect> out;
		extractChangedBoxes(prev, cur, w, h, out);

		TS_ASSERT_EQUALS(out.size(), (uint)2);
		bool hasA = false, hasB = false;
		for (uint i = 0; i < out.size(); i++) {
			if (out[i].contains(2, 2) && out[i].contains(3, 2)) hasA = true;
			if (out[i].contains(6, 6)) hasB = true;
		}
		TS_ASSERT(hasA);
		TS_ASSERT(hasB);
	}

	void test_extract_changed_boxes_identical_is_empty() {
		using namespace Sci::Roger;
		byte a[16], b[16];
		for (int i = 0; i < 16; i++) { a[i] = (byte)i; b[i] = (byte)i; }
		Common::Array<Common::Rect> out;
		extractChangedBoxes(a, b, 4, 4, out);
		TS_ASSERT_EQUALS(out.size(), (uint)0);
	}

	void test_native_rows_to_overlay_uses_caps() {
		Sci::Roger::RogerCompositor c;
		Sci::Roger::RogerCapabilities caps =
			Sci::Roger::RogerCapabilities::fromProbes(true, true, 200);
		c.setCapabilities(caps);
		// 10 status rows of 200, into a 2000px overlay -> 100px.
		TS_ASSERT_EQUALS(c.nativeRowsToOverlay(10, 2000), 100);
	}
};

// Rollback containment for pixel stamps (menu-close residue class). A menu dropdown's
// bitsShow rect and its bitsSave/bitsRestore rect differ by the 1px byte-aligned frame:
// the stamp is created from the SHOW rect, the rollback fires on the (narrower) RESTORE
// rect. Strict rect.contains() misses that inset and the stamp is retained forever
// (tracked non-enhanced residue, ego renders behind it). restoreReclaimsStamp must reclaim
// it via the same >= 90% coverage rule the capture-time reveal suppression uses.
class TestStampRollback : public CxxTest::TestSuite {
public:
	// The exact QFG1 case observed in the diag trace: show (60,9,214,59), restore (61,9,214,59).
	void test_restore_reclaims_stamp_one_px_inset() {
		const Common::Rect show(60, 9, 214, 59);   // stamp celRect (from bitsShow)
		const Common::Rect restore(61, 9, 214, 59); // restore rect (byte-aligned, 1px narrower left)
		// The regression before the fix: strict containment fails (60 < 61).
		TS_ASSERT(!restore.contains(show));
		// The fix: coverage-based reclaim succeeds (99% of the stamp is inside the restore).
		TS_ASSERT(Sci::Roger::restoreReclaimsStamp(restore, show, 90));
	}

	void test_restore_reclaims_stamp_inset_all_sides() {
		// A restore 1px narrower on every side still reclaims (frame-pixel byte alignment).
		const Common::Rect show(40, 20, 200, 120);
		const Common::Rect restore(41, 21, 199, 119);
		TS_ASSERT(!restore.contains(show));
		TS_ASSERT(Sci::Roger::restoreReclaimsStamp(restore, show, 90));
	}

	void test_exact_containment_still_reclaims() {
		// The common case (restore fully covers the stamp) is unaffected.
		const Common::Rect show(60, 9, 214, 59);
		const Common::Rect restore(50, 5, 220, 65);
		TS_ASSERT(restore.contains(show));
		TS_ASSERT(Sci::Roger::restoreReclaimsStamp(restore, show, 90));
	}

	void test_disjoint_restore_does_not_reclaim() {
		// A restore of a different region must NOT drop an unrelated stamp.
		const Common::Rect show(60, 9, 214, 59);
		const Common::Rect restore(0, 0, 20, 10);
		TS_ASSERT(!Sci::Roger::restoreReclaimsStamp(restore, show, 90));
	}

	void test_partial_overlap_below_threshold_does_not_reclaim() {
		// A restore covering < 90% of the stamp leaves it (adjacent dropdown restore must
		// not reclaim a neighbouring menu's stamp). show 154 wide; restore overlaps 112px
		// (102..214) => ~72% coverage < 90.
		const Common::Rect show(60, 9, 214, 59);
		const Common::Rect restore(102, 9, 222, 59);
		TS_ASSERT(Sci::Roger::rectCoverageFraction(show, restore) < 90);
		TS_ASSERT(!Sci::Roger::restoreReclaimsStamp(restore, show, 90));
	}

	void test_empty_stamp_not_reclaimed() {
		const Common::Rect show(60, 9, 60, 9); // empty
		const Common::Rect restore(0, 0, 320, 200);
		TS_ASSERT(!Sci::Roger::restoreReclaimsStamp(restore, show, 90));
	}
};

// Round-2 residue #1: the stuck menu-title strip. On the mouse menu path SCI closes the
// menu with bitsRestore of the full menu strip (reverting native to the score banner)
// WITHOUT a follow-up kernelDrawStatus, so onRestore is the only seam that can
// re-apply the enhanced banner. The provider gates that re-apply on the restore rect
// covering the status strip (>= 90% of _statusRect) so a dropdown's own narrower restore
// (which never touches the banner row 0) does not spuriously re-push the banner. These
// pin that pure coverage discrimination on the exact geometry from the diag trace.
class TestStatusStripRestore : public CxxTest::TestSuite {
public:
	// _statusRect is the top strip; the menu-bar save-under restore is the full strip.
	void test_full_strip_restore_covers_status_rect() {
		const Common::Rect statusRect(0, 0, 320, 9);  // _ports->_menuBarRect (banner strip)
		const Common::Rect barRestore(0, 0, 320, 10); // bitsRestore(_barSaveHandle) rect (diag line)
		TS_ASSERT(Sci::Roger::rectCoverageFraction(statusRect, barRestore) >= 90);
	}

	// A dropdown's own save-under restore starts at row 9 and is far narrower â€” it must NOT
	// be mistaken for a strip revert (else every dropdown close would re-push the banner).
	void test_dropdown_restore_does_not_cover_status_rect() {
		const Common::Rect statusRect(0, 0, 320, 9);
		const Common::Rect dropRestore(7, 9, 141, 27); // File dropdown restore (diag line 375/378)
		TS_ASSERT(Sci::Roger::rectCoverageFraction(statusRect, dropRestore) < 90);
	}

	// A deep dropdown (Game/Action, reaching well into the scene) still never covers the
	// banner row 0, so a scene-deep dropdown close does not re-push the banner either.
	void test_deep_dropdown_restore_does_not_cover_status_rect() {
		const Common::Rect statusRect(0, 0, 320, 9);
		const Common::Rect deepDrop(7, 9, 141, 59);
		TS_ASSERT(Sci::Roger::rectCoverageFraction(statusRect, deepDrop) < 90);
	}
};

class TestInitFrameSpriteSet : public CxxTest::TestSuite {
	static Sci::Roger::Sprite mk(int v, int l, int c, int prio, int x, int y) {
		Sci::Roger::Sprite s;
		s.viewId = v; s.loopNo = l; s.celNo = c; s.priority = prio;
		s.celRect = Common::Rect((int16)x, (int16)y, (int16)(x + 10), (int16)(y + 10));
		return s;
	}
public:
	// The transition frame mirrors the native buffer at animateShowPic time:
	// addToPic statics + EVERY init-frame cast draw (live actors included),
	// deduped by cel identity + rect, ascending-priority draw order.
	void test_dedups_and_sorts_by_priority() {
		Common::Array<Sci::Roger::Sprite> statics, initCels, out;
		statics.push_back(mk(300, 2, 0, 5, 10, 20));   // baked sign (addToPic)
		initCels.push_back(mk(300, 2, 0, 5, 10, 20));  // same sign captured again on frame 1
		initCels.push_back(mk(0, 0, 0, 12, 50, 60));   // ego (live actor, still included)
		initCels.push_back(mk(301, 1, 0, 3, 70, 80));  // low-priority prop
		Sci::Roger::buildInitFrameSpriteSet(statics, initCels, out);
		TS_ASSERT_EQUALS(out.size(), 3u);
		TS_ASSERT_EQUALS(out[0].priority, 3);
		TS_ASSERT_EQUALS(out[1].priority, 5);
		TS_ASSERT_EQUALS(out[2].priority, 12);
	}
	// Same cel stamped at a second position (e.g. a repeated decoration) is
	// two draws, not a duplicate â€” dedup key includes celRect.
	void test_same_cel_different_rect_is_not_a_dup() {
		Common::Array<Sci::Roger::Sprite> statics, initCels, out;
		statics.push_back(mk(300, 2, 0, 5, 10, 20));
		initCels.push_back(mk(300, 2, 0, 5, 90, 20));
		Sci::Roger::buildInitFrameSpriteSet(statics, initCels, out);
		TS_ASSERT_EQUALS(out.size(), 2u);
	}
	void test_empty_inputs_yield_empty_output() {
		Common::Array<Sci::Roger::Sprite> statics, initCels, out;
		Sci::Roger::buildInitFrameSpriteSet(statics, initCels, out);
		TS_ASSERT_EQUALS(out.size(), 0u);
	}
};
