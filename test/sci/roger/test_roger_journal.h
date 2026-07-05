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
#include "sci/roger/roger_journal.h"

using namespace Sci::Roger;

static UiElement op(UiElementType t, int l, int tp, int r, int b, const char *txt = "") {
	UiElement e; e.type = t; e.nativeRect = Common::Rect(l, tp, r, b); e.text = txt; return e;
}

class TestRogerJournal : public CxxTest::TestSuite {
public:
	void test_append_preserves_draw_order() {
		RogerJournal j;
		j.append(op(kUiText, 0, 0, 10, 10, "a"));
		j.append(op(kUiText, 20, 0, 30, 10, "b"));
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("a"));
		TS_ASSERT_EQUALS(j.ops()[1].text, Common::String("b"));
	}

	void test_containing_same_type_redraw_supersedes_and_moves_to_end() {
		// Native is immediate-mode: a redraw at (or over) the same box replaces what
		// was there AND is now on top of any overlapping neighbor. Both properties
		// were point-fixed in d8be4749b4e; the journal owns them structurally.
		RogerJournal j;
		j.append(op(kUiText, 0, 0, 10, 10, "25"));
		j.append(op(kUiIcon, 0, 9, 10, 19));           // overlapping neighbor
		j.append(op(kUiText, 0, 0, 10, 10, "30"));     // value redraw, same box
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
		TS_ASSERT_EQUALS(j.ops()[0].type, kUiIcon);
		TS_ASSERT_EQUALS(j.ops()[1].text, Common::String("30")); // last draw on top
	}

	void test_supersede_requires_same_type_and_containment() {
		RogerJournal j;
		j.append(op(kUiText, 0, 0, 10, 10, "under"));
		j.append(op(kUiIcon, 0, 0, 10, 10));           // different type: no supersede
		j.append(op(kUiText, 2, 2, 8, 8, "partial"));  // contained does NOT kill container
		TS_ASSERT_EQUALS(j.ops().size(), 3u);
	}

	void test_clear_token_removes_only_exact_token_singletons() {
		RogerJournal j;
		UiElement fb = op(kUiWindow, 5, 5, 50, 20); fb.token = 0x70000000u;
		j.append(fb);
		j.append(op(kUiText, 0, 0, 10, 10, "keep"));
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.clearToken(0x70000000u, &removed));
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(removed.size(), 1u);
		TS_ASSERT(!j.clearToken(0x70000000u, &removed)); // idempotent, reports no removal
	}
};
