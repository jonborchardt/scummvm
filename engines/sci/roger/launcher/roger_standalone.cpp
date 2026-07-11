/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// The gate must honor the same per-process env override automation uses for
// the in-engine picker (build_and_run.ps1 -SkipPicker / -Game), read via
// getenv() below.
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sci/roger/launcher/roger_standalone.h"
#include "sci/roger/launcher/roger_launcher.h"
#include "sci/roger/launcher/roger_picker_model.h"
#include "common/config-manager.h"
#include "common/textconsole.h"

namespace Sci {
namespace Roger {

bool rogerStandaloneLauncher() {
	// No active domain here, so ConfMan reads fall through to the [scummvm]
	// application section -- that is where the opt-out lives.
	const bool hasKey = ConfMan.hasKey("roger_no_launcher");
	const bool keyVal = hasKey && ConfMan.getBool("roger_no_launcher");
	const bool envSkip = (getenv("ROGER_NO_LAUNCHER") != nullptr);
	if (!standalonePickerWanted(hasKey, keyVal, envSkip))
		return false;

	warning("ROGER-PICKER: standalone launcher round");
	RogerLauncher launcher(nullptr, /*standalone=*/true);
	launcher.run();
	// A launch set the active domain inside requestCrossGame; a plain close
	// left it empty. Either way this round is handled.
	return true;
}

} // namespace Roger
} // namespace Sci
