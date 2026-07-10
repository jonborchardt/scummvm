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

#include "sci/roger/launcher/roger_picker_view.h"
#include "gui/gui-manager.h"
#include "gui/ThemeEngine.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "common/system.h"
#include "common/util.h"
#ifdef USE_FREETYPE2
#include "graphics/fonts/ttf.h"
#endif

namespace Sci {
namespace Roger {

// Pull palette constants into this scope from the shared PickerColors namespace.
using PickerColors::Rgb;
using PickerColors::kText;
using PickerColors::kTextDim;
using PickerColors::kGreen;
using PickerColors::kAmber;
using PickerColors::kBlue;
using PickerColors::kRed;
using PickerColors::kPanelLine;

PickerViewWidget::PickerViewWidget(GUI::GuiObject *boss, int x, int y, int w, int h,
                                   const LauncherState &state, PickerActionListener *listener)
	: GUI::Widget(boss, x, y, w, h), _state(state), _listener(listener) {
	setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_TRACK_MOUSE);
	_canvas.create(w, h, g_system->getOverlayFormat());
	loadFonts();
	bakeBackground();
	rebuild();
}

PickerViewWidget::~PickerViewWidget() {
	for (int i = 0; i < kFontCount; ++i)
		delete _ttf[i];
}

void PickerViewWidget::setPassOptions(const Common::Array<PassOption> &opts) {
	_passOptions = opts;
}

void PickerViewWidget::loadFonts() {
#ifdef USE_FREETYPE2
	struct { const char *file; int size; } spec[kFontCount] = {
		{ "LiberationSans-Regular.ttf", _h / 16 }, // kFTitle
		{ "LiberationSans-Regular.ttf", _h / 34 }, // kFSub
		{ "LiberationSans-Regular.ttf", _h / 38 }, // kFBody
		{ "LiberationSans-Regular.ttf", _h / 46 }, // kFSmall
		{ "GoMono-Regular.ttf",         _h / 38 }, // kFMono
	};
	for (int i = 0; i < kFontCount; ++i)
		_ttf[i] = Graphics::loadTTFFontFromArchive(spec[i].file, spec[i].size,
		                                           Graphics::kTTFSizeModeCell, 0, 0,
		                                           Graphics::kTTFRenderModeLight);
#endif
	const Graphics::Font *gui = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
	const Graphics::Font *big = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	for (int i = 0; i < kFontCount; ++i)
		_use[i] = _ttf[i] ? _ttf[i] : ((i == kFTitle && big) ? big : gui);
}

void PickerViewWidget::bakeBackground() {
	_bgBaked.create(_w, _h, _canvas.format);
	// Procedural vertical gradient, near-black navy -> dark blue.
	for (int yy = 0; yy < _h; ++yy) {
		const int r = 10 + (26 - 10) * yy / MAX(1, _h - 1);
		const int g = 14 + (36 - 14) * yy / MAX(1, _h - 1);
		const int b = 26 + (56 - 26) * yy / MAX(1, _h - 1);
		_bgBaked.fillRect(Common::Rect(0, yy, _w, yy + 1),
		                  _bgBaked.format.RGBToColor(r, g, b));
	}
}

void PickerViewWidget::blendFill(const Common::Rect &rIn, byte cr, byte cg, byte cb, byte ca) {
	Common::Rect r = rIn;
	r.clip(Common::Rect(0, 0, _w, _h));
	if (r.isEmpty()) return;
	const Graphics::PixelFormat &f = _canvas.format;
	// Non-32bpp overlay: skip alpha blend; fill opaque with the nearest color.
	if (f.bytesPerPixel != 4) {
		_canvas.fillRect(r, f.RGBToColor(cr, cg, cb));
		return;
	}
	for (int yy = r.top; yy < r.bottom; ++yy) {
		for (int xx = r.left; xx < r.right; ++xx) {
			uint32 *px = (uint32 *)_canvas.getBasePtr(xx, yy);
			byte dr, dg, db;
			f.colorToRGB(*px, dr, dg, db);
			dr = (byte)((cr * ca + dr * (255 - ca)) / 255);
			dg = (byte)((cg * ca + dg * (255 - ca)) / 255);
			db = (byte)((cb * ca + db * (255 - ca)) / 255);
			*px = f.RGBToColor(dr, dg, db);
		}
	}
}

void PickerViewWidget::strokeRect(const Common::Rect &rIn, byte cr, byte cg, byte cb) {
	Common::Rect r = rIn;
	r.clip(Common::Rect(0, 0, _w, _h));
	if (r.isEmpty()) return;
	const uint32 c = _canvas.format.RGBToColor(cr, cg, cb);
	_canvas.hLine(r.left, r.top, r.right - 1, c);
	_canvas.hLine(r.left, r.bottom - 1, r.right - 1, c);
	_canvas.vLine(r.left, r.top, r.bottom - 1, c);
	_canvas.vLine(r.right - 1, r.top, r.bottom - 1, c);
}

void PickerViewWidget::drawTextIn(int fontRole, const Common::String &s, const Common::Rect &r,
                                  byte cr, byte cg, byte cb, Graphics::TextAlign align) {
	const Graphics::Font *font = _use[fontRole];
	if (!font || r.isEmpty()) return;
	const int y = r.top + (r.height() - font->getFontHeight()) / 2;
	font->drawString(&_canvas, s, r.left, MAX((int)r.top, y), r.width(),
	                 _canvas.format.RGBToColor(cr, cg, cb), align);
}

void PickerViewWidget::drawButtonRect(const Common::Rect &r, const Common::String &label,
                                      byte cr, byte cg, byte cb, bool filled, bool enabled,
                                      uint32 id) {
	const bool hovered = enabled && _hoverId == id;
	const byte a = enabled ? (byte)(filled ? 235 : 90) : (byte)40;
	if (filled)
		blendFill(r, cr / 2, cg / 2, cb / 2, a);       // dark fill of the accent
	else
		blendFill(r, 10, 14, 24, hovered ? 200 : 160); // translucent dark pill
	byte br = cr, bg2 = cg, bb = cb;
	if (hovered) { br = (byte)MIN(255, cr + 40); bg2 = (byte)MIN(255, cg + 40); bb = (byte)MIN(255, cb + 40); }
	if (!enabled) { br = cr / 2; bg2 = cg / 2; bb = cb / 2; }
	strokeRect(r, br, bg2, bb);
	drawTextIn(kFSmall, label, r, enabled ? kText.r : kTextDim.r,
	           enabled ? kText.g : kTextDim.g, enabled ? kText.b : kTextDim.b,
	           Graphics::kTextAlignCenter);
}

void PickerViewWidget::buildWidgets() {
	_widgets.clear();
	// Dropdown options FIRST: they draw on top, so they must hit-test first.
	if (_dropdownOpen) {
		for (uint i = 0; i < _passOptions.size(); ++i) {
			PanelWidget pw;
			pw.rect = passOptionRect(_layout, (int)i, (int)_passOptions.size(), _h);
			pw.id = widId(kPickPassOption, (int)i);
			pw.on = false;
			pw.enabled = true;
			_widgets.push_back(pw);
		}
	}
	const bool busy = _state.precaching;
	for (uint i = 0; i < _layout.rows.size(); ++i) {
		const int row = _scroll + (int)i;
		const GameEntry &g = _state.games[row];
		PanelWidget card;
		card.rect = _layout.rows[i].card;
		card.id = widId(kPickRow, row);
		card.on = (row == _state.selectedIndex);
		card.enabled = !busy;
		// Precache slot: visible when not cached, or as Cancel on the active
		// row while precaching.
		const bool isActive = (row == _state.activeRow);
		if ((!g.cached && !busy) || (isActive && busy)) {
			PanelWidget pre;
			pre.rect = _layout.rows[i].precache;
			pre.id = widId(kPickRowPrecache, row);
			pre.label = (isActive && busy) ? "Cancel" : "Precache";
			pre.on = false;
			pre.enabled = (isActive && busy) || !busy;
			_widgets.push_back(pre);
		}
		PanelWidget rem;
		rem.rect = _layout.rows[i].remove;
		rem.id = widId(kPickRowRemove, row);
		rem.label = "Remove";
		rem.on = false;
		rem.enabled = !busy;
		_widgets.push_back(rem);
		_widgets.push_back(card); // card AFTER its buttons: buttons hit-test first
	}
	PanelWidget add;
	add.rect = _layout.addGame; add.id = widId(kPickAddGame);
	add.label = "+ Add Game"; add.on = false; add.enabled = !busy;
	_widgets.push_back(add);
	if (!_state.games.empty()) {
		PanelWidget pf;
		pf.rect = _layout.passesField; pf.id = widId(kPickPasses);
		pf.on = _dropdownOpen; pf.enabled = !busy;
		_widgets.push_back(pf);
		PanelWidget dt;
		dt.rect = _layout.debugToggle; dt.id = widId(kPickDebugToggle);
		dt.on = _state.settings.debugLog; dt.enabled = !busy;
		_widgets.push_back(dt);
	}
	PanelWidget lb;
	lb.rect = _layout.launch; lb.id = widId(kPickLaunch);
	lb.label = "Launch Game"; lb.on = false;
	lb.enabled = !_state.games.empty() && !busy;
	_widgets.push_back(lb);
}

void PickerViewWidget::drawRow(int visIdx, int row) {
	const PickerRowLayout &r = _layout.rows[visIdx];
	const GameEntry &g = _state.games[row];
	const bool selected = (row == _state.selectedIndex);
	const int pad = MAX(2, _h / 80);

	blendFill(r.card, 22, 30, 50, 235);
	if (selected)
		strokeRect(r.card, kBlue.r, kBlue.g, kBlue.b);
	else
		strokeRect(r.card, kPanelLine.r, kPanelLine.g, kPanelLine.b);

	// Title + subtitle (left column)
	const int leftW = r.badge.left - r.card.left - 2 * pad;
	Common::Rect titleR(r.card.left + 2 * pad, r.card.top + pad,
	                    r.card.left + 2 * pad + leftW, r.card.top + r.card.height() / 2);
	Common::Rect subR(titleR.left, titleR.bottom,
	                  titleR.right, r.card.bottom - pad);
	drawTextIn(kFBody, Common::String::format("%d. %s", row + 1, g.description.c_str()),
	           titleR, selected ? kBlue.r : kText.r, selected ? kBlue.g : kText.g,
	           selected ? kBlue.b : kText.b, Graphics::kTextAlignLeft);

	// Second line: join non-empty facts with "  |  " (ASCII, byte-safe).
	// targetName leads so entries with the same description are distinguishable.
	{
		Common::String info;
		auto append = [&](const Common::String &part) {
			if (part.empty()) return;
			if (!info.empty()) info += "  |  ";
			info += part;
		};
		append(g.targetName);
		append(g.subtitle);
		// Skip gameId when it equals targetName to avoid "sq3 | sq3".
		if (g.gameId != g.targetName) append(g.gameId);
		if (g.ega) append("EGA");
		append(g.sciVersion);
		append(g.gamePath.toString());
		drawTextIn(kFSmall, info, subR, kTextDim.r, kTextDim.g, kTextDim.b,
		           Graphics::kTextAlignLeft);
	}

	// Badge area: precache progress on the active row while running, else status.
	const bool isActive = (row == _state.activeRow);
	if (isActive && _state.precaching) {
		Common::Rect txt(r.badge.left, r.badge.top, r.badge.right,
		                 r.badge.top + r.badge.height() / 2);
		drawTextIn(kFSmall, _state.precacheStatus, txt, kText.r, kText.g, kText.b,
		           Graphics::kTextAlignLeft);
		Common::Rect bar(r.badge.left, txt.bottom + pad / 2, r.badge.right,
		                 txt.bottom + pad / 2 + MAX(3, _h / 160));
		strokeRect(bar, kPanelLine.r, kPanelLine.g, kPanelLine.b);
		const int total = MAX(1, _state.precacheTotal);
		Common::Rect fill = bar;
		fill.right = fill.left + (int16)((bar.width() * CLIP(_state.precacheDone, 0, total)) / total);
		blendFill(fill, kBlue.r, kBlue.g, kBlue.b, 255);
	} else {
		const Rgb c = g.cached ? kGreen : kAmber;
		Common::Rect line1(r.badge.left, r.badge.top, r.badge.right,
		                   r.badge.top + r.badge.height() / 2);
		Common::Rect line2(r.badge.left, line1.bottom, r.badge.right, r.badge.bottom);
		drawTextIn(kFBody, g.cached ? "Cached" : "! Not Cached", line1,
		           c.r, c.g, c.b, Graphics::kTextAlignLeft);
		drawTextIn(kFSmall, g.cached ? "Ready" : "Not ready", line2,
		           kTextDim.r, kTextDim.g, kTextDim.b, Graphics::kTextAlignLeft);
	}

	// Buttons (labels/enabled state mirror buildWidgets exactly).
	const bool busy = _state.precaching;
	if ((!g.cached && !busy) || (isActive && busy))
		drawButtonRect(r.precache, (isActive && busy) ? "Cancel" : "Precache",
		               (isActive && busy) ? kRed.r : kBlue.r,
		               (isActive && busy) ? kRed.g : kBlue.g,
		               (isActive && busy) ? kRed.b : kBlue.b,
		               false, (isActive && busy) || !busy, widId(kPickRowPrecache, row));
	drawButtonRect(r.remove, "Remove", kRed.r, kRed.g, kRed.b, false,
	               !busy, widId(kPickRowRemove, row));
}

void PickerViewWidget::drawSettings() {
	blendFill(_layout.settingsPanel, 16, 22, 36, 216);
	strokeRect(_layout.settingsPanel, kPanelLine.r, kPanelLine.g, kPanelLine.b);
	if (_state.games.empty())
		return;
	const GameEntry &g = _state.games[_state.selectedIndex];
	Common::String forWhom = g.subtitle.empty()
		? g.description
		: Common::String::format("%s (%s)", g.description.c_str(), g.subtitle.c_str());
	drawTextIn(kFBody, "Settings for: " + forWhom, _layout.settingsTitle,
	           kBlue.r, kBlue.g, kBlue.b, Graphics::kTextAlignLeft);

	drawTextIn(kFBody, "Omyac passes", _layout.passesLabel, kText.r, kText.g, kText.b,
	           Graphics::kTextAlignLeft);
	blendFill(_layout.passesField, 10, 14, 24, 200);
	strokeRect(_layout.passesField,
	           _hoverId == widId(kPickPasses) ? kBlue.r : kPanelLine.r,
	           _hoverId == widId(kPickPasses) ? kBlue.g : kPanelLine.g,
	           _hoverId == widId(kPickPasses) ? kBlue.b : kPanelLine.b);
	const int pad = MAX(2, _h / 80);
	Common::Rect fieldText = _layout.passesField;
	fieldText.left += 2 * pad;
	drawTextIn(kFMono, _state.settings.passes + "  v", fieldText,
	           kText.r, kText.g, kText.b, Graphics::kTextAlignLeft);
	drawTextIn(kFSmall, "Rendering pass sequence (roger_omyac_passes).",
	           _layout.passesHint, kTextDim.r, kTextDim.g, kTextDim.b,
	           Graphics::kTextAlignLeft);

	drawTextIn(kFBody, "Debug Logging", _layout.debugLabel,
	           kText.r, kText.g, kText.b, Graphics::kTextAlignLeft);
	// Toggle pill: filled+knob-right when on.
	const bool on = _state.settings.debugLog;
	blendFill(_layout.debugToggle, on ? kBlue.r / 2 : 10, on ? kBlue.g / 2 : 14,
	          on ? kBlue.b / 2 : 24, 220);
	strokeRect(_layout.debugToggle, on ? kBlue.r : kPanelLine.r,
	           on ? kBlue.g : kPanelLine.g, on ? kBlue.b : kPanelLine.b);
	Common::Rect knob = _layout.debugToggle;
	knob.grow(-2);
	if (on)
		knob.left = knob.right - knob.height();
	else
		knob.right = knob.left + knob.height();
	blendFill(knob, kText.r, kText.g, kText.b, 255);
	drawTextIn(kFSmall, "Enable roger_debug in this game's INI section.",
	           _layout.debugHint, kTextDim.r, kTextDim.g, kTextDim.b,
	           Graphics::kTextAlignLeft);
}

void PickerViewWidget::drawDropdown() {
	for (uint i = 0; i < _passOptions.size(); ++i) {
		const Common::Rect r = passOptionRect(_layout, (int)i, (int)_passOptions.size(), _h);
		const bool hovered = _hoverId == widId(kPickPassOption, (int)i);
		blendFill(r, hovered ? 26 : 14, hovered ? 38 : 20, hovered ? 64 : 34, 245);
		strokeRect(r, kPanelLine.r, kPanelLine.g, kPanelLine.b);
		const int pad = MAX(2, _h / 80);
		Common::Rect txt = r;
		txt.left += 2 * pad;
		drawTextIn(kFMono, _passOptions[i].label, txt, kText.r, kText.g, kText.b,
		           Graphics::kTextAlignLeft);
	}
}

void PickerViewWidget::renderAll() {
	_canvas.blitFrom(_bgBaked);

	drawTextIn(kFTitle, "ROGER", _layout.titleBox, kText.r, kText.g, kText.b,
	           Graphics::kTextAlignLeft);
	drawTextIn(kFSmall,
	           "A high-resolution overlay renderer for classic Sierra SCI games. "
	           "Select a game to configure rendering passes and settings.",
	           _layout.descBox, kTextDim.r, kTextDim.g, kTextDim.b, Graphics::kTextAlignLeft);

	blendFill(_layout.listPanel, 16, 22, 36, 216);
	strokeRect(_layout.listPanel, kPanelLine.r, kPanelLine.g, kPanelLine.b);

	if (_state.games.empty()) {
		drawTextIn(kFBody, "No SCI games found - use + Add Game below.",
		           _layout.listPanel, kTextDim.r, kTextDim.g, kTextDim.b,
		           Graphics::kTextAlignCenter);
	} else {
		for (uint i = 0; i < _layout.rows.size(); ++i)
			drawRow((int)i, _scroll + (int)i);
	}

	drawButtonRect(_layout.addGame, "+ Add Game", kBlue.r, kBlue.g, kBlue.b,
	               false, !_state.precaching, widId(kPickAddGame));
	drawSettings();
	drawButtonRect(_layout.launch, "Launch Game", kBlue.r, kBlue.g, kBlue.b,
	               true, !_state.games.empty() && !_state.precaching, widId(kPickLaunch));
	if (_dropdownOpen)
		drawDropdown();
}

void PickerViewWidget::rebuild() {
	_scroll = clampScroll(_scroll, (int)_state.games.size(),
	                      _layout.rowsVisible > 0 ? _layout.rowsVisible : 1);
	_layout = layoutPicker(_w, _h, (int)_state.games.size(), _scroll);
	buildWidgets();
	renderAll();
	markAsDirty();
}

void PickerViewWidget::drawWidget() {
	g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), _canvas, Graphics::ALPHA_OPAQUE);
}

