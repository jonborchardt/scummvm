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

namespace Graphics { class Font; class ManagedSurface; }

namespace Sci {
namespace Roger {

// Index of the largest font (fonts ascending by size) whose rendering of `text`
// fits (boxW,boxH). 0 if none fit; -1 if fonts is empty. Pure: unit-testable.
int fitFontIndex(const Common::Array<const Graphics::Font *> &fonts,
                 const Common::String &text, int boxW, int boxH);

class RogerTextRenderer {
public:
	// ttfName: a TTF inside ScummVM's fonts.dat (e.g. "FreeSans.ttf"); empty or
	// load failure => built-in FontMan fonts. sizes: pixel sizes (ascending).
	RogerTextRenderer(const Common::String &ttfName, const Common::Array<int> &sizes);
	~RogerTextRenderer();

	bool ok() const { return !_fonts.empty(); }
	// Scale the fit box by pct/100 before choosing a font, so text can be rendered
	// larger than the literal native rect (Roger hires dialogs). 100 = exact fit.
	void setFitScale(int pct) { _fitScalePct = pct > 0 ? pct : 100; }
	const Graphics::Font *fitFont(const Common::String &text, int boxW, int boxH) const;
	void draw(Graphics::ManagedSurface &dst, const Common::String &text,
	          const Common::Rect &rect, uint32 color, int align) const;
	int caretX(const Common::String &text, int cursorPos, int boxW, int boxH) const;

private:
	Common::Array<const Graphics::Font *> _fonts; // ascending by size
	Common::Array<bool> _owned;                   // parallel: delete on dtor?
	int _fitScalePct = 100;                        // fit-box scale (pct); >100 = larger text
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_TEXT_H
