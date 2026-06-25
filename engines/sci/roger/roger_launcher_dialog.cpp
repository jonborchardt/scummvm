#include "sci/roger/roger_launcher_dialog.h"
#include "gui/widget.h"
#include "gui/widgets/list.h"
#include "gui/widgets/popup.h"
#include "common/system.h"

namespace Sci {
namespace Roger {

RogerLauncherDialog::RogerLauncherDialog(RogerLauncher &launcher)
	: GUI::Dialog(0, 0, g_system->getOverlayWidth(), g_system->getOverlayHeight()),
	  _launcher(launcher), _state(launcher.state()) {
}

void RogerLauncherDialog::open() { GUI::Dialog::open(); }
void RogerLauncherDialog::handleTickle() { GUI::Dialog::handleTickle(); }
void RogerLauncherDialog::handleCommand(GUI::CommandSender *, uint32, uint32) {}
void RogerLauncherDialog::reflowLayout() { GUI::Dialog::reflowLayout(); }
void RogerLauncherDialog::rebuildGameList() {}
void RogerLauncherDialog::rebuildSettings() {}
void RogerLauncherDialog::updateProgress() {}

} // namespace Roger
} // namespace Sci
