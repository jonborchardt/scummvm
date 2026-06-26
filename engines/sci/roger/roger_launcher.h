#ifndef SCI_ROGER_ROGER_LAUNCHER_H
#define SCI_ROGER_ROGER_LAUNCHER_H

#include "common/array.h"
#include "common/str.h"
#include "common/path.h"
#include "sci/graphics/helpers.h"  // GuiResourceId

namespace Sci {
class RogerArtProvider;

namespace Roger {

struct CacheStatus {
	int picCount  = 0;   // .omyac.* files in cache/
	int viewCount = 0;   // .scale6x.* files in cache/
};

struct GameEntry {
	Common::String targetName;   // ConfMan domain key (e.g. "sq3-1")
	Common::String description;  // human-readable (e.g. "Space Quest III")
	Common::String gameId;       // ConfMan "gameid" (e.g. "sq3")
	Common::Path   gamePath;
	Common::Path   rogerPath;    // <gamepath>/../<gameid>-roger/
	CacheStatus    cache;
};

struct LauncherSettings {
	Common::String precache;     // "off"|"pics"|"views"|"all"
	Common::String enhancement;  // "off"|"fast"|"balanced"|"quality"
	Common::String font;         // roger_ui_font value
	Common::String fallback;     // "prebuilt"|"cache"|"memory"|"always"
};

struct LauncherState {
	Common::Array<GameEntry> games;
	int selectedIndex = 0;
	LauncherSettings settings;
	// Precache iterator state (used by dialog handleTickle).
	Common::Array<GuiResourceId> picQueue;   // pic IDs left to cache
	Common::Array<int>           viewQueue;  // view IDs left to cache
	int precacheDone  = 0;
	int precacheTotal = 0;
	bool precaching   = false;
	bool cancelPrecache = false;
};

class RogerLauncher {
public:
	explicit RogerLauncher(RogerArtProvider *provider);

	// Run the launcher modal dialog. Returns true to proceed with the current
	// game, false if a game-switch event was pushed (caller should return).
	bool run();

	// Called by the dialog's handleTickle() while precaching.
	// Generates the next item in the queue; returns false when the queue is empty.
	bool precacheStep();

	// Accessors for the dialog.
	LauncherState &state() { return _state; }
	void loadSettingsForSelected();
	void flushSettingsForSelected();
	void selectGame(int index);
	void buildPrecacheQueues();

	// Returns false and pushes EVENT_RETURN_TO_LAUNCHER if switching games.
	bool handleLaunch();

	// Public so the dialog can refresh counts after precaching.
	void inspectCacheStatus(GameEntry &entry) const;

	// Public so Task 5 Add Game can trigger re-discovery.
	void discoverGames();

private:
	RogerArtProvider   *_provider;
	LauncherState       _state;
	bool                _switchTriggered = false;

	// Add one game to _state.games, creating the roger dir if needed.
	void tryAddEntry(const Common::String &dom, const Common::Path &gamePath,
	                 const Common::String &gameId, const Common::String &desc);
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_LAUNCHER_H
