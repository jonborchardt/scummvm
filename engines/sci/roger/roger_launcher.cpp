#include "sci/roger/roger_launcher.h"
#include "sci/roger/roger_launcher_dialog.h"
#include "sci/roger/roger_art_provider.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"

namespace Sci {
namespace Roger {

RogerLauncher::RogerLauncher(RogerArtProvider *provider)
	: _provider(provider) {}

void RogerLauncher::discoverGames() {
	_state.games.clear();
	const Common::ConfigManager::DomainMap &domains = ConfMan.getGameDomains();
	for (Common::ConfigManager::DomainMap::const_iterator it = domains.begin();
	     it != domains.end(); ++it) {
		const Common::String &dom = it->_key;
		if (!ConfMan.hasKey("engineid", dom) || ConfMan.get("engineid", dom) != "sci")
			continue;
		if (!ConfMan.hasKey("path", dom))
			continue;
		Common::Path gamePath = ConfMan.getPath("path", dom);
		Common::String gameId = ConfMan.get("gameid", dom);
		Common::Path rogerPath = gamePath.getParent().appendComponent(gameId + "-roger");
		Common::FSNode rogerNode(rogerPath);
		if (!rogerNode.exists() || !rogerNode.isDirectory())
			continue;

		GameEntry entry;
		entry.targetName  = dom;
		entry.gameId      = gameId;
		entry.gamePath    = gamePath;
		entry.rogerPath   = rogerPath;
		entry.description = ConfMan.hasKey("description", dom)
		                  ? ConfMan.get("description", dom) : gameId;
		inspectCacheStatus(entry);
		_state.games.push_back(entry);
	}

	// Sort active domain first.
	const Common::String &active = ConfMan.getActiveDomainName();
	for (uint i = 1; i < _state.games.size(); ++i) {
		if (_state.games[i].targetName == active) {
			GameEntry tmp = _state.games[0]; _state.games[0] = _state.games[i]; _state.games[i] = tmp;
			break;
		}
	}
	_state.selectedIndex = 0;
}

void RogerLauncher::inspectCacheStatus(GameEntry &entry) const {
	entry.cache.picCount = entry.cache.viewCount = 0;
	Common::FSNode cacheNode(entry.rogerPath.appendComponent("cache"));
	if (!cacheNode.exists() || !cacheNode.isDirectory()) return;
	Common::FSList children;
	if (!cacheNode.getChildren(children, Common::FSNode::kListFilesOnly)) return;
	for (uint i = 0; i < children.size(); ++i) {
		const Common::String name = children[i].getName();
		if (name.contains(".omyac."))    ++entry.cache.picCount;
		else if (name.contains(".scale6x.")) ++entry.cache.viewCount;
	}
}

static const char *enhancementPasses(const Common::String &label) {
	if (label == "off")      return "";
	if (label == "fast")     return "2";
	if (label == "balanced") return "2 1";
	if (label == "quality")  return "2 2 1 1 0";
	return nullptr;
}

void RogerLauncher::loadSettingsForSelected() {
	if (_state.games.empty()) return;
	const Common::String &dom = _state.games[_state.selectedIndex].targetName;
	LauncherSettings &s = _state.settings;
	s.precache    = ConfMan.hasKey("roger_precache",   dom) ? ConfMan.get("roger_precache",   dom) : "all";
	s.fallback    = ConfMan.hasKey("roger_gen_mode",   dom) ? ConfMan.get("roger_gen_mode",   dom) : "cache";
	s.font        = ConfMan.hasKey("roger_ui_font",    dom) ? ConfMan.get("roger_ui_font",    dom) : "GoMono-Regular.ttf";
	if (!ConfMan.hasKey("roger_omyac_passes", dom)) {
		s.enhancement = "balanced";
	} else {
		const Common::String p = ConfMan.get("roger_omyac_passes", dom);
		if (p.empty())        s.enhancement = "off";
		else if (p == "2")    s.enhancement = "fast";
		else if (p == "2 1")  s.enhancement = "balanced";
		else if (p == "2 2 1 1 0") s.enhancement = "quality";
		else                  s.enhancement = "custom";
	}
}

void RogerLauncher::flushSettingsForSelected() {
	if (_state.games.empty()) return;
	const Common::String &dom = _state.games[_state.selectedIndex].targetName;
	const LauncherSettings &s = _state.settings;
	ConfMan.set("roger_precache",  s.precache,    dom);
	ConfMan.set("roger_gen_mode",  s.fallback,     dom);
	ConfMan.set("roger_ui_font",   s.font,         dom);
	const char *passes = enhancementPasses(s.enhancement);
	if (passes) ConfMan.set("roger_omyac_passes", Common::String(passes), dom);
	ConfMan.flushToDisk();
}

void RogerLauncher::selectGame(int index) {
	if (index < 0 || index >= (int)_state.games.size()) return;
	_state.selectedIndex = index;
	loadSettingsForSelected();
}

bool RogerLauncher::handleLaunch() {
	if (_state.games.empty()) return true;
	const Common::String &active   = ConfMan.getActiveDomainName();
	const Common::String &selected = _state.games[_state.selectedIndex].targetName;
	flushSettingsForSelected();
	if (selected != active) {
		ConfMan.setActiveDomain(selected);
		ConfMan.flushToDisk();
		Common::Event e;
		e.type = Common::EVENT_RETURN_TO_LAUNCHER;
		g_system->getEventManager()->pushEvent(e);
		return false; // caller returns Common::kNoError
	}
	return true;
}

void RogerLauncher::buildPrecacheQueues() {}
bool RogerLauncher::precacheStep() { return false; }

bool RogerLauncher::run() {
	discoverGames();
	if (_state.games.empty()) return true;
	loadSettingsForSelected();
	for (uint i = 0; i < _state.games.size(); ++i)
		warning("ROGER launcher: [%u] %s pics=%d views=%d",
		        i, _state.games[i].description.c_str(),
		        _state.games[i].cache.picCount, _state.games[i].cache.viewCount);
	return true; // dialog call added in Task 4
}

} // namespace Roger
} // namespace Sci
