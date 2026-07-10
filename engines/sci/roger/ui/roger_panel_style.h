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

#ifndef SCI_ROGER_UI_ROGER_PANEL_STYLE_H
#define SCI_ROGER_UI_ROGER_PANEL_STYLE_H

// Shared visual language for Roger's custom-drawn panels (game picker, pass
// builder, F12 tune panel, Studio): palette, TTF role fonts with a FontMan
// bitmap fallback, and a painter bound to a target surface. SCI-free and
// GUI-theme-free (graphics/ + common/ + FontMan only) so it is a stable seam
// for the quarantined utils/ tools and the launcher alike.

#include "common/rect.h"
#include "common/str.h"
#include "graphics/font.h"

namespace Graphics {
class ManagedSurface;
}

namespace Sci {
namespace Roger {

// Palette shared by all Roger panel surfaces. No theme dependence: Roger
// panels paint themselves. (Formerly PickerColors in roger_picker_view.h.)
namespace PanelStyle {
struct Rgb { byte r, g, b; };
const Rgb kText       = { 232, 236, 244 };
const Rgb kTextDim    = { 154, 164, 184 };
const Rgb kGreen      = {  76, 195, 138 };
const Rgb kAmber      = { 229, 184,  75 };
const Rgb kBlue       = { 100, 148, 237 };
const Rgb kRed        = { 214,  86,  78 };
const Rgb kPanelLine  = {  42,  52,  80 };
// Fills (previously inline literals in the picker draw code).
const Rgb kGradTop    = {  10,  14,  26 };  // background gradient, top
const Rgb kGradBottom = {  26,  36,  56 };  // background gradient, bottom
const Rgb kPanelFill  = {  16,  22,  36 };  // panel background
const Rgb kCardFill   = {  22,  30,  50 };  // list-row card background
const Rgb kFieldFill  = {  10,  14,  24 };  // field / outline-button pill
} // namespace PanelStyle

enum PanelFontRole {
	kFontTitle = 0,   // LiberationSans
	kFontSub,         // LiberationSans
	kFontBody,        // LiberationSans
	kFontSmall,       // LiberationSans
	kFontMono,        // GoMono
	kFontRoleCount
};

// Owns the TTF role fonts; falls back to the FontMan GUI fonts when FreeType
// is unavailable or a load fails (kBigGUIFont for the title role, kGUIFont
// otherwise). load() is a no-op when sizes are unchanged, so callers may call
// it whenever their reference geometry may have changed. A size <= 0 marks a
// role unused by this panel (no TTF is loaded; get() still returns fallback).
class PanelFonts {
public:
	PanelFonts() {}
	~PanelFonts();

	void load(const int sizesPx[kFontRoleCount]);
	const Graphics::Font *get(int role) const;

private:
	// Owns raw font pointers -> copying would double-delete.
	PanelFonts(const PanelFonts &) = delete;
	PanelFonts &operator=(const PanelFonts &) = delete;

	Graphics::Font *_ttf[kFontRoleCount] = {};       // owned; null = fallback
	const Graphics::Font *_use[kFontRoleCount] = {}; // ttf or FontMan fallback
	int _sizes[kFontRoleCount] = { -1, -1, -1, -1, -1 };
};

// Immediate-mode draw helpers bound to one target surface + one font set.
// All rects clip to the surface; colors come from PanelStyle.
class PanelPainter {
public:
	PanelPainter(Graphics::ManagedSurface &canvas, const PanelFonts &fonts)
		: _canvas(canvas), _fonts(fonts) {}

	void gradientFill(const Common::Rect &r, const PanelStyle::Rgb &top,
	                  const PanelStyle::Rgb &bottom);
	void blendFill(const Common::Rect &r, const PanelStyle::Rgb &c, byte alpha);
	void strokeRect(const Common::Rect &r, const PanelStyle::Rgb &c);
	void drawTextIn(int role, const Common::String &s, const Common::Rect &r,
	                const PanelStyle::Rgb &c, Graphics::TextAlign align);
	// The picker's button: translucent dark pill (or dark accent fill when
	// `filled`), accent border (+40 brighten on hover, halved when disabled),
	// centered label (kText, kTextDim when disabled).
	void drawButton(const Common::Rect &r, const Common::String &label,
	                const PanelStyle::Rgb &accent, bool filled, bool enabled,
	                bool hovered, int labelRole = kFontSmall);
	// The picker's settings toggle: pill + knob, kBlue when on.
	void drawTogglePill(const Common::Rect &r, bool on);

private:
	Graphics::ManagedSurface &_canvas;
	const PanelFonts &_fonts;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_UI_ROGER_PANEL_STYLE_H
