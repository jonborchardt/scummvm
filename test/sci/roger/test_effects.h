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
#include "sci/roger/overlay/roger_effects.h"
#include "graphics/surface.h"
#include "common/rect.h"
using namespace Sci::Roger;

class RogerEffectsTestSuite : public CxxTest::TestSuite {
public:
	static Graphics::Surface *solid(int w, int h, uint8 r, uint8 g, uint8 b) {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *s = new Graphics::Surface();
		s->create(w, h, rgba);
		s->fillRect(Common::Rect(0, 0, w, h), rgba.RGBToColor(r, g, b));
		return s;
	}
	void test_family_mapping() {
		TS_ASSERT_EQUALS(transitionFamilyFor(10),  kFxFade);     // FADEPALETTE
		TS_ASSERT_EQUALS(transitionFamilyFor(8),   kFxDissolve); // BLOCKS
		TS_ASSERT_EQUALS(transitionFamilyFor(9),   kFxDissolve); // PIXELATION
		TS_ASSERT_EQUALS(transitionFamilyFor(2),   kFxWipe);     // STRAIGHT_FROM_RIGHT
		TS_ASSERT_EQUALS(transitionFamilyFor(3),   kFxWipe);     // STRAIGHT_FROM_LEFT
		TS_ASSERT_EQUALS(transitionFamilyFor(4),   kFxWipe);     // STRAIGHT_FROM_BOTTOM
		TS_ASSERT_EQUALS(transitionFamilyFor(5),   kFxWipe);     // STRAIGHT_FROM_TOP
		TS_ASSERT_EQUALS(transitionFamilyFor(11),  kFxScroll);   // SCROLL_RIGHT
		TS_ASSERT_EQUALS(transitionFamilyFor(15),  kFxNone);     // NONE_LONGBOW
		TS_ASSERT_EQUALS(transitionFamilyFor(100), kFxNone);     // NONE
		TS_ASSERT_EQUALS(transitionFamilyFor(9999),kFxFade);     // unknown -> safe default
		// Roll / diagonal types now have their own families
		TS_ASSERT_EQUALS(transitionFamilyFor(0),   kFxSplitV);   // VERTICALROLL_FROMCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(300), kFxSplitV);   // VERTICALROLL_TOCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(1),   kFxSplitH);   // HORIZONTALROLL_FROMCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(301), kFxSplitH);   // HORIZONTALROLL_TOCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(7),   kFxDiagonal); // DIAGONALROLL_FROMCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(6),   kFxDiagonal); // DIAGONALROLL_TOCENTER
	}
	void test_effectiveFamily_identity() {
		// effectiveFamily is now an identity â€” each family renders faithfully.
		TS_ASSERT_EQUALS(effectiveFamily(kFxWipe),     kFxWipe);
		TS_ASSERT_EQUALS(effectiveFamily(kFxScroll),   kFxScroll);
		TS_ASSERT_EQUALS(effectiveFamily(kFxFade),     kFxFade);
		TS_ASSERT_EQUALS(effectiveFamily(kFxDissolve), kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxNone),     kFxNone);
	}
	void test_effectiveFamily_includes_new_families() {
		// effectiveFamily is identity â€” verify new families pass through unchanged.
		TS_ASSERT_EQUALS(effectiveFamily(kFxSplitV),   kFxSplitV);
		TS_ASSERT_EQUALS(effectiveFamily(kFxSplitH),   kFxSplitH);
		TS_ASSERT_EQUALS(effectiveFamily(kFxDiagonal), kFxDiagonal);
	}
	void test_splitFromCenter() {
		// FromCenter types: 0 (VerticalFromCenter), 1 (HorizontalFromCenter), 7 (DiagonalFromCenter)
		TS_ASSERT(splitFromCenter(0));
		TS_ASSERT(splitFromCenter(1));
		TS_ASSERT(splitFromCenter(7));
		// ToCenter types: 300 (VerticalToCenter), 301 (HorizontalToCenter), 6 (DiagonalToCenter)
		TS_ASSERT(!splitFromCenter(300));
		TS_ASSERT(!splitFromCenter(301));
		TS_ASSERT(!splitFromCenter(6));
		// Unknown: defaults to false
		TS_ASSERT(!splitFromCenter(9999));
	}
	void test_blockPxForSciType() {
		TS_ASSERT_EQUALS(blockPxForSciType(9),  8);  // PIXELATION: fine grain
		TS_ASSERT_EQUALS(blockPxForSciType(8),  24); // BLOCKS: ~10K blocks at 2862-wide overlay
		TS_ASSERT_EQUALS(blockPxForSciType(10), 24); // any non-pixelation -> 24
		TS_ASSERT_EQUALS(blockPxForSciType(0),  24); // roll types: 24 (not called for dissolve, but safe)
	}
	void test_wipeDirectionFor() {
		TS_ASSERT_EQUALS(wipeDirectionFor(2),   0); // STRAIGHT_FROM_RIGHT -> right
		// Types 0, 1, 6, 7, 300, 301 now route to kFxSplitV/H/kFxDiagonal and never
		// reach wipeDirectionFor in runTransition, but the function still returns defined values:
		TS_ASSERT_EQUALS(wipeDirectionFor(0),   0); // VERTICALROLL_FROMCENTER (dead: -> kFxSplitV)
		TS_ASSERT_EQUALS(wipeDirectionFor(300), 0); // VERTICALROLL_TOCENTER   (dead: -> kFxSplitV)
		TS_ASSERT_EQUALS(wipeDirectionFor(6),   0); // DIAGONALROLL_TOCENTER   (dead: -> kFxDiagonal)
		TS_ASSERT_EQUALS(wipeDirectionFor(7),   0); // DIAGONALROLL_FROMCENTER (dead: -> kFxDiagonal)
		TS_ASSERT_EQUALS(wipeDirectionFor(3),   1); // STRAIGHT_FROM_LEFT -> left
		TS_ASSERT_EQUALS(wipeDirectionFor(1),   1); // HORIZONTALROLL_FROMCENTER (dead: -> kFxSplitH)
		TS_ASSERT_EQUALS(wipeDirectionFor(301), 1); // HORIZONTALROLL_TOCENTER   (dead: -> kFxSplitH)
		TS_ASSERT_EQUALS(wipeDirectionFor(4),   2); // STRAIGHT_FROM_BOTTOM -> bottom
		TS_ASSERT_EQUALS(wipeDirectionFor(5),   3); // STRAIGHT_FROM_TOP -> top
		TS_ASSERT_EQUALS(wipeDirectionFor(9999), 0); // unknown -> safe default (right)
	}
	void test_blendWipe_endpoints() {
		// At t=0 the output must equal 'from'; at t=1 it must equal 'to'.
		// Test all four directions.
		for (int dir = 0; dir < 4; dir++) {
			Graphics::Surface *from = solid(16, 16, 255, 0, 0);   // red
			Graphics::Surface *to   = solid(16, 16, 0, 0, 255);   // blue
			const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
			Graphics::Surface out; out.create(16, 16, rgba);
			uint8 a, r, g, b;

			blendWipe(*from, *to, out, 0.0f, dir);
			// At t=0 all pixels should be 'from' (red)
			out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
			TS_ASSERT_EQUALS((int)r, 255);
			TS_ASSERT_EQUALS((int)b, 0);

			blendWipe(*from, *to, out, 1.0f, dir);
			// At t=1 all pixels should be 'to' (blue)
			out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
			TS_ASSERT_EQUALS((int)r, 0);
			TS_ASSERT_EQUALS((int)b, 255);

			from->free(); delete from; to->free(); delete to; out.free();
		}
	}
	void test_fade_endpoints_and_midpoint() {
		Graphics::Surface *from = solid(4, 4, 200, 0, 0);   // red
		Graphics::Surface *to   = solid(4, 4, 0, 0, 200);   // blue
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface out; out.create(4, 4, rgba);
		uint8 a, r, g, b;
		blendFadeThroughBlack(*from, *to, out, 0.0f);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 200); TS_ASSERT_EQUALS((int)b, 0);   // == from
		blendFadeThroughBlack(*from, *to, out, 1.0f);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 200); // == to
		blendFadeThroughBlack(*from, *to, out, 0.5f);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0); TS_ASSERT_EQUALS((int)g, 0); TS_ASSERT_EQUALS((int)b, 0); // black at mid
		from->free(); delete from; to->free(); delete to; out.free();
	}
	void test_dissolve_endpoints() {
		Graphics::Surface *from = solid(8, 8, 255, 0, 0);
		Graphics::Surface *to   = solid(8, 8, 0, 255, 0);
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface out; out.create(8, 8, rgba);
		uint8 a, r, g, b;
		blendDissolve(*from, *to, out, 0.0f, 4);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); // all `from` at t=0
		blendDissolve(*from, *to, out, 1.0f, 4);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)g, 255); // all `to` at t=1
		from->free(); delete from; to->free(); delete to; out.free();
	}
	void test_blendDissolve_hash_density() {
		// At t=0.5, roughly half the blocks should show `to`. Verify 20%â€“80% mix.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *from = solid(64, 64, 255, 0, 0);   // red = from
		Graphics::Surface *to   = solid(64, 64, 0, 0, 255);   // blue = to
		Graphics::Surface out;  out.create(64, 64, rgba);
		blendDissolve(*from, *to, out, 0.5f, 4);
		int toCount = 0;
		uint8 a, r, g, b;
		for (int y = 0; y < 64; y++) {
			for (int x = 0; x < 64; x++) {
				out.format.colorToARGB(out.getPixel(x, y), a, r, g, b);
				if (b > r) toCount++;   // blue pixel came from 'to'
			}
		}
		// 64*64 = 4096 pixels; expect 20%â€“80% showing to (819â€“3277)
		TS_ASSERT_LESS_THAN(819,  toCount);
		TS_ASSERT_LESS_THAN(toCount, 3277);
		from->free(); delete from; to->free(); delete to; out.free();
	}
	void test_blendSplitVertical() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *from = solid(32, 16, 255, 0, 0);   // red
		Graphics::Surface *to   = solid(32, 16, 0, 0, 255);   // blue
		Graphics::Surface out;  out.create(32, 16, rgba);
		uint8 a, r, g, b;

		// Endpoints: t=0 -> all from, t=1 -> all to (both fromCenter variants)
		blendSplitVertical(*from, *to, out, 0.0f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		blendSplitVertical(*from, *to, out, 1.0f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		blendSplitVertical(*from, *to, out, 0.0f, false);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		blendSplitVertical(*from, *to, out, 1.0f, false);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		// fromCenter=true at t=0.5:
		//   x=0  (edge):   threshold = |2*0/32 - 1| = 1.0 > 0.5 -> from (red)
		//   x=16 (center): threshold = |2*16/32 - 1| = 0.0 < 0.5 -> to (blue)
		blendSplitVertical(*from, *to, out, 0.5f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		out.format.colorToARGB(out.getPixel(16, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		// fromCenter=false (toCenter) at t=0.5:
		//   x=0  (edge):   threshold = 1 - 1.0 = 0.0 < 0.5 -> to (blue)
		//   x=16 (center): threshold = 1 - 0.0 = 1.0 > 0.5 -> from (red)
		blendSplitVertical(*from, *to, out, 0.5f, false);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);
		out.format.colorToARGB(out.getPixel(16, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);

		from->free(); delete from; to->free(); delete to; out.free();
	}
	void test_blendSplitHorizontal() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *from = solid(16, 32, 255, 0, 0);   // red
		Graphics::Surface *to   = solid(16, 32, 0, 0, 255);   // blue
		Graphics::Surface out;  out.create(16, 32, rgba);
		uint8 a, r, g, b;

		// Endpoints
		blendSplitHorizontal(*from, *to, out, 0.0f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		blendSplitHorizontal(*from, *to, out, 1.0f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		// fromCenter=true at t=0.5:
		//   y=0  (edge):   threshold = |2*0/32 - 1| = 1.0 > 0.5 -> from (red)
		//   y=16 (center): threshold = |2*16/32 - 1| = 0.0 < 0.5 -> to (blue)
		blendSplitHorizontal(*from, *to, out, 0.5f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		out.format.colorToARGB(out.getPixel(0, 16), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		// fromCenter=false at t=0.5:
		//   y=0  (edge):   threshold = 1 - 1.0 = 0.0 < 0.5 -> to (blue)
		//   y=16 (center): threshold = 1 - 0.0 = 1.0 > 0.5 -> from (red)
		blendSplitHorizontal(*from, *to, out, 0.5f, false);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);
		out.format.colorToARGB(out.getPixel(0, 16), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);

		from->free(); delete from; to->free(); delete to; out.free();
	}
	void test_blendDiagonal() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *from = solid(32, 32, 255, 0, 0);   // red
		Graphics::Surface *to   = solid(32, 32, 0, 0, 255);   // blue
		Graphics::Surface out;  out.create(32, 32, rgba);
		uint8 a, r, g, b;

		// Endpoints: t=0 -> all from, t=1 -> all to
		blendDiagonal(*from, *to, out, 0.0f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		blendDiagonal(*from, *to, out, 1.0f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		// fromCenter=true at t=0.5:
		//   corner (0,0):   Lâˆž = max(|2*0/32-1|, |2*0/32-1|) = 1.0 > 0.5 -> from (red)
		//   center (16,16): Lâˆž = max(|2*16/32-1|, |2*16/32-1|) = 0.0 < 0.5 -> to (blue)
		blendDiagonal(*from, *to, out, 0.5f, true);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);
		out.format.colorToARGB(out.getPixel(16, 16), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);

		// fromCenter=false (toCenter) at t=0.5:
		//   corner (0,0):   threshold = 1 - 1.0 = 0.0 < 0.5 -> to (blue)
		//   center (16,16): threshold = 1 - 0.0 = 1.0 > 0.5 -> from (red)
		blendDiagonal(*from, *to, out, 0.5f, false);
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255);
		out.format.colorToARGB(out.getPixel(16, 16), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);

		from->free(); delete from; to->free(); delete to; out.free();
	}
	void test_scrollDirectionFor() {
		// kSciTrScrollDown=14, Up=13, Right=11, Left=12  (from transitions.h enum)
		TS_ASSERT_EQUALS(scrollDirectionFor(14), 3); // ScrollDown: new enters from top
		TS_ASSERT_EQUALS(scrollDirectionFor(13), 2); // ScrollUp: new enters from bottom
		TS_ASSERT_EQUALS(scrollDirectionFor(11), 0); // ScrollRight: new enters from right
		TS_ASSERT_EQUALS(scrollDirectionFor(12), 1); // ScrollLeft: new enters from left
		TS_ASSERT_EQUALS(scrollDirectionFor(9999), 3); // unknown -> safe default (top)
	}
	void test_blendScroll_endpoints() {
		// At t=0 the output must equal 'from'; at t=1 it must equal 'to'.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		for (int dir = 0; dir < 4; dir++) {
			Graphics::Surface *from = solid(16, 16, 255, 0, 0); // red
			Graphics::Surface *to   = solid(16, 16, 0, 0, 255); // blue
			Graphics::Surface out;  out.create(16, 16, rgba);
			uint8 a, r, g, b;

			blendScroll(*from, *to, out, 0.0f, dir);
			out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
			TS_ASSERT_EQUALS((int)r, 255); TS_ASSERT_EQUALS((int)b, 0);   // == from

			blendScroll(*from, *to, out, 1.0f, dir);
			out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
			TS_ASSERT_EQUALS((int)r, 0);   TS_ASSERT_EQUALS((int)b, 255); // == to

			from->free(); delete from; to->free(); delete to; out.free();
		}
	}
	void test_blendScroll_midpoint_has_both_frames() {
		// At t=0.5, direction=3 (new enters from top): the top half of the output
		// comes from 'to' (new frame, entering from top) and the bottom half comes
		// from 'from' (old frame, exiting downward).
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface *from = solid(4, 4, 255, 0, 0); // red (old)
		Graphics::Surface *to   = solid(4, 4, 0, 0, 255); // blue (new)
		Graphics::Surface out;  out.create(4, 4, rgba);
		blendScroll(*from, *to, out, 0.5f, 3); // new from top
		uint8 a, r, g, b;
		// Top pixel (y=0): comes from the new frame (blue)
		out.format.colorToARGB(out.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)b, 255);
		// Bottom pixel (y=3): comes from the old frame (red)
		out.format.colorToARGB(out.getPixel(0, 3), a, r, g, b);
		TS_ASSERT_EQUALS((int)r, 255);
		from->free(); delete from; to->free(); delete to; out.free();
	}
};
