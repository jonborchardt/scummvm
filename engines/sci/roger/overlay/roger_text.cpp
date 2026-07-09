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

#include "sci/roger/overlay/roger_text.h"
#include "sci/roger/overlay/roger_tokens.h"
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

int fitFontIndexByHeightAndWidth(const Common::Array<const Graphics::Font *> &fonts,
                                 const Common::String &text, int maxH, int maxW) {
	if (fonts.empty())
		return -1;
	int best = 0;
	for (uint i = 0; i < fonts.size(); i++) {
		if (fonts[i]->getFontHeight() > maxH)
			continue;
		if (maxW > 0 && fonts[i]->getStringWidth(text) > maxW)
			continue;
		best = (int)i; // ascending -> last fitting is largest
	}
	return best;
}

int rogerTargetPx(int nativeFontH, int overlayH, int globalScalePct) {
	if (nativeFontH <= 0 || overlayH <= 0)
		return 0;
	if (globalScalePct <= 0)
		globalScalePct = 100;
	int px = nativeFontH * overlayH / 200 * globalScalePct / 100;
	return px < 1 ? 1 : px;
}

int firstLineTop(int top, int boxH, int lineCount, int lineH, bool vAlignTop) {
	if (vAlignTop)
		return top;
	int y = top + (boxH - lineCount * lineH) / 2;
	return y < top ? top : y;
}

int opticalBlockTop(int top, int boxH, int lineCount, int lineH, int inkTop, int inkBottom) {
	if (inkBottom <= inkTop || lineCount < 1)
		return firstLineTop(top, boxH, lineCount, lineH, false);
	const int blockInkTop = inkTop;
	const int blockInkBottom = (lineCount - 1) * lineH + inkBottom;
	int y = top + (boxH - (blockInkBottom - blockInkTop)) / 2 - blockInkTop;
	// Never let the ink start above the box (oversized ink degenerates to top-ink-flush).
	if (y + blockInkTop < top)
		y = top - blockInkTop;
	return y;
}

bool lineDrawsWithinBox(uint lineIndex, int y, int inkBottom, int rectBottom) {
	if (lineIndex == 0)
		return true; // the only/first line always draws; overhang beats an empty field
	return y + inkBottom <= rectBottom;
}

uint32 textScaleGroup(uint32 token, bool useAltFont, bool multiLine, uint elemIndex) {
	// Group labels only need uniqueness, not meaning. Window/port ids live in the low
	// bits of tokens and are small; the 0x0A/0x06 prefixes cannot collide with them.
	uint32 g;
	if (multiLine)
		g = 0x0A000000u + elemIndex;         // singleton per element
	else if ((token & kTokenNamespaceMask) == kGenericTextTokenNs)
		g = 0x06000000u;                     // generic text: one scale per screen
	else
		g = token & 0x0FFFFFFFu;             // controls: one scale per window
	if (useAltFont)
		g |= 0x80000000u;
	return g;
}

void applySharedGroupScale(Common::Array<TextSizeFit> &items) {
	// Track each group's worst fit/ideal ratio as an exact fraction (num/den) so
	// the element that set the minimum lands back on exactly its own fitPx.
	Common::Array<uint32> groups;
	Common::Array<int> nums, dens;
	for (uint i = 0; i < items.size(); i++) {
		if (items[i].idealPx <= 0)
			continue;
		if (items[i].fitPx > items[i].idealPx)
			items[i].fitPx = items[i].idealPx; // never grow past the game-suggested size
		uint g;
		for (g = 0; g < groups.size(); g++)
			if (groups[g] == items[i].group)
				break;
		if (g == groups.size()) {
			groups.push_back(items[i].group);
			nums.push_back(1);
			dens.push_back(1);
		}
		// fit/ideal < num/den ?
		if ((int64)items[i].fitPx * dens[g] < (int64)nums[g] * items[i].idealPx) {
			nums[g] = items[i].fitPx;
			dens[g] = items[i].idealPx;
		}
	}
	for (uint i = 0; i < items.size(); i++) {
		if (items[i].idealPx <= 0)
			continue;
		for (uint g = 0; g < groups.size(); g++) {
			if (groups[g] != items[i].group)
				continue;
			int px = (int)((int64)items[i].idealPx * nums[g] / dens[g]);
			items[i].fitPx = px < 1 ? 1 : px;
			break;
		}
	}
}

// Sane bounds for on-demand font sizes: below ~7 px TTF glyphs are unreadable
// noise, and the cap keeps a corrupt metric from allocating a monster font.
static const int kMinFontPx = 7;
static const int kMaxFontPx = 300;

