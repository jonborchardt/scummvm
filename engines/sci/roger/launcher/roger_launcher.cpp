#include "sci/roger/launcher/roger_launcher.h"
#include "sci/roger/launcher/roger_launcher_dialog.h"
#include "sci/roger/launcher/roger_picker_model.h"
#include "sci/roger/file_roger_art_provider.h"
#include "sci/roger/gen/roger_asset_gen.h"
#include "sci/roger/gen/roger_passes.h"
#include "sci/sci.h"
#include "sci/resource/resource.h"
#include "sci/version.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"
#include "engines/engine.h"      // ChainedGamesMan (engine-initiated game switch)
#include "engines/metaengine.h"

namespace Sci {
namespace Roger {

RogerLauncher::RogerLauncher(FileRogerArtProvider *provider)
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
	entry.targetName = dom;
	entry.gameId     = gameId;
	entry.gamePath   = gamePath;
	entry.rogerPath  = rogerPath;
	Common::String title, subtitle;
	splitGameDescription(desc.empty() ? gameId : desc, title, subtitle);
	entry.description = title;
	entry.subtitle    = subtitle;

	// EGA flag: check for the "ega" token in guioptions (exact token match).
	if (ConfMan.hasKey("guioptions", dom)) {
		Common::String guiOpts = ConfMan.get("guioptions", dom);
		Common::StringTokenizer tok(guiOpts, " \t");
		while (!tok.empty()) {
			if (tok.nextToken() == "ega") {
				entry.ega = true;
				break;
			}
		}
	}

	// SCI version: available only for the active (running) game.
	if (dom == ConfMan.getActiveDomainName() && g_sci) {
		entry.sciVersion = Common::String(getSciVersionDesc(getSciVersion()));
	}

	refreshCacheState(entry);
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

