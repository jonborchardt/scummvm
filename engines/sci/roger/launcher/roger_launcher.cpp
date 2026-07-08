#include "sci/roger/launcher/roger_launcher.h"
#include "sci/roger/launcher/roger_launcher_dialog.h"
#include "sci/roger/roger_art_provider.h"
#include "sci/roger/gen/roger_passes.h"
#include "sci/sci.h"
#include "sci/resource/resource.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "engines/engine.h"      // ChainedGamesMan (engine-initiated game switch)
#include "engines/metaengine.h"

namespace Sci {
namespace Roger {

RogerLauncher::RogerLauncher(RogerArtProvider *provider)
	: _provider(provider) {}

// Helper: add one game entry if it's a valid SCI game at gamePath with domain dom.
// rogerPath is created if it doesn't exist. No-ops if already in _state.games.
void RogerLauncher::tryAddEntry(const Common::String &dom,
                                const Common::Path &gamePath,
                                const Common::String &gameId,
                                const Common::String &desc) {
	if (dom.empty() || gamePath.empty() || gameId.empty()) return;
	for (uint i = 0; i < _state.games.size(); ++i)
		if (_state.games[i].targetName == dom) return; // deduplicate

	Common::Path rogerPath = gamePath.getParent().appendComponent(gameId + "-roger");
	// Create the roger directory if it doesn't exist yet (best-effort; silently ignore failure).
	Common::FSNode rogerNode(rogerPath);
	if (!rogerNode.exists())
		rogerNode.createDirectory();

	GameEntry entry;
	entry.targetName  = dom;
	entry.gameId      = gameId;
	entry.gamePath    = gamePath;
	entry.rogerPath   = rogerPath;
	entry.description = desc.empty() ? gameId : desc;
	inspectCacheStatus(entry);
	_state.games.push_back(entry);
}

void RogerLauncher::discoverGames() {
	_state.games.clear();

	// Resolve a friendly description: prefer the ConfMan value, fall back to
	// EngineMan for command-line launches where no "description" key was written.
	auto resolveDesc = [](const Common::String &gameId,
	                       const Common::String &confDesc) -> Common::String {
		if (!confDesc.empty()) return confDesc;
		QualifiedGameList matches = EngineMan.findGamesMatching("sci", gameId);
		if (!matches.empty()) return matches[0].description;
		return gameId;  // last resort: raw gameId
	};

	// Persistent game domains from scummvm.ini.
	const Common::ConfigManager::DomainMap &domains = ConfMan.getGameDomains();
	for (Common::ConfigManager::DomainMap::const_iterator it = domains.begin();
	     it != domains.end(); ++it) {
		const Common::String &dom = it->_key;
		if (!ConfMan.hasKey("engineid", dom) || ConfMan.get("engineid", dom) != "sci")
			continue;
		if (!ConfMan.hasKey("path", dom))
			continue;
		tryAddEntry(dom,
		            ConfMan.getPath("path", dom),
		            ConfMan.hasKey("gameid", dom) ? ConfMan.get("gameid", dom) : dom,
		            resolveDesc(
		                ConfMan.hasKey("gameid", dom) ? ConfMan.get("gameid", dom) : dom,
		                ConfMan.hasKey("description", dom) ? ConfMan.get("description", dom) : ""));
	}

	// Also check the active domain â€” handles command-line games not persisted in scummvm.ini.
	// When launched as "scummvm -p /path gameid", getGameDomains() returns empty because
	// the domain only exists in memory; ConfMan.hasKey("path") reads from the active chain.
	{
		const Common::String active = ConfMan.getActiveDomainName();
		if (!active.empty() && ConfMan.hasKey("path")) {
			const Common::String activeGameId = ConfMan.hasKey("gameid") ? ConfMan.get("gameid") : active;
			tryAddEntry(active,
			            ConfMan.getPath("path"),
			            activeGameId,
			            resolveDesc(activeGameId,
			                        ConfMan.hasKey("description") ? ConfMan.get("description") : ""));
		}
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

void RogerLauncher::loadSettingsForSelected() {
	if (_state.games.empty()) return;
	const Common::String &dom = _state.games[_state.selectedIndex].targetName;
	LauncherSettings &s = _state.settings;
	s.precache    = ConfMan.hasKey("roger_precache",   dom) ? ConfMan.get("roger_precache",   dom) : "all";
	s.fallback    = ConfMan.hasKey("roger_gen_mode",   dom) ? ConfMan.get("roger_gen_mode",   dom) : "cache";
	s.font        = ConfMan.hasKey("roger_ui_font",    dom) ? ConfMan.get("roger_ui_font",    dom) : "GoMono-Regular.ttf";
	s.passes      = ConfMan.hasKey("roger_omyac_passes", dom)
		? ConfMan.get("roger_omyac_passes", dom)
		: Common::String(kDefaultPassString);
}

void RogerLauncher::flushSettingsForSelected() {
	if (_state.games.empty()) return;
	const Common::String &dom = _state.games[_state.selectedIndex].targetName;
	const LauncherSettings &s = _state.settings;
	ConfMan.set("roger_precache",  s.precache,    dom);
	ConfMan.set("roger_gen_mode",  s.fallback,     dom);
	ConfMan.set("roger_ui_font",   s.font,         dom);
	// Verbatim passthrough: whatever the Passes field holds is what the ini
	// gets â€” the engine parser warns about unknown tokens at load. An unset
	// key becomes explicit (kDefaultPassString) after the first launch;
	// effective behavior is identical. An EMPTIED field means "use the
	// default": the key is removed (unset -> defaultPasses()). Wireframe
	// (explicit "") is no longer expressible from the picker â€” hand-edit the
	// ini for that debug state.
	Common::String trimmedPasses = s.passes;
	trimmedPasses.trim();
	if (trimmedPasses.empty()) {
		if (ConfMan.hasKey("roger_omyac_passes", dom))
			ConfMan.removeKey("roger_omyac_passes", dom);
	} else {
		ConfMan.set("roger_omyac_passes", s.passes, dom);
	}
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
		// Switch to a different game. setActiveDomain() alone does NOT work here:
		// after this engine returns, base/main.cpp's post-run cleanup calls
		// setActiveDomain("") and drops to the GUI launcher. The engine-initiated
		// switch path is ChainedGamesMan â€” main.cpp pops it (after the
		// return-to-launcher event) and runs it as the next game.
		ChainedGamesMan.push(selected);
		Common::Event e;
		e.type = Common::EVENT_RETURN_TO_LAUNCHER;
		g_system->getEventManager()->pushEvent(e);
		_switchTriggered = true;
		return false; // caller returns Common::kNoError
	}
	return true;
}

void RogerLauncher::buildPrecacheQueues() {
	_state.picQueue.clear();
	_state.viewQueue.clear();
	if (!g_sci) {
		warning("RogerLauncher::buildPrecacheQueues: g_sci is null, queues not built");
		return;
	}
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan) return;

	const LauncherSettings &s = _state.settings;
	const bool doPics  = (s.precache == "all" || s.precache == "pics");
	const bool doViews = (s.precache == "all" || s.precache == "views");

	if (doPics) {
		Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
		for (Common::List<ResourceId>::const_iterator it = pics.begin(); it != pics.end(); ++it)
			_state.picQueue.push_back((GuiResourceId)it->getNumber());
	}
	if (doViews) {
		Common::List<ResourceId> views = resMan->listResources(kResourceTypeView);
		for (Common::List<ResourceId>::const_iterator it = views.begin(); it != views.end(); ++it)
			_state.viewQueue.push_back((int)it->getNumber());
	}
	_state.precacheDone  = 0;
	_state.precacheTotal = (int)(_state.picQueue.size() + _state.viewQueue.size());
	_state.precaching    = true;
	_state.cancelPrecache = false;
	_state.precacheStatus = Common::String::format(
		"Caching %d items...", _state.precacheTotal);
	warning("ROGER launcher precache: starting (%d items)", _state.precacheTotal);
}

bool RogerLauncher::precacheStep() {
	if (_state.cancelPrecache) {
		_state.picQueue.clear();
		_state.viewQueue.clear();
	}
	if (!_state.picQueue.empty()) {
		GuiResourceId id = _state.picQueue.front();
		_state.picQueue.remove_at(0);  // O(n) but pic counts stay well under 200
		uint32 ms = 0;
		_provider->precacheOnePic(id, ms);
		++_state.precacheDone;
		_state.precacheStatus = Common::String::format(
			"Caching pic %d  (%d / %d)", (int)id, _state.precacheDone, _state.precacheTotal);
		warning("ROGER launcher precache: pic %d (%d/%d) %u ms",
		        (int)id, _state.precacheDone, _state.precacheTotal, ms);
		return true; // more to do
	}
	if (!_state.viewQueue.empty()) {
		int id = _state.viewQueue.front();
		_state.viewQueue.remove_at(0);  // O(n) but view counts stay well under 200
		_provider->precacheOneView(id);
		++_state.precacheDone;
		_state.precacheStatus = Common::String::format(
			"Caching view %d  (%d / %d)", id, _state.precacheDone, _state.precacheTotal);
		warning("ROGER launcher precache: view %d (%d/%d)",
		        id, _state.precacheDone, _state.precacheTotal);
		return true;
	}
	_state.precaching = false;
	_state.precacheStatus = Common::String::format(
		"Caching complete: %d items", _state.precacheDone);
	warning("ROGER launcher precache: complete (%d items)", _state.precacheDone);
	return false; // done
}

bool RogerLauncher::run() {
	discoverGames();
	if (_state.games.empty()) return true;
	loadSettingsForSelected();

	RogerLauncherDialog dialog(*this);
	dialog.runModal();

	return !_switchTriggered;
}

} // namespace Roger
} // namespace Sci
