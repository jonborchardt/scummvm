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
#include "sci/roger/roger_effects.h"
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
		TS_ASSERT_EQUALS(transitionFamilyFor(10), kFxFade);   // FADEPALETTE
		TS_ASSERT_EQUALS(transitionFamilyFor(8),  kFxDissolve); // BLOCKS
		TS_ASSERT_EQUALS(transitionFamilyFor(9),  kFxDissolve); // PIXELATION
		TS_ASSERT_EQUALS(transitionFamilyFor(2),  kFxWipe);   // STRAIGHT_FROM_RIGHT
		TS_ASSERT_EQUALS(transitionFamilyFor(0),  kFxWipe);   // VERTICALROLL_FROMCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(300), kFxWipe);  // VERTICALROLL_TOCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(11), kFxScroll); // SCROLL_RIGHT
		TS_ASSERT_EQUALS(transitionFamilyFor(15), kFxNone);   // NONE_LONGBOW
		TS_ASSERT_EQUALS(transitionFamilyFor(100), kFxNone);  // NONE
		TS_ASSERT_EQUALS(transitionFamilyFor(9999), kFxFade); // unknown -> safe default
	}
	void test_phase1_collapse() {
		// Phase 1: Wipe/Scroll temporarily render as Dissolve; Fade/Dissolve/None unchanged.
		TS_ASSERT_EQUALS(effectiveFamily(kFxWipe),     kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxScroll),   kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxFade),     kFxFade);
		TS_ASSERT_EQUALS(effectiveFamily(kFxDissolve), kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxNone),     kFxNone);
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
};
