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
#include "sci/roger/launcher/roger_picker_view.h"
#include "sci/roger/gen/roger_passes.h"
#include "sci/roger/ui/roger_widgets.h"
#include "sci/roger/ui/roger_panel_style.h"
#include "gui/gui-manager.h"
#include "gui/ThemeEngine.h"
#include "gui/widget.h"
#include "gui/browser.h"
#include "gui/message.h"
#include "graphics/managed_surface.h"
#include "common/system.h"
#include "common/str.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/util.h"
#include "engines/metaengine.h"

namespace Sci {
namespace Roger {

static int gW() { return g_system->getOverlayWidth(); }
static int gH() { return g_system->getOverlayHeight(); }

// ---------------------------------------------------------------------------
// PassBuilderWidget -- custom-drawn, mouse-first pass builder in the picker's
// own visual language (dark navy, blue accents, PanelWidget hit-testing).
// Private to this translation unit.
// ---------------------------------------------------------------------------
namespace {

// Widget kinds for the pass builder.
enum PassBuilderKind {
	kPBNone = 0,
	kPBAppendF,
	kPBAppendL,
	kPBAppendA,
	kPBDel,
	kPBClear,
	kPBOK,
	kPBCancel,
};

} // anonymous namespace

class PassBuilderWidget : public GUI::Widget {
public:
	PassBuilderWidget(GUI::GuiObject *boss, int x, int y, int w, int h,
	                  const Common::String &initial);

	bool accepted() const { return _accepted; }
	Common::String value() const { return _working; }

protected:
	void drawWidget() override;
	void handleMouseDown(int x, int y, int button, int clickCount) override;
	void handleMouseMoved(int x, int y, int button) override;
	void handleMouseLeft(int button) override;
	bool handleKeyDown(Common::KeyState state) override;

private:
	Graphics::ManagedSurface   _canvas;
	Common::String             _working;     // the pass string being built
	bool                       _accepted = false;
	Common::Array<PanelWidget> _widgets;
	uint32                     _hoverId = 0;
	PanelFonts                 _fonts;

