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

	void test_ops_inside_open_bracket_die_with_it() {
		RogerJournal j;
		j.append(op(kUiText, 5, 100, 60, 112, "on picture port")); // before bracket
		j.openBracket(3, Common::Rect(0, 9, 321, 200));
		j.append(op(kUiText, 83, 45, 132, 57, "Strength"));        // inside window 3
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.closeBracket(3, &removed));
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("on picture port"));
		TS_ASSERT_EQUALS(removed.size(), 1u);
	}

	void test_bracket_assignment_ignores_draw_time_identity() {
		// The QFG1 char sheet draws under port 3 AND port 2 while window 3 is the
		// only open bracket over the rect: both draws must belong to window 3.
		// (Geometry + bracket stack, never the current-port id — the 0x60000002
		// vs 0x60000003 doubling class from d8be4749b4e.)
		RogerJournal j;
		j.openBracket(3, Common::Rect(0, 9, 321, 200));
		UiElement a = op(kUiText, 170, 45, 192, 57, "25"); a.token = 0x60000003u;
		UiElement b = op(kUiText, 103, 158, 163, 170, "20 / 20"); b.token = 0x60000002u;
		j.append(a);
		j.append(b);
		TS_ASSERT_EQUALS(j.ops()[0].windowId, 3u);
		TS_ASSERT_EQUALS(j.ops()[1].windowId, 3u);
	}

	void test_innermost_bracket_wins() {
		RogerJournal j;
		j.openBracket(3, Common::Rect(0, 9, 321, 200));   // full-screen window
		j.openBracket(5, Common::Rect(60, 60, 260, 140)); // popup over it
		j.append(op(kUiText, 70, 70, 120, 82, "popup text"));
		Common::Array<Common::Rect> removed;
		j.closeBracket(5, &removed);
		TS_ASSERT_EQUALS(j.ops().size(), 0u); // popup text died with the popup,
		TS_ASSERT(j.closeBracket(3, nullptr) == false); // nothing left for window 3
	}

	void test_erase_contained_removes_any_op_kind() {
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 40, 22, "text"));
		UiElement ic = op(kUiIcon, 10, 30, 40, 42); j.append(ic);
		j.append(op(kUiButton, 10, 50, 40, 62, "btn"));
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.eraseContained(Common::Rect(0, 0, 50, 45), &removed)); // covers text+icon
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].type, kUiButton);
		TS_ASSERT_EQUALS(removed.size(), 2u);
	}

	void test_partial_overlap_never_erases() {
		// The char-sheet-popup trap (CLAUDE.md): geometry may only remove on strict
		// containment. Partial coverage keeps the op.
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 40, 22, "stats"));
		TS_ASSERT(!j.eraseContained(Common::Rect(0, 0, 25, 45), nullptr));
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
	}

	void test_prune_drops_ops_covered_by_later_opaque_op() {
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 40, 22, "buried"));
		UiElement win = op(kUiWindow, 0, 0, 100, 100); win.backColor = 15; // opaque fill
		j.append(win);
		j.prune();
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].type, kUiWindow);
	}

	void test_prune_keeps_ops_over_transparent_cover() {
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 40, 22, "visible"));
		UiElement fb = op(kUiWindow, 0, 0, 100, 100); fb.backColor = -1; // frame box: no fill
		j.append(fb);
		j.prune();
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
	}

	void test_append_stays_bounded_under_redraw_storm() {
		// One hour on the char sheet must not grow the journal unboundedly: an
		// opaque window redraw covering everything triggers threshold pruning.
		RogerJournal j;
		UiElement win = op(kUiWindow, 0, 0, 320, 200); win.backColor = 15;
		for (int i = 0; i < 4000; i++) {
			j.append(win);
			UiElement t = op(kUiText, 10 + (i % 7), 10, 60 + (i % 7), 22, "churn");
			j.append(t); // rect varies: containment supersede alone cannot cap it
		}
		TS_ASSERT_LESS_THAN_EQUALS(j.ops().size(), 300u);
	}
};
