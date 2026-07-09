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

#ifndef SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_DIALOG_H
#define SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_DIALOG_H

#include "gui/dialog.h"
#include "sci/roger/launcher/roger_launcher.h"
#include "sci/roger/launcher/roger_picker_view.h"

namespace Sci {
namespace Roger {

// Modal shell for the custom-drawn picker: PickerViewWidget paints and
// hit-tests everything; this class owns the flows (precache tickle, add,
// remove-confirm, custom-passes input, cross-game switch, auto-actions).
class RogerLauncherDialog : public GUI::Dialog, public PickerActionListener {
public:
	explicit RogerLauncherDialog(RogerLauncher &launcher);

	void open() override;
	void handleTickle() override;

	// PickerActionListener
	void pickerSelectRow(int row) override;
	void pickerPrecacheRow(int row) override;
	void pickerRemoveRow(int row) override;
	void pickerAddGame() override;
	void pickerPassOption(int optionIndex) override;
	void pickerToggleDebug() override;
	void pickerLaunch() override;

private:
	RogerLauncher    &_launcher;
	LauncherState    &_state;   // alias for _launcher.state()
	PickerViewWidget *_view = nullptr;
	Common::Array<PassOption> _passOptions;
	bool _launchAfterPrecache = false;

	void rebuildPassOptions();
	void startPrecache(bool launchAfter);
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_DIALOG_H
