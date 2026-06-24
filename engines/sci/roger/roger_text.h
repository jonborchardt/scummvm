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

#ifndef SCI_ROGER_ROGER_TEXT_H
#define SCI_ROGER_ROGER_TEXT_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_ui_layer.h"

namespace Graphics { class Font; class ManagedSurface; }

namespace Sci {
namespace Roger {

// Index of the largest font (fonts ascending by size) whose rendering of `text`
// fits (boxW,boxH). 0 if none fit; -1 if fonts is empty. Pure: unit-testable.
int fitFontIndex(const Common::Array<const Graphics::Font *> &fonts,
                 const Common::String &text, int boxW, int boxH);

// Index of the largest font (ascending by size) no taller than maxH. 0 if none
// fit (smallest); -1 if fonts is empty. Pure: unit-testable. This drives Roger's
// role-based type scale — text is sized by a target on-screen cell HEIGHT, not by
// the (wildly varying) width/height of each element's native rect, so a dialog
// prompt and its input field render at one consistent size.
int fitFontIndexByHeight(const Common::Array<const Graphics::Font *> &fonts, int maxH);

// Keep only printable-ASCII characters (0x20..0x7e); drop the rest. The TTF UI
// fonts lack SCI's stylized/high-bit glyphs (e.g. the "III" title glyph), which
// would otherwise render as tofu boxes on the now-opaque status strip. A no-op for
// ordinary ASCII text (the score banner). Pure: unit-testable.
Common::String stripUnrenderable(const Common::String &s);

// Y of the first text line inside a box. Centred vertically by default (matches
// dialogs/buttons); when vAlignTop is set the text starts at the top of the box
// (matches SCI's native top-aligned text-edit fields). Pure: unit-testable.
int firstLineTop(int top, int boxH, int lineCount, int lineH, bool vAlignTop);

class RogerTextRenderer {
public:
	// ttfName: a TTF inside ScummVM's fonts.dat (e.g. "LiberationSans-Regular.ttf");
	// empty or load failure => built-in FontMan fonts. sizes: pixel sizes (ascending).
	RogerTextRenderer(const Common::String &ttfName, const Common::Array<int> &sizes);
	~RogerTextRenderer();

	bool ok() const { return !_fonts.empty(); }
	// True only if the requested TTF actually loaded (false => bitmap fallback).
	bool ttfLoaded() const { return _ttfLoaded; }
	// Global size multiplier (percent) applied to every target height — the user's
	// roger_ui_font_scale knob. 100 = use the role's target height as-is.
	void setGlobalScale(int pct) { _globalScalePct = pct > 0 ? pct : 100; }
	// Draw word-wrapped, vertically-centred text. The font is chosen by `targetPx`
	// (an on-screen cell height in dest pixels), scaled by the global multiplier and
	// then capped to rect.height() so tight strips/rows shrink to fit rather than
	// overlapping. targetPx <= 0 => fill the box height.
	void drawPx(Graphics::ManagedSurface &dst, const Common::String &text,
	            const Common::Rect &rect, uint32 color, int align, int targetPx,
	            bool vAlignTop = false, const Common::Array<UiGlyph> *glyphs = nullptr) const;
	int caretPx(const Common::String &text, int cursorPos,
	            const Common::Rect &rect, int targetPx) const;

private:
	// Pick the font for `rect` at the given target cell height (with global scale +
	// box-height cap applied). nullptr only if no fonts at all.
	const Graphics::Font *fontForBox(const Common::Rect &rect, int targetPx) const;

	Common::Array<const Graphics::Font *> _fonts; // ascending by size
	Common::Array<bool> _owned;                   // parallel: delete on dtor?
	bool _ttfLoaded = false;                       // requested TTF loaded (not bitmap fallback)?
	int _globalScalePct = 100;                     // user size multiplier (roger_ui_font_scale)
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_TEXT_H