RogerTextRenderer::RogerTextRenderer(const Common::String &ttfName) : _ttfName(ttfName) {
#ifdef USE_FREETYPE2
	if (!_ttfName.empty()) {
		// Probe load: proves the TTF exists in fonts.dat and seeds the size cache.
		Graphics::Font *probe = Graphics::loadTTFFontFromArchive(
			_ttfName, 32, Graphics::kTTFSizeModeCell, 0, 0,
			Graphics::kTTFRenderModeLight);
		if (probe) {
			SizedFont sf; sf.px = 32; sf.font = probe;
			_sizeCache.push_back(sf);
			_ttfLoaded = true;
		}
	}
#endif
	if (!_ttfLoaded) {
		// Fallback: built-in bitmap fonts (always present, no files/FreeType).
		const Graphics::Font *g = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
		const Graphics::Font *b = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		if (g) _fonts.push_back(g);
		if (b) _fonts.push_back(b);
	}
}

RogerTextRenderer::~RogerTextRenderer() {
	for (uint i = 0; i < _sizeCache.size(); i++)
		delete _sizeCache[i].font;
}

const Graphics::Font *RogerTextRenderer::fontForPx(int px) const {
	if (px < kMinFontPx) px = kMinFontPx;
	if (px > kMaxFontPx) px = kMaxFontPx;
#ifdef USE_FREETYPE2
	if (_ttfLoaded) {
		for (uint i = 0; i < _sizeCache.size(); i++)
			if (_sizeCache[i].px == px)
				return _sizeCache[i].font;
		Graphics::Font *f = Graphics::loadTTFFontFromArchive(
			_ttfName, px, Graphics::kTTFSizeModeCell, 0, 0,
			Graphics::kTTFRenderModeLight);
		if (f) {
			SizedFont sf; sf.px = px; sf.font = f;
			_sizeCache.push_back(sf);
			return f;
		}
		// Load failure at this size: fall through to the bitmap fallback (if any).
	}
#endif
	if (_fonts.empty())
		return nullptr;
	int idx = fitFontIndexByHeight(_fonts, px);
	return _fonts[idx < 0 ? 0 : idx];
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
			if (g->h > 0) { const int gh = lineH * 3 / 4; w += g->w * gh / g->h; }
		} else {
			run += (char)c;
		}
	}
	if (!run.empty()) w += f->getStringWidth(run);
	return w;
}

// FNV-1a-ish hash of the fitPx inputs. Deterministic key: identical inputs => same
// key => cached result served (fitPx is a pure function of these). The glyph set is
// folded in by count + each (ch, surf-pointer) â€” the surfaces for one element are
// stable for its lifetime, and a different element/glyph set yields a different key.
static uint64 fitCacheKey(const Common::String &text, int boxW, int boxH, int idealPx,
                          int maxTextW, const Common::Array<UiGlyph> *glyphs) {
	uint64 h = 1469598103934665603ULL;
	const uint64 prime = 1099511628211ULL;
	for (uint i = 0; i < text.size(); i++) {
		h ^= (byte)text[i];
		h *= prime;
	}
	const int dims[5] = { boxW, boxH, idealPx, maxTextW, glyphs ? (int)glyphs->size() : 0 };
	for (int i = 0; i < 5; i++) {
		h ^= (uint32)dims[i];
		h *= prime;
	}
	if (glyphs) {
		for (uint i = 0; i < glyphs->size(); i++) {
			h ^= (uint64)(uintptr)(*glyphs)[i].surf;
			h *= prime;
			h ^= (*glyphs)[i].ch;
			h *= prime;
		}
	}
	return h;
}

int RogerTextRenderer::fitPx(const Common::String &text, int boxW, int boxH, int idealPx,
                             int maxTextW, const Common::Array<UiGlyph> *glyphs) const {
	// Memoized: identical inputs return the previously computed fit (walking re-renders
	// the UI every cycle with unchanged text/box/scale â€” the wrap+shrink loops below
	// are the hot cost). The cache is transparent; a differing input recomputes.
	const uint64 ck = fitCacheKey(text, boxW, boxH, idealPx, maxTextW, glyphs);
	for (uint i = 0; i < _fitCache.size(); i++) {
		if (_fitCache[i].key == ck)
			return _fitCache[i].result;
	}

	// Start from the ideal, capped to the box height (a tight strip can never host
	// text taller than itself).
	int h = idealPx > 0 ? idealPx : boxH;
	if (h > boxH)
		h = boxH;
	if (h < 1)
		h = 1;
	if (text.empty())
		return storeFit(ck, h);
	// Shrink proportionally until the constraint is met, re-measuring each step
	// because glyph metrics do not scale perfectly linearly. Bounded iterations:
	// with the bitmap fallback the measured width may not shrink with h at all,
	// so every path must terminate without convergence.
	//
	// TWO independent constraints, both enforced (smaller wins):
	//  (1) width cap â€” only when maxTextW > 0: the single-line native footprint
	//      width a control-hooked element carries (nTextW). Keeps the crisp text
	//      inside the same on-screen width the original occupied.
	//  (2) box height â€” ALWAYS: the word-wrapped block (at the box width) must fit
	//      the rect height. The metric-carrying control copy sets a huge single-line
	//      width cap that never binds, but Roger re-wraps at the TTF font, which
	//      yields MORE lines than native SCI packed into the same box (the documented
	//      multi-line re-wrap drift). Without this arm a long dialog renders at its
	//      ideal size and overflows/clips the box bottom (QFG1 room 320 "look").
	//      A genuine single-line field wraps to one line here, so this is a no-op for
	//      it â€” it never over-shrinks the short dialogs that already fit at ideal.
	if (maxTextW > 0) {
		// Constraint (1): the rendered string must fit the native footprint width.
		for (int i = 0; i < 5; i++) {
			const Graphics::Font *f = fontForPx(h);
			if (!f)
				return storeFit(ck, h);
			const int w = mixedLineWidth(f, text, glyphs, f->getFontHeight());
			if (w <= maxTextW || h <= 1)
				break;
			int nh = (int)((int64)h * maxTextW / w);
			if (nh >= h)
				nh = h - 1;
			h = nh < 1 ? 1 : nh;
		}
	}
	// Constraint (2): the word-wrapped block must fit the box height. Comparing the
	// FULL unwrapped string width against the box would reject every usable size â€”
	// wrap first, then compare heights.
	Common::Array<Common::String> lines;
	for (int i = 0; i < 5; i++) {
		const Graphics::Font *f = fontForPx(h);
		if (!f)
			return storeFit(ck, h);
		lines.clear();
		f->wordWrapText(text, boxW, lines);
		const int totalH = (int)lines.size() * f->getFontHeight();
		if (totalH <= boxH || h <= 1)
			break;
		int nh = (int)((int64)h * boxH / totalH);
		if (nh >= h)
			nh = h - 1;
		h = nh < 1 ? 1 : nh;
	}
	return storeFit(ck, h);
}

