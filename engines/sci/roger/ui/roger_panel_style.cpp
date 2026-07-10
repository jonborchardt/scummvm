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

#include "sci/roger/ui/roger_panel_style.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
#include "common/util.h"
#ifdef USE_FREETYPE2
#include "graphics/fonts/ttf.h"
#endif

namespace Sci {
namespace Roger {

using PanelStyle::Rgb;

static Rgb halfRgb(const Rgb &c) {
	Rgb h = { (byte)(c.r / 2), (byte)(c.g / 2), (byte)(c.b / 2) };
	return h;
}

static Rgb brightenRgb(const Rgb &c, int d) {
	Rgb b = { (byte)MIN(255, c.r + d), (byte)MIN(255, c.g + d), (byte)MIN(255, c.b + d) };
	return b;
}

PanelFonts::~PanelFonts() {
	for (int i = 0; i < kFontRoleCount; ++i)
		delete _ttf[i];
}

void PanelFonts::load(const int sizesPx[kFontRoleCount]) {
	bool same = true;
	for (int i = 0; i < kFontRoleCount; ++i)
		same = same && (_sizes[i] == sizesPx[i]);
	if (same && _use[0])
		return;
	for (int i = 0; i < kFontRoleCount; ++i) {
		delete _ttf[i];
		_ttf[i] = nullptr;
		_sizes[i] = sizesPx[i];
	}
#ifdef USE_FREETYPE2
	for (int i = 0; i < kFontRoleCount; ++i) {
		if (_sizes[i] <= 0)
			continue; // role unused by this panel
		const char *file = (i == kFontMono) ? "GoMono-Regular.ttf"
		                                    : "LiberationSans-Regular.ttf";
		_ttf[i] = Graphics::loadTTFFontFromArchive(file, _sizes[i],
		                                           Graphics::kTTFSizeModeCell, 0, 0,
		                                           Graphics::kTTFRenderModeLight);
	}
#endif
	const Graphics::Font *gui = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
	const Graphics::Font *big = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	for (int i = 0; i < kFontRoleCount; ++i)
		_use[i] = _ttf[i] ? _ttf[i] : ((i == kFontTitle && big) ? big : gui);
}

const Graphics::Font *PanelFonts::get(int role) const {
	if (role < 0 || role >= kFontRoleCount)
		return nullptr;
	return _use[role];
}

void PanelPainter::gradientFill(const Common::Rect &r, const Rgb &top, const Rgb &bottom) {
	const int hh = MAX(1, r.height() - 1);
	for (int yy = r.top; yy < r.bottom; ++yy) {
		const int t = yy - r.top;
		const int cr = top.r + (bottom.r - top.r) * t / hh;
		const int cg = top.g + (bottom.g - top.g) * t / hh;
		const int cb = top.b + (bottom.b - top.b) * t / hh;
		_canvas.fillRect(Common::Rect(r.left, yy, r.right, yy + 1),
		                 _canvas.format.RGBToColor(cr, cg, cb));
	}
}

void PanelPainter::blendFill(const Common::Rect &rIn, const Rgb &c, byte alpha) {
	Common::Rect r = rIn;
	r.clip(Common::Rect(0, 0, _canvas.w, _canvas.h));
	if (r.isEmpty())
		return;
	const Graphics::PixelFormat &f = _canvas.format;
	// Non-32bpp overlay: skip alpha blend; fill opaque with the nearest color.
	if (f.bytesPerPixel != 4) {
		_canvas.fillRect(r, f.RGBToColor(c.r, c.g, c.b));
		return;
	}
	for (int yy = r.top; yy < r.bottom; ++yy) {
		for (int xx = r.left; xx < r.right; ++xx) {
			uint32 *px = (uint32 *)_canvas.getBasePtr(xx, yy);
			byte dr, dg, db;
			f.colorToRGB(*px, dr, dg, db);
			dr = (byte)((c.r * alpha + dr * (255 - alpha)) / 255);
			dg = (byte)((c.g * alpha + dg * (255 - alpha)) / 255);
			db = (byte)((c.b * alpha + db * (255 - alpha)) / 255);
			*px = f.RGBToColor(dr, dg, db);
		}
	}
}

void PanelPainter::strokeRect(const Common::Rect &rIn, const Rgb &c) {
	Common::Rect r = rIn;
	r.clip(Common::Rect(0, 0, _canvas.w, _canvas.h));
	if (r.isEmpty())
		return;
	const uint32 col = _canvas.format.RGBToColor(c.r, c.g, c.b);
	_canvas.hLine(r.left, r.top, r.right - 1, col);
	_canvas.hLine(r.left, r.bottom - 1, r.right - 1, col);
	_canvas.vLine(r.left, r.top, r.bottom - 1, col);
	_canvas.vLine(r.right - 1, r.top, r.bottom - 1, col);
}

void PanelPainter::drawTextIn(int role, const Common::String &s, const Common::Rect &r,
                              const Rgb &c, Graphics::TextAlign align) {
	const Graphics::Font *font = _fonts.get(role);
	if (!font || r.isEmpty())
		return;
	const int y = r.top + (r.height() - font->getFontHeight()) / 2;
	font->drawString(&_canvas, s, r.left, MAX((int)r.top, y), r.width(),
	                 _canvas.format.RGBToColor(c.r, c.g, c.b), align);
}

void PanelPainter::drawButton(const Common::Rect &r, const Common::String &label,
                              const Rgb &accent, bool filled, bool enabled,
                              bool hovered, int labelRole) {
	if (filled)
		blendFill(r, halfRgb(accent), enabled ? 235 : 40);
	else
		blendFill(r, PanelStyle::kFieldFill, hovered ? 200 : 160);
	Rgb border = accent;
	if (hovered)
		border = brightenRgb(accent, 40);
	if (!enabled)
		border = halfRgb(accent);
	strokeRect(r, border);
	drawTextIn(labelRole, label, r, enabled ? PanelStyle::kText : PanelStyle::kTextDim,
	           Graphics::kTextAlignCenter);
}

void PanelPainter::drawTogglePill(const Common::Rect &r, bool on) {
	blendFill(r, on ? halfRgb(PanelStyle::kBlue) : PanelStyle::kFieldFill, 220);
	strokeRect(r, on ? PanelStyle::kBlue : PanelStyle::kPanelLine);
	Common::Rect knob = r;
	knob.grow(-2);
	if (on)
		knob.left = knob.right - knob.height();
	else
		knob.right = knob.left + knob.height();
	blendFill(knob, PanelStyle::kText, 255);
}

} // namespace Roger
} // namespace Sci