	// Also check the active domain -- handles command-line games not persisted in scummvm.ini.
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
	// Set activeRow: index of the running game in the sorted list, or -1 if absent.
	_state.activeRow = -1;
	for (uint i = 0; i < _state.games.size(); ++i) {
		if (_state.games[i].targetName == active) {
			_state.activeRow = (int)i;
			break;
		}
	}
}

Common::String RogerLauncher::currentPassStamp(const Common::String &domain) const {
	const bool hasKey = ConfMan.hasKey("roger_omyac_passes", domain);
	const Common::String iniVal = hasKey ? ConfMan.get("roger_omyac_passes", domain) : Common::String();
	return omyacPassStamp(effectivePasses(hasKey, iniVal));
}

void RogerLauncher::refreshCacheState(GameEntry &entry) const {
	const Common::String &dom = entry.targetName;

	// One-time cleanup: remove legacy ini stamp if present (it's superseded by
	// the marker-file scheme and would accumulate stale data in the ini).
	if (ConfMan.hasKey("roger_cache_stamp", dom)) {
		ConfMan.removeKey("roger_cache_stamp", dom);
		ConfMan.flushToDisk();
	}

	const Common::String stamp = currentPassStamp(dom);
	const Common::String markerName = cacheMarkerName(entry.gameId, kTransformVersion, stamp);
	const Common::Path markerPath = entry.rogerPath.appendComponent("cache").appendComponent(markerName);
	entry.cached = Common::FSNode(markerPath).exists();
}

void RogerLauncher::loadSettingsForSelected() {
	if (_state.games.empty()) return;
	const Common::String &dom = _state.games[_state.selectedIndex].targetName;
	LauncherSettings &s = _state.settings;
	s.passes = ConfMan.hasKey("roger_omyac_passes", dom)
		? ConfMan.get("roger_omyac_passes", dom)
		: Common::String(kDefaultPassString);
	s.debugLog = ConfMan.hasKey("roger_debug", dom) && ConfMan.getBool("roger_debug", dom);
}

// Verbatim passthrough: the engine parser warns about unknown tokens at load.
// Empty -> key removed (unset -> defaultPasses()); wireframe (explicit "")
// stays ini-only.
void RogerLauncher::setPassesForSelected(const Common::String &raw) {
	if (_state.games.empty()) return;
	GameEntry &g = _state.games[_state.selectedIndex];
	Common::String v = raw;
	v.trim();
	if (v.empty()) {
		if (ConfMan.hasKey("roger_omyac_passes", g.targetName))
			ConfMan.removeKey("roger_omyac_passes", g.targetName);
	} else {
		ConfMan.set("roger_omyac_passes", v, g.targetName);
	}
	ConfMan.flushToDisk();
	_state.settings.passes = v.empty() ? Common::String(kDefaultPassString) : v;
	refreshCacheState(g);
}

void RogerLauncher::setDebugLogForSelected(bool on) {
	if (_state.games.empty()) return;
	const Common::String &dom = _state.games[_state.selectedIndex].targetName;
	ConfMan.setBool("roger_debug", on, dom);
	ConfMan.flushToDisk();
	_state.settings.debugLog = on;
}

void RogerLauncher::selectGame(int index) {
	if (index < 0 || index >= (int)_state.games.size()) return;
	_state.selectedIndex = index;
	loadSettingsForSelected();
}

void RogerLauncher::requestCrossGame(int index, bool launchAfter) {
	if (index < 0 || index >= (int)_state.games.size()) return;
	const Common::String &dom = _state.games[index].targetName;
	// One-shot: consumed (removed + flushed) by the picker on the other side
	// BEFORE acting, so a crash mid-precache cannot loop the trigger.
	ConfMan.setBool(launchAfter ? "roger_picker_launch" : "roger_picker_precache",
	                true, dom);
	ConfMan.flushToDisk();
	ChainedGamesMan.push(dom);
	Common::Event e;
	e.type = Common::EVENT_RETURN_TO_LAUNCHER;
	g_system->getEventManager()->pushEvent(e);
	_switchTriggered = true;
}

bool RogerLauncher::handleLaunch() {
	if (_state.games.empty()) return true;
	const Common::String &active = ConfMan.getActiveDomainName();
	if (_state.games[_state.selectedIndex].targetName != active) {
		requestCrossGame(_state.selectedIndex, true);
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

	{
		Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
		for (Common::List<ResourceId>::const_iterator it = pics.begin(); it != pics.end(); ++it)
			_state.picQueue.push_back((GuiResourceId)it->getNumber());
	}
	{
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
	if (!_state.cancelPrecache) {
		if (_state.activeRow < 0 || _state.activeRow >= (int)_state.games.size()) {
			warning("RogerLauncher::precacheStep: no active game in list at completion, skipping marker write");
		} else {
			GameEntry &active = _state.games[_state.activeRow];
			const Common::String stamp = currentPassStamp(active.targetName);
			const Common::String markerName = cacheMarkerName(active.gameId, kTransformVersion, stamp);
			// Ensure the cache directory exists (the precache just wrote files there, but
			// best-effort create in case of an unusual edge).
			const Common::Path cachePath = active.rogerPath.appendComponent("cache");
			Common::FSNode(cachePath).createDirectory();
			// Write an empty marker file; existence is the signal.
			Common::DumpFile f;
			if (f.open(cachePath.appendComponent(markerName)))
				f.close();
			refreshCacheState(active);
		}
	}
	_state.precacheStatus = Common::String::format(
		"Caching complete: %d items", _state.precacheDone);
	warning("ROGER launcher precache: complete (%d items)", _state.precacheDone);
	return false; // done
}

bool RogerLauncher::run() {
	discoverGames();
	loadSettingsForSelected();

	// Consume one-shot cross-game keys written by a previous picker instance.
	const Common::String active = ConfMan.getActiveDomainName();
	if (!active.empty()) {
		if (ConfMan.hasKey("roger_picker_launch", active)) {
			ConfMan.removeKey("roger_picker_launch", active);
			ConfMan.flushToDisk();
			_autoLaunch = true;
		} else if (ConfMan.hasKey("roger_picker_precache", active)) {
			ConfMan.removeKey("roger_picker_precache", active);
			ConfMan.flushToDisk();
			_autoPrecache = true;
		}
	}
	// Cross-game launch of an already-cached game: straight in, no dialog.
	if (_autoLaunch && _state.activeRow >= 0 && _state.games[_state.activeRow].cached)
		return true;

	RogerLauncherDialog dialog(*this);
	dialog.runModal();
	return !_switchTriggered;
}

} // namespace Roger
} // namespace Sci