void RogerTextRenderer::drawPx(Graphics::ManagedSurface &dst, const Common::String &text,
                               const Common::Rect &rect, uint32 color, int align, int targetPx,
                               bool vAlignTop, const Common::Array<UiGlyph> *glyphs,
                               int maxTextW) const {
	const int ideal = targetPx > 0 ? scaledIdealPx(targetPx) : rect.height();
	const int fit = fitPx(text, rect.width(), rect.height(), ideal, maxTextW, glyphs);
	drawAtPx(dst, text, rect, color, align, fit, vAlignTop, glyphs);
}

void RogerTextRenderer::drawAtPx(Graphics::ManagedSurface &dst, const Common::String &text,
                                 const Common::Rect &rect, uint32 color, int align, int finalPx,
                                 bool vAlignTop, const Common::Array<UiGlyph> *glyphs) const {
	const Graphics::Font *f = fontForPx(finalPx);
	if (!f)
		return;

	Graphics::TextAlign ta = Graphics::kTextAlignLeft;
	if (align == 1) ta = Graphics::kTextAlignCenter;
	else if (align == -1) ta = Graphics::kTextAlignRight;

	Common::Array<Common::String> lines;
	f->wordWrapText(text, rect.width(), lines);
	if (lines.empty())
		return;
	// Centred vertically by default; vAlignTop draws from the top of the box (SCI's
	// native text-edit position). Centring uses the INK extent, not the font cell:
	// TTF cells carry internal leading above the glyphs, so cell centring sat label
	// text visibly low in buttons (QFG1 main menu vs the native mirror, 2026-07-04).
	const int lh = f->getFontHeight();
	Common::Rect inkFirst = f->getBoundingBox(lines[0]);
	Common::Rect inkLast = lines.size() == 1 ? inkFirst
	                                         : f->getBoundingBox(lines[lines.size() - 1]);
	int y;
	if (vAlignTop)
		y = rect.top;
	else
		y = opticalBlockTop(rect.top, rect.height(), (int)lines.size(), lh,
		                    inkFirst.top, inkLast.bottom);
	// Clip guard uses the ink bottom, not the cell bottom: an ink-centred cell may
	// legitimately overhang the box with empty descent/leading rows. The first line
	// is exempt (see lineDrawsWithinBox): descender ink can exceed the fitted cell.
	const int inkBot = inkLast.bottom > 0 ? inkLast.bottom : lh;
	for (uint i = 0; i < lines.size(); i++) {
		if (!lineDrawsWithinBox(i, y, inkBot, rect.bottom))
			break; // later line's ink would pass the box bottom â€” clip silently
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
						const int gh = lh * 3 / 4;   // 25% smaller than the line height
						const int gw = g->w * gh / g->h;
						const int gy = y + lh - gh;  // bottom-align to the text baseline
						dst.blitFrom(*g, Common::Rect(0, 0, g->w, g->h),
						             Common::Rect(x, gy, x + gw, gy + gh));
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

int RogerTextRenderer::caretAtPx(const Common::String &text, int cursorPos, int finalPx) const {
	const Graphics::Font *f = fontForPx(finalPx);
	if (!f)
		return 0;
	int n = cursorPos;
	if (n > (int)text.size()) n = (int)text.size();
	if (n < 0) n = 0;
	return f->getStringWidth(Common::String(text.c_str(), n));
}

} // namespace Roger
} // namespace Sci
