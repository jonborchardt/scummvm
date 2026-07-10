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

#include "sci/roger/launcher/roger_launcher_dialog.h"
#include "sci/roger/gen/roger_passes.h"
#include "gui/gui-manager.h"
#include "gui/widget.h"
#include "gui/widgets/edittext.h"
#include "gui/browser.h"
#include "gui/message.h"
#include "common/system.h"
#include "common/str.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "engines/metaengine.h"

namespace Sci {
namespace Roger {

static int gW() { return g_system->getOverlayWidth(); }
static int gH() { return g_system->getOverlayHeight(); }

// Minimal themed input box for the "Custom..." passes entry. Themed widgets
// are fine here: it stacks above the custom-drawn picker like the browser
// and message dialogs do.
class PassInputDialog : public GUI::Dialog {
public:
	explicit PassInputDialog(const Common::String &initial)
		: GUI::Dialog(gW() / 3, gH() * 2 / 5, gW() / 3, gH() / 5) {
		const int M = _w / 20, LH = _h / 5;
		new GUI::StaticTextWidget(this, M, M, _w - 2 * M, LH,
			Common::U32String("Passes (f/l/a per char; empty = default):"),
			Graphics::kTextAlignLeft);
		_edit = new GUI::EditTextWidget(this, M, M + LH + M / 2, _w - 2 * M, LH,
			Common::U32String(initial));
		new GUI::ButtonWidget(this, _w - 2 * (M + _w / 5), _h - M - LH, _w / 5, LH,
			Common::U32String("Cancel"), Common::U32String(), kCancelCmd);
		new GUI::ButtonWidget(this, _w - M - _w / 5, _h - M - LH, _w / 5, LH,
			Common::U32String("OK"), Common::U32String(), kOkCmd);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kOkCmd) { _accepted = true; close(); return; }
		if (cmd == kCancelCmd) { close(); return; }
		GUI::Dialog::handleCommand(sender, cmd, data);
	}
	bool accepted() const { return _accepted; }
	Common::String value() const { return _edit->getEditString().encode(); }
private:
	enum { kOkCmd = 'POK ', kCancelCmd = 'PCAN' };
	GUI::EditTextWidget *_edit = nullptr;
	bool _accepted = false;
};

RogerLauncherDialog::RogerLauncherDialog(RogerLauncher &launcher)
	: GUI::Dialog(gW() / 10, gH() / 10, gW() * 8 / 10, gH() * 8 / 10),
	  _launcher(launcher), _state(launcher.state()) {
	_view = new PickerViewWidget(this, 0, 0, _w, _h, _state, this);
}

void RogerLauncherDialog::open() {
	GUI::Dialog::open();
	rebuildPassOptions();
	_view->rebuild();
	// Empty-games guard: if no games are known, skip auto-actions (leave dialog
	// open so the user can Add Game). A zero-item precache would write a
	// meaningless roger_cache_stamp and auto-launch into nothing.
	if (_state.games.empty())
		return;
	// One-shot cross-game continuations consumed by RogerLauncher::run().
	// autoLaunch + already-cached never reaches the dialog (run() skips it).
	if (_launcher.autoLaunchPending())
		startPrecache(true);
	else if (_launcher.autoPrecachePending())
		startPrecache(false);
}

void RogerLauncherDialog::rebuildPassOptions() {
	_passOptions.clear();
	PassOption def;
	def.label = Common::String::format("%s  (default)", kDefaultPassString);
	def.value = ""; // empty value -> setPassesForSelected removes the key
	_passOptions.push_back(def);
	for (int i = 0; i < goodPassPatternCount(); ++i) {
		const GoodPassPattern &p = goodPassPattern(i);
		if (Common::String(p.compact) == kDefaultPassString)
			continue;
		PassOption o;
		o.label = Common::String::format("%s  (%s)", p.compact, p.note);
		o.value = p.compact;
		_passOptions.push_back(o);
	}
	const Common::String &cur = _state.settings.passes;
	bool listed = (cur == kDefaultPassString);
	for (uint i = 0; i < _passOptions.size() && !listed; ++i)
		listed = (_passOptions[i].value == cur);
	if (!listed && !cur.empty()) {
		PassOption o;
		o.label = cur + "  (current)";
		o.value = cur;
		_passOptions.push_back(o);
	}
	PassOption custom;
	custom.label = "Custom...";
	custom.isCustom = true;
	_passOptions.push_back(custom);
	_view->setPassOptions(_passOptions);
}

void RogerLauncherDialog::startPrecache(bool launchAfter) {
	_launchAfterPrecache = launchAfter;
	_launcher.buildPrecacheQueues();
	_view->rebuild();
	g_gui.scheduleTopDialogRedraw();
}

void RogerLauncherDialog::handleTickle() {
	if (_state.precaching) {
		const bool more = _launcher.precacheStep();
		_view->rebuild();
		g_gui.scheduleTopDialogRedraw();
		if (!more) {
			for (uint i = 0; i < _state.games.size(); ++i)
				_launcher.refreshCacheState(_state.games[i]);
			_view->rebuild();
			if (_launchAfterPrecache && !_state.cancelPrecache) {
				_launchAfterPrecache = false;
				_launcher.handleLaunch();
				close();
				return;
			}
			_launchAfterPrecache = false;
		}
	}
	GUI::Dialog::handleTickle();
}

void RogerLauncherDialog::pickerSelectRow(int row) {
	_launcher.selectGame(row);
	rebuildPassOptions();
	_view->rebuild();
}

void RogerLauncherDialog::pickerPrecacheRow(int row) {
	if (_state.precaching) { // Cancel (only offered on the active row)
		_launchAfterPrecache = false;
		_state.cancelPrecache = true;
		return;
	}
	if (isActiveGame(row)) {
		startPrecache(false);
	} else {
		_launcher.requestCrossGame(row, false);
		close();
	}
}

void RogerLauncherDialog::pickerRemoveRow(int row) {
	if (row < 0 || row >= (int)_state.games.size())
		return;
	const GameEntry &g = _state.games[row];
	GUI::MessageDialog confirm(Common::U32String(Common::String::format(
		"Remove %s from ScummVM?\nGame files on disk are not touched.",
		g.description.c_str())),
		Common::U32String("Remove"), Common::U32String("Cancel"));
	if (confirm.runModal() != GUI::kMessageOK)
		return;
	ConfMan.removeGameDomain(g.targetName);
	ConfMan.flushToDisk();
	_launcher.discoverGames();
	_launcher.selectGame(0);
	rebuildPassOptions();
	_view->rebuild();
	g_gui.scheduleTopDialogRedraw();
}

void RogerLauncherDialog::pickerAddGame() {
	// ── the ENTIRE body of the old `case kAddGameCmd:` moves here VERBATIM ──
	// (browser -> SCI detection -> VGA block -> resource.map fallback ->
	//  create <gameid>-roger/ -> ConfMan.flushToDisk()), then:

	// 1. Open directory browser.
	GUI::BrowserDialog browser(Common::U32String("Select SCI Game Directory"), true);
	if (browser.runModal() <= 0)
		return;

	const Common::FSNode &dir = browser.getResult();
	if (!dir.isDirectory())
		return;

	// 2. List directory contents for detection.
	Common::FSList files;
	if (!dir.getChildren(files, Common::FSNode::kListAll)) {
		GUI::MessageDialog err(Common::U32String("Could not open the selected directory."));
		err.runModal();
		return;
	}

	Common::String gameId;
	Common::String targetDomain;

	// 3. Try engine-level detection (MD5 matching).
	//    SCI's fallback detector calls assert(!g_sci), so it can fail during
	//    engine execution. MD5-based detection still works for known versions.
	bool vgaBlocked = false;
	{
		DetectionResults detectionResults = EngineMan.detectGames(files);
		DetectedGames candidates = detectionResults.listDetectedGames();
		for (uint i = 0; i < candidates.size(); ++i) {
			if (candidates[i].engineId == "sci") {
				// Roger is EGA-only. Block VGA games at add-time.
				if (candidates[i].getGUIOptions().contains("vga")) {
					vgaBlocked = true;
				} else {
					targetDomain = EngineMan.createTargetForGame(candidates[i]);
					ConfMan.setPath("path", dir.getPath(), targetDomain);
					gameId = candidates[i].gameId;
				}
				break;
			}
		}
	}
	if (vgaBlocked) {
		GUI::MessageDialog err(Common::U32String(
			"Roger supports EGA SCI games only.\n"
			"This game requires VGA graphics and cannot be added."));
		err.runModal();
		return;
	}

	// 4. If engine detection found nothing, check for SCI resource files directly.
	//    SCI games always have resource.map + resource.001 (or equivalent).
	if (gameId.empty()) {
		bool hasResMap = false, hasResVol = false;
		for (uint i = 0; i < files.size(); ++i) {
			const Common::String n = files[i].getName();
			if (n.equalsIgnoreCase("resource.map") ||
			    n.equalsIgnoreCase("resmap.000")   ||
			    n.equalsIgnoreCase("resmap.001"))
				hasResMap = true;
			if (n.equalsIgnoreCase("resource.000") ||
			    n.equalsIgnoreCase("resource.001") ||
			    n.equalsIgnoreCase("ressci.000")   ||
			    n.equalsIgnoreCase("ressci.001"))
				hasResVol = true;
		}
		if (!hasResMap || !hasResVol) {
			GUI::MessageDialog err(Common::U32String(
				"No SCI game found in that directory.\n"
				"Please select the folder that contains resource.map."));
			err.runModal();
			return;
		}
		// Use directory name as game ID (lowercased).
		gameId = dir.getName();
		gameId.toLowercase();
		if (gameId.empty()) gameId = "sci_game";
		// Generate unique ConfMan domain.
		Common::String baseDomain = gameId;
		int suffix = 1;
		targetDomain = baseDomain;
		while (ConfMan.hasGameDomain(targetDomain))
			targetDomain = Common::String::format("%s-%d", baseDomain.c_str(), suffix++);
		ConfMan.addGameDomain(targetDomain);
		ConfMan.set("engineid",    "sci",         targetDomain);
		ConfMan.set("gameid",      gameId,         targetDomain);
		ConfMan.set("description", dir.getName(),  targetDomain);
		ConfMan.setPath("path",    dir.getPath(),  targetDomain);
	}

	// 5. Create <gameid>-roger/ sibling directory.
	Common::Path rogerPath = dir.getPath().getParent()
	                             .appendComponent(gameId + "-roger");
	Common::FSNode(rogerPath).createDirectory();

	// 6. Persist and refresh.
	ConfMan.flushToDisk();
	_launcher.discoverGames();
	_launcher.selectGame(0);
	rebuildPassOptions();
	_view->rebuild();
	g_gui.scheduleTopDialogRedraw();
}

void RogerLauncherDialog::pickerPassOption(int optionIndex) {
	if (optionIndex < 0 || optionIndex >= (int)_passOptions.size())
		return;
	const PassOption &o = _passOptions[optionIndex];
	if (o.isCustom) {
		PassInputDialog input(_state.settings.passes);
		input.runModal();
		if (input.accepted())
			_launcher.setPassesForSelected(input.value());
	} else {
		_launcher.setPassesForSelected(o.value);
	}
	rebuildPassOptions();
	_view->rebuild();
	g_gui.scheduleTopDialogRedraw();
}

void RogerLauncherDialog::pickerToggleDebug() {
	_launcher.setDebugLogForSelected(!_state.settings.debugLog);
	_view->rebuild();
}

void RogerLauncherDialog::pickerLaunch() {
	if (_state.games.empty() || _state.precaching)
		return;
	const int ar = _state.activeRow;
	if (isActiveGame(_state.selectedIndex) && ar >= 0 && !_state.games[ar].cached) {
		startPrecache(true); // always-precache-before-launch, active game
		return;
	}
	_launcher.handleLaunch(); // same game -> proceed; other game -> one-shot switch
	close();
}

} // namespace Roger
} // namespace Sci
