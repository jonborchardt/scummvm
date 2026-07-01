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

#ifndef SCI_ROGER_ROGER_UI_LAYER_H
#define SCI_ROGER_ROGER_UI_LAYER_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Graphics { struct Surface; }

namespace Sci {
namespace Roger {

enum UiElementType { kUiWindow, kUiText, kUiButton, kUiTextEdit, kUiIcon };

// Type-scale role: the compositor turns this into a target on-screen cell height
// (one "body" size for dialog/message/input/list/button text, one larger "heading"
// size for the score banner + menu titles), so sizes stay consistent across
// elements instead of tracking each one's tiny native rect.
enum UiTextRole { kRoleBody = 0, kRoleHeading = 1 };

// A non-ASCII glyph the TTF font can't render, pre-rendered from the game's own SCI
// font (borrowed surface, owned elsewhere). The renderer blits it inline between TTF
// runs, scaled to the line height. `ch` is the raw byte (0..255) it stands in for.
struct UiGlyph {
	uint16 ch;
	const Graphics::Surface *surf;
};

// Resolution-independent UI element. Rects are in global 320x200 screen space
// (the same space sprite celRects use), so the compositor maps them through the
// same game-rect placement as the plate. No SCI engine types here.
struct UiElement {
	UiElementType type;
	Common::Rect  nativeRect;
	Common::String text;
	int  backColor;   // SCI palette index for fill (-1 = no fill)
	int  penColor;    // SCI palette index for text/border
	int  fontId;      // SCI font id (informational; TTF size is chosen by fit)
	int  style;       // SCI_CONTROLS_STYLE_* bits (enabled/disabled/selected)
	int  align;       // SCI_TEXT16_ALIGNMENT_* (-1 right, 0 left, 1 center)
	int  cursorPos;   // kUiTextEdit caret char index
	bool hasFrame;
	const Graphics::Surface *iconSurface; // kUiIcon: borrowed RGBA cel, not owned
	Common::Array<UiGlyph> glyphs; // non-ASCII glyphs in `text`, rendered from the game font
	uint32 token;     // clear-token (window id or save-under handle)
	int  textRole;    // UiTextRole: body vs heading target size
	bool useAltFont;   // render with the header/menu font instead of the dialog font
	bool vAlignTop;    // draw text from the top of the box (SCI native text-edit position)
	int  nativeFontH;  // SCI font cell height (px, 320x200 space); 0 = unknown -> role/box fallback
	int  nativeTextW;  // native single-line string width (px); 0 = multi-line/unknown -> no width cap

	UiElement() : type(kUiText), backColor(-1), penColor(0), fontId(0), style(0),
		align(0), cursorPos(0), hasFrame(false), iconSurface(nullptr), token(0),
		textRole(kRoleBody), useAltFont(false), vAlignTop(false),
		nativeFontH(0), nativeTextW(0) {}
};

// Forward-declare the pure helper from roger_compositor.h so RogerUiLayer can
// expose a thin forwarder without a circular include (roger_compositor.h already
// includes this file, so we cannot include it here in return).
void dedupeGenericTextElements(Common::Array<UiElement>&, uint32);

class RogerUiLayer {
public:
	// Append, or replace an existing element with the same type+token+rect.
	void push(const UiElement &e);
	// Remove every element carrying this clear-token. Returns true if any element was removed.
	// If `removedNativeRects` is non-null, the nativeRect of each removed element is appended
	// to it so the caller can dirty the vacated overlay regions (else stale pixels linger).
	bool clearToken(uint32 token, Common::Array<Common::Rect> *removedNativeRects = nullptr);
	void clearAll() { _elems.clear(); }
	bool empty() const { return _elems.empty(); }
	const Common::Array<UiElement> &elements() const { return _elems; }
	// Drop each generic-token element whose rect is already covered by a non-generic
	// text-rendering element (kUiText/kUiButton/kUiTextEdit). Forwards to the pure helper.
	void dedupeGenericText(uint32 genericToken) { dedupeGenericTextElements(_elems, genericToken); }

private:
	Common::Array<UiElement> _elems;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_UI_LAYER_H