void PickerViewWidget::handleMouseDown(int x, int y, int button, int clickCount) {
	const uint32 id = hitTestWidgets(_widgets, x, y);
	if (_dropdownOpen && widKind(id) != kPickPassOption) {
		_dropdownOpen = false; // click-away closes
		rebuild();
		return;
	}
	switch (widKind(id)) {
	case kPickRow:
		_listener->pickerSelectRow(widIndex(id));
		if (clickCount >= 2)
			_listener->pickerLaunch();
		break;
	case kPickRowPrecache: _listener->pickerPrecacheRow(widIndex(id)); break;
	case kPickRowRemove:   _listener->pickerRemoveRow(widIndex(id)); break;
	case kPickAddGame:     _listener->pickerAddGame(); break;
	case kPickPasses:
		_dropdownOpen = true;
		rebuild();
		break;
	case kPickPassOption:
		_dropdownOpen = false;
		rebuild(); // repaint sans dropdown NOW: the listener may open a modal over us
		_listener->pickerPassOption(widIndex(id));
		break;
	case kPickDebugToggle: _listener->pickerToggleDebug(); break;
	case kPickLaunch:      _listener->pickerLaunch(); break;
	default:
		break;
	}
}

void PickerViewWidget::handleMouseMoved(int x, int y, int button) {
	const uint32 id = hitTestWidgets(_widgets, x, y);
	if (id != _hoverId) {
		_hoverId = id;
		renderAll();
		markAsDirty();
	}
}

void PickerViewWidget::handleMouseWheel(int x, int y, int direction) {
	const int before = _scroll;
	_scroll = clampScroll(_scroll + direction, (int)_state.games.size(), _layout.rowsVisible);
	if (_scroll != before)
		rebuild();
}

void PickerViewWidget::handleMouseLeft(int button) {
	if (_hoverId != 0) {
		_hoverId = 0;
		renderAll();
		markAsDirty();
	}
}

} // namespace Roger
} // namespace Sci
