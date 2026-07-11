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
#include "sci/roger/overlay/roger_journal.h"

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
		// are owned structurally by the journal (append-only, opSupersedes retires in place).
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

	void test_generic_text_does_not_supersede_control_at_same_rect() {
		// The dialog-text-size-flip bug (this fix): a QFG1 Print/look narration is drawn
		// by BOTH the semantic control hook (kControlText -> onText source=control, token 0x40000003,
		// accurate native font height + single-line width cap) AND the generic GfxText16::Box
		// hook (onText source=box, token 0x60000003, no width cap -> wrap-fit). They are the SAME
		// native draw seen by two hooks -- reconciled by dedupeGenericText (keeps the control).
		// append()'s geometry-only supersede must NOT let the later generic copy replace the
		// control copy: doing so left the multi-line wrap-fit generic in the journal, which
		// rendered too small once a later present re-fitted it (large-first -> small-settled).
		RogerJournal j;
		UiElement ctrl = op(kUiText, 10, 26, 310, 86, "look narration");
		ctrl.token = 0x40000003u; ctrl.nativeFontH = 8; ctrl.nativeTextW = 1356;
		UiElement gen = op(kUiText, 10, 26, 310, 86, "look narration");
		gen.token = 0x60000003u; gen.nativeFontH = 12; gen.nativeTextW = 0;
		j.append(ctrl);
		j.append(gen);
		// Both survive append (no cross-namespace supersede); dedupe then keeps the control.
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
		j.dedupeGenericText(0x60000000u);
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].token, 0x40000003u);
	}

	void test_same_namespace_value_redraw_still_supersedes() {
		// Guard: the fix must NOT break the char-sheet stat-value refresh -- a generic
		// redraw at the same box within the SAME generic namespace still supersedes.
		RogerJournal j;
		UiElement a = op(kUiText, 170, 45, 192, 57, "25"); a.token = 0x60000003u;
		UiElement b = op(kUiText, 170, 45, 192, 57, "30"); b.token = 0x60000003u;
		j.append(a);
		j.append(b);
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("30"));
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
		// (Geometry + bracket stack, never the current-port id -- prevents the
		// 0x60000002 vs 0x60000003 token-doubling class.)
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
		// The char-sheet-popup trap: geometry may only remove on strict
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

	void test_rollback_removes_only_ops_after_checkpoint_inside_rect() {
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 60, 22, "before save"));      // predates checkpoint
		j.checkpoint(0x00170ab0u, Common::Rect(0, 0, 100, 100));
		j.append(op(kUiText, 10, 30, 60, 42, "dialog text"));      // drawn after save, inside
		j.append(op(kUiText, 150, 30, 200, 42, "outside rect"));   // after save, outside
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.rollback(0x00170ab0u, Common::Rect(0, 0, 100, 100), &removed));
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("before save"));
		TS_ASSERT_EQUALS(j.ops()[1].text, Common::String("outside rect"));
		TS_ASSERT_EQUALS(removed.size(), 1u);
	}

	void test_rollback_unknown_handle_reports_false() {
		// Caller falls back to plain erase-containment semantics on false.
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 60, 22, "x"));
		TS_ASSERT(!j.rollback(0xdeadbeefu, Common::Rect(0, 0, 100, 100), nullptr));
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
	}

	void test_checkpoint_is_consumed_by_rollback_and_by_drop() {
		// Contract: rollback returns true iff the handle HAD a checkpoint (even when
		// zero ops are removed) -- the caller uses the return value to decide whether
		// to fall back to plain eraseContained semantics. Both rollback and
		// dropCheckpoint consume the checkpoint.
		RogerJournal j;
		j.checkpoint(0x00170ab0u, Common::Rect(0, 0, 50, 50));
		j.dropCheckpoint(0x00170ab0u);
		TS_ASSERT(!j.rollback(0x00170ab0u, Common::Rect(0, 0, 50, 50), nullptr)); // dropped
		j.checkpoint(0x00170ab0u, Common::Rect(0, 0, 50, 50));
		TS_ASSERT(j.rollback(0x00170ab0u, Common::Rect(0, 0, 50, 50), nullptr));  // known: true, no removals
		TS_ASSERT(!j.rollback(0x00170ab0u, Common::Rect(0, 0, 50, 50), nullptr)); // consumed
	}

	void test_rollback_spares_persistent_status_and_frame_box_singletons() {
		// Menu-close regression (this fix): closing the game menu restores the
		// menu-bar save-under. The status banner (token 0x10000000) is redrawn into
		// the top strip while the menu is open, so it postdates the bar's checkpoint
		// and lies inside the restored strip -- a plain rollback dropped it, reverting
		// the enhanced TTF banner to the native bitmap font. Its lifetime is owned by
		// its token (reapply / clearToken), never by a save-under, so rollback must
		// spare it. Same for the overlay-only frame box (0x70000000). The menu dropdown
		// (0x20000000) is NOT spared -- its own restore is what must remove it.
		RogerJournal j;
		j.checkpoint(0x00170ab0u, Common::Rect(0, 0, 320, 10)); // bar save-under
		UiElement banner = op(kUiWindow, 0, 0, 320, 10); banner.token = 0x10000000u;
		j.append(banner);                                       // status/menu-bar strip
		UiElement fb = op(kUiWindow, 5, 20, 60, 40); fb.token = 0x70000000u;
		fb.nativeRect = Common::Rect(2, 2, 40, 8);              // inside the restored rect
		j.append(fb);
		UiElement drop = op(kUiWindow, 6, 2, 140, 9); drop.token = 0x20000000u;
		j.append(drop);                                         // dropdown: must be rolled back
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.rollback(0x00170ab0u, Common::Rect(0, 0, 320, 10), &removed));
		// Banner + frame box survive; only the dropdown was removed.
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
		TS_ASSERT_EQUALS(j.ops()[0].token, 0x10000000u);
		TS_ASSERT_EQUALS(j.ops()[1].token, 0x70000000u);
		TS_ASSERT_EQUALS(removed.size(), 1u);
	}

	void test_popup_over_chars_sheet_rolls_back_cleanly() {
		// Regression guard: a popup over the char sheet must not wipe
		// the stat text beneath it. With checkpoints this is exact: the stats predate
		// the popup's save, so rollback reveals them untouched.
		RogerJournal j;
		j.append(op(kUiText, 83, 45, 132, 57, "Strength"));
		j.checkpoint(0x00181111u, Common::Rect(60, 30, 260, 140));  // popup saves under itself
		j.append(op(kUiText, 70, 70, 120, 82, "popup body"));
		j.rollback(0x00181111u, Common::Rect(60, 30, 260, 140), nullptr);
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("Strength"));
	}

	void test_erase_contained_can_spare_saveunder_singletons() {
		// The unknown-handle restore fallback must not drop the status banner /
		// frame box (nothing redraws them afterwards); the default erase (the
		// kernelGraphRedrawBox path) still removes them (SCI actively repaints
		// that region and re-pushes the banner).
		RogerJournal j;
		UiElement banner = op(kUiText, 0, 0, 320, 10, "banner"); banner.token = 0x10000000u;
		j.append(banner);
		j.append(op(kUiText, 5, 2, 100, 9, "menu title"));
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.eraseContained(Common::Rect(0, 0, 320, 10), &removed, true)); // spare
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].token, 0x10000000u);
		TS_ASSERT_EQUALS(removed.size(), 1u);
		TS_ASSERT(j.eraseContained(Common::Rect(0, 0, 320, 10), nullptr)); // default: removes it
		TS_ASSERT_EQUALS(j.ops().size(), 0u);
	}

	void test_supersede_blocked_across_different_windows() {
		// Contract: a larger window-B op must not swallow window-A's still-open op
		// of the same type. A's pixels return via A's save-under when B closes; A's
		// ops die with A's bracket, not with B's overprint.
		RogerJournal j;
		j.openBracket(3, Common::Rect(40, 40, 200, 120));
		UiElement a = op(kUiText, 50, 50, 120, 62, "window A text"); a.token = 0x40000003u;
		j.append(a);
		j.openBracket(5, Common::Rect(20, 20, 300, 180)); // larger window over it
		UiElement b = op(kUiText, 45, 45, 260, 70, "window B text"); b.token = 0x40000005u;
		j.append(b); // same type, contains A's rect -- must NOT supersede
		TS_ASSERT_EQUALS(j.ops().size(), 2u); // A text + B text, nothing removed
		Common::Array<Common::Rect> removed;
		TS_ASSERT(j.closeBracket(5, &removed));
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("window A text"));
	}

	void test_supersede_still_works_within_same_window() {
		RogerJournal j;
		j.openBracket(3, Common::Rect(0, 9, 321, 200));
		j.append(op(kUiText, 170, 45, 192, 57, "25"));
		j.append(op(kUiText, 170, 45, 192, 57, "30")); // same window, same box: refresh
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("30"));
	}

	void test_window_op_does_not_erase_picture_port_ops() {
		// Pre-window picture-port content (windowId 0) must survive a covering
		// window op (windowId N): it is under the window in paint order and is
		// revealed again when the window's save-under restores.
		RogerJournal j;
		j.append(op(kUiText, 100, 100, 200, 112, "pic port label"));
		j.openBracket(7, Common::Rect(80, 80, 240, 160));
		UiElement w = op(kUiWindow, 80, 80, 240, 160); w.windowId = 7; w.backColor = 15;
		j.append(w);
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
		TS_ASSERT_EQUALS(j.ops()[0].text, Common::String("pic port label")); // still first (painted under)
	}

	void test_append_preserves_preset_window_id() {
		// onWindowOpen pre-tags the window's own box op with its id; a surrounding
		// open bracket must not re-tag it.
		RogerJournal j;
		j.openBracket(3, Common::Rect(0, 0, 320, 200));
		UiElement e = op(kUiWindow, 60, 60, 260, 140); e.windowId = 5;
		j.append(e);
		TS_ASSERT_EQUALS(j.ops()[0].windowId, 5u);
	}

	void test_clear_resets_open_brackets() {
		// Room change calls clear(); a stale bracket must not tag the new room's ops.
		RogerJournal j;
		j.openBracket(3, Common::Rect(0, 0, 320, 200));
		j.clear();
		j.append(op(kUiText, 10, 10, 60, 22, "fresh room"));
		TS_ASSERT_EQUALS(j.ops()[0].windowId, 0u);
	}

	void test_op_is_opaque_button_and_edit_arms() {
		UiElement b = op(kUiButton, 0, 0, 10, 10); b.backColor = 15;
		UiElement e = op(kUiTextEdit, 0, 0, 10, 10); e.backColor = -1;
		UiElement t = op(kUiText, 0, 0, 10, 10); t.backColor = 15;
		TS_ASSERT(opIsOpaque(b));   // filled button hides what it covers
		TS_ASSERT(!opIsOpaque(e));  // unfilled edit does not
		TS_ASSERT(!opIsOpaque(t));  // text never does, regardless of backColor
	}

	void test_prune_drops_multiple_buried_ops() {
		RogerJournal j;
		j.append(op(kUiText, 10, 10, 40, 22, "one"));
		j.append(op(kUiText, 10, 30, 40, 42, "two"));
		UiElement win = op(kUiWindow, 0, 0, 100, 100); win.backColor = 15;
		j.append(win);
		j.prune();
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].type, kUiWindow);
	}
};
