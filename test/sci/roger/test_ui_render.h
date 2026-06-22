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
#include "sci/roger/roger_coords.h"
#include "sci/roger/roger_ui_layer.h"
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
};
