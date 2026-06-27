#include "sci/roger/roger_launcher_dialog.h"
#include "gui/gui-manager.h"
#include "gui/widget.h"
#include "gui/widgets/list.h"
#include "gui/widgets/popup.h"
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

static const char *kPrecacheVals[] = { "off", "pics", "views", "all" };
static const char *kEnhanceVals[]  = { "off", "fast", "balanced", "quality" };
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
	const int M = W / 30;    // margin
	const int LH = H / 20;  // line height

	// Title: "ROGER" static label.
	new GUI::StaticTextWidget(this, M, M, W / 4, LH,
	                          Common::U32String("ROGER"), Graphics::kTextAlignLeft);

	// [Skip] button — top-right.
	_skipBtn = new GUI::ButtonWidget(this, W - M - W/8, M, W/8, LH,
	                                 Common::U32String("Skip"), Common::U32String(), kSkipCmd);

	// GAMES section header.
	const int listTop = M + LH + M;
	const int listH   = H * 2 / 5;
	new GUI::StaticTextWidget(this, M, listTop, W/4, LH,
	                          Common::U32String("GAMES"), Graphics::kTextAlignLeft);

	// Game list.
	_gameList = new GUI::ListWidget(this, M, listTop + LH, W - 2*M, listH - LH,
	                                Common::U32String(), kGameSelCmd);

	// [+ Add Game] button below game list.
	const int addY = listTop + listH + M/2;
	_addGameBtn = new GUI::ButtonWidget(this, M, addY, W/5, LH,
	                                    Common::U32String("+ Add Game"), Common::U32String(), kAddGameCmd);

	// SETTINGS section header.
	const int settTop = addY + LH + M;
	new GUI::StaticTextWidget(this, M, settTop, W - 2*M, LH,
	                          Common::U32String("SETTINGS"), Graphics::kTextAlignLeft);

	// Four settings rows: label + popup.
	_precachePop = addSettingsRow(settTop + LH + 0*(LH + M/3), M, LH, "Pre-cache",   kPrecachePopCmd);
	_enhancePop  = addSettingsRow(settTop + LH + 1*(LH + M/3), M, LH, "Enhancement", kEnhancePopCmd);
	_fontPop     = addSettingsRow(settTop + LH + 2*(LH + M/3), M, LH, "Font",        kFontPopCmd);
	_fallbackPop = addSettingsRow(settTop + LH + 3*(LH + M/3), M, LH, "Fallback",    kFallbackPopCmd);

	// Populate popup options.
	// Tags are used as indices so we can use setSelectedTag() for matching.
	for (int i = 0; i < 4; ++i)
		_precachePop->appendEntry(Common::U32String(kPrecacheVals[i]), (uint32)i);

	for (int i = 0; i < 4; ++i)
		_enhancePop->appendEntry(Common::U32String(kEnhanceVals[i]), (uint32)i);

	for (int i = 0; i < 7; ++i)
		_fontPop->appendEntry(Common::U32String(kFontVals[i]), (uint32)i);

	// Fallback popup: tag encodes index for mapping back to string.
	_fallbackPop->appendEntry(Common::U32String("native (prebuilt)"), 0);
	_fallbackPop->appendEntry(Common::U32String("cache"),             1);
	_fallbackPop->appendEntry(Common::U32String("memory"),            2);
	_fallbackPop->appendEntry(Common::U32String("always"),            3);

	// Bottom buttons.
	const int btnY = H - M - LH;
	_precacheBtn = new GUI::ButtonWidget(this, M, btnY, W/5, LH,
	                                     Common::U32String("Precache Now"),
	                                     Common::U32String(), kPrecacheCmd);
	_launchBtn = new GUI::ButtonWidget(this, W - M - W/6, btnY, W/6, LH,
	                                   Common::U32String("Launch"),
	                                   Common::U32String(), kLaunchCmd);

	// Progress label (hidden until precaching starts).
	_progressLbl = new GUI::StaticTextWidget(this, M, btnY - LH - M/2, W - 2*M, LH,
	                                         Common::U32String(""), Graphics::kTextAlignLeft);
}

void RogerLauncherDialog::open() {
	GUI::Dialog::open();
	rebuildGameList();
	rebuildSettings();
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
	for (int i = 0; i < 4; ++i)
		if (s.enhancement == kEnhanceVals[i]) { _enhancePop->setSelectedTag((uint32)i); break; }
	for (int i = 0; i < 7; ++i)
		if (s.font == kFontVals[i]) { _fontPop->setSelectedTag((uint32)i); break; }
	for (int i = 0; i < 4; ++i)
		if (s.fallback == kFallbackVals[i]) { _fallbackPop->setSelectedTag((uint32)i); break; }
}

void RogerLauncherDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kLaunchCmd:
		if (_launcher.handleLaunch())
			close();
		else
			close(); // game switch: dialog closes, caller returns kNoError
		break;
	case kSkipCmd:
		close();
		break;
	case kPrecacheCmd:
		if (_state.precaching) {
			_state.cancelPrecache = true;
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
	case kGameSelCmd:
		_launcher.selectGame(_gameList->getSelected());
		rebuildSettings();
		break;
	case kPrecachePopCmd: {
		uint32 tag = _precachePop->getSelectedTag();
		if (tag < 4) _state.settings.precache = kPrecacheVals[tag];
		break;
	}
	case kEnhancePopCmd: {
		uint32 tag = _enhancePop->getSelectedTag();
		if (tag < 4) _state.settings.enhancement = kEnhanceVals[tag];
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
			// Refresh cache counts for all games.
			for (uint i = 0; i < _state.games.size(); ++i)
				_launcher.inspectCacheStatus(_state.games[i]);
			rebuildGameList();
			g_gui.scheduleTopDialogRedraw();
		}
	}
	GUI::Dialog::handleTickle();
}

void RogerLauncherDialog::updateProgress() {
	if (!_state.precaching && _state.precacheDone == 0) {
		_progressLbl->setLabel(Common::U32String(""));
		return;
	}
	const Common::String s = Common::String::format(
		"Caching: %d / %d", _state.precacheDone, _state.precacheTotal);
	_progressLbl->setLabel(Common::U32String(s));
}

void RogerLauncherDialog::reflowLayout() {
	GUI::Dialog::reflowLayout();
}

} // namespace Roger
} // namespace Sci
