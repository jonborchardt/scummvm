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
#include "common/system.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

using namespace PanelStyle;

PickerViewWidget::PickerViewWidget(GUI::GuiObject *boss, int x, int y, int w, int h,
                                   const LauncherState &state, PickerActionListener *listener)
	: GUI::Widget(boss, x, y, w, h), _state(state), _listener(listener) {
	setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_TRACK_MOUSE);
	_canvas.create(w, h, g_system->getOverlayFormat());
	loadFonts();
	bakeBackground();
	rebuild();
}

void PickerViewWidget::setPassOptions(const Common::Array<PassOption> &opts) {
	_passOptions = opts;
}

void PickerViewWidget::loadFonts() {
	const int sizes[kFontRoleCount] = { _h / 16, _h / 34, _h / 38, _h / 46, _h / 38 };
	_fonts.load(sizes);
}

void PickerViewWidget::bakeBackground() {
	_bgBaked.create(_w, _h, _canvas.format);
	PanelPainter bg(_bgBaked, _fonts);
	bg.gradientFill(Common::Rect(0, 0, _w, _h), kGradTop, kGradBottom);
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

void PickerViewWidget::drawButtonRect(PanelPainter &paint, const Common::Rect &r,
                                      const Common::String &label, const Rgb &accent,
                                      bool filled, bool enabled, uint32 id) {
	paint.drawButton(r, label, accent, filled, enabled, enabled && _hoverId == id);
}

void PickerViewWidget::drawRow(PanelPainter &paint, int visIdx, int row) {
	const PickerRowLayout &r = _layout.rows[visIdx];
	const GameEntry &g = _state.games[row];
	const bool selected = (row == _state.selectedIndex);
	const int pad = MAX(2, _h / 80);

	paint.blendFill(r.card, kCardFill, 235);
	if (selected)
		paint.strokeRect(r.card, kBlue);
	else
		paint.strokeRect(r.card, kPanelLine);

	// Title + subtitle (left column)
	const int leftW = r.badge.left - r.card.left - 2 * pad;
	Common::Rect titleR(r.card.left + 2 * pad, r.card.top + pad,
	                    r.card.left + 2 * pad + leftW, r.card.top + r.card.height() / 2);
	Common::Rect subR(titleR.left, titleR.bottom,
	                  titleR.right, r.card.bottom - pad);
	paint.drawTextIn(kFontBody, Common::String::format("%d. %s", row + 1, g.description.c_str()),
	           titleR, selected ? kBlue : kText, Graphics::kTextAlignLeft);

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
		paint.drawTextIn(kFontSmall, info, subR, kTextDim, Graphics::kTextAlignLeft);
	}

	// Badge area: precache progress on the active row while running, else status.
	const bool isActive = (row == _state.activeRow);
	if (isActive && _state.precaching) {
		Common::Rect txt(r.badge.left, r.badge.top, r.badge.right,
		                 r.badge.top + r.badge.height() / 2);
		paint.drawTextIn(kFontSmall, _state.precacheStatus, txt, kText,
		           Graphics::kTextAlignLeft);
		Common::Rect bar(r.badge.left, txt.bottom + pad / 2, r.badge.right,
		                 txt.bottom + pad / 2 + MAX(3, _h / 160));
		paint.strokeRect(bar, kPanelLine);
		const int total = MAX(1, _state.precacheTotal);
		Common::Rect fill = bar;
		fill.right = fill.left + (int16)((bar.width() * CLIP(_state.precacheDone, 0, total)) / total);
		paint.blendFill(fill, kBlue, 255);
	} else {
		const Rgb c = g.cached ? kGreen : kAmber;
		Common::Rect line1(r.badge.left, r.badge.top, r.badge.right,
		                   r.badge.top + r.badge.height() / 2);
		Common::Rect line2(r.badge.left, line1.bottom, r.badge.right, r.badge.bottom);
		paint.drawTextIn(kFontBody, g.cached ? "Cached" : "! Not Cached", line1,
		           c, Graphics::kTextAlignLeft);
		paint.drawTextIn(kFontSmall, g.cached ? "Ready" : "Not ready", line2,
		           kTextDim, Graphics::kTextAlignLeft);
	}

	// Buttons (labels/enabled state mirror buildWidgets exactly).
	const bool busy = _state.precaching;
	if ((!g.cached && !busy) || (isActive && busy))
		drawButtonRect(paint, r.precache, (isActive && busy) ? "Cancel" : "Precache",
		               (isActive && busy) ? kRed : kBlue,
		               false, (isActive && busy) || !busy, widId(kPickRowPrecache, row));
	drawButtonRect(paint, r.remove, "Remove", kRed, false,
	               !busy, widId(kPickRowRemove, row));
}

