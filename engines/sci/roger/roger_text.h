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

// Index of the largest font (ascending) whose cell height <= maxH AND whose
// rendering of `text` is no wider than maxW. maxW <= 0 => width unbounded. 0 if
// none fit (smallest); -1 if fonts is empty. Pure: unit-testable. This drives the
// native-metric type scale: maxH comes from the SCI font's cell height (scaled to
// the overlay) and maxW from the native string width, so the crisp text lands in
// the same footprint the original occupied.
int fitFontIndexByHeightAndWidth(const Common::Array<const Graphics::Font *> &fonts,
                                 const Common::String &text, int maxH, int maxW);

// Target on-screen cell height in dest pixels for a native cell height: scale the
// native (320x200) height by the overlay game-area height, then by the user's
// global percent. Returns 0 when nativeFontH <= 0 (caller falls back). Pure.
int rogerTargetPx(int nativeFontH, int overlayH, int globalScalePct);

// Y of the first text line inside a box. Centred vertically by default (matches
// dialogs/buttons); when vAlignTop is set the text starts at the top of the box
// (matches SCI's native top-aligned text-edit fields). Pure: unit-testable.
int firstLineTop(int top, int boxH, int lineCount, int lineH, bool vAlignTop);

// Cell-top Y that OPTICALLY centres a text block in [top, top+boxH): centres the
// ink span, not the font cell. inkTop/inkBottom are the first/last line's drawn
// extent relative to their cell top (Font::getBoundingBox); TTF cells carry
// internal leading above the ink, so cell centring sits label text visibly low.
// Degenerate ink (inkBottom <= inkTop) falls back to cell centring. Pure.
int opticalBlockTop(int top, int boxH, int lineCount, int lineH, int inkTop, int inkBottom);

// One text element's sizing inputs/outputs for the shared type-scale pass.
struct TextSizeFit {
	uint32 group;  // sizing group (window id + font namespace); scale is shared per group
	int idealPx;   // desired cell height (dest px, global multiplier applied); 0 = not text
	int fitPx;     // in: largest height <= idealPx that fits THIS element's own caps
	               // out: final render height after the group's shared scale
	TextSizeFit() : group(0), idealPx(0), fitPx(0) {}
};

// Sizing-group key for one text element. Groups share a scale in applySharedGroupScale:
// - multi-line (wrap-fit) elements are singletons keyed by element index: their squeeze
//   is local (TTF wrap metrics) and must never drag sibling single-line text down;
// - generic text (0x6 namespace) groups per SCREEN, ignoring the port id in the token —
//   the same screen draws under different current ports (QFG1 char sheet: labels in the
//   window port, stat redraws in the picture port) and must not change size across that;
// - everything else (controls) groups per window id (token low bits);
// - the alt/header font is always a separate group from the body font. Pure.
uint32 textScaleGroup(uint32 token, bool useAltFont, bool multiLine, uint elemIndex);

// Shared proportional type scale: every element in a group shrinks TOGETHER by the
// group's worst (smallest) fit/ideal ratio, so siblings keep the size relationship
// the game gave them (a 2x heading and 1x label become 1.8x/0.9x, never one unified
// size) while the tightest element still fits its box. Entries with idealPx <= 0 are
// ignored (left untouched, excluded from the ratio). Pure: unit-testable.
void applySharedGroupScale(Common::Array<TextSizeFit> &items);

class RogerTextRenderer {
public:
	// ttfName: a TTF inside ScummVM's fonts.dat (e.g. "GoMono-Regular.ttf"); empty or
	// load failure => built-in FontMan bitmap fonts. TTF sizes are loaded on demand at
	// the exact requested cell height and cached — there is no fixed size ladder.
	explicit RogerTextRenderer(const Common::String &ttfName);
	~RogerTextRenderer();

	bool ok() const { return _ttfLoaded || !_fonts.empty(); }
	// True only if the requested TTF actually loaded (false => bitmap fallback).
	bool ttfLoaded() const { return _ttfLoaded; }
	// Global size multiplier (percent) applied to every target height — the user's
	// roger_ui_font_scale knob. 100 = use the role's target height as-is.
	void setGlobalScale(int pct) { _globalScalePct = pct > 0 ? pct : 100; }
	// The size an element WANTS: target cell height * the global multiplier.
	int scaledIdealPx(int targetPx) const {
		return targetPx > 0 ? targetPx * _globalScalePct / 100 : 0;
	}
	// Largest cell height <= idealPx that keeps `text` inside its caps: the box
	// height always; for single-line fields (maxTextW > 0) the rendered string width
	// must not exceed maxTextW; for multi-line text (maxTextW == 0) the word-wrapped
	// block (wrapped to boxW) must fit boxH. Re-measures at each candidate size (TTF
	// metrics are not linear in size). Feed the result to applySharedGroupScale, then
	// drawAtPx. idealPx <= 0 => fill the box height.
	int fitPx(const Common::String &text, int boxW, int boxH, int idealPx,
	          int maxTextW, const Common::Array<UiGlyph> *glyphs = nullptr) const;
	// Draw word-wrapped, vertically-centred text at EXACTLY finalPx (as chosen by
	// fitPx + applySharedGroupScale): no re-fitting, no global multiplier; lines that
	// fall past rect.bottom clip silently.
	void drawAtPx(Graphics::ManagedSurface &dst, const Common::String &text,
	              const Common::Rect &rect, uint32 color, int align, int finalPx,
	              bool vAlignTop = false, const Common::Array<UiGlyph> *glyphs = nullptr) const;
	// Legacy convenience: fitPx (with the global multiplier) + drawAtPx in one call.
	// targetPx <= 0 => fill the box height.
	void drawPx(Graphics::ManagedSurface &dst, const Common::String &text,
	            const Common::Rect &rect, uint32 color, int align, int targetPx,
	            bool vAlignTop = false, const Common::Array<UiGlyph> *glyphs = nullptr,
	            int maxTextW = 0) const;
	// Caret x offset (px from the text left edge) for a caret before char cursorPos,
	// measured with the font drawAtPx would use at finalPx.
	int caretAtPx(const Common::String &text, int cursorPos, int finalPx) const;

private:
	// Font for an exact cell height: TTF loaded on demand (kTTFSizeModeCell) and
	// cached per size; bitmap fallback picks the largest built-in font that fits.
	const Graphics::Font *fontForPx(int px) const;

	struct SizedFont { int px; Graphics::Font *font; };
	Common::String _ttfName;                       // TTF to load sizes from (empty = bitmap)
	mutable Common::Array<SizedFont> _sizeCache;   // exact-size TTF instances (owned; lazy)
	Common::Array<const Graphics::Font *> _fonts;  // bitmap fallback fonts, ascending (borrowed)
	bool _ttfLoaded = false;                       // requested TTF loaded (not bitmap fallback)?
	int _globalScalePct = 100;                     // user size multiplier (roger_ui_font_scale)
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_TEXT_H
