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
#include "common/array.h"
#include "common/rect.h"
#include "engines/sci/roger/roger_compositor.h"
#include "engines/sci/roger/roger_ui_layer.h"

using namespace Sci;
using namespace Sci::Roger;

class GenericTextCaptureTestSuite : public CxxTest::TestSuite {
	static UiElement txt(int l, int t, int r, int b, uint32 token) {
		UiElement e; e.type = kUiText; e.nativeRect = Common::Rect(l, t, r, b); e.token = token; return e;
	}
public:
	void test_dedupe_drops_generic_covered_by_control() {
		const uint32 G = 0x60000000u, C = 0x40000000u;
		Common::Array<UiElement> elems;
		elems.push_back(txt(10, 10, 100, 22, C));   // control text
		elems.push_back(txt(12, 11, 90, 21, G));    // generic, covered by the control -> drop
		elems.push_back(txt(10, 50, 100, 62, G));   // generic, not covered -> keep
		Roger::dedupeGenericTextElements(elems, G);
		TS_ASSERT_EQUALS(elems.size(), 2u);
		TS_ASSERT(elems[0].token == C);
		TS_ASSERT(elems[1].token == G && elems[1].nativeRect == Common::Rect(10, 50, 100, 62));
	}
	void test_dedupe_keeps_generic_when_no_control_covers() {
		const uint32 G = 0x60000000u;
		Common::Array<UiElement> elems;
		elems.push_back(txt(10, 10, 100, 22, G));
		elems.push_back(txt(10, 30, 100, 42, G)); // two generics, no non-generic -> both kept
		Roger::dedupeGenericTextElements(elems, G);
		TS_ASSERT_EQUALS(elems.size(), 2u);
	}
	void test_dedupe_does_not_drop_generic_covered_only_by_window() {
		// A kUiWindow (non-generic token) whose nativeRect fully contains a generic
		// kUiText element must NOT cause the generic text to be dropped.
		// Only a containing non-generic kUiText may trigger the drop.
		const uint32 G = 0x60000000u, W = 0x40000000u;
		Common::Array<UiElement> elems;
		// kUiWindow that covers the generic text rect
		UiElement win; win.type = kUiWindow; win.nativeRect = Common::Rect(0, 0, 320, 200); win.token = W;
		elems.push_back(win);
		elems.push_back(txt(10, 10, 100, 22, G)); // generic text fully inside the window -> must be KEPT
		Roger::dedupeGenericTextElements(elems, G);
		TS_ASSERT_EQUALS(elems.size(), 2u);
		TS_ASSERT(elems[0].type == kUiWindow);
		TS_ASSERT(elems[1].token == G); // generic text survived
	}
	void test_collect_ui_text_rects_gathers_all_text() {
		const uint32 G = 0x60000000u, C = 0x40000000u;
		Common::Array<UiElement> elems;
		elems.push_back(txt(10, 10, 100, 22, C));
		elems.push_back(txt(10, 30, 100, 42, G));
		UiElement win; win.type = kUiWindow; win.nativeRect = Common::Rect(0,0,5,5); win.token = C;
		elems.push_back(win); // not text -> not collected
		Common::Array<Common::Rect> out;
		Roger::collectUiTextRects(elems, G, out);
		TS_ASSERT_EQUALS(out.size(), 2u);
	}
};
