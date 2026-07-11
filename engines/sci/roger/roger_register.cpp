/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Roger: build_and_run.ps1 sets ROGER_STUDIO / ROGER_EYETEST / ROGER_NO_LAUNCHER
// as non-sticky per-launch gates, read via getenv() below (same pattern as the
// env-first knobs in file_roger_art_provider.cpp).
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sci/roger/file_roger_art_provider.h"
#include "sci/roger/launcher/roger_launcher.h"
#include "sci/roger/utils/studio/roger_studio.h" // quarantined dev utility (Roger Studio)
#include "sci/roger/utils/eyetest/roger_eyetest.h" // quarantined dev utility (eye exam)
#include "common/config-manager.h"

namespace Sci {

// The neutral factory declared in sci_gfx_observer.h: this is the ONLY place
// the concrete Roger provider is constructed for the engine. SciEngine::run()
// registers the result via setSciGfxObserver() without ever naming the type.
SciGfxObserver *createSciGfxObserver(const Common::String &gameId, const Common::Path &gamePath) {
	FileRogerArtProvider *provider = new FileRogerArtProvider(gameId, gamePath);
	g_rogerProvider = provider; // roger-internal downcast slot (cleared in ~FileRogerArtProvider)
	return provider;
}

// One-time startup hook (SciGfxObserver::onEngineStartup), called from
// SciEngine::run() after graphics init and before any game script runs.
// Returning true = a standalone tool ran (or an engine restart was pushed);
// the engine exits without running the game. Order is load-bearing and
// mirrors the pre-factoring sci.cpp sequence exactly:
// studio gate -> eyetest gate -> no-launcher decision -> launcher-or-precache.
bool FileRogerArtProvider::onEngineStartup() {
	// Roger Studio, tuning environment (quarantined dev utility,
	// engines/sci/roger/utils/studio/) — build_and_run.ps1 -Studio /
	// ROGER_STUDIO=1. Runs its own blocking loop at this seam — resources and
	// graphics are alive, no game scripts have run — then exits the process.
	// This env-gated block is its ONLY engine reference.
	if (getenv("ROGER_STUDIO") != nullptr) {
		Roger::RogerStudio studio(_gameId);
		studio.run();
		return true;
	}

	// Eye Exam, interactive OMYAC pass-sequence tuner (quarantined dev
	// utility, engines/sci/roger/utils/eyetest/) — same seam and lifecycle as
	// Roger Studio above. This env-gated block is its ONLY engine reference.
	if (getenv("ROGER_EYETEST") != nullptr) {
		Roger::RogerEyeTest eyetest(_gameId);
		eyetest.run();
		return true;
	}

	// Skip the picker when roger_no_launcher is set (scummvm.ini) OR the
	// ROGER_NO_LAUNCHER env var is present. The env var is a non-sticky
	// dev convenience so build_and_run.ps1 -SkipPicker can boot straight
	// into the game / auto-loaded save without touching scummvm.ini.
	const bool skipLauncher =
		(ConfMan.hasKey("roger_no_launcher") && ConfMan.getBool("roger_no_launcher")) ||
		(getenv("ROGER_NO_LAUNCHER") != nullptr);

	if (!skipLauncher) {
		// The launcher owns precaching and shows on-screen progress + a per-item
		// log. Do NOT run the synchronous startup warm-up here — it would do all
		// the work (potentially many seconds, fully blocking) before the dialog
		// ever appears, with no visible progress.
		Roger::RogerLauncher launcher(this);
		if (!launcher.run())
			return true; // game-switch pushed; ScummVM restarts engine
	} else {
		// No launcher: fall back to the synchronous warm-up (roger_precache).
		// No-op unless roger_precache is set and a generating roger_gen_mode is active.
		precacheAll();
	}

	return false;
}

} // namespace Sci
