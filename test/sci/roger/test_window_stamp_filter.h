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
#include "engines/sci/roger/overlay/roger_compositor.h"
#include "engines/sci/roger/overlay/roger_ui_layer.h"

using namespace Sci;
using namespace Sci::Roger;

// regionIsCapturedWindowBody: the window's own frame+fill bitsShow (tagged with the
// window token) must never be pixel-stamped â€” it is already reproduced semantically as
// a kUiWindow element â€” while graphics drawn INSIDE the window (dialog icons) must
// stay stampable. Rects below are the real SQ3 death-message / Deceleration Trauma
// dialog numbers from the 2026-07-04 under-draw bug.
class WindowStampFilterTestSuite : public CxxTest::TestSuite {
public:
	void test_window_own_show_rect_matches() {
		const uint32 tok = 0x40000003u;
		Common::Array<UiElement> elems;
		UiElement w; w.type = kUiWindow; w.nativeRect = Common::Rect(65, 82, 255, 128); w.token = tok;
		elems.push_back(w);
		// bitsShow byte-aligns left/right (65..255 -> 64..256); still the window body.
		TS_ASSERT(Roger::regionIsCapturedWindowBody(elems, tok, Common::Rect(64, 82, 256, 128), 90));
	}
	void test_icon_inside_window_is_not_body() {
		const uint32 tok = 0x40000003u;
		Common::Array<UiElement> elems;
		UiElement w; w.type = kUiWindow; w.nativeRect = Common::Rect(29, 59, 290, 142); w.token = tok;
		elems.push_back(w);
		// The death-dialog corpse icon: fully inside the window but covers little of it.
		TS_ASSERT(!Roger::regionIsCapturedWindowBody(elems, tok, Common::Rect(34, 74, 96, 119), 90));
	}
	void test_other_token_does_not_match() {
		Common::Array<UiElement> elems;
		UiElement w; w.type = kUiWindow; w.nativeRect = Common::Rect(65, 82, 255, 128); w.token = 0x40000003u;
		elems.push_back(w);
		TS_ASSERT(!Roger::regionIsCapturedWindowBody(elems, 0x40000004u, Common::Rect(64, 82, 256, 128), 90));
	}
	void test_ownerless_region_never_matches() {
		Common::Array<UiElement> elems;
		UiElement w; w.type = kUiWindow; w.nativeRect = Common::Rect(65, 82, 255, 128); w.token = 0x40000003u;
		elems.push_back(w);
		TS_ASSERT(!Roger::regionIsCapturedWindowBody(elems, 0, Common::Rect(64, 82, 256, 128), 90));
	}
	void test_non_window_element_does_not_match() {
		const uint32 tok = 0x40000003u;
		Common::Array<UiElement> elems;
		UiElement t; t.type = kUiText; t.nativeRect = Common::Rect(65, 82, 255, 128); t.token = tok;
		elems.push_back(t);
		TS_ASSERT(!Roger::regionIsCapturedWindowBody(elems, tok, Common::Rect(64, 82, 256, 128), 90));
	}
};
