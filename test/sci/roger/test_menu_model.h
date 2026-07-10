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
#include "sci/roger/overlay/roger_menu_model.h"

using namespace Sci::Roger;

class TestMenuModel : public CxxTest::TestSuite {
public:
	// Bar: begin/add/add/end rebuilds the title array; the titleIsText policy
	// (printable-ASCII only) lives on the model so graphical-glyph titles are
	// classified for the renderer.
	void test_bar_collects_titles_and_classifies_glyphs() {
		MenuModel m;
		m.beginBar();
		m.addBarTitle(Common::Rect(8, 0, 40, 10), "File");
		m.addBarTitle(Common::Rect(40, 0, 70, 10), "Game");
		m.endBar();
		TS_ASSERT_EQUALS(m.barTitles().size(), 2u);
		TS_ASSERT_EQUALS(m.barTitles()[0].text, Common::String("File"));
		TS_ASSERT(m.barTitles()[0].isText);   // printable ASCII
	}
	void test_bar_graphical_glyph_title_not_text() {
		MenuModel m;
		m.beginBar();
		m.addBarTitle(Common::Rect(0, 0, 12, 10), Common::String("\x01")); // Sierra glyph (high/control byte)
		m.endBar();
		TS_ASSERT_EQUALS(m.barTitles().size(), 1u);
		TS_ASSERT(!m.barTitles()[0].isText);   // graphical glyph -> leave native bar
	}
	void test_beginbar_clears_previous_titles() {
		MenuModel m;
		m.beginBar(); m.addBarTitle(Common::Rect(0,0,10,10), "A"); m.endBar();
		m.beginBar(); m.addBarTitle(Common::Rect(0,0,10,10), "B"); m.endBar();
		TS_ASSERT_EQUALS(m.barTitles().size(), 1u);
		TS_ASSERT_EQUALS(m.barTitles()[0].text, Common::String("B"));
	}
	// Dropdown: open resets rows + box; addRow appends; the box rect is retained.
	void test_dropdown_open_resets_and_stores_box() {
		MenuModel m;
		m.openDropdown(Common::Rect(10, 12, 120, 90));
		m.addRow(Common::Rect(11, 13, 119, 22), "Restore", 1);
		m.addRow(Common::Rect(11, 22, 119, 31), "Save", 2);
		TS_ASSERT_EQUALS(m.box(), Common::Rect(10, 12, 120, 90));
		TS_ASSERT_EQUALS(m.rows().size(), 2u);
		TS_ASSERT_EQUALS(m.rows()[1].id, (uint16)2);
		// Re-open clears the previous rows (drawMenu re-entry).
		m.openDropdown(Common::Rect(0, 12, 50, 40));
		TS_ASSERT_EQUALS(m.rows().size(), 0u);
	}
	// Highlight dedup (the guard exiled from invertMenuSelection): a repeat of the
	// current highlight (or the itemId==0 no-op) does NOT report a change, so the
	// caller skips the re-push (present-storm guard).
	void test_highlight_change_dedup() {
		MenuModel m;
		m.openDropdown(Common::Rect(0, 0, 50, 40));
		m.addRow(Common::Rect(0, 0, 50, 10), "One", 1);
		m.addRow(Common::Rect(0, 10, 50, 20), "Two", 2);
		TS_ASSERT(m.setHighlight(1));   // 0 -> 1: changed
		TS_ASSERT(!m.setHighlight(1));  // 1 -> 1: no change
		TS_ASSERT(m.setHighlight(2));   // 1 -> 2: changed
		TS_ASSERT_EQUALS(m.highlight(), (uint16)2);
	}
	void test_highlight_zero_is_noop() {
		MenuModel m;
		m.openDropdown(Common::Rect(0, 0, 50, 40));
		m.setHighlight(1);
		TS_ASSERT(!m.setHighlight(0)); // itemId==0 (old-row re-invert) never re-pushes
		TS_ASSERT_EQUALS(m.highlight(), (uint16)1);
	}
	// A selected row is reported inverted (white pen on black back); the model owns
	// the pen/back selection so the renderer stays mechanical.
	void test_row_selection_inversion() {
		MenuModel m;
		m.openDropdown(Common::Rect(0, 0, 50, 40));
		m.addRow(Common::Rect(0, 0, 50, 10), "One", 1);
		m.addRow(Common::Rect(0, 10, 50, 20), "Two", 2);
		m.setHighlight(2);
		// pen/back for row 0 (unselected) vs row 1 (selected).
		TS_ASSERT(!m.rows()[0].selected(m.highlight()));
		TS_ASSERT(m.rows()[1].selected(m.highlight()));
	}
	// closeDropdown drops the rows (dispose) but leaves bar titles intact (they are
	// independent lifetimes — the bar strip survives a dropdown close).
	void test_close_dropdown_keeps_bar() {
		MenuModel m;
		m.beginBar(); m.addBarTitle(Common::Rect(0,0,10,10), "File"); m.endBar();
		m.openDropdown(Common::Rect(0, 0, 50, 40));
		m.addRow(Common::Rect(0, 0, 50, 10), "One", 1);
		m.closeDropdown();
		TS_ASSERT_EQUALS(m.rows().size(), 0u);
		TS_ASSERT_EQUALS(m.barTitles().size(), 1u);
	}
};
