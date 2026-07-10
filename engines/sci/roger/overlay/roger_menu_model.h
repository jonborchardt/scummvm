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

#ifndef SCI_ROGER_OVERLAY_ROGER_MENU_MODEL_H
#define SCI_ROGER_OVERLAY_ROGER_MENU_MODEL_H

// Observer-side retained menu state. SCI-free / GUI-free / ConfMan-free —
// unit-tested in test/sci/roger/test_menu_model.h. This is the home of the four
// state members exiled from GfxMenu (FORK_AUDIT M3): the bar titles, the dropdown
// rows, the dropdown box, and the highlight. The provider feeds it the neutral L3
// menu event stream (onText(menuBar)/onWindowOpen(dropdown)/onText(menuRow)/
// onMenuHighlight/onWindowClose) and re-emits the overlay from this state — exactly
// reproducing the pre-exile GfxMenu bar/dropdown overlay-push behavior.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

struct MenuBarTitle {
	Common::Rect rect;
	Common::String text;
	bool isText;      // printable-ASCII: renderable with the TTF header font (else native)
	int nativeFontH;  // SCI font cell height at draw time (0 = unknown)
	int nativeTextW;  // native single-line string width (0 = unknown)
};

struct MenuRow {
	Common::Rect rect;
	Common::String text;
	uint16 id;
	int nativeFontH;  // SCI font cell height at draw time (0 = unknown)
	int nativeTextW;  // native single-line string width (0 = unknown)
	bool selected(uint16 highlight) const { return id == highlight; }
};

class MenuModel {
public:
	// --- bar strip (token kGfxTokenStatus, mutually exclusive with the banner) ---
	void beginBar() { _barTitles.clear(); }
	void addBarTitle(const Common::Rect &rect, const Common::String &text,
	                 int nativeFontH = 0, int nativeTextW = 0) {
		MenuBarTitle t; t.rect = rect; t.text = text; t.isText = titleIsText(text);
		t.nativeFontH = nativeFontH; t.nativeTextW = nativeTextW;
		_barTitles.push_back(t);
	}
	void endBar() {}   // bar is complete; the provider re-emits from _barTitles

	// --- dropdown (token kGfxTokenMenuDropdown) ---
	void openDropdown(const Common::Rect &box) { _box = box; _rows.clear(); _highlight = 0; }
	void addRow(const Common::Rect &rect, const Common::String &text, uint16 id,
	            int nativeFontH = 0, int nativeTextW = 0) {
		MenuRow r; r.rect = rect; r.text = text; r.id = id;
		r.nativeFontH = nativeFontH; r.nativeTextW = nativeTextW;
		_rows.push_back(r);
	}
	void closeDropdown() { _rows.clear(); }

	// Returns true only when the highlight actually changed (the dedup exiled
	// from GfxMenu::invertMenuSelection — the present-storm guard). itemId == 0
	// is the old-row re-invert no-op.
	bool setHighlight(uint16 itemId) {
		if (itemId == 0 || itemId == _highlight)
			return false;
		_highlight = itemId;
		return true;
	}

	const Common::Array<MenuBarTitle> &barTitles() const { return _barTitles; }
	const Common::Array<MenuRow> &rows() const { return _rows; }
	const Common::Rect &box() const { return _box; }
	uint16 highlight() const { return _highlight; }

	// A menu-bar title is renderable by the TTF header font only if every char is
	// printable ASCII (the leftmost SQ3 menu title is a graphical "Sierra" glyph the
	// TTF lacks). Exiled verbatim from menu.cpp's title classifier.
	static bool titleIsText(const Common::String &s) {
		if (s.empty())
			return false;
		for (uint i = 0; i < s.size(); i++) {
			const byte c = (byte)s[i];
			if (c < 0x20 || c >= 0x7f)
				return false;
		}
		return true;
	}

private:
	Common::Array<MenuBarTitle> _barTitles;
	Common::Array<MenuRow> _rows;
	Common::Rect _box;
	uint16 _highlight = 0;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_OVERLAY_ROGER_MENU_MODEL_H
