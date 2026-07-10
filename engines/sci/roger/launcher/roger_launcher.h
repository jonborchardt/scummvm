#ifndef SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_H
#define SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_H

#include "common/array.h"
#include "common/str.h"
#include "common/path.h"
#include "sci/graphics/helpers.h"  // GuiResourceId

namespace Sci {
class RogerArtProvider;

namespace Roger {

struct GameEntry {
	Common::String targetName;   // ConfMan domain key (e.g. "sq3-1")
	Common::String description;  // title without the trailing "(...)" group
	Common::String subtitle;     // e.g. "DOS/English" (may be empty)
	Common::String gameId;       // ConfMan "gameid" (e.g. "sq3")
	Common::Path   gamePath;
	Common::Path   rogerPath;    // <gamepath>/../<gameid>-roger/
	bool           cached = false; // marker file exists in cache dir for current (version, passes)
};

struct LauncherSettings {
	Common::String passes;       // effective (ini value, or kDefaultPassString)
	bool           debugLog = false; // roger_debug
};

struct LauncherState {
	Common::Array<GameEntry> games;
	int selectedIndex = 0;
	// Index of the active (currently running) game in games[], -1 if absent.
	// Set by discoverGames(); use this instead of assuming row 0 is active.
	int activeRow = -1;
	LauncherSettings settings;
	// Precache iterator state (used by dialog handleTickle).
	Common::Array<GuiResourceId> picQueue;   // pic IDs left to cache
	Common::Array<int>           viewQueue;  // view IDs left to cache
	int precacheDone  = 0;
	int precacheTotal = 0;
	bool precaching   = false;
	bool cancelPrecache = false;
	Common::String precacheStatus;  // human-readable current step, shown in the dialog
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
	void selectGame(int index);
	void buildPrecacheQueues();

	// Returns false and pushes EVENT_RETURN_TO_LAUNCHER if switching games.
	bool handleLaunch();

	// Recompute entry.cached by checking for the marker file in the cache dir
	// (current kTransformVersion + effective roger_omyac_passes). Removes the
	// legacy roger_cache_stamp ini key when present (one-time cleanup).
	void refreshCacheState(GameEntry &entry) const;

	// Immediate write-through settings (selected game's ini section + flush).
	void setPassesForSelected(const Common::String &raw); // empty -> remove key (= default)
	void setDebugLogForSelected(bool on);

	// Write a one-shot key (roger_picker_launch / roger_picker_precache) into
	// games[index]'s domain and switch engines to it via ChainedGamesMan.
	void requestCrossGame(int index, bool launchAfter);

	// One-shot keys consumed from the ACTIVE domain at run() start.
	bool autoLaunchPending() const { return _autoLaunch; }
	bool autoPrecachePending() const { return _autoPrecache; }

	// Public so Task 5 Add Game can trigger re-discovery.
	void discoverGames();

private:
	RogerArtProvider   *_provider;
	LauncherState       _state;
	bool                _switchTriggered = false;
	bool                _autoLaunch = false;
	bool                _autoPrecache = false;

	// Add one game to _state.games, creating the roger dir if needed.
	void tryAddEntry(const Common::String &dom, const Common::Path &gamePath,
	                 const Common::String &gameId, const Common::String &desc);

	// Canonical pass stamp for a domain (used to key marker files).
	Common::String currentPassStamp(const Common::String &domain) const;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_H
