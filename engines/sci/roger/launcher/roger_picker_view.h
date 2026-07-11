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

#ifndef SCI_ROGER_LAUNCHER_ROGER_PICKER_VIEW_H
#define SCI_ROGER_LAUNCHER_ROGER_PICKER_VIEW_H

// Custom-drawn picker view: composes the entire picker into a ManagedSurface
// (background gradient, card panels, styled buttons, text) and hit-tests clicks
// itself -- the same immediate-mode pattern as the F12 tune panel / Studio,
// hosted inside the GUI::Dialog shell. All geometry comes from layoutPicker()
// (roger_picker_model, unit-tested); this file only paints.

#include "gui/widget.h"
#include "graphics/managed_surface.h"
#include "sci/roger/ui/roger_widgets.h"
#include "sci/roger/ui/roger_panel_style.h"
#include "sci/roger/launcher/roger_launcher.h"
#include "sci/roger/launcher/roger_picker_model.h"

namespace Sci {
namespace Roger {

struct PassOption {
	Common::String label;   // display text
	Common::String value;   // roger_omyac_passes value; "" = Default (remove key)
	bool isCustom = false;  // "Custom..." -> open the input sub-dialog
};

class PickerActionListener {
public:
	virtual ~PickerActionListener() {}
	virtual void pickerSelectRow(int row) = 0;
	virtual void pickerPrecacheRow(int row) = 0; // Cancel while precaching (row 0)
	virtual void pickerRemoveRow(int row) = 0;
	virtual void pickerAddGame() = 0;
	virtual void pickerPassOption(int optionIndex) = 0;
	virtual void pickerToggleDebug() = 0;
	virtual void pickerLaunch() = 0;
};

class PickerViewWidget : public GUI::Widget {
public:
	PickerViewWidget(GUI::GuiObject *boss, int x, int y, int w, int h,
	                 const LauncherState &state, PickerActionListener *listener);

	void setPassOptions(const Common::Array<PassOption> &opts);
	void rebuild(); // recompute layout + widgets, re-render, markAsDirty

protected:
	void drawWidget() override;
	void handleMouseDown(int x, int y, int button, int clickCount) override;
	void handleMouseMoved(int x, int y, int button) override;
	void handleMouseWheel(int x, int y, int direction) override;
	void handleMouseLeft(int button) override;

private:
	const LauncherState      &_state;
	PickerActionListener     *_listener;
	Graphics::ManagedSurface  _canvas;   // full widget, re-rendered on state change
	Graphics::ManagedSurface  _bgBaked;  // background pre-scaled to widget size
	Common::Array<PassOption> _passOptions;
	PickerLayout              _layout;
	Common::Array<PanelWidget> _widgets; // hit-test set; options first (drawn on top)
	uint32 _hoverId = 0;
	bool   _dropdownOpen = false;
	int    _scroll = 0;
	PanelFonts _fonts;

	void loadFonts();
	void bakeBackground();
	void buildWidgets();
	void renderAll();
	void drawButtonRect(PanelPainter &paint, const Common::Rect &r,
	                    const Common::String &label, const PanelStyle::Rgb &accent,
	                    bool filled, bool enabled, uint32 id);
	void drawRow(PanelPainter &paint, int visIdx, int row);
	void drawSettings(PanelPainter &paint);
	void drawDropdown(PanelPainter &paint);
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_LAUNCHER_ROGER_PICKER_VIEW_H