	void loadFonts();
	void buildWidgets();
	void render();
};

PassBuilderWidget::PassBuilderWidget(GUI::GuiObject *boss, int x, int y, int w, int h,
                                     const Common::String &initial)
	: GUI::Widget(boss, x, y, w, h) {
	setFlags(GUI::WIDGET_ENABLED | GUI::WIDGET_TRACK_MOUSE);
	_canvas.create(w, h, g_system->getOverlayFormat());
	// Canonicalize initial string to compact form so legacy "f f f" seeds don't
	// mix separators with appended chars.
	_working = passString(parsePassString(initial));
	loadFonts();
	buildWidgets();
	render();
}

void PassBuilderWidget::loadFonts() {
	const int sizes[kFontRoleCount] = { 0, 0, _h / 8, 0, _h / 9 };
	_fonts.load(sizes);
}

void PassBuilderWidget::buildWidgets() {
	_widgets.clear();
	// Layout constants (all relative to widget-local coords).
	const int M  = MAX(4, _h / 14);   // outer margin
	const int BH = MAX(12, _h / 7);   // button height
	const int BW = MAX(20, _w / 7);   // chip width (+f/+l/+a/del/clear)
	const int gap = MAX(2, _w / 60);  // inter-button gap

	// Row 3: builder chips (+f +l +a del clear)
	const int chipY = _h / 2 - BH / 2;
	// Center the 5 chips.
	const int totalChips = 5 * BW + 4 * gap;
	int chipX = (_w - totalChips) / 2;

	struct { PassBuilderKind kind; const char *label; } chips[5] = {
		{ kPBAppendF, "+f" },
		{ kPBAppendL, "+l" },
		{ kPBAppendA, "+a" },
		{ kPBDel,     "del" },
		{ kPBClear,   "clear" },
	};
	for (int i = 0; i < 5; ++i) {
		PanelWidget pw;
		pw.rect = Common::Rect(chipX, chipY, chipX + BW, chipY + BH);
		pw.id = widId(chips[i].kind);
		pw.label = chips[i].label;
		pw.on = false;
		pw.enabled = true;
		_widgets.push_back(pw);
		chipX += BW + gap;
	}

	// Row 4: Cancel (left) and OK (right).
	const int btnY  = _h - M - BH;
	const int btnW  = MAX(36, _w / 5);

	PanelWidget cancel;
	cancel.rect    = Common::Rect(M, btnY, M + btnW, btnY + BH);
	cancel.id      = widId(kPBCancel);
	cancel.label   = "Cancel";
	cancel.on      = false;
	cancel.enabled = true;
	_widgets.push_back(cancel);

	PanelWidget ok;
	ok.rect    = Common::Rect(_w - M - btnW, btnY, _w - M, btnY + BH);
	ok.id      = widId(kPBOK);
	ok.label   = "OK";
	ok.on      = false;
	ok.enabled = true;
	_widgets.push_back(ok);
}

void PassBuilderWidget::render() {
	using namespace PanelStyle;

	PanelPainter paint(_canvas, _fonts);

	// Background: same dark navy as the picker's panels.
	const Common::Rect full(0, 0, _w, _h);
	paint.blendFill(full, kPanelFill, 255);
	paint.strokeRect(full, kPanelLine);

	const int M  = MAX(4, _h / 14);
	const int BH = MAX(12, _h / 7);
	const int pad = MAX(3, _w / 60);

	// Row 1: title.
	{
		Common::Rect titleR(M, M, _w - M, M + BH);
		paint.drawTextIn(kFontBody, "Omyac passes", titleR, kBlue,
		           Graphics::kTextAlignLeft);
	}

	// Row 2: current string display field.
	{
		const int fieldY = M + BH + pad;
		const int fieldH = BH;
		Common::Rect fieldR(M, fieldY, _w - M, fieldY + fieldH);
		paint.blendFill(fieldR, kFieldFill, 220);
		paint.strokeRect(fieldR, kPanelLine);
		Common::Rect txtR = fieldR;
		txtR.left += 2 * pad;
		if (_working.empty())
			paint.drawTextIn(kFontMono, "(empty = default)", txtR,
			           kTextDim, Graphics::kTextAlignLeft);
		else
			paint.drawTextIn(kFontMono, _working, txtR,
			           kText, Graphics::kTextAlignLeft);
	}

	// Row 3: builder chips (drawn from _widgets).
	for (uint i = 0; i < _widgets.size(); ++i) {
		const PanelWidget &pw = _widgets[i];
		const int kind = widKind(pw.id);
		if (kind == kPBCancel || kind == kPBOK)
			continue;
		// del and clear in amber; append chips in blue.
		const bool isDestructive = (kind == kPBDel || kind == kPBClear);
		const Rgb &accent = isDestructive ? kAmber : kBlue;
		paint.drawButton(pw.rect, pw.label, accent, false, true, _hoverId == pw.id, kFontBody);
	}

	// Row 4: Cancel (red outline) and OK (blue filled).
	for (uint i = 0; i < _widgets.size(); ++i) {
		const PanelWidget &pw = _widgets[i];
		const int kind = widKind(pw.id);
		if (kind == kPBCancel)
			paint.drawButton(pw.rect, pw.label, kRed, false, true, _hoverId == pw.id, kFontBody);
		else if (kind == kPBOK)
			paint.drawButton(pw.rect, pw.label, kBlue, true, true, _hoverId == pw.id, kFontBody);
	}
}

void PassBuilderWidget::drawWidget() {
	g_gui.theme()->drawManagedSurface(Common::Point(_x, _y), _canvas, Graphics::ALPHA_OPAQUE);
}

void PassBuilderWidget::handleMouseDown(int x, int y, int button, int clickCount) {
	const uint32 id = hitTestWidgets(_widgets, x, y);
	switch (widKind(id)) {
	case kPBAppendF: _working += 'f'; break;
	case kPBAppendL: _working += 'l'; break;
	case kPBAppendA: _working += 'a'; break;
	case kPBDel:
		if (!_working.empty())
			_working.deleteLastChar();
		break;
	case kPBClear:
		_working.clear();
		break;
	case kPBOK:
		_accepted = true;
		((GUI::Dialog *)_boss)->close();
		return;
	case kPBCancel:
		((GUI::Dialog *)_boss)->close();
		return;
	default:
		break;
	}
	render();
	markAsDirty();
}

void PassBuilderWidget::handleMouseMoved(int x, int y, int button) {
	const uint32 id = hitTestWidgets(_widgets, x, y);
	if (id != _hoverId) {
		_hoverId = id;
		render();
		markAsDirty();
	}
}

void PassBuilderWidget::handleMouseLeft(int button) {
	if (_hoverId != 0) {
		_hoverId = 0;
		render();
		markAsDirty();
	}
}

bool PassBuilderWidget::handleKeyDown(Common::KeyState state) {
	// Escape = cancel.
	if (state.keycode == Common::KEYCODE_ESCAPE) {
		((GUI::Dialog *)_boss)->close();
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// PassBuilderDialog -- modal shell hosting PassBuilderWidget.
// Same stacking as the old PassInputDialog (runModal from pickerPassOption).
// ---------------------------------------------------------------------------
class PassBuilderDialog : public GUI::Dialog {
public:
	explicit PassBuilderDialog(const Common::String &initial)
		: GUI::Dialog(gW() / 4, gH() * 3 / 8, gW() / 2, gH() / 4) {
		_widget = new PassBuilderWidget(this, 0, 0, _w, _h, initial);
	}
	bool accepted() const { return _widget->accepted(); }
	Common::String value() const { return _widget->value(); }
private:
	PassBuilderWidget *_widget = nullptr;
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
	// open so the user can Add Game). A zero-item precache would
	// start a zero-item precache and auto-launch into nothing.
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
		PassBuilderDialog input(_state.settings.passes);
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
