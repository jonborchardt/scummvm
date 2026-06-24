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

#include "sci/roger/roger_text.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
#ifdef USE_FREETYPE2
#include "graphics/fonts/ttf.h"
#endif

namespace Sci {
namespace Roger {

int fitFontIndex(const Common::Array<const Graphics::Font *> &fonts,
                 const Common::String &text, int boxW, int boxH) {
	if (fonts.empty())
		return -1;
	int best = 0;
	for (uint i = 0; i < fonts.size(); i++) {
		if (fonts[i]->getStringWidth(text) <= boxW && fonts[i]->getFontHeight() <= boxH)
			best = (int)i; // ascending -> last fitting is largest
	}
	return best;
}

int fitFontIndexByHeight(const Common::Array<const Graphics::Font *> &fonts, int maxH) {
	if (fonts.empty())
		return -1;
	int best = 0;
	for (uint i = 0; i < fonts.size(); i++) {
		if (fonts[i]->getFontHeight() <= maxH)
			best = (int)i; // ascending -> last fitting is largest
	}
	return best;
}

Common::String stripUnrenderable(const Common::String &s) {
	Common::String out;
	for (uint i = 0; i < s.size(); i++) {
		const byte c = (byte)s[i];
		if (c >= 0x20 && c < 0x7f)
			out += (char)c;
	}
	return out;
}

int firstLineTop(int top, int boxH, int lineCount, int lineH, bool vAlignTop) {
	if (vAlignTop)
		return top;
	int y = top + (boxH - lineCount * lineH) / 2;
	return y < top ? top : y;
}

RogerTextRenderer::RogerTextRenderer(const Common::String &ttfName,
                                     const Common::Array<int> &sizes) {
#ifdef USE_FREETYPE2
	if (!ttfName.empty()) {
		for (uint i = 0; i < sizes.size(); i++) {
			Graphics::Font *f = Graphics::loadTTFFontFromArchive(
				ttfName, sizes[i], Graphics::kTTFSizeModeCell, 0, 0,
				Graphics::kTTFRenderModeLight);
			if (f) {
				_fonts.push_back(f);
				_owned.push_back(true);
			}
		}
	}
	_ttfLoaded = !_fonts.empty();
#endif
	if (_fonts.empty()) {
		// Fallback: built-in bitmap fonts (always present, no files/FreeType).
		const Graphics::Font *g = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
		const Graphics::Font *b = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		if (g) { _fonts.push_back(g); _owned.push_back(false); }
		if (b) { _fonts.push_back(b); _owned.push_back(false); }
	}
}

RogerTextRenderer::~RogerTextRenderer() {
	for (uint i = 0; i < _fonts.size(); i++)
		if (_owned[i])
			delete _fonts[i];
}

const Graphics::Font *RogerTextRenderer::fontForBox(const Common::Rect &rect, int targetPx) const {
	// targetPx is the desired on-screen cell height; apply the user's global scale,
	// then cap to the box height so a tight strip/row shrinks to fit instead of
	// overlapping its neighbours. targetPx <= 0 => simply fill the box.
	int h = targetPx > 0 ? targetPx * _globalScalePct / 100 : rect.height();
	if (h > rect.height())
		h = rect.height();
	int idx = fitFontIndexByHeight(_fonts, h);
	return idx < 0 ? nullptr : _fonts[idx];
}

// Look up the pre-rendered surface for byte `c` in the element's glyph list (nullptr if none).
static const Graphics::Surface *findGlyph(const Common::Array<UiGlyph> *glyphs, uint16 c) {
	if (!glyphs)
		return nullptr;
	for (uint i = 0; i < glyphs->size(); i++)
		if ((*glyphs)[i].ch == c)
			return (*glyphs)[i].surf;
	return nullptr;
}

// Does this line contain a byte outside printable ASCII that we have a glyph for?
static bool lineHasGlyph(const Common::String &line, const Common::Array<UiGlyph> *glyphs) {
	if (!glyphs || glyphs->empty())
		return false;
	for (uint i = 0; i < line.size(); i++) {
		const byte c = (byte)line[i];
		if ((c < 0x20 || c >= 0x7f) && findGlyph(glyphs, c))
			return true;
	}
	return false;
}

// Width of a mixed line: ASCII measured by the TTF font, each glyph scaled to lineH.
static int mixedLineWidth(const Graphics::Font *f, const Common::String &line,
                          const Common::Array<UiGlyph> *glyphs, int lineH) {
	int w = 0;
	Common::String run;
	for (uint i = 0; i < line.size(); i++) {
		const byte c = (byte)line[i];
		const Graphics::Surface *g = (c < 0x20 || c >= 0x7f) ? findGlyph(glyphs, c) : nullptr;
		if (g) {
			if (!run.empty()) { w += f->getStringWidth(run); run.clear(); }
			if (g->h > 0) w += g->w * lineH / g->h;
		} else {
			run += (char)c;
		}
	}
	if (!run.empty()) w += f->getStringWidth(run);
	return w;
}

void RogerTextRenderer::drawPx(Graphics::ManagedSurface &dst, const Common::String &text,
                               const Common::Rect &rect, uint32 color, int align, int targetPx,
                               bool vAlignTop, const Common::Array<UiGlyph> *glyphs) const {
	if (_fonts.empty())
		return;
	// Target on-screen cell height (role scale * global multiplier), capped to the box.
	int h = targetPx > 0 ? targetPx * _globalScalePct / 100 : rect.height();
	if (h > rect.height())
		h = rect.height();
	int idx = fitFontIndexByHeight(_fonts, h);
	if (idx < 0)
		return;

	Graphics::TextAlign ta = Graphics::kTextAlignLeft;
	if (align == 1) ta = Graphics::kTextAlignCenter;
	else if (align == -1) ta = Graphics::kTextAlignRight;

	// Pick the largest font (at or below the target) whose word-wrapped block also
	// FITS the box height. SCI sizes its dialog boxes for the text, so the hires text
	// must not spill past the box bottom (which the bold window border makes obvious).
	Common::Array<Common::String> lines;
	const Graphics::Font *f = _fonts[idx];
	for (;;) {
		f = _fonts[idx];
		lines.clear();
		f->wordWrapText(text, rect.width(), lines);
		const int totalH = (int)lines.size() * f->getFontHeight();
		if (totalH <= rect.height() || idx == 0)
			break;
		idx--;
	}
	if (lines.empty())
		return;
	// Centred vertically by default; vAlignTop draws from the top of the box (SCI's
	// native text-edit position). firstLineTop clamps to the top if still too tall.
	const int lh = f->getFontHeight();
	int y = firstLineTop(rect.top, rect.height(), (int)lines.size(), lh, vAlignTop);
	for (uint i = 0; i < lines.size(); i++) {
		if (lineHasGlyph(lines[i], glyphs)) {
			// Mixed TTF + native-glyph layout: lay out left->right, drawing ASCII runs
			// with the TTF font and blitting each non-ASCII glyph scaled to the line
			// height, inline. Honours the element alignment via a measured start x.
			const int lineW = mixedLineWidth(f, lines[i], glyphs, lh);
			int x = rect.left;
			if (align == 1)       x = rect.left + (rect.width() - lineW) / 2; // center
			else if (align == -1) x = rect.right - lineW;                     // right
			if (x < rect.left) x = rect.left;
			Common::String run;
			for (uint c = 0; c < lines[i].size(); c++) {
				const byte ch = (byte)lines[i][c];
				const Graphics::Surface *g = (ch < 0x20 || ch >= 0x7f) ? findGlyph(glyphs, ch) : nullptr;
				if (g) {
					if (!run.empty()) {
						f->drawString(&dst, run, x, y, rect.right - x, color, Graphics::kTextAlignLeft);
						x += f->getStringWidth(run);
						run.clear();
					}
					if (g->h > 0) {
						const int gw = g->w * lh / g->h;
						dst.blitFrom(*g, Common::Rect(0, 0, g->w, g->h),
						             Common::Rect(x, y, x + gw, y + lh));
						x += gw;
					}
				} else {
					run += (char)ch;
				}
			}
			if (!run.empty())
				f->drawString(&dst, run, x, y, rect.right - x, color, Graphics::kTextAlignLeft);
		} else {
			f->drawString(&dst, lines[i], rect.left, y, rect.width(), color, ta);
		}
		y += lh;
	}
}

int RogerTextRenderer::caretPx(const Common::String &text, int cursorPos,
                               const Common::Rect &rect, int targetPx) const {
	const Graphics::Font *f = fontForBox(rect, targetPx);
	if (!f)
		return 0;
	int n = cursorPos;
	if (n > (int)text.size()) n = (int)text.size();
	return f->getStringWidth(Common::String(text.c_str(), n));
}

} // namespace Roger
} // namespace Sci
