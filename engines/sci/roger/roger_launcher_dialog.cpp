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

// Layout helpers: all coordinates are overlay pixels.
static int gW() { return g_system->getOverlayWidth(); }
static int gH() { return g_system->getOverlayHeight(); }

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
	const int labelW = W / 5, popW = W / 4;
	const char *labels[] = { "Pre-cache", "Enhancement", "Font", "Fallback" };
	GUI::PopUpWidget **pops[] = { &_precachePop, &_enhancePop, &_fontPop, &_fallbackPop };
	const uint32 popCmds[] = { kPrecachePopCmd, kEnhancePopCmd, kFontPopCmd, kFallbackPopCmd };

	for (int i = 0; i < 4; ++i) {
		const int rowY = settTop + LH + i * (LH + M/3);
		new GUI::StaticTextWidget(this, M, rowY, labelW, LH,
		                          Common::U32String(labels[i]), Graphics::kTextAlignLeft);
		*pops[i] = new GUI::PopUpWidget(this, M + labelW + M/2, rowY, popW, LH,
		                                Common::U32String(), popCmds[i]);
	}

	// Populate popup options.
	// Tags are used as indices so we can use setSelectedTag() for matching.
	_precachePop->appendEntry(Common::U32String("off"),   0);
	_precachePop->appendEntry(Common::U32String("pics"),  1);
	_precachePop->appendEntry(Common::U32String("views"), 2);
	_precachePop->appendEntry(Common::U32String("all"),   3);

	_enhancePop->appendEntry(Common::U32String("off"),      0);
	_enhancePop->appendEntry(Common::U32String("fast"),     1);
	_enhancePop->appendEntry(Common::U32String("balanced"), 2);
	_enhancePop->appendEntry(Common::U32String("quality"),  3);

	static const char *kFontShortlist[] = {
		"ms_sans_serif.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf",
		"LiberationSerif-Regular.ttf", "GoMono-Regular.ttf",
		"LiberationMono-Regular.ttf", "SourceCodeVariable-Roman.ttf"
	};
	for (int i = 0; i < 7; ++i)
		_fontPop->appendEntry(Common::U32String(kFontShortlist[i]), (uint32)i);

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

	// Precache: match by string value to tag index.
	static const char *precacheVals[] = { "off", "pics", "views", "all" };
	for (int i = 0; i < 4; ++i)
		if (s.precache == precacheVals[i]) { _precachePop->setSelectedTag((uint32)i); break; }

	// Enhancement: match by string value to tag index.
	static const char *enhanceVals[] = { "off", "fast", "balanced", "quality" };
	for (int i = 0; i < 4; ++i)
		if (s.enhancement == enhanceVals[i]) { _enhancePop->setSelectedTag((uint32)i); break; }

	// Font: match by string value to tag index.
	static const char *fontVals[] = {
		"ms_sans_serif.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf",
		"LiberationSerif-Regular.ttf", "GoMono-Regular.ttf",
		"LiberationMono-Regular.ttf", "SourceCodeVariable-Roman.ttf"
	};
	for (int i = 0; i < 7; ++i)
		if (s.font == fontVals[i]) { _fontPop->setSelectedTag((uint32)i); break; }

	// Fallback: match by string value to tag index.
	// Note: ConfMan stores "prebuilt" but display shows "native (prebuilt)".
	static const char *fbVals[] = { "prebuilt", "cache", "memory", "always" };
	for (int i = 0; i < 4; ++i)
		if (s.fallback == fbVals[i]) { _fallbackPop->setSelectedTag((uint32)i); break; }
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

		// 3. Detect games in the chosen directory.
		DetectionResults detectionResults = EngineMan.detectGames(files);
		DetectedGames candidates = detectionResults.listDetectedGames();

		// 4. Find first SCI game in the results.
		bool foundSci = false;
		DetectedGame chosen;
		for (uint i = 0; i < candidates.size(); ++i) {
			if (candidates[i].engineId == "sci") {
				chosen = candidates[i];
				foundSci = true;
				break;
			}
		}
		if (!foundSci) {
			GUI::MessageDialog err(Common::U32String("No SCI game detected in that directory."));
			err.runModal();
			break;
		}

		// 5. Add to ConfMan.
		Common::String newTarget = EngineMan.createTargetForGame(chosen);
		ConfMan.setPath("path", dir.getPath(), newTarget);

		// 6. Create <gameid>-roger/ sibling directory.
		Common::Path rogerPath = dir.getPath().getParent()
		                             .appendComponent(chosen.gameId + "-roger");
		Common::FSNode(rogerPath).createDirectory();

		// 7. Persist and refresh.
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
		// Map selected tag (== index) back to string value.
		static const char *precacheVals[] = { "off", "pics", "views", "all" };
		uint32 tag = _precachePop->getSelectedTag();
		if (tag < 4) _state.settings.precache = precacheVals[tag];
		break;
	}
	case kEnhancePopCmd: {
		static const char *enhanceVals[] = { "off", "fast", "balanced", "quality" };
		uint32 tag = _enhancePop->getSelectedTag();
		if (tag < 4) _state.settings.enhancement = enhanceVals[tag];
		break;
	}
	case kFontPopCmd: {
		static const char *fontVals[] = {
			"ms_sans_serif.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf",
			"LiberationSerif-Regular.ttf", "GoMono-Regular.ttf",
			"LiberationMono-Regular.ttf", "SourceCodeVariable-Roman.ttf"
		};
		uint32 tag = _fontPop->getSelectedTag();
		if (tag < 7) _state.settings.font = fontVals[tag];
		break;
	}
	case kFallbackPopCmd: {
		static const char *fbVals[] = { "prebuilt", "cache", "memory", "always" };
		uint32 tag = _fallbackPop->getSelectedTag();
		if (tag < 4) _state.settings.fallback = fbVals[tag];
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
