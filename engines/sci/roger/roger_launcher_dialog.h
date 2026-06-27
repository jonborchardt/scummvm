#ifndef SCI_ROGER_ROGER_LAUNCHER_DIALOG_H
#define SCI_ROGER_ROGER_LAUNCHER_DIALOG_H

#include "gui/dialog.h"
#include "sci/roger/roger_launcher.h"

namespace GUI {
	class ListWidget;
	class ButtonWidget;
	class StaticTextWidget;
	class PopUpWidget;
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
	GUI::PopUpWidget     *_precachePop  = nullptr;
	GUI::PopUpWidget     *_enhancePop   = nullptr;
	GUI::PopUpWidget     *_fontPop      = nullptr;
	GUI::PopUpWidget     *_fallbackPop  = nullptr;
	GUI::StaticTextWidget *_progressLbl = nullptr;

	void rebuildGameList();
	void rebuildSettings();
	void updateProgress();
	GUI::PopUpWidget *addSettingsRow(int y, int M, int LH, const char *label, uint32 cmd);

	enum {
		kLaunchCmd   = 'RLNC',
		kDeleteCmd   = 'RDEL',
		kPrecacheCmd = 'RPRC',
		kAddGameCmd  = 'RADG',
		kGameSelCmd  = 'RGSL',
		kPrecachePopCmd  = 'RPCP',
		kEnhancePopCmd   = 'RECP',
		kFontPopCmd      = 'RFCP',
		kFallbackPopCmd  = 'RBCP',
	};
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_LAUNCHER_DIALOG_H
