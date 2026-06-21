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

const Graphics::Font *RogerTextRenderer::fitFont(const Common::String &text,
                                                 int boxW, int boxH, int scalePct) const {
	// Scale the fit box so text may be rendered larger than the literal native rect.
	const int pct = scalePct > 0 ? scalePct : _fitScalePct;
	const int w = boxW * pct / 100;
	const int h = boxH * pct / 100;
	int idx = fitFontIndex(_fonts, text, w, h);
	return idx < 0 ? nullptr : _fonts[idx];
}

void RogerTextRenderer::draw(Graphics::ManagedSurface &dst, const Common::String &text,
                             const Common::Rect &rect, uint32 color, int align, int scalePct) const {
	const Graphics::Font *f = fitFont(text, rect.width(), rect.height(), scalePct);
	if (!f)
		return;
	Graphics::TextAlign ta = Graphics::kTextAlignLeft;
	if (align == 1) ta = Graphics::kTextAlignCenter;
	else if (align == -1) ta = Graphics::kTextAlignRight;

	// Word-wrap to the box width and draw the lines stacked, vertically centred.
	// When the block is taller than the box (chosen font is large), start at the top
	// so it grows downward rather than clipping the first lines.
	Common::Array<Common::String> lines;
	f->wordWrapText(text, rect.width(), lines);
	if (lines.empty())
		return;
	const int lh = f->getFontHeight();
	const int totalH = (int)lines.size() * lh;
	int y = rect.top + (rect.height() - totalH) / 2;
	if (y < rect.top)
		y = rect.top;
	for (uint i = 0; i < lines.size(); i++) {
		f->drawString(&dst, lines[i], rect.left, y, rect.width(), color, ta);
		y += lh;
	}
}

int RogerTextRenderer::caretX(const Common::String &text, int cursorPos,
                              int boxW, int boxH, int scalePct) const {
	const Graphics::Font *f = fitFont(text, boxW, boxH, scalePct);
	if (!f)
		return 0;
	int n = cursorPos;
	if (n > (int)text.size()) n = (int)text.size();
	return f->getStringWidth(Common::String(text.c_str(), n));
}

} // namespace Roger
} // namespace Sci
