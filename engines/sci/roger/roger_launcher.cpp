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

bool RogerLauncher::run() { return true; } // stub

void RogerLauncher::discoverGames() {}
void RogerLauncher::inspectCacheStatus(GameEntry &) const {}
void RogerLauncher::loadSettingsForSelected() {}
void RogerLauncher::flushSettingsForSelected() {}
void RogerLauncher::selectGame(int) {}
void RogerLauncher::buildPrecacheQueues() {}
bool RogerLauncher::precacheStep() { return false; }
bool RogerLauncher::handleLaunch() { return true; }

} // namespace Roger
} // namespace Sci
