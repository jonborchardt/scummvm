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
#include "sci/roger/overlay/roger_coords.h"
#include "sci/roger/overlay/roger_ui_layer.h"
#include "sci/roger/overlay/roger_text.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "common/array.h"

using namespace Sci::Roger;

class TestUiRender : public CxxTest::TestSuite {
public:
	void test_sci_rect_to_dest_maps_proportionally() {
		Common::Rect game(0, 0, 320, 200); // identity mapping
		Common::Rect r = sciRectToDest(Common::Rect(10, 20, 30, 40), game);
		TS_ASSERT_EQUALS(r.left, 10);
		TS_ASSERT_EQUALS(r.top, 20);
		TS_ASSERT_EQUALS(r.right, 30);
		TS_ASSERT_EQUALS(r.bottom, 40);

		Common::Rect game2(0, 0, 640, 400); // 2x
		Common::Rect r2 = sciRectToDest(Common::Rect(10, 20, 30, 40), game2);
		TS_ASSERT_EQUALS(r2.left, 20);
		TS_ASSERT_EQUALS(r2.top, 40);
		TS_ASSERT_EQUALS(r2.right, 60);
		TS_ASSERT_EQUALS(r2.bottom, 80);
	}

	void test_render_ui_fills_window_box_with_back_color() {
		// 320x200 dest, identity gameRect. One opaque window filling 0,0..160,100
		// with palette index 5 = pure blue.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dest(320, 200, rgba);
		dest.fillRect(Common::Rect(0, 0, 320, 200), rgba.ARGBToColor(255, 0, 0, 0));

		byte pal[256 * 3];
		for (int i = 0; i < 256 * 3; i++) pal[i] = 0;
		pal[5 * 3 + 0] = 0; pal[5 * 3 + 1] = 0; pal[5 * 3 + 2] = 255; // index 5 = blue

		UiElement w;
		w.type = kUiWindow;
		w.nativeRect = Common::Rect(0, 0, 160, 100);
		w.backColor = 5;
		w.penColor = 0;
		w.hasFrame = true;
		Common::Array<UiElement> elems;
		elems.push_back(w);

		RogerCompositor comp;
		comp.renderUiLayer(dest, elems, pal, Common::Rect(0, 0, 320, 200), nullptr);

		uint8 a, r, g, b;
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(80, 50), a, r, g, b);
		TS_ASSERT_EQUALS(b, 255); TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 0);
		TS_ASSERT_EQUALS(a, 255);
		// Outside the window stays black.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(200, 150), a, r, g, b);
		TS_ASSERT_EQUALS(b, 0); TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 0);
	}

	void test_render_scene_fills_letterbox_opaque_black() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dest(320, 200, rgba);
		RogerCompositor comp;
		// Picture occupies only the centre; corners are letterbox.
		comp.setPictureDest(Common::Rect(40, 0, 280, 200));
		comp.renderScene(dest, Common::Array<Sprite>());
		uint8 a, r, g, b;
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(5, 5), a, r, g, b);
		TS_ASSERT_EQUALS(a, 255); // opaque, not transparent
		TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 0); TS_ASSERT_EQUALS(b, 0); // black
	}

	void test_render_scene_letterbox_black_status_strip_transparent() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dest(320, 200, rgba);
		RogerCompositor comp;
		// Game rect inset on the sides (letterbox at x<20 and x>=300); the picture
		// sits below a 10px status strip at the top of the game rect.
		Common::Rect gameRect(20, 0, 300, 200);
		comp.setPictureDest(Common::Rect(20, 10, 300, 200));
		comp.renderScene(dest, Common::Array<Sprite>(), gameRect);
		uint8 a, r, g, b;
		// Letterbox (x=5, outside the game rect) -> opaque black blocker.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(5, 100), a, r, g, b);
		TS_ASSERT_EQUALS(a, 255); TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 0); TS_ASSERT_EQUALS(b, 0);
		// Status strip (x=160, y=5: inside the game rect, above the picture) -> transparent
		// so the native Sierra menu icon shows through.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(160, 5), a, r, g, b);
		TS_ASSERT_EQUALS(a, 0);
	}

	void test_render_window_has_black_border() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dest(320, 200, rgba);
		byte pal[256 * 3]; for (int i = 0; i < 256 * 3; i++) pal[i] = 255; // all white
		UiElement w; w.type = kUiWindow; w.nativeRect = Common::Rect(10, 10, 100, 60);
		w.backColor = 15; w.penColor = 15; w.hasFrame = true; // white pen on purpose
		Common::Array<UiElement> els; els.push_back(w);
		RogerCompositor comp;
		comp.renderUiLayer(dest, els, pal, Common::Rect(0, 0, 320, 200), nullptr);
		uint8 a, r, g, b;
		// A framed window is expanded by 2px (union-with-controls + padding), so its
		// left border sits at x=8 (10 - 2) for this lone window.
		dest.surfacePtr()->format.colorToARGB(dest.surfacePtr()->getPixel(8, 35), a, r, g, b);
		TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 0); TS_ASSERT_EQUALS(b, 0); // black border
	}

	void test_text_edit_text_inset_from_border() {
		// kUiTextEdit draws a 1px border (x=0 for a left-edge box), then left-aligned
		// text starting at textRect.left = d.left + textThick(1) + kUiTextPad(1) = 2.
		// Without the inset fix, text draws from x=0 (d.left), so the "W" glyph inks
		// x=1 in at least one row. With the fix, x=1 is always background (white).
		//
		// Non-vacuity: we also assert that at least one black pixel exists at x>=2
		// within the text rows, proving text actually drew. If drawPx is a no-op (e.g.
		// font not available) this assertion fails loudly instead of passing silently.
		//
		// Font-geometry independence: instead of relying on a single hard-coded glyph
		// pixel, we scan ALL rows of the box interior (y=1..box.bottom-2) for x=1,
		// so the test works regardless of which bitmap font is loaded.
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::ManagedSurface dst(320, 200, rgba);
		dst.fillRect(Common::Rect(0, 0, 320, 200), rgba.ARGBToColor(255, 0, 128, 0)); // green sentinel

		byte pal[256 * 3] = {};
		pal[15*3+0] = pal[15*3+1] = pal[15*3+2] = 255; // index 15 = white (edit bg)
		// palette[0] = black (zero-initialised) â€” used for border AND text

		UiElement e;
		e.type       = kUiTextEdit;
		e.nativeRect = Common::Rect(0, 0, 50, 20);
		e.backColor  = 15;   // white fill
		e.penColor   = 0;    // black border + text
		e.text       = "WWWWWWWWWWWW"; // wide text so leftmost glyph columns are filled
		e.align      = 0;    // left
		e.vAlignTop  = true; // SCI text-edit position
		e.style      = 0;    // no caret
		Common::Array<UiElement> elems;
		elems.push_back(e);

		RogerTextRenderer tr(""); // bitmap fallback â€” no game files needed
		RogerCompositor comp;
		comp.renderUiLayer(dst, elems, pal, Common::Rect(0, 0, 320, 200), &tr);

		const Graphics::Surface *surf = dst.surfacePtr();
		uint8 a, r, g, b;

		// 1. x=0: the 1px border â€” must be black across the text rows.
		surf->format.colorToARGB(surf->getPixel(0, 5), a, r, g, b);
		TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 0); TS_ASSERT_EQUALS(b, 0);

		// 2. x=1: the column immediately inside the border â€” must be pure white (background)
		//    in EVERY interior row of the box. With the inset (textRect.left=2), the font
		//    never draws there. Without it (textRect.left=0), "W" inks x=1 in >=1 row.
		bool anyBlackAtX1 = false;
		for (int y = 1; y < 19; y++) { // interior rows: skip top/bottom border pixels
			surf->format.colorToARGB(surf->getPixel(1, y), a, r, g, b);
			if (r == 0 && g == 0 && b == 0) {
				anyBlackAtX1 = true;
				break;
			}
		}
		TS_ASSERT(!anyBlackAtX1); // x=1 must be background (white), never text-coloured (black)

		// 3. Prove text actually drew: at least one black pixel must exist at x>=2 in the
		//    interior rows. If drawPx is a no-op (no font), this fails loudly.
		bool anyBlackAtX2plus = false;
		for (int y = 1; y < 19 && !anyBlackAtX2plus; y++) {
			for (int x = 2; x < 48 && !anyBlackAtX2plus; x++) {
				surf->format.colorToARGB(surf->getPixel(x, y), a, r, g, b);
				if (r == 0 && g == 0 && b == 0)
					anyBlackAtX2plus = true;
			}
		}
		TS_ASSERT(anyBlackAtX2plus); // text must have actually drawn (font available + inset works)
	}
};