void PickerViewWidget::drawSettings(PanelPainter &paint) {
	paint.blendFill(_layout.settingsPanel, kPanelFill, 216);
	paint.strokeRect(_layout.settingsPanel, kPanelLine);
	if (_state.games.empty())
		return;
	const GameEntry &g = _state.games[_state.selectedIndex];
	Common::String forWhom = g.subtitle.empty()
		? g.description
		: Common::String::format("%s (%s)", g.description.c_str(), g.subtitle.c_str());
	paint.drawTextIn(kFontBody, "Settings for: " + forWhom, _layout.settingsTitle,
	           kBlue, Graphics::kTextAlignLeft);

	paint.drawTextIn(kFontBody, "Omyac passes", _layout.passesLabel, kText,
	           Graphics::kTextAlignLeft);
	paint.blendFill(_layout.passesField, kFieldFill, 200);
	paint.strokeRect(_layout.passesField,
	           _hoverId == widId(kPickPasses) ? kBlue : kPanelLine);
	const int pad = MAX(2, _h / 80);
	Common::Rect fieldText = _layout.passesField;
	fieldText.left += 2 * pad;
	paint.drawTextIn(kFontMono, _state.settings.passes + "  v", fieldText,
	           kText, Graphics::kTextAlignLeft);
	paint.drawTextIn(kFontSmall, "Rendering pass sequence (roger_omyac_passes).",
	           _layout.passesHint, kTextDim, Graphics::kTextAlignLeft);

	paint.drawTextIn(kFontBody, "Debug Logging", _layout.debugLabel,
	           kText, Graphics::kTextAlignLeft);
	// Toggle pill: filled+knob-right when on.
	const bool on = _state.settings.debugLog;
	paint.drawTogglePill(_layout.debugToggle, on);
	paint.drawTextIn(kFontSmall, "Enable roger_debug in this game's INI section.",
	           _layout.debugHint, kTextDim, Graphics::kTextAlignLeft);
}

void PickerViewWidget::drawDropdown(PanelPainter &paint) {
	Rgb rowHi = {26, 38, 64};
	Rgb row   = {14, 20, 34};
	for (uint i = 0; i < _passOptions.size(); ++i) {
		const Common::Rect r = passOptionRect(_layout, (int)i, (int)_passOptions.size(), _h);
		const bool hovered = _hoverId == widId(kPickPassOption, (int)i);
		paint.blendFill(r, hovered ? rowHi : row, 245);
		paint.strokeRect(r, kPanelLine);
		const int pad = MAX(2, _h / 80);
		Common::Rect txt = r;
		txt.left += 2 * pad;
		paint.drawTextIn(kFontMono, _passOptions[i].label, txt, kText,
		           Graphics::kTextAlignLeft);
	}
}

void PickerViewWidget::renderAll() {
	_canvas.blitFrom(_bgBaked);
	PanelPainter paint(_canvas, _fonts);

	paint.drawTextIn(kFontTitle, "ROGER", _layout.titleBox, kText, Graphics::kTextAlignLeft);
	paint.drawTextIn(kFontSmall,
	           "A high-resolution overlay renderer for classic Sierra SCI games. "
	           "Select a game to configure rendering passes and settings.",
	           _layout.descBox, kTextDim, Graphics::kTextAlignLeft);

	paint.blendFill(_layout.listPanel, kPanelFill, 216);
	paint.strokeRect(_layout.listPanel, kPanelLine);

	if (_state.games.empty()) {
		paint.drawTextIn(kFontBody, "No SCI games found - use + Add Game below.",
		           _layout.listPanel, kTextDim, Graphics::kTextAlignCenter);
	} else {
		for (uint i = 0; i < _layout.rows.size(); ++i)
			drawRow(paint, (int)i, _scroll + (int)i);
	}

	drawButtonRect(paint, _layout.addGame, "+ Add Game", kBlue, false,
	               !_state.precaching, widId(kPickAddGame));
	drawSettings(paint);
	drawButtonRect(paint, _layout.launch, "Launch Game", kBlue, true,
	               !_state.games.empty() && !_state.precaching, widId(kPickLaunch));
	if (_dropdownOpen)
		drawDropdown(paint);
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
