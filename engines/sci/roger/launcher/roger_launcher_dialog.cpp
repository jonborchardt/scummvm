#include "sci/roger/launcher/roger_launcher_dialog.h"
#include "sci/roger/gen/roger_passes.h"
#include "gui/gui-manager.h"
#include "gui/widget.h"
#include "gui/widgets/list.h"
#include "gui/widgets/popup.h"
#include "gui/widgets/edittext.h"
#include "common/util.h"  // CLIP
#include "gui/browser.h"
#include "gui/message.h"
#include "common/system.h"
#include "common/str.h"
#include "common/translation.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "engines/metaengine.h"

namespace Sci {
namespace Roger {

// Read-only progress bar: a SliderWidget that draws a themed track + fill
// (drawSlider) but ignores all mouse input so the user cannot drag it.
class ProgressBarWidget : public GUI::SliderWidget {
public:
	ProgressBarWidget(GUI::GuiObject *boss, int x, int y, int w, int h)
		: GUI::SliderWidget(boss, x, y, w, h) {}
	void handleMouseMoved(int, int, int) override {}
	void handleMouseDown(int, int, int, int) override {}
	void handleMouseUp(int, int, int, int) override {}
	void handleMouseWheel(int, int, int) override {}
};

static const char *kPrecacheVals[] = { "off", "pics", "views", "all" };
static const char *kFontVals[]     = {
	"ms_sans_serif.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf",
	"LiberationSerif-Regular.ttf", "GoMono-Regular.ttf",
	"LiberationMono-Regular.ttf", "SourceCodeVariable-Roman.ttf"
};
static const char *kFallbackVals[] = { "prebuilt", "cache", "memory", "always" };

// Layout helpers: all coordinates are overlay pixels.
static int gW() { return g_system->getOverlayWidth(); }
static int gH() { return g_system->getOverlayHeight(); }

GUI::PopUpWidget *RogerLauncherDialog::addSettingsRow(int y, int M, int LH,
                                                       const char *label, uint32 cmd) {
	const int labelW = _w / 5;
	const int popW   = _w / 4;
	new GUI::StaticTextWidget(this, M, y, labelW, LH,
	                          Common::U32String(label), Graphics::kTextAlignLeft);
	return new GUI::PopUpWidget(this, M + labelW + M/2, y, popW, LH,
	                            Common::U32String(), cmd);
}

RogerLauncherDialog::RogerLauncherDialog(RogerLauncher &launcher)
	: GUI::Dialog(gW() / 10, gH() / 10, gW() * 8 / 10, gH() * 8 / 10),
	  _launcher(launcher), _state(launcher.state()) {

	const int W = _w, H = _h;
	const int M  = W / 30;   // margin
	const int LH = H / 20;   // line height

	// â”€â”€ Title â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
	new GUI::StaticTextWidget(this, M, M, W / 4, LH,
	                          Common::U32String("ROGER"), Graphics::kTextAlignLeft);

	// â”€â”€ Games section â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
	const int listTop = M + LH + M;
	const int listH   = H * 2 / 5;
	const int btnColW = W / 5;                       // width of Add/Delete buttons
	const int listW   = W - 2*M - btnColW - M;       // list narrowed for right column

	new GUI::StaticTextWidget(this, M, listTop, W / 4, LH,
	                          Common::U32String("GAMES"), Graphics::kTextAlignLeft);

	_gameList = new GUI::ListWidget(this, M, listTop + LH, listW, listH - LH,
	                                Common::U32String(), kGameSelCmd);

	// Right column: stacked Add / Delete buttons flush with list top.
	const int rightX = M + listW + M;
	_addGameBtn = new GUI::ButtonWidget(this, rightX, listTop + LH, btnColW, LH,
	                                    Common::U32String("+ Add Game"),
	                                    Common::U32String(), kAddGameCmd);
	_deleteBtn  = new GUI::ButtonWidget(this, rightX, listTop + LH*2 + M/2, btnColW, LH,
	                                    Common::U32String("Delete"),
	                                    Common::U32String(), kDeleteCmd);

	// â”€â”€ Settings section â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
	const int settTop = listTop + listH + M;
	new GUI::StaticTextWidget(this, M, settTop, W - 2*M, LH,
	                          Common::U32String("SETTINGS"), Graphics::kTextAlignLeft);

	_precachePop = addSettingsRow(settTop + LH + 0*(LH + M/3), M, LH, "Pre-cache",   kPrecachePopCmd);
	{
		const int y = settTop + LH + 1*(LH + M/3);
		const int labelW = _w / 5;
		new GUI::StaticTextWidget(this, M, y, labelW, LH,
		                          Common::U32String("Passes"), Graphics::kTextAlignLeft);
		_passesEdit = new GUI::EditTextWidget(this, M + labelW + M/2, y, _w / 4, LH,
		                                      Common::U32String());
		// Known-good suggestions (roger_passes registry): one click fills the
		// field. An emptied field launches with the default (key removed).
		int bx = M + labelW + M/2 + _w / 4 + M/2;
		const int bw = (W - M - bx - (goodPassPatternCount() - 1) * M/4) / MAX(1, goodPassPatternCount());
		for (int i = 0; i < goodPassPatternCount() && i < 4; ++i) {
			const GoodPassPattern &p = goodPassPattern(i);
			new GUI::ButtonWidget(this, bx, y, bw, LH,
			                      Common::U32String(p.compact),
			                      Common::U32String(Common::String::format(
			                          "known-good: %s (click to use)", p.note)),
			                      kGoodPass0Cmd + (uint32)i);
			bx += bw + M/4;
		}
	}
	_fontPop     = addSettingsRow(settTop + LH + 2*(LH + M/3), M, LH, "Font",        kFontPopCmd);
	_fallbackPop = addSettingsRow(settTop + LH + 3*(LH + M/3), M, LH, "Fallback",    kFallbackPopCmd);

	// â”€â”€ Bottom row â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
	const int btnY = H - M - LH;

	// Progress label sits in its own row above the buttons.
	_progressLbl = new GUI::StaticTextWidget(this, M, btnY - LH - M/2, W - 2*M, LH,
	                                         Common::U32String(""), Graphics::kTextAlignLeft);

	_precacheBtn = new GUI::ButtonWidget(this, M, btnY, W/5, LH,
	                                     Common::U32String("Precache Now"),
	                                     Common::U32String(), kPrecacheCmd);

	// Progress bar immediately to the right of the Precache button, vertically
	// centered against the button. Hidden until precaching is active.
	const int barX = M + W/5 + M/2;
	const int barH = LH / 2;
	const int barW = (W - M - W/6) - barX - M;   // up to the Launch button
	_progressBar = new ProgressBarWidget(this, barX, btnY + (LH - barH) / 2, barW, barH);
	_progressBar->setMinValue(0);
	_progressBar->setMaxValue(1);
	_progressBar->setValue(0);
	_progressBar->setVisible(false);

	_launchBtn   = new GUI::ButtonWidget(this, W - M - W/6, btnY, W/6, LH,
	                                     Common::U32String("Launch"),
	                                     Common::U32String(), kLaunchCmd);

	// â”€â”€ Populate popup options â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
	_precachePop->appendEntry(Common::U32String("off"),   0);
	_precachePop->appendEntry(Common::U32String("pics"),  1);
	_precachePop->appendEntry(Common::U32String("views"), 2);
	_precachePop->appendEntry(Common::U32String("all"),   3);

	for (int i = 0; i < 7; ++i)
		_fontPop->appendEntry(Common::U32String(kFontVals[i]), (uint32)i);

	_fallbackPop->appendEntry(Common::U32String("native (prebuilt)"), 0);
	_fallbackPop->appendEntry(Common::U32String("cache"),             1);
	_fallbackPop->appendEntry(Common::U32String("memory"),            2);
	_fallbackPop->appendEntry(Common::U32String("always"),            3);

	// Launch and Delete start disabled; enabled on game selection.
	_launchBtn->setEnabled(false);
	_deleteBtn->setEnabled(false);
}

void RogerLauncherDialog::open() {
	GUI::Dialog::open();
	rebuildGameList();
	rebuildSettings();
	bool hasGames = !_state.games.empty();
	_launchBtn->setEnabled(hasGames);
	_deleteBtn->setEnabled(hasGames);
}

void RogerLauncherDialog::rebuildGameList() {
	Common::U32StringArray entries;
	for (uint i = 0; i < _state.games.size(); ++i) {
		const GameEntry &g = _state.games[i];
		Common::String status;
		if (g.cache.picCount == 0 && g.cache.viewCount == 0)
			status = "not cached";
		else if (g.cache.viewCount == 0)
			status = Common::String::format("%d pics cached", g.cache.picCount);
		else
			status = Common::String::format("%d pics + %d views", g.cache.picCount, g.cache.viewCount);
		entries.push_back(Common::U32String(
			Common::String::format("%-30s  %s", g.description.c_str(), status.c_str())));
	}
	_gameList->setList(entries);
	_gameList->setSelected(_state.selectedIndex);
}

void RogerLauncherDialog::rebuildSettings() {
	const LauncherSettings &s = _state.settings;

	for (int i = 0; i < 4; ++i)
		if (s.precache == kPrecacheVals[i]) { _precachePop->setSelectedTag((uint32)i); break; }
	_passesEdit->setEditString(Common::U32String(s.passes));
	for (int i = 0; i < 7; ++i)
		if (s.font == kFontVals[i]) { _fontPop->setSelectedTag((uint32)i); break; }
	for (int i = 0; i < 4; ++i)
		if (s.fallback == kFallbackVals[i]) { _fallbackPop->setSelectedTag((uint32)i); break; }
}

// EditTextWidget doesn't push per-keystroke commands the way the popups do;
// pull its text into the settings at the moments they are consumed.
void RogerLauncherDialog::syncPassesFromField() {
	if (_passesEdit)
		_state.settings.passes = _passesEdit->getEditString().encode();
}

void RogerLauncherDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kGoodPass0Cmd + 0:
	case kGoodPass0Cmd + 1:
	case kGoodPass0Cmd + 2:
	case kGoodPass0Cmd + 3: {
		// Known-good suggestion clicked: fill the Passes field with it.
		const GoodPassPattern &p = goodPassPattern((int)(cmd - kGoodPass0Cmd));
		if (_passesEdit) {
			_passesEdit->setEditString(Common::U32String(p.compact));
			_passesEdit->markAsDirty();
		}
		_state.settings.passes = p.compact;
		break;
	}
	case kLaunchCmd: {
		syncPassesFromField();
		if (_state.games.empty()) break;
		const GameEntry &g    = _state.games[_state.selectedIndex];
		const bool isCurrent  = (g.targetName == ConfMan.getActiveDomainName());
		const bool noCache    = (g.cache.picCount == 0 && g.cache.viewCount == 0);
		if (isCurrent && noCache) {
			_launchAfterPrecache = true;
			_launcher.buildPrecacheQueues();
			_precacheBtn->setLabel(Common::U32String("Cancel"));
			updateProgress();
			g_gui.scheduleTopDialogRedraw();
		} else {
			if (_launcher.handleLaunch())
				close();
			else
				close();
		}
		break;
	}
	case kPrecacheCmd:
		syncPassesFromField();
		if (_state.precaching) {
			_launchAfterPrecache = false;
			_state.cancelPrecache = true;
			_precacheBtn->setLabel(Common::U32String("Precache Now"));
		} else {
			_launcher.buildPrecacheQueues();
			_precacheBtn->setLabel(Common::U32String("Cancel"));
			updateProgress();
			g_gui.scheduleTopDialogRedraw();
		}
		break;
	case kAddGameCmd: {
		// 1. Open directory browser.
		GUI::BrowserDialog browser(Common::U32String("Select SCI Game Directory"), true);
		if (browser.runModal() <= 0)
			break;

		const Common::FSNode &dir = browser.getResult();
		if (!dir.isDirectory())
			break;

		// 2. List directory contents for detection.
		Common::FSList files;
		if (!dir.getChildren(files, Common::FSNode::kListAll)) {
			GUI::MessageDialog err(Common::U32String("Could not open the selected directory."));
			err.runModal();
			break;
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
			break;
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
				break;
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
		rebuildGameList();
		g_gui.scheduleTopDialogRedraw();
		break;
	}
	case kDeleteCmd: {
		if (_state.selectedIndex < 0 ||
			_state.selectedIndex >= (int)_state.games.size()) break;
		const Common::String dom = _state.games[_state.selectedIndex].targetName;
		ConfMan.removeGameDomain(dom);
		ConfMan.flushToDisk();
		_launcher.discoverGames();
		rebuildGameList();
		bool hasGames = !_state.games.empty();
		if (hasGames)
			_launcher.selectGame(0);
		rebuildSettings();
		_launchBtn->setEnabled(hasGames);
		_deleteBtn->setEnabled(hasGames);
		g_gui.scheduleTopDialogRedraw();
		break;
	}
	// ListWidget emits kListSelectionChangedCmd on selection (NOT the _cmd we
	// passed at construction), so listen for the real command here. Without this
	// the selected game never updated: settings stayed frozen and Launch always
	// fired on the active game (index 0).
	case GUI::kListSelectionChangedCmd:
	case kGameSelCmd: {
		int sel = _gameList->getSelected();
		if (sel >= 0) {
			_launcher.selectGame(sel);
			rebuildSettings();
			_launchBtn->setEnabled(true);
			_deleteBtn->setEnabled(true);
		}
		g_gui.scheduleTopDialogRedraw();
		break;
	}
	// Double-click or Enter on a game selects it and launches.
	case GUI::kListItemDoubleClickedCmd:
	case GUI::kListItemActivatedCmd: {
		int sel = _gameList->getSelected();
		if (sel >= 0) {
			_launcher.selectGame(sel);
			rebuildSettings();
			handleCommand(sender, kLaunchCmd, 0);
		}
		break;
	}
	case kPrecachePopCmd: {
		uint32 tag = _precachePop->getSelectedTag();
		if (tag < 4) _state.settings.precache = kPrecacheVals[tag];
		break;
	}
	case kFontPopCmd: {
		uint32 tag = _fontPop->getSelectedTag();
		if (tag < 7) _state.settings.font = kFontVals[tag];
		break;
	}
	case kFallbackPopCmd: {
		uint32 tag = _fallbackPop->getSelectedTag();
		if (tag < 4) _state.settings.fallback = kFallbackVals[tag];
		break;
	}
	default:
		GUI::Dialog::handleCommand(sender, cmd, data);
	}
}

void RogerLauncherDialog::handleTickle() {
	if (_state.precaching) {
		bool more = _launcher.precacheStep();
		updateProgress();
		g_gui.scheduleTopDialogRedraw();
		if (!more) {
			_precacheBtn->setLabel(Common::U32String("Precache Now"));
			for (uint i = 0; i < _state.games.size(); ++i)
				_launcher.inspectCacheStatus(_state.games[i]);
			rebuildGameList();
			g_gui.scheduleTopDialogRedraw();
			if (_launchAfterPrecache) {
				_launchAfterPrecache = false;
				if (_launcher.handleLaunch())
					close();
				else
					close();
			}
		}
	}
	GUI::Dialog::handleTickle();
}

void RogerLauncherDialog::updateProgress() {
	if (!_state.precaching && _state.precacheDone == 0) {
		_progressLbl->setLabel(Common::U32String(""));
		_progressBar->setVisible(false);
		return;
	}
	Common::String s = _state.precacheStatus;
	if (s.empty())
		s = Common::String::format("Caching: %d / %d", _state.precacheDone, _state.precacheTotal);
	_progressLbl->setLabel(Common::U32String(s));

	const int total = _state.precacheTotal > 0 ? _state.precacheTotal : 1;
	_progressBar->setMaxValue(total);
	_progressBar->setValue(CLIP(_state.precacheDone, 0, total));
	_progressBar->setVisible(true);
}

void RogerLauncherDialog::reflowLayout() {
	GUI::Dialog::reflowLayout();
}

} // namespace Roger
} // namespace Sci
