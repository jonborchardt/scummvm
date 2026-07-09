#ifndef SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_DIALOG_H
#define SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_DIALOG_H

#include "gui/dialog.h"
#include "sci/roger/launcher/roger_launcher.h"

namespace GUI {
	class ListWidget;
	class ButtonWidget;
	class StaticTextWidget;
	class PopUpWidget;
	class SliderWidget;
	class EditTextWidget;
}

namespace Sci {
namespace Roger {

class RogerLauncherDialog : public GUI::Dialog {
public:
	explicit RogerLauncherDialog(RogerLauncher &launcher);

	void open() override;
	void handleTickle() override;
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override;
	void reflowLayout() override;

private:
	RogerLauncher    &_launcher;
	LauncherState    &_state;   // alias for _launcher.state()

	GUI::ListWidget      *_gameList     = nullptr;
	GUI::ButtonWidget    *_launchBtn    = nullptr;
	GUI::ButtonWidget    *_precacheBtn  = nullptr;
	GUI::ButtonWidget    *_addGameBtn   = nullptr;
	GUI::ButtonWidget    *_deleteBtn    = nullptr;
	GUI::EditTextWidget  *_passesEdit   = nullptr;
	GUI::StaticTextWidget *_progressLbl = nullptr;
	GUI::SliderWidget     *_progressBar = nullptr;

	bool _launchAfterPrecache = false;

	void rebuildGameList();
	void rebuildSettings();
	void syncPassesFromField();
	void updateProgress();

	enum {
		kLaunchCmd   = 'RLNC',
		kDeleteCmd   = 'RDEL',
		kPrecacheCmd = 'RPRC',
		kAddGameCmd  = 'RADG',
		kGameSelCmd  = 'RGSL',
		kGoodPass0Cmd    = 'RGP0', // +i for goodPassPattern(i), i < 4
	};
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_LAUNCHER_ROGER_LAUNCHER_DIALOG_H
