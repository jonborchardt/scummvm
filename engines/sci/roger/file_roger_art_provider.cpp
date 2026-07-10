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

// Roger: env-first knob reading (ROGER_INPUT_SCRIPT / ROGER_INPUT_LIVE /
// ROGER_CYCLE_LOG) uses getenv() Ã¢â‚¬â€ same pattern as sci.cpp's ROGER_NO_LAUNCHER.
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sci/roger/file_roger_art_provider.h"
#include "sci/roger/roger_input.h"
#include "sci/roger/roger_selftest.h"
#include "sci/roger/overlay/roger_cursor.h"
#include "sci/roger/png_loader.h"
#include "sci/roger/gen/roger_asset_gen.h"
#include "sci/roger/overlay/roger_palette_remap.h"
#include "sci/roger/overlay/roger_compositor.h"
#include "sci/roger/overlay/roger_coords.h"
#include "sci/roger/gen/roger_omyac.h"
#include "sci/roger/gen/roger_passes.h"
#include "sci/roger/overlay/roger_journal.h"
#include "sci/roger/overlay/roger_tokens.h"
#include "sci/roger/overlay/roger_text.h"
#include "sci/roger/overlay/view_cache.h"
#include "sci/roger/gen/slice_set.h"
#include "sci/roger/gen/roger_pic_parser.h"
#include "sci/roger/gen/roger_view_scaler.h"
// animate.h references these SCI engine types in GfxAnimate's interface but does
// not declare them itself. This translation unit includes animate.h (to iterate
// the AnimateList in onAnimateFrame) without first pulling in the full
// engine-state headers, so forward-declare them here. Confined to this Roger file
// to keep animate.h itself untouched.
namespace Sci {
struct EngineState;
class ScriptPatcher;
struct List;
class GfxCompare;
}
#include "sci/graphics/animate.h"
#include "sci/graphics/screen.h"
#include "sci/sci.h"
#include "sci/resource/resource.h"
#include "sci/graphics/cache.h"
#include "sci/graphics/view.h"
#include "sci/graphics/palette16.h"
#include "sci/engine/state.h"
#include "sci/engine/seg_manager.h"
#include "sci/engine/kernel.h"
#include "sci/engine/selector.h"
#include "sci/engine/vm.h"
#include "graphics/managed_surface.h"
#include "graphics/paletteman.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"
#include "common/array.h"
#include "common/file.h"
#include "common/path.h"
#include "common/fs.h"
#include "graphics/cursorman.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "sci/graphics/cursor.h"

namespace Sci {

FileRogerArtProvider::FileRogerArtProvider(const Common::String &gameId,
                                            const Common::Path &gamePath) {
	// Build path: gamePath/../<gameId>-roger/ using pure path ops (no FS access).
	// gamePath is already a Common::Path (native separators parsed), so
	// getParent() works on Windows backslash paths too.
	Common::Path rogerPath = gamePath.getParent().appendComponent(gameId + "-roger");
	_basePath = rogerPath.toString('/');
	_gameId = gameId;

	// roger_visual_variant / roger_priority_variant selected prebuilt PNG files (the
	// hires visual and the EGA-color overlay-occlusion map). Under in-engine
	// generation both the visual and the occlusion bands are produced from the SCI
	// resource, so these knobs are obsolete and are no longer read.

	_selfTest = ConfMan.hasKey("roger_selftest") && ConfMan.getBool("roger_selftest");
	// roger_debug: per-frame + per-UI-element diagnostic logging (also toggled in-game
	// with Ctrl+Shift+L). Read it here so the documented config knob actually works.
	_debugLog = ConfMan.hasKey("roger_debug") && ConfMan.getBool("roger_debug");
	// roger_diag: revertible overlay-state trace at room/present/transition seams (off by default).
	// Env-first (ROGER_DIAG=1) so build_and_run.ps1 -Diag arms a single launch without editing
	// scummvm.ini Ã¢â‚¬â€ ini edits race against a running instance's config rewrite-on-exit; the ini
	// knob still works for ini-based setups (same pattern as ROGER_INPUT_SCRIPT / ROGER_DISPLAY_MODE).
	{
		const char *envDiag = getenv("ROGER_DIAG");
		_diag = envDiag ? (Common::String(envDiag) != "0" && Common::String(envDiag) != "false")
		                : (ConfMan.hasKey("roger_diag") && ConfMan.getBool("roger_diag"));
	}
	// roger_debug_capture: write per-pic manifest + PNG per graphic sprite to screenshots/ (off by default).
	_debugCapture = ConfMan.hasKey("roger_debug_capture") && ConfMan.getBool("roger_debug_capture");
	// roger_diff_check: gated in-engine native-vs-overlay diff (off by default; once per pic;
	// never on steady-state path). Logs ROGER-DIAG[diff] boxes for the missing-graphics audit.
	_diffCheck = ConfMan.hasKey("roger_diff_check") && ConfMan.getBool("roger_diff_check");

	// Overlay-truth captures (spec Phase 2 / Phase 1 fix): with this on, a pending
	// .rin capture grabs the REAL overlay pixels via grabOverlay Ã¢â‚¬â€ the presented pixels,
	// what the player actually sees. The scratch buffer self-heals every cycle (renderFrame
	// fully recomposes it), so a scratch-sourced capture can never witness a missing
	// invalidation mark; grabOverlay reveals stale regions that were never pushed.
	const char *envTruth = getenv("ROGER_TRUTH_CAPTURE");
	_truthCapture = envTruth ? (Common::String(envTruth) != "0" && Common::String(envTruth) != "false")
	                         : (ConfMan.hasKey("roger_truth_capture") && ConfMan.getBool("roger_truth_capture"));

	// Cycle-diff backstop net (spec Phase 2): at the onFrameEnd seam, diff
	// the native visual buffer against the previous cycle's copy and invalidate the
	// changed boxes Ã¢â‚¬â€ a native change that slipped past every invalidation hook heals
	// on the next cycle's present (brief flicker at worst, never persistent staleness).
	// Default ON; roger_diff_net=false is the runtime escape hatch (spec Ã‚Â§6).
	{
		const char *envNet = getenv("ROGER_DIFF_NET");
		_diffNet = envNet ? (Common::String(envNet) != "0" && Common::String(envNet) != "false")
		                  : (!ConfMan.hasKey("roger_diff_net") || ConfMan.getBool("roger_diff_net"));
	}

	// Input automation (scripted verification loop / live remote control). Env-first
	// so build_and_run.ps1 -Script/-Live/-CycleLog can arm a single launch without
	// touching scummvm.ini (same pattern as ROGER_NO_LAUNCHER); the ConfMan knobs
	// work for ini-based setups. Off by default: no knob -> no driver -> zero change.
	const char *envScript = getenv("ROGER_INPUT_SCRIPT");
	Common::String inputScript = envScript ? Common::String(envScript)
		: (ConfMan.hasKey("roger_input_script") ? ConfMan.get("roger_input_script") : Common::String());
	const char *envLive = getenv("ROGER_INPUT_LIVE");
	Common::String inputLive = envLive ? Common::String(envLive)
		: (ConfMan.hasKey("roger_input_live") ? ConfMan.get("roger_input_live") : Common::String());
	_cycleLog = (getenv("ROGER_CYCLE_LOG") != nullptr) ||
	            (ConfMan.hasKey("roger_cycle_log") && ConfMan.getBool("roger_cycle_log"));
	if (!inputScript.empty() || !inputLive.empty()) {
		_inputDriver = new Roger::InputScriptDriver();
		_inputDriver->setScriptHost(this);
		if (!inputScript.empty() && !_inputDriver->loadScriptFile(inputScript))
			warning("ROGER-SCRIPT: script not loaded, running without: %s", inputScript.c_str());
		if (!inputLive.empty())
			_inputDriver->setLiveFile(inputLive);
		// Registered as a backend event source: due events flow through the normal
		// pollEvent path (dispatch drains sources on every poll Ã¢â‚¬â€ blocking dialogs
		// included). Not autoFree: we own it and unregister in the destructor.
		g_system->getEventManager()->getEventDispatcher()->registerSource(_inputDriver, false);
	}

	// Initial display mode (default Enhanced; F10 still cycles from wherever this
	// starts). Env-first so build_and_run.ps1 -Mode can pin a single launch for
	// evidence capture Ã¢â‚¬â€ e.g. -Mode sbs boots straight into Side-by-Side for an
	// enhanced-vs-native comparison shot with no F10 keypress choreography Ã¢â‚¬â€ without
	// touching scummvm.ini (same pattern as ROGER_INPUT_SCRIPT); roger_display_mode
	// works for ini-based setups. Values: enhanced | original | sbs.
	{
		const char *envMode = getenv("ROGER_DISPLAY_MODE");
		Common::String modeStr = envMode ? Common::String(envMode)
			: (ConfMan.hasKey("roger_display_mode") ? ConfMan.get("roger_display_mode") : Common::String());
		if (modeStr == "original")
			_mode = Roger::kModeOriginal;
		else if (modeStr == "sbs" || modeStr == "side-by-side")
			_mode = Roger::kModeSideBySide;
		else if (!modeStr.empty() && modeStr != "enhanced")
			warning("ROGER: unknown display mode '%s' (want enhanced|original|sbs) - using enhanced", modeStr.c_str());
		if (_mode != Roger::kModeEnhanced)
			warning("ROGER: display mode -> %s (startup)",
			        _mode == Roger::kModeOriginal ? "ORIGINAL (native)" : "SIDE-BY-SIDE (enhanced|original)");
	}

	// Cursor: the backend hardware cursor is composited ABOVE the OSystem overlay
	// and leaks at the screen edge (verified in live play), so Roger composites its
	// own arrow into the overlay scene and actively hides the HW cursor while
	// Enhanced/Side-by-Side is on-screen (see hidesNativeCursor()). Default to the
	// composited cursor. roger_hw_cursor=true opts back into the stock native
	// cursor for experimentation. Default false.
	_useHwCursor = false;
	if (ConfMan.hasKey("roger_hw_cursor"))
		_useHwCursor = ConfMan.getBool("roger_hw_cursor");

	// roger_transitions: mirror SCI room transitions + shake in the overlay (default on).
	if (ConfMan.hasKey("roger_transitions"))
		_transitionsEnabled = ConfMan.getBool("roger_transitions");

	// roger_palette_live: mirror live EGA palette changes (fades, flashes) into the
	// overlay plate via per-frame palette diff + partial/full re-blend (default on).
	if (ConfMan.hasKey("roger_palette_live"))
		_paletteLive = ConfMan.getBool("roger_palette_live");

	// roger_gen_mode: controls on-the-fly art generation. Default "cache" =>
	// generate on a miss, load from the content cache on a hit (in-engine generation
	// is the art path). "prebuilt" is the off-switch (native-only render). Other
	// modes: "memory" (generate, never write), "always" (regenerate + overwrite).
	Roger::GenMode genMode = Roger::kGenCache;
	if (ConfMan.hasKey("roger_gen_mode")) {
		const Common::String modeStr = ConfMan.get("roger_gen_mode");
		if (modeStr == "prebuilt")
			genMode = Roger::kGenPrebuilt;
		else if (modeStr == "cache")
			genMode = Roger::kGenCache;
		else if (modeStr == "memory")
			genMode = Roger::kGenMemory;
		else if (modeStr == "always")
			genMode = Roger::kGenAlways;
		// else: unrecognized => keep the default kGenCache
	}

	const Common::String cacheDir = _basePath + "/cache";
	_assetGen = new Roger::RogerAssetGen(gameId, cacheDir, genMode);

	// Aspect-ratio correction stretches the 320x200 frame to 4:3 (200 -> 240 rows,
	// +20% vertical) and upstream enables it BY DEFAULT (commit 2870f3627c3). Roger's
	// art is square-pixel (the plate is an exact 6x of the 320x190 picture), so the
	// default stretch resamples the enhanced scene 20% too tall Ã¢â‚¬â€ the picture band
	// measures 228 game-rows instead of 190. While the generating path is active, pin
	// the correction off. An EXPLICIT aspect_ratio in the config (ini / command line)
	// still wins: ConfMan.hasKey skips the defaults domain, so only the silent
	// upstream default is overridden.
	if (genMode != Roger::kGenPrebuilt && !ConfMan.hasKey("aspect_ratio") &&
	    g_system->hasFeature(OSystem::kFeatureAspectRatioCorrection) &&
	    g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection)) {
		g_system->beginGFXTransaction();
		g_system->setFeatureState(OSystem::kFeatureAspectRatioCorrection, false);
		g_system->endGFXTransaction();
		warning("ROGER: aspect-ratio correction (default-on upstream) disabled for square-pixel art; set aspect_ratio in scummvm.ini to override");
	}

	// roger_omyac_passes: three-state semantics Ã¢â‚¬â€
	//   unset           => default sequence (kDefaultPassString)
	//   set to ""       => wireframe (empty array = zero passes)
	//   set to tokens   => parsed list (compact chars or fill/f=2, line/l=1, all/a=0)
	_assetGen->setEnhancePasses(
		Roger::effectivePasses(ConfMan.hasKey("roger_omyac_passes"),
		                       ConfMan.hasKey("roger_omyac_passes") ? ConfMan.get("roger_omyac_passes") : "")
	);

	applyNativeCursorVisibility();
}

bool FileRogerArtProvider::isOverlayVisible() const {
	return overlayShown();
}

bool FileRogerArtProvider::hasBackground(GuiResourceId pictureId) const {
	if (!enabled)
		return false;
	// No per-frame view-type check here Ã¢â‚¬â€ it stays off the hot render path. A non-EGA
	// game that slips past the launcher's add-time VGA block is caught once on its first
	// pushHiresBackground(), which disables the overlay (enabled=false) so we never reach
	// here again for it.
	return _assetGen && _assetGen->mode() != Roger::kGenPrebuilt;
}

void FileRogerArtProvider::precacheAll() {
	// One-time startup warm-up. Only runs in a generating mode (prebuilt mode has
	// nothing to cache). roger_precache selects the scope: all|pics|views|off
	// (default all when the key is unset). Generic: no per-game table Ã¢â‚¬â€ we ask the
	// live engine for its pic/view resources.
	if (!_assetGen || _assetGen->mode() == Roger::kGenPrebuilt)
		return;
	// Default "off": precache is opt-in via the Roger launcher per-game settings.
	// Games configured via the launcher will have roger_precache set explicitly.
	Common::String scope = "off";
	if (ConfMan.hasKey("roger_precache"))
		scope = ConfMan.get("roger_precache");
	if (scope == "off")
		return;
	const bool doPics  = (scope == "all" || scope == "pics");
	const bool doViews = (scope == "all" || scope == "views");
	if (!doPics && !doViews)
		return; // unrecognized value -> nothing to do
	if (!g_sci)
		return;
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan)
		return;

	// EGA SCI0 only Ã¢â‚¬â€ by design, permanently. VGA/SCI1 is out of scope (not deferred);
	// the omyac pipeline is EGA-specific. Reject cleanly and fall back to native render.
	if (resMan->getViewType() != kViewEga) {
		warning("ROGER: VGA game detected Ã¢â‚¬â€ Roger art replacement supports EGA games only. Overlay disabled.");
		enabled = false;
		applyNativeCursorVisibility();
		return;
	}

	uint32 t0 = g_system->getMillis();

	if (doPics) {
		Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
		const int total = (int)pics.size();
		int done = 0, skipped = 0, alreadyCached = 0;
		const bool isEga = (resMan->getViewType() == kViewEga);
		warning("ROGER precache: warming %d pic plates (mode=%d)...", total, (int)_assetGen->mode());
		for (Common::List<ResourceId>::const_iterator it = pics.begin(); it != pics.end(); ++it) {
			GuiResourceId id = (GuiResourceId)it->getNumber();

			// Skip non-EGA pics Ã¢â‚¬â€ Roger only processes EGA pics via omyac.
			Resource *picRes = resMan->findResource(ResourceId(kResourceTypePic, (uint16)id), false);
			if (picRes && picRes->size() >= 2) {
				const Roger::PicFormat picFmt = Roger::picResourceFormat(
					picRes->data(), (uint32)picRes->size(), isEga);
				if (picFmt != Roger::kPicSci0Ega) {
					++skipped;
					continue;
				}
			}

			if (_assetGen->isPicCached(id)) {
				++done;
				++alreadyCached;
				continue; // keyed files exist; nothing to warm, skip the decode
			}

			uint32 ms = 0;
			Graphics::Surface *s = _assetGen->generatePlate(id, ms); // cache mode writes the PNG
			if (s) { s->free(); delete s; }                          // we only wanted it on disk
			// Warm the hires priority cache too (same content hash + passes).
			Common::Array<byte> bands; int bw = 0, bh = 0; uint32 pms = 0;
			_assetGen->generatePriorityMap(id, bands, bw, bh, pms);
			++done;
			warning("ROGER precache: pic %d (%d/%d) plate %u ms, prio %u ms", id, done, total, ms, pms);
		}
		warning("ROGER precache: %d pic plates warmed (%d already cached), %d non-EGA skipped",
		        done, alreadyCached, skipped);
	}

	if (doViews && g_sci->_gfxCache) {
		Common::List<ResourceId> views = resMan->listResources(kResourceTypeView);
		int warmed = 0, celsCached = 0;
		for (Common::List<ResourceId>::const_iterator it = views.begin(); it != views.end(); ++it) {
			const int viewId = it->getNumber();
			GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
			if (!view)
				continue; // missing/malformed view -> skip (Hard Constraint 6)
			// Snapshot loop/cel counts NOW, while 'view' is valid. generateViewCel()
			// below calls GfxCache::getView(), which purges the WHOLE view cache when
			// it is full (cache.cpp) Ã¢â‚¬â€ that frees this 'view' pointer. Dereferencing
			// view->getCelCount() after a generate call would read freed memory and
			// trip the assert in GfxView::getCelCount. So never touch 'view' again
			// once generation starts (Hard Constraint 6).
			const int loopCount = (int)view->getLoopCount();
			Common::Array<int> celCounts;
			for (int lp = 0; lp < loopCount; ++lp)
				celCounts.push_back((int)view->getCelCount((int16)lp));
			view = nullptr; // pointer may be invalidated by generateViewCel below
			for (int lp = 0; lp < loopCount; ++lp) {
				for (int cl = 0; cl < celCounts[lp]; ++cl) {
					if (_assetGen->isViewCelCached(viewId, lp, cl)) {
						++warmed;
						++celsCached;
						continue;
					}
					uint32 ms = 0;
					Graphics::Surface *s = _assetGen->generateViewCel(viewId, lp, cl, ms);
					if (s) { s->free(); delete s; } // cache mode wrote it; discard the surface
					++warmed;
				}
			}
		}
		warning("ROGER precache: %d view cels warmed (%d already cached)", warmed, celsCached);
	}

	warning("ROGER precache: done in %u ms total", g_system->getMillis() - t0);
}

bool FileRogerArtProvider::precacheOnePic(GuiResourceId picId, uint32 &ms) {
	if (!_assetGen || _assetGen->mode() == Roger::kGenPrebuilt) return false;
	// Skip non-EGA pics Ã¢â‚¬â€ only EGA pics have the omyac path.
	ResourceManager *resMan = g_sci ? g_sci->getResMan() : nullptr;
	if (resMan) {
		Resource *picRes = resMan->findResource(ResourceId(kResourceTypePic, (uint16)picId), false);
		if (picRes && picRes->size() >= 2) {
			const bool isEga = (resMan->getViewType() == kViewEga);
			if (Roger::picResourceFormat(picRes->data(), (uint32)picRes->size(), isEga)
			        != Roger::kPicSci0Ega) {
				ms = 0;
				return false; // not cached; generates on-demand
			}
		}
	}
	if (_assetGen->isPicCached(picId)) {
		ms = 0;
		return true; // both keyed files exist; skip the decode entirely
	}
	Graphics::Surface *s = _assetGen->generatePlate(picId, ms);
	if (s) { s->free(); delete s; }
	uint32 pms = 0;
	Common::Array<byte> bands; int bw = 0, bh = 0;
	_assetGen->generatePriorityMap(picId, bands, bw, bh, pms);
	return true;
}

bool FileRogerArtProvider::precacheOneView(int viewId) {
	if (!_assetGen || _assetGen->mode() == Roger::kGenPrebuilt) return false;
	if (!g_sci || !g_sci->_gfxCache) return false;
	GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
	if (!view) return false;
	const int loopCount = (int)view->getLoopCount();
	Common::Array<int> celCounts;
	for (int lp = 0; lp < loopCount; ++lp)
		celCounts.push_back((int)view->getCelCount((int16)lp));
	view = nullptr;
	for (int lp = 0; lp < loopCount; ++lp) {
		for (int cl = 0; cl < celCounts[lp]; ++cl) {
			if (_assetGen->isViewCelCached(viewId, lp, cl))
				continue;
			uint32 ms = 0;
			Graphics::Surface *s = _assetGen->generateViewCel(viewId, lp, cl, ms);
			if (s) { s->free(); delete s; }
		}
	}
	return true;
}

void FileRogerArtProvider::pushHiresBackground(GuiResourceId pictureId) {
	// A full (screen-clearing) kDrawPic starts a new scene: reset the pic stack.
	_picStack.clear();
	_picStack.push_back((int)pictureId);
	pushHiresBackgroundInternal(pictureId);
}

void FileRogerArtProvider::pushHiresBackgroundAddTo(GuiResourceId pictureId) {
	// An addTo kDrawPic paints over the current scene WITHOUT clearing it (the
	// SQ3 intro title logo over the starfield, the scanner overlays): append to
	// the pic stack so the regenerated plate/priority map contain the whole
	// sequence. Replacing the plate with the overlay pic's standalone render
	// (the old behavior: this path simply called pushHiresBackground) lost the
	// base pic Ã¢â‚¬â€ a mostly-white "926" plate where the SQ3 logo should be.
	//
	// The internal body treats every call as a room ENTRY and clears the
	// per-room captures; like regenInPlace, an addTo draw is mid-room, so carry
	// them across (they are captured once at the room's entry draws and cannot
	// be re-captured). A redraw of the pic already on top appends nothing
	// (replaying the same commands yields the same pixels).
	if (_picStack.empty() || _picStack.back() != (int)pictureId)
		_picStack.push_back((int)pictureId);
	Common::Array<Roger::Sprite> keepStatics = _staticSprites;
	Common::Array<Roger::Sprite> keepInitCels = _initCels;
	Common::Array<Roger::Sprite> keepText = _textSprites;
	// The copies above are shallow Ã¢â‚¬â€ _textSprites entries OWN their celOverride
	// surfaces and clearTextSprites() (inside the internal body) frees them.
	// Empty the source first so the clear frees nothing (regenInPlace rule).
	_textSprites.clear();
	pushHiresBackgroundInternal(pictureId);
	_staticSprites = keepStatics;
	_initCels = keepInitCels;
	_textSprites = keepText;
	// Mid-room scene change: make the next present a full one (the transition
	// kernelDrawPicture schedules right after this covers the normal case; the
	// full mark is the uncertainty fallback Ã¢â‚¬â€ never a stale frame).
	markFullDirty();
}

void FileRogerArtProvider::pushHiresBackgroundInternal(GuiResourceId pictureId) {
	// EGA SCI0 only Ã¢â‚¬â€ by design, permanently. VGA/SCI1 is out of scope (not deferred);
	// the omyac pipeline is EGA-specific. Reject cleanly and fall back to native render.
	// The launcher blocks VGA games at add-time, but a target configured another way
	// (manual ConfMan / normal ScummVM launcher) can still reach here. Setting enabled = false
	// makes hasBackground() return false from here on, so this fires once per engine instance.
	if (g_sci && g_sci->getResMan() && g_sci->getResMan()->getViewType() != kViewEga) {
		warning("ROGER: not an EGA SCI game - Roger art replacement supports EGA games only. "
		        "Disabling the hires overlay.");
		enabled = false;
		applyNativeCursorVisibility();
		if (_compositor)
			_compositor->setRoom(nullptr, nullptr);
		_loadedPicId = -1;
		return;
	}

	// Same pic re-entered (a script kDrawPic redraw, or an in-game restore into the
	// room already shown): the plate and occlusion map are content-keyed to the pic,
	// so keep them Ã¢â‚¬â€ but SCI just rebuilt the native surface from scratch, and the
	// overlay may hold foreign pixels (the ScummVM GUI after the restore chooser), so
	// the room-entry reset + full re-present below must still run. Early-returning
	// here instead left the restore dialog on screen for seconds (heal-frame latency)
	// with only incrementally-dirtied regions repainting. The plate must also have
	// been generated from the SAME pic stack (an addTo overlay changes the stack
	// without changing _loadedPicId's room identity).
	const bool samePlate = (_loadedPicId == pictureId && _plate && _plateStack == _picStack);

	diagDumpState(samePlate ? "pushBG-enter-samepic" : "pushBG-enter");

	// New room: drop the previous room's captured addToPic cels (Feeder A) + init-baked cels.
	_staticSprites.clear();
	_initCels.clear();
	clearTextSprites();
	_genTextPending.clear(); // discard any pending generic text from the departing room
	_revealRects.clear();   // stale reveal suppressions must not bleed into the new room
	_debugDumpedPic = -1;   // allow a fresh debug-capture dump for this room
	_diffCheckedPic = -1;   // allow a fresh diff-check run for this room
	_haveNetPrev = false;   // room changed: don't diff across rooms (full present covers entry)
	// New room must fully refresh the cursor-restore cache; the transition path pre-validates
	// _bgCache via composeRoomScene so the first renderFrame may not be a full-seed.
	_compositeCacheValid = false;

	uint32 tEnter = g_system->getMillis();

	// New room: drop any dialogs/icons left from the previous room so they do not
	// bleed onto the new scene. _haveScene is rebuilt by the next renderFrame.
	if (_journal) _journal->clear();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_genericGlyphCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_drawCelNativeCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_haveScene = false;

	uint32 genMs = 0;
	const char *plateSrc = samePlate ? "kept(same-pic)" : "none";
	uint32 tAcq0 = g_system->getMillis();
	if (!samePlate) {
		// Evict previous room.
		if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
		_plateStack.clear();
		if (_assetGen && _assetGen->mode() != Roger::kGenPrebuilt) {
			_plateIndex.clear();
			_plate = _assetGen->generatePlateStackWithIndex(_picStack, _plateIndex, genMs);
			if (_plate) {
				plateSrc = genMs ? "generated(miss)" : "cache-hit";
				_plateStack = _picStack;
			}
		}
	}
	// NOTE: no prebuilt-visual fallback. If generation yields nothing, the native
	// render shows (handled by the !_plate block below).
	uint32 plateMs = g_system->getMillis() - tAcq0;
	if (!_plate) {
		// No hires bg -> native shows. Clear the compositor's borrowed plate pointer
		// so it does not retain the plate we just deleted above.
		if (_compositor)
			_compositor->setRoom(nullptr, nullptr);
		_loadedPicId = -1;
		return;
	}

	if (!_viewCache) {
		_viewCache = new Roger::ViewCache(_basePath + "/views");
		_viewCache->setGenerator(_assetGen);
	}
	if (!_compositor)
		_compositor = new Roger::RogerCompositor();
	if (!_capsProbed) {
		_caps = Roger::RogerCapabilities::probe();
		_capsProbed = true;
		_statusBarH = _caps.statusBarRows;   // was hard-coded 10
		if (_debugLog)
			warning("ROGER caps[%s]: ega=%d rows=%d statusBar=%d parser=%d",
			        g_sci ? g_sci->getGameIdStr() : "?",
			        _caps.isEga, _caps.screenRows, _caps.statusBarRows, _caps.hasParser);
	}
	_compositor->setCapabilities(_caps);
	// roger_dirty_present (default on): convert+push only changed regions each frame.
	bool dirtyPresent = true;
	if (ConfMan.hasKey("roger_dirty_present"))
		dirtyPresent = ConfMan.getBool("roger_dirty_present");
	_compositor->setDirtyPresent(dirtyPresent);
	_compositor->setDiag(_diag);
	_compositor->setPresentLog(_cycleLog);
	_compositor->setRoom(_plate, _viewCache);

	// Derive the per-pixel overlay occlusion in-engine from the omyac-enhanced HIRES
	// priority map (1920x1140) whose band edges ride the SAME geometry as the plate,
	// so occlusion tracks the displayed background instead of a clean 6x grid (the old
	// native-res priorityBands drift). Cached as the "omyacprio" transform. No prebuilt
	// occlusion map is consumed.
	uint32 occMs = 0;
	// _picW x _picH is the SCI picture window (320x190); sprite cel rects live in that
	// space, so picH stays 190 regardless of the priority map's hires resolution. (The
	// old code passed prH here only because native priorityBands also returned 190.)
	_compositor->setPicture(320, 190, 0);
	if (!samePlate) {
		int prW = 0, prH = 0;
		_priorityMap.clear();
		uint32 occGenMs = 0;
		uint32 tOcc0 = g_system->getMillis();
		bool haveOcc = (_assetGen && _assetGen->mode() != Roger::kGenPrebuilt &&
		                _assetGen->generatePriorityMapStack(_picStack, _priorityMap, prW, prH, occGenMs));
		occMs = g_system->getMillis() - tOcc0;
		if (haveOcc)
			_compositor->setPriorityMask(_priorityMap.begin(), prW, prH); // hires bands (1920x1140)
		else
			_compositor->setPriorityMask(nullptr, 0, 0); // no bands -> sprites draw without occlusion
	} // samePlate: the compositor already holds this pic's mask; _priorityMap is intact
	_loadedPicId = pictureId;

	// Snapshot the room-load EGA palette for live re-apply. The first 16 OSystem palette
	// entries are the EGA base colors in SCI0 (GfxPalette16::setEGA fills them at indices
	// 0..15). grabPalette(buf, start, count) fills count*3 RGB bytes.
	g_system->getPaletteManager()->grabPalette(_palSnapshot, 0, 16); // 16 colors = 48 bytes
	_haveSnapshot = true;

	// Re-push the cached score/title banner into the UI layer so it is enhanced again
	// after the room change (the game only redraws status on score/text change). The
	// present is deferred to the first kAnimate frame (presentWithUi no-ops until the
	// scene is composited) Ã¢â‚¬â€ presenting the sprite-less plate here flashed a wrong
	// frame over in-progress animations (e.g. the intro pod door open/shut/open).
	reapplyStatus();

	// Per-room-entry timing breakdown so cache benefit is measurable: plate is the
	// cost of acquiring the visual (full omyac generation on a miss, PNG decode on a
	// cache-hit, or prebuilt-PNG load); occlusion-map is the EGA priority-band decode;
	// rest is compositor/UI setup. Compare "generated(miss)" vs "cache-hit" plate ms.
	if (_debugLog) {
		uint32 totalMs = g_system->getMillis() - tEnter;
		debug("Roger: enter room %d in %u ms  [plate %u ms (%s), occlusion-map %u ms, rest %u ms]",
		      pictureId, totalMs, plateMs, plateSrc, occMs, totalMs - plateMs - occMs);
	}

	if (_selfTest) {
		Roger::SelfTestInputs in;
		in.plateGenerated = (_plate != nullptr);
		in.plateW = _plate ? _plate->w : 0;
		in.plateH = _plate ? _plate->h : 0;
		in.expectW = in.plateW;  // plate is the upscale of the pic; the invariant is "non-empty + matches what we built"
		in.expectH = in.plateH;
		in.priorityMapPresent = !_priorityMap.empty();
		in.overlayEnabled = enabled;
		Roger::SelfTestResult r = Roger::evaluateInvariants(in, _caps);
		Roger::logSelfTest(g_sci ? g_sci->getGameIdStr() : "?", (int)pictureId, r);
	}

	diagDumpState("pushBG-exit");
}

// The backend places the native game screen from three live inputs Ã¢â‚¬â€ aspect-ratio
// correction, stretch mode (Ctrl+Alt+S cycles it at runtime), and the software-scaler
// factor. Mirror all three so the overlay geometry tracks the native placement exactly
// in every mode, keeping Enhanced/Original toggles shift-free and the backend's
// game-space mouse mapping aligned with what Roger paints.
static Common::Rect currentGameRect(int overlayW, int overlayH) {
	const bool aspect = g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection);
	const int stretch = g_system->hasFeature(OSystem::kFeatureStretchMode)
	                  ? g_system->getStretchMode() : (int)Roger::kStretchFit;
	return Roger::computeGameRect(overlayW, overlayH, aspect, stretch,
	                              (int)g_system->getScaleFactor());
}

void FileRogerArtProvider::observeLivePalette() {
	if (!_paletteLive || _plateIndex.empty() || !_haveSnapshot || !_plate)
		return;
	byte live[48];
	g_system->getPaletteManager()->grabPalette(live, 0, 16);
	bool changed[16];
	const int n = Roger::paletteDiffMask(_palSnapshot, live, changed);
	if (n == 0)
		return; // common case: zero extra present cost

	uint32 table[256];
	Roger::buildLivePaletteTable(_palSnapshot, live, table);

	const int kPartialMax = 4;
	if (n <= kPartialMax) {
		// Partial: re-blend only changed-index pixels, mark just that region dirty.
		Common::Rect plateDirty;
		Roger::reblendChangedPixels(_plateIndex.begin(), Roger::OMYAC_HYBRID_W, Roger::OMYAC_HYBRID_H,
		                            table, changed, *_plate, plateDirty);
		_compositor->invalidateBackgroundCache(); // plate pixels mutated; force bgCache rebuild
		if (!plateDirty.isEmpty()) {
			// Map plate-space bbox -> dest/overlay space (same scale renderScene uses).
			const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
			const Common::Rect gameRect = currentGameRect(OW, OH);
			const Common::Rect picRect = Roger::computePictureRect(gameRect, _statusBarH);
			const int pw = Roger::OMYAC_HYBRID_W, ph = Roger::OMYAC_HYBRID_H;
			Common::Rect d(
				(int16)(picRect.left + plateDirty.left   * picRect.width()  / pw),
				(int16)(picRect.top  + plateDirty.top    * picRect.height() / ph),
				(int16)(picRect.left + plateDirty.right  * picRect.width()  / pw + 1),
				(int16)(picRect.top  + plateDirty.bottom * picRect.height() / ph + 1));
			_compositor->addDirtyRect(d);
		}
		for (int i = 0; i < 48; i++) _palSnapshot[i] = live[i];
		return;
	}

	// Whole-palette change (fade/flash/day-night, e.g. the pod shutting down).
	// The omyac-enhanced plate contains blended/anti-aliased colors that are NOT pure
	// EGA palette indices, so a per-pixel reblend through the 16-entry index map
	// mis-recolors them Ã¢â‚¬â€ on a large palette change the whole plate turns to garbage.
	// Skip the whole-plate reblend: the plate keeps its room-load colors (no fade on the
	// hires background) rather than corrupting. The partial color-cycle path above
	// (n <= kPartialMax) is unaffected, so per-index animations still work.
	const uint32 now = g_system->getMillis();
	if (now - _lastPaletteCheckMs < 16)
		return;
	_lastPaletteCheckMs = now;
}

void FileRogerArtProvider::renderFrame(const Common::Array<Roger::Sprite> &sprites) {
	if (!overlayShown() || !_compositor || !_plate)
		return;
	diagDumpState("renderFrame");
	observeLivePalette();
	// Composite in RGBA32 so the alpha-aware blendBlitFrom (used for view cels) works
	// - it requires an RGBA32 destination. presentToOverlay converts the finished
	// scene to the actual overlay format before pushing it.
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();

	// H4 geometry: place the picture (plate + sprites) where the native game is
	// actually drawn on screen. The overlay fills the whole window but is alpha-
	// blended over the still-rendered native game, which the backend draws into a
	// sub-rect governed by its stretch mode + aspect flag. Replicate that placement
	// so the overlay lines up 1:1 (no shift when toggled), reserving the top
	// status-bar strip so the native "Score:" bar shows through the (transparent)
	// overlay there.
	const Common::Rect gameRect = currentGameRect(OW, OH);
	const Common::Rect picRect = Roger::computePictureRect(gameRect, _statusBarH);
	_compositor->setPictureDest(picRect);

	// Reuse a persistent scratch buffer; renderScene clears + fully redraws it, so no
	// stale pixels survive between frames.
	Graphics::ManagedSurface &scene = *scratchScene(OW, OH);
	_compositor->renderScene(scene, sprites, gameRect);
	const bool drewGeneric = drawGenericRegions(scene, picRect);

	// Cache the composed room+sprite scene so a UI-only change can be re-presented
	// cheaply (blocking dialogs do not tick kernelAnimate).
	bool sceneCacheRealloc = false;
	if (!_sceneCache || _sceneCache->w != scene.w || _sceneCache->h != scene.h ||
	    _sceneCache->format != scene.format) {
		delete _sceneCache;
		_sceneCache = new Graphics::ManagedSurface(scene.w, scene.h, scene.format);
		sceneCacheRealloc = true;
	}
	// Bound the copy to the regions renderScene re-drew this frame (the sprite-rect union):
	// the persistent _sceneCache keeps outside-union pixels valid from prior frames, exactly
	// like the scratch scene's static background. A FULL copy runs on (re)alloc, on any
	// full-seed frame (rebuild/transition/shake/heal Ã¢â‚¬â€ lastSceneWasFull), and when generic
	// regions (inventory/close-up upscales) were drawn outside the sprite union Ã¢â‚¬â€ so the cache
	// is never left partial. Copies into the existing allocation (no per-frame free+malloc).
	const bool fullSceneCopy = sceneCacheRealloc || _compositor->lastSceneWasFull() || drewGeneric;
	if (fullSceneCopy) {
		_sceneCache->copyRectToSurface(scene.rawSurface(), 0, 0, Common::Rect(0, 0, scene.w, scene.h));
	} else {
		const Common::Array<Common::Rect> &u = _compositor->lastSeedUnion();
		for (uint i = 0; i < u.size(); i++)
			_sceneCache->copyRectToSurface(scene.rawSurface(), u[i].left, u[i].top, u[i]);
	}
	_haveScene = true;
	_lastGameRect = gameRect;

	// If a dialog is already up, re-blend it on top of the freshly composed scene.
	if (_journal && !_journal->empty() && _textRenderer) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		_compositor->renderUiLayer(scene, _journal->ops(), pal, gameRect, _textRenderer, _altTextRenderer);
	}
	// Snapshot scene+UI (no cursor) Ã¢â‚¬â€ the barrier's bounded path patches and
	// presents from this cache, so it must stay valid under BOTH cursor modes.
	// _compositeCacheValid coming in tells us a full cursor-free snapshot from a prior
	// frame is intact; ensureCompositeCache clears it on (re)alloc. The bounded path
	// reads arbitrary rects from this cache (not just the seed union), so a bounded copy
	// is only safe when that full prior snapshot exists AND this frame touched only the
	// union; otherwise (realloc, full-seed, generic regions, or a prior invalidation) do
	// a full copy. Copies into the existing allocation.
	const bool priorValid = _compositeCacheValid;
	ensureCompositeCache(OW, OH);
	if (fullSceneCopy || !priorValid || !_compositeCacheValid) {
		_compositeCache->copyRectToSurface(scene.rawSurface(), 0, 0, Common::Rect(0, 0, scene.w, scene.h));
	} else {
		const Common::Array<Common::Rect> &u = _compositor->lastSeedUnion();
		for (uint i = 0; i < u.size(); i++)
			_compositeCache->copyRectToSurface(scene.rawSurface(), u[i].left, u[i].top, u[i]);
	}
	_compositeCacheValid = true;
	_frameJustComposed = true; // presentBarrier() (the onAnimateFrame tail) presents this frame
}

void FileRogerArtProvider::dumpOverlaySnap(const Common::String &label,
                                           const Common::Rect &gameRect) {
	// Presented-frame evidence: dump the REAL overlay pixels via grabOverlay Ã¢â‚¬â€
	// what the player sees right now, including any stale never-pushed regions.
	// Shared by truth-mode `capture` (present-consumed) and `snap` (immediate).
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0 || gameRect.isEmpty()) {
		warning("ROGER-SCRIPT: snap '%s' before first present - skipped", label.c_str());
		return;
	}
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	const Graphics::PixelFormat overlayFmt = g_system->getOverlayFormat();
	Graphics::Surface raw;
	raw.create(OW, OH, overlayFmt);
	g_system->grabOverlay(raw);
	Graphics::ManagedSurface overlayRGBA(OW, OH, rgba);
	Graphics::Surface *converted = raw.convertTo(rgba);
	if (converted) {
		overlayRGBA.copyRectToSurface(*converted, 0, 0, Common::Rect(0, 0, OW, OH));
		converted->free();
		delete converted;
	}
	raw.free();
	dumpAutoshot(overlayRGBA, gameRect, ("-" + label).c_str());
}

void FileRogerArtProvider::maybeScriptCapture(Graphics::ManagedSurface &scene,
                                              const Common::Rect &gameRect) {
	// Scripted `capture <label>`: one-shot dump at the next present after the
	// command's due time. Reuses the autoshot writer (same screenshotpath +
	// naming: roger-<pic>-<label>-{overlay,preview}.png). O(1) when idle.
	if (!_inputDriver)
		return;
	Common::String label;
	if (!_inputDriver->takeCaptureRequest(label))
		return;
	if (_truthCapture) {
		// Truth-capture mode: dump the REAL overlay pixels via grabOverlay Ã¢â‚¬â€ called
		// AFTER presentToOverlay has pushed this present's regions, so the grab
		// reflects those pushes plus any regions that were never pushed (stale).
		dumpOverlaySnap(label, gameRect);
	} else {
		dumpAutoshot(scene, gameRect, ("-" + label).c_str());
	}
}

// Roger::ScriptHost implementation Ã¢â‚¬â€ game-side services for .rin loop commands.

int FileRogerArtProvider::uiWindowCount() const {
	if (!_journal)
		return 0;
	int n = 0;
	const Common::Array<Roger::UiElement> &es = _journal->ops();
	for (uint i = 0; i < es.size(); i++) {
		if (es[i].type == Roger::kUiWindow)
			n++;
	}
	return n;
}

Common::String FileRogerArtProvider::describeState() {
	int egoX = -1, egoY = -1;
	if (g_sci && g_sci->getEngineState() && g_sci->getEngineState()->_segMan) {
		EngineState *s = g_sci->getEngineState();
		SegManager *segMan = s->_segMan;
		// global var 0 holds the live ego instance (kGlobalVarEgo).
		// findObjectByName("ego") resolves the class template (or NULL on ambiguity),
		// not the live object Ã¢â‚¬â€ so egox/egoy always read 0,0 from it.
		if (s->variables[VAR_GLOBAL]) {
			const reg_t ego = s->variables[VAR_GLOBAL][kGlobalVarEgo];
			if (!ego.isNull() && segMan->isObject(ego)) {
				egoX = readSelectorValue(segMan, ego, SELECTOR(x));
				egoY = readSelectorValue(segMan, ego, SELECTOR(y));
			}
		}
	}
	const char *modeStr = (_mode == Roger::kModeOriginal) ? "original"
	                    : (_mode == Roger::kModeSideBySide) ? "sbs" : "enhanced";
	return Common::String::format("pic=%d ego=%d,%d windows=%d mode=%s",
	                              _loadedPicId, egoX, egoY, uiWindowCount(), modeStr);
}

int FileRogerArtProvider::stateValue(const Common::String &key) {
	if (key == "pic")
		return _loadedPicId;
	if (key == "windows")
		return uiWindowCount();
	if (key == "mode")
		return (int)_mode;
	if (key == "egox" || key == "egoy") {
		if (g_sci && g_sci->getEngineState() && g_sci->getEngineState()->_segMan) {
			EngineState *s = g_sci->getEngineState();
			SegManager *segMan = s->_segMan;
			// global var 0 holds the live ego instance (kGlobalVarEgo).
			if (s->variables[VAR_GLOBAL]) {
				const reg_t ego = s->variables[VAR_GLOBAL][kGlobalVarEgo];
				if (!ego.isNull() && segMan->isObject(ego))
					return readSelectorValue(segMan, ego,
						(key == "egox") ? SELECTOR(x) : SELECTOR(y));
			}
		}
		return -1;
	}
	warning("ROGER-SCRIPT: unknown state key '%s' (want pic|windows|egox|egoy|mode)", key.c_str());
	return -1;
}

void FileRogerArtProvider::onSnap(const Common::String &label) {
	dumpOverlaySnap(label, _lastGameRect);
}

void FileRogerArtProvider::onRestore(int slot) {
	// Delayed restore: SciEngine::loadGameState just sets _delayedRestoreGameId;
	// the game loop performs the restore at its own safe point. During a frozen
	// blocking dialog it is deferred until the dialog closes.
	if (g_sci) {
		warning("ROGER-SCRIPT: restore slot %d (delayed)", slot);
		g_sci->loadGameState(slot);
	}
}

void FileRogerArtProvider::dumpAutoshot(Graphics::ManagedSurface &scene,
                                        const Common::Rect &gameRect, const char *suffix) {
	// Output goes to the configured screenshotpath. Two PNGs:
	//   roger-<id><suffix>-overlay.png Ã¢â‚¬â€ Roger's composited layer alone (letterbox +
	//                            status strip are transparent, shown as black by a viewer)
	//   roger-<id><suffix>-preview.png Ã¢â‚¬â€ the true on-screen result: the native 320x200
	//                            game scaled into gameRect with Roger's layer blended over
	//                            it (mirrors the backend draw order), so plate/native
	//                            alignment (and, for the "-ui" dump, native-vs-hires dialog
	//                            alignment) and the status bar showing through are visible.
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	const int OW = scene.w, OH = scene.h;
	Common::String dir;
	if (ConfMan.hasKey("screenshotpath"))
		dir = ConfMan.getPath("screenshotpath").toString('/');
	if (!dir.empty() && dir.lastChar() != '/')
		dir += '/';
	const Common::String base = dir + Common::String::format("roger-%d%s", _loadedPicId, suffix);

	if (Roger::dumpSurfacePng(*scene.surfacePtr(), base + "-overlay.png"))
		warning("ROGER: capture wrote %s-overlay.png", base.c_str());

	Graphics::Surface *nativeScreen = g_system->lockScreen();
	if (nativeScreen) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		Graphics::Surface *nativeRGBA = nativeScreen->convertTo(rgba, pal, 256);
		g_system->unlockScreen();
		if (nativeRGBA) {
			Graphics::ManagedSurface preview(OW, OH, rgba);
			preview.fillRect(Common::Rect(0, 0, OW, OH), rgba.ARGBToColor(255, 0, 0, 0));
			preview.blitFrom(*nativeRGBA, Common::Rect(0, 0, nativeRGBA->w, nativeRGBA->h), gameRect);
			preview.blendBlitFrom(*scene.surfacePtr(), Common::Rect(0, 0, scene.w, scene.h),
			                      Common::Rect(0, 0, (int16)OW, (int16)OH));
			if (Roger::dumpSurfacePng(*preview.surfacePtr(), base + "-preview.png"))
				warning("ROGER: capture wrote %s-preview.png", base.c_str());
			nativeRGBA->free();
			delete nativeRGBA;
		}
	}
}

void FileRogerArtProvider::dumpCaptureDebug() {
	if (!_debugCapture || _loadedPicId == _debugDumpedPic)
		return;
	_debugDumpedPic = _loadedPicId;

	// Build output dir from screenshotpath (same convention as dumpAutoshot).
	Common::String dir;
	if (ConfMan.hasKey("screenshotpath"))
		dir = ConfMan.getPath("screenshotpath").toString('/');
	if (dir.empty())
		dir = "screenshots";
	if (!dir.empty() && dir.lastChar() != '/')
		dir += '/';
	const Common::String base = Common::String::format("%s%s.pic%d.capture",
		dir.c_str(), _gameId.c_str(), _loadedPicId);

	// Write the text manifest: one line per kUiText element and one line per graphic sprite.
	Common::DumpFile mf;
	if (mf.open(Common::Path(base + ".txt"), true)) {
		if (_journal) {
			const Common::Array<Roger::UiElement> &es = _journal->ops();
			for (uint i = 0; i < es.size(); i++) {
				if (es[i].type == Roger::kUiText) {
					Common::String line = Common::String::format(
						"TEXT rect=(%d,%d,%d,%d) font=%d color=%d \"%s\"\n",
						es[i].nativeRect.left, es[i].nativeRect.top,
						es[i].nativeRect.right, es[i].nativeRect.bottom,
						es[i].fontId, es[i].penColor, es[i].text.c_str());
					mf.write(line.c_str(), line.size());
				}
			}
		}
		for (uint i = 0; i < _textSprites.size(); i++) {
			const Common::Rect &r = _textSprites[i].celRect;
			Common::String line = Common::String::format(
				"GFX  rect=(%d,%d,%d,%d) -> %s.pic%d.gfx.%d_%d.png\n",
				r.left, r.top, r.right, r.bottom,
				_gameId.c_str(), _loadedPicId, r.left, r.top);
			mf.write(line.c_str(), line.size());
		}
		mf.close();
		warning("ROGER: debug_capture wrote %s.txt", base.c_str());
	}

	// PNG per pixel-captured graphic sprite (reuses Roger::dumpSurfacePng from png_loader.h,
	// the same helper used by the .rin capture path via dumpAutoshot).
	for (uint i = 0; i < _textSprites.size(); i++) {
		if (!_textSprites[i].celOverride) continue;
		const Common::Rect &r = _textSprites[i].celRect;
		Common::String png = Common::String::format("%s%s.pic%d.gfx.%d_%d.png",
			dir.c_str(), _gameId.c_str(), _loadedPicId, r.left, r.top);
		if (Roger::dumpSurfacePng(*_textSprites[i].celOverride, png))
			warning("ROGER: debug_capture wrote gfx sprite %s", png.c_str());
	}
}

void FileRogerArtProvider::runDiffCheck() {
	// Off by default (roger_diff_check); once per pic; never on steady-state path.
	if (!_diffCheck || _loadedPicId == _diffCheckedPic || !_compositeCacheValid || !_compositeCache)
		return;
	if (!g_sci || !g_sci->_gfxScreen)
		return;
	_diffCheckedPic = _loadedPicId;

	GfxScreen *screen = g_sci->_gfxScreen;
	const int sw = screen->getWidth();   // 320 (SCI0)
	const int sh = screen->getHeight();  // 200

	// 1) Snapshot native visual buffer as EGA indices (1 byte/pixel, 320x200).
	Common::Array<byte> natIdx;
	natIdx.resize((uint)sw * sh);
	for (int y = 0; y < sh; y++)
		for (int x = 0; x < sw; x++)
			natIdx[(uint)y * sw + x] = screen->getVisual((int16)x, (int16)y);

	// 2) Downscale the composited overlay (_compositeCache, RGBA32, OWxOH) to 320x200.
	//    Nearest-neighbour: for each native pixel, sample the overlay at the proportional
	//    source coordinate. _compositeCache format: RGBA32 (4 bytes/px, alpha at byte 3).
	const int OW = _compositeCache->w;
	const int OH = _compositeCache->h;
	// Build a per-pixel "overlay has visible content" mask (1 = alpha > 0, 0 = transparent).
	Common::Array<byte> ovlMask;
	ovlMask.resize((uint)sw * sh, 0);
	for (int y = 0; y < sh; y++) {
		for (int x = 0; x < sw; x++) {
			const int sx = x * OW / sw;
			const int sy = y * OH / sh;
			if (sx < 0 || sy < 0 || sx >= OW || sy >= OH)
				continue;
			const uint32 px = _compositeCache->getPixel(sx, sy);
			// RGBA32 format: aShift=0 Ã¢â€ â€™ alpha is in bits 7..0 (lowest byte).
			const byte alpha = (byte)(px & 0xFF);
			ovlMask[(uint)y * sw + x] = (alpha > 0) ? 1 : 0;
		}
	}

	// 3) Build "native non-BG, overlay missing" mask.
	//    native non-BG: EGA index 0 is black (the universal SCI0 background clear color);
	//    treat index 0 as background and any other index as potentially visible content.
	//    diff[i] = 1 where native has visible content (idx != 0) AND overlay is transparent.
	Common::Array<byte> diffMask;
	diffMask.resize((uint)sw * sh, 0);
	int missingPixels = 0;
	for (int i = 0; i < sw * sh; i++) {
		if (natIdx[(uint)i] != 0 && ovlMask[(uint)i] == 0) {
			diffMask[(uint)i] = 1;
			missingPixels++;
		}
	}

	if (missingPixels == 0) {
		warning("ROGER-DIAG[diff]: pic=%d no missing pixels (native visible pixels all covered by overlay)", _loadedPicId);
		return;
	}

	// 4) Coalesce into boxes using the existing changed-box extractor.
	//    extractChangedBoxes(prev, cur, w, h, out): boxes where prev != cur.
	//    Using all-zeros as "prev" and diffMask as "cur" gives boxes of non-zero diff pixels.
	Common::Array<byte> zeros;
	zeros.resize((uint)sw * sh, 0);
	Common::Array<Common::Rect> boxes;
	Roger::extractChangedBoxes(zeros.begin(), diffMask.begin(), sw, sh, boxes);

	warning("ROGER-DIAG[diff]: pic=%d missingPx=%d boxes=%u (native visible, overlay transparent)",
	        _loadedPicId, missingPixels, (unsigned)boxes.size());
	for (uint i = 0; i < boxes.size(); i++) {
		const Common::Rect &b = boxes[i];
		// Also note native EGA index at box centre for identification.
		const int cx = (b.left + b.right) / 2, cy = (b.top + b.bottom) / 2;
		const byte cIdx = natIdx[(uint)cy * sw + cx];
		warning("ROGER-DIAG[diff]: pic=%d box[%u] rect=(%d,%d,%d,%d) w=%d h=%d nativeIdxAtCenter=%d",
		        _loadedPicId, i, b.left, b.top, b.right, b.bottom,
		        b.width(), b.height(), (int)cIdx);
	}
}

// Ray-casting point-in-polygon test (design-space coords).
static bool rogerPointInPoly(const double *vx, const double *vy, int n, double x, double y) {
	bool in = false;
	for (int i = 0, j = n - 1; i < n; j = i++) {
		if (((vy[i] > y) != (vy[j] > y)) &&
		    (x < (vx[j] - vx[i]) * (y - vy[i]) / (vy[j] - vy[i]) + vx[i]))
			in = !in;
	}
	return in;
}

void FileRogerArtProvider::ensureCursor() {
	if (_cursorSurf)
		return;
	// A classic arrow pointer (tip at design 0,0): white fill + black outline. Built
	// by supersampling a polygon and dilating for the outline, drawn large so it is
	// visible over the hires overlay (the native 16px SCI cursor is invisible there).
	// CLUT8 (index 0 transparent, 1 black, 2 white) Ã¢â‚¬â€ the same proven cursor path the
	// SCI driver uses; an RGBA cursor was invisible under the backend's premultiplied
	// cursor blend. dontScale keeps it this pixel size in the window.
	int side = 44;
	if (ConfMan.hasKey("roger_cursor_size"))
		side = ConfMan.getInt("roger_cursor_size");
	const int S = side;
	static const double vx[] = { 0, 0, 4.2, 6.8, 9.0, 5.3, 10.5 };
	static const double vy[] = { 0, 15, 11.5, 17.5, 16.3, 11.0, 11.0 };
	const int N = 7;
	const double scale = (double)S / 20.0; // design box ~20 tall

	Common::Array<double> cov;
	cov.resize(S * S);
	const int SS = 4;
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			int inside = 0;
			for (int i = 0; i < SS; i++) {
				for (int j = 0; j < SS; j++) {
					const double fx = (x + (i + 0.5) / SS) / scale;
					const double fy = (y + (j + 0.5) / SS) / scale;
					if (rogerPointInPoly(vx, vy, N, fx, fy))
						inside++;
				}
			}
			cov[y * S + x] = (double)inside / (SS * SS);
		}
	}

	// RGBA with straight alpha Ã¢â‚¬â€ Roger composites this into its own overlay scene
	// (its alpha-aware blit honours partial alpha), so the edges are anti-aliased.
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	_cursorSurf = new Graphics::Surface();
	_cursorSurf->create(S, S, rgba);
	const int R = 2; // outline radius (px)
	for (int y = 0; y < S; y++) {
		for (int x = 0; x < S; x++) {
			const double fillA = cov[y * S + x];
			double dil = 0.0; // dilated coverage -> black outline reaches R px out
			for (int dy = -R; dy <= R && dil < 1.0; dy++) {
				for (int dx = -R; dx <= R; dx++) {
					const int nx = x + dx, ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= S || ny >= S)
						continue;
					if (dx * dx + dy * dy > R * R)
						continue;
					dil = MAX(dil, cov[ny * S + nx]);
				}
			}
			const double outA = fillA + dil * (1.0 - fillA);
			uint32 px;
			if (outA <= 0.0) {
				px = rgba.ARGBToColor(0, 0, 0, 0); // transparent
			} else {
				// white fill over black outline; luminance = white's weight (black=0).
				const int lum = (int)((255.0 * fillA) / outA + 0.5);
				const int a = (int)(outA * 255.0 + 0.5);
				px = rgba.ARGBToColor((byte)a, (byte)lum, (byte)lum, (byte)lum);
			}
			_cursorSurf->setPixel(x, y, px);
		}
	}
	_cursorHotspot = Common::Point(2, 2); // tip is 2px from top-left corner of the surface
}

void FileRogerArtProvider::onCursorShape(int cursorId) {
	if (cursorId == _cursorShapeId)
		return;
	_cursorShapeId = cursorId;
	buildCursorForShape(cursorId);
}

void FileRogerArtProvider::onCursorHidden(bool hidden) {
	_cursorVisible = !hidden;
}

void FileRogerArtProvider::onCursorView(int viewId, int loopNo, int celNo) {
	buildCursorFromView(viewId, loopNo, celNo);
}

bool FileRogerArtProvider::hidesNativeCursor() const {
	// prebuilt = native-only mode: the overlay is never presented and Roger
	// composites no cursor, so keep the stock hardware cursor (don't veto it).
	// The added generation gate is static per-run, so it cannot reintroduce
	// cursor-flap on room-change hideOverlay().
	return enabled && !_useHwCursor && overlayShown() &&
	       _assetGen && _assetGen->mode() != Roger::kGenPrebuilt;
}

void FileRogerArtProvider::applyNativeCursorVisibility() {
	// GfxCursor::_isVisible is the game's logical cursor state; may not exist yet
	// at provider construction — default to visible, the first kernelShow re-syncs.
	const bool gameVisible = (g_sci && g_sci->_gfxCursor) ? g_sci->_gfxCursor->isVisible() : true;
	CursorMan.showMouse(gameVisible && !hidesNativeCursor());
}

void FileRogerArtProvider::buildCursorForShape(int cursorId) {
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	_cursorHotspot = Common::Point(2, 2); // fallback: arrow hotspot if resource missing
	_cursorNativeSize = Common::Point(0, 0); // fallback arrow: legacy fixed-px blit

	if (!g_sci || !g_sci->getResMan() || cursorId < 0)
		return;

	Resource *res = g_sci->getResMan()->findResource(
		ResourceId(kResourceTypeCursor, (uint16)cursorId), false);
	if (!res || (int)res->size() != 68)
		return;

	Common::Point hs;
	_cursorSurf = Roger::decodeSci0Cursor(res->data(), (int)res->size(), hs);
	_cursorHotspot = hs;
	if (_cursorSurf) {
		// Native footprint: SCI0 cursors are 16x16 game px. Recover the game-px
		// hotspot from the surface-px one (hs is at the surface's uniform scale).
		_cursorNativeSize = Common::Point(16, 16);
		_cursorNativeHotspot = Common::Point(hs.x * 16 / _cursorSurf->w,
		                                     hs.y * 16 / _cursorSurf->h);
	}
	_compositeCacheValid = false; // cursor surface changed -> next present is full rebuild
}

void FileRogerArtProvider::buildCursorFromView(int viewId, int loopNo, int celNo) {
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	_cursorHotspot = Common::Point(0, 0);
	_cursorNativeSize = Common::Point(0, 0); // fallback arrow: legacy fixed-px blit

	if (!g_sci || !g_sci->_gfxCache) return;
	GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
	if (!view) return;

	const CelInfo *ci = view->getCelInfo((int16)loopNo, (int16)celNo);
	if (!ci) return;
	const int16 w = ci->width, h = ci->height;
	const int16 dx = ci->displaceX, dy = ci->displaceY;

	const int kScale = 6; // same enhanced scale as sprite cels
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);

	// Enhanced path: the same scale6x cel every sprite draws (borrowed — copy it,
	// _cursorSurf is owned and freed on the next shape change).
	const Graphics::Surface *hi = _viewCache ? _viewCache->getCel(viewId, loopNo, celNo) : nullptr;
	if (hi && hi->w == w * kScale && hi->h == h * kScale) {
		_cursorSurf = new Graphics::Surface();
		_cursorSurf->copyFrom(*hi);
	} else {
		// Fallback (prebuilt mode / generation failure): nearest replication of the
		// native cel at the same 6x so the hotspot math below holds either way.
		Graphics::Surface *native = renderNativeCel(viewId, loopNo, celNo);
		if (!native) return;
		const int W = native->w * kScale, H = native->h * kScale;
		_cursorSurf = new Graphics::Surface();
		_cursorSurf->create(W, H, rgba);
		for (int y = 0; y < native->h; y++) {
			for (int x = 0; x < native->w; x++) {
				const uint32 px = native->getPixel(x, y);
				for (int sy = 0; sy < kScale; sy++)
					for (int sx = 0; sx < kScale; sx++)
						_cursorSurf->setPixel(x * kScale + sx, y * kScale + sy, px);
			}
		}
		native->free(); delete native;
	}
	// Hotspot from VIEW cel metadata (matches GfxCursor::kernelSetView formula), scaled.
	_cursorHotspot = Common::Point(
		(int)(w / 2 - dx) * kScale,
		(int)(h - dy - 1) * kScale
	);
	// Native footprint: the cel's own game-px dims + game-px hotspot.
	_cursorNativeSize = Common::Point(w, h);
	_cursorNativeHotspot = Common::Point(w / 2 - dx, h - dy - 1);
	_compositeCacheValid = false;
}

void FileRogerArtProvider::compositeCursor(Graphics::ManagedSurface &scene,
                                           const Common::Rect &gameRect) {
	// DEBUG TOOL: quick-tune panel rides the cursor layer Ã¢â‚¬â€ drawn at every
	// present site, above scene+UI, below the cursor; the bake caches the rendered panel between presents.
	if (_tunePanel.open && _mode == Roger::kModeEnhanced)
		Roger::drawTunePanel(scene, gameRect, _tunePanel, _tuneWidgets, _tuneBake);

	// The native OS cursor is invisible over the OSystem overlay, so draw our own
	// arrow into the overlay scene at the mouse position.
	const Common::Rect dst = cursorDstRect(gameRect);
	if (dst.isEmpty())
		return;
	// Both cursor kinds draw through blendScaleBlitNearest (exact rational sampling,
	// alpha-aware; the fallback arrow's dst == surface size, so it collapses to a 1:1
	// blend). NEVER blendBlitFrom here: its right/bottom clip computes the source crop
	// against the SOURCE size instead of the dest surface, so a cursor rect hanging off
	// the screen's right or bottom edge empties the src rect and the whole cursor
	// silently vanishes. blendScaleBlitNearest clips the PAINT to the scene and keeps
	// sampling against the full dst — off-screen extent is cropped, never dropped.
	Roger::blendScaleBlitNearest(scene, *_cursorSurf, dst, false);
	if (_compositor)
		_compositor->addDirtyRect(dst); // cursor moved here this frame (dirty-rect present)
	_lastCursorDstRect = dst; // fast path uses this to restore the old cursor region
}

void FileRogerArtProvider::ensureUi() {
	if (!_journal)
		_journal = new Roger::RogerJournal();
	if (!_textRenderer) {
		// Default to a monospace TTF that ships in ScummVM's fonts.dat: the fixed-width
		// DOS/terminal look matches SCI0's native bitmap font far better than a
		// proportional sans (judged in-game via the Ctrl+Shift+F cycle). Override with
		// roger_ui_font; per-game targets can each set their own.
		Common::String ttf = "GoMono-Regular.ttf";
		if (ConfMan.hasKey("roger_ui_font"))
			ttf = ConfMan.get("roger_ui_font");
		// Exact sizes are loaded on demand (no ladder): each element renders at the
		// cell height the shared type-scale pass computes for it.
		_textRenderer = new Roger::RogerTextRenderer(ttf);
		// roger_ui_font_scale: global size multiplier (percent) applied to each element's
		// target cell height. Wrapping text fits its box by height, so a larger scale grows
		// (and re-wraps) the text rather than clipping. Default 150.
		int scale = 150;
		if (ConfMan.hasKey("roger_ui_font_scale"))
			scale = ConfMan.getInt("roger_ui_font_scale");
		_textRenderer->setGlobalScale(scale);
		if (!_textRenderer->ttfLoaded())
			warning("ROGER: dialog font '%s' did NOT load from fonts.dat Ã¢â‚¬â€ using bitmap fallback", ttf.c_str());
	}
	if (!_altTextRenderer) {
		// Header (score/title banner) + menus use a distinct, more modern font.
		Common::String headerTtf = "NotoSans-Regular.ttf";
		if (ConfMan.hasKey("roger_ui_header_font"))
			headerTtf = ConfMan.get("roger_ui_header_font");
		_altTextRenderer = new Roger::RogerTextRenderer(headerTtf);
		// Same global size multiplier so headings scale with the body text.
		int scale = 150;
		if (ConfMan.hasKey("roger_ui_font_scale"))
			scale = ConfMan.getInt("roger_ui_font_scale");
		_altTextRenderer->setGlobalScale(scale);
		if (!_altTextRenderer->ttfLoaded())
			warning("ROGER: header font '%s' did NOT load from fonts.dat Ã¢â‚¬â€ using bitmap fallback", headerTtf.c_str());
	}
}

Graphics::ManagedSurface *FileRogerArtProvider::scratchScene(int w, int h) {
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	if (!_scratchScene || _scratchScene->w != w || _scratchScene->h != h) {
		delete _scratchScene;
		_scratchScene = new Graphics::ManagedSurface(w, h, rgba);
	}
	return _scratchScene;
}

void FileRogerArtProvider::ensureCompositeCache(int w, int h) {
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	if (!_compositeCache || _compositeCache->w != w || _compositeCache->h != h) {
		delete _compositeCache;
		_compositeCache = new Graphics::ManagedSurface(w, h, rgba);
		_compositeCacheValid = false;
	}
}

void FileRogerArtProvider::presentWithUi() {
	if (!overlayShown() || !_compositor || !_haveScene || !_sceneCache)
		return;
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	// The window may have been resized since the scene was cached. A blocking dialog/
	// menu/inventory does NOT tick kernelAnimate, so renderFrame can't refresh the
	// cache Ã¢â‚¬â€ and the OSystem overlay was reallocated to the new size on resize.
	// Pushing the stale (now over-sized) cache to the smaller overlay asserts in the
	// backend (copyRectToTexture bounds check) Ã¢â€ â€™ crash. Rescale the cached scene to the
	// current overlay size and recompute the placement so the present is always valid.
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();
	if (OW > 0 && OH > 0 && (_sceneCache->w != OW || _sceneCache->h != OH)) {
		Graphics::ManagedSurface *resized = new Graphics::ManagedSurface(OW, OH, rgba);
		resized->blitFrom(*_sceneCache->surfacePtr(),
		                  Common::Rect(0, 0, _sceneCache->w, _sceneCache->h),
		                  Common::Rect(0, 0, (int16)OW, (int16)OH));
		delete _sceneCache;
		_sceneCache = resized;
		_lastGameRect = currentGameRect(OW, OH);
		_compositeCacheValid = false;
		_lastCursorDstRect = Common::Rect(); // position was in old overlay space; invalid
	}
	byte pal[256 * 3];
	g_system->getPaletteManager()->grabPalette(pal, 0, 256);
	ensureCompositeCache(OW, OH); // invalidates _compositeCacheValid on (re)alloc
	Graphics::ManagedSurface &scene = *scratchScene(_sceneCache->w, _sceneCache->h);
	const Common::Rect fullR(0, 0, (int16)OW, (int16)OH);

	// A present that
	// presentToOverlay will decide to push FULL (heal frame / dirty-present off /
	// bg rebuild) needs a fully composed frame: the bounded path only makes the
	// pushed regions valid. A pending .rin capture also forces the full source Ã¢â‚¬â€
	// UNLESS _truthCapture: then the capture grabs the REAL overlay pixels via
	// grabOverlay (NOT the scratch buffer), so stale never-pushed regions are
	// visible in the capture. The scratch self-heals every cycle (renderFrame
	// fully recomposes it), so scratch-sourced captures can never witness a
	// missing invalidation mark.
	const bool captureForcesFull = _inputDriver && _inputDriver->capturePending() && !_truthCapture;
	const bool needFullSource = captureForcesFull || _compositor->nextPresentIsFull();

	if (_compositeCacheValid && !needFullSource && _mode != Roger::kModeSideBySide) {
		// Ã‚Â§3.3 region-bounded recompose: patch the composite cache only inside the
		// dirty union, then source the present from it. No full-frame copy, no
		// full UI re-render Ã¢â‚¬â€ this is the latency win at dialog time.
		Common::Array<Common::Rect> regions;
		_compositor->dirtyUnion(fullR, regions);
		if (!regions.empty()) {
			if (_journal && !_journal->empty() && _textRenderer)
				_compositor->patchCompositeRegions(*_compositeCache, *_sceneCache,
				                                   _journal->ops(), regions, pal,
				                                   _lastGameRect, _textRenderer, _altTextRenderer);
			else
				for (uint i = 0; i < regions.size(); i++)
					_compositeCache->copyRectToSurface(_sceneCache->rawSurface(),
					                                   regions[i].left, regions[i].top, regions[i]);
		}
		// Present source: composite pixels over every region this present pushes.
		// patchCompositeRegions may have expanded beyond `regions`; re-read the
		// union AFTER adding the expanded rects is unnecessary because expansion
		// only recomputes pixels that are bit-identical outside `regions` (the
		// re-rendered elements were unchanged there) Ã¢â‚¬â€ pushing `regions` suffices.
		for (uint i = 0; i < regions.size(); i++)
			scene.copyRectToSurface(_compositeCache->rawSurface(),
			                        regions[i].left, regions[i].top, regions[i]);
		// Cursor: vacate the old position and prepare the base under the new one.
		if (!_lastCursorDstRect.isEmpty()) {
			Common::Rect oldCur = _lastCursorDstRect;
			oldCur.clip(fullR);
			if (!oldCur.isEmpty()) {
				scene.copyRectToSurface(_compositeCache->rawSurface(), oldCur.left, oldCur.top, oldCur);
				_compositor->addDirtyRect(oldCur);
			}
		}
		Common::Rect newCur = cursorDstRect(_lastGameRect);
		newCur.clip(fullR);
		if (!newCur.isEmpty())
			scene.copyRectToSurface(_compositeCache->rawSurface(), newCur.left, newCur.top, newCur);
		// Tune panel: restore from cache before alpha-blending so the 216-alpha bake
		// always composites over clean cache pixels, not last present's already-blended
		// output (which would progressively darken toward opaque). Mirrors the cursor
		// treatment above: restore base, then draw, then dirty the region.
		if (_tunePanel.open && _mode == Roger::kModeEnhanced && !_lastGameRect.isEmpty()) {
			Common::Rect panelR = Roger::sciRectToDest(
				Roger::tunePanelRect(_tunePanel.leftSide), _lastGameRect);
			panelR.clip(fullR);
			if (!panelR.isEmpty()) {
				scene.copyRectToSurface(_compositeCache->rawSurface(),
				                        panelR.left, panelR.top, panelR);
				_compositor->addDirtyRect(panelR);
			}
		}
		compositeCursor(scene, _lastGameRect); // paints + addDirtyRect + _lastCursorDstRect
		_compositor->presentToOverlay(scene);
		maybeScriptCapture(scene, _lastGameRect); // grabs real overlay when _truthCapture; otherwise no-op (capture forces full path above)
		return;
	}

	// Legacy full path: rebuild scene+UI wholesale. Runs on room/geometry/F10/font
	// changes, resize, sbs mode, hw-cursor-invalidated caches, capture.
	scene.copyFrom(*_sceneCache); // fully overwrites the scratch buffer
	if (_journal && !_journal->empty() && _textRenderer) {
		// Diagnostic dump of the UI element rects (roger_debug or -Diag), throttled to one
		// dump per distinct dialog (signature over token/rect/type) so it does not spam
		// per frame. Under -Diag this is the decisive "which element is on screen" tool.
		if (_debugLog || _diag) {
			const Common::Array<Roger::UiElement> &els = _journal->ops();
			uint32 dsig = 2166136261u;
			for (uint i = 0; i < els.size(); i++) {
				dsig = (dsig ^ (uint32)els[i].token) * 16777619u;
				dsig = (dsig ^ (uint32)(els[i].nativeRect.left * 31 + els[i].nativeRect.top)) * 16777619u;
				dsig = (dsig ^ (uint32)(els[i].type * 7 + els[i].textRole)) * 16777619u;
			}
			if (dsig != _lastUiDiagSig) {
				_lastUiDiagSig = dsig;
				warning("ROGER-UI: gameRect=(%d,%d,%d,%d)", _lastGameRect.left, _lastGameRect.top,
				        _lastGameRect.right, _lastGameRect.bottom);
				for (uint i = 0; i < els.size(); i++) {
					const Roger::UiElement &e = els[i];
					const Common::Rect d = Roger::sciRectToDest(e.nativeRect, _lastGameRect);
					warning("ROGER-UI: [%u] type=%d tok=%08x native=(%d,%d,%d,%d) dest=(%d,%d,%d,%d) text='%.32s'",
					        i, (int)e.type, e.token, e.nativeRect.left, e.nativeRect.top,
					        e.nativeRect.right, e.nativeRect.bottom, d.left, d.top, d.right, d.bottom,
					        e.text.c_str());
				}
			}
		}
		_compositor->renderUiLayer(scene, _journal->ops(), pal, _lastGameRect, _textRenderer, _altTextRenderer);
	}
	_compositeCache->copyFrom(scene);
	_compositeCacheValid = true;
	if (_mode == Roger::kModeSideBySide) {
		// _compositeCache now holds scene + UI (this frame's dialog/banner). Hand off to
		// the side-by-side renderer, which uses it for the enhanced (left) panel.
		presentComparison();
		return;
	}
	compositeCursor(scene, _lastGameRect);
	_compositor->presentToOverlay(scene);
	maybeScriptCapture(scene, _lastGameRect);
}

void FileRogerArtProvider::markUiDirty(const Common::Rect &nativeRect) {
	if (!overlayShown() || !_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	_compositor->addDirtyRect(Roger::uiPaintExtent(nativeRect, _lastGameRect));
	// No addSceneDirtyRect here (unlike markVacated/markNativeDirty): markUiDirty accompanies a
	// UI element being pushed/repainted, and renderFrame's renderUiLayer draws that element into
	// the SAME fresh-frame scratch the deferred present emits. There is no stale-pixel window Ã¢â‚¬â€
	// the element is present in the scratch this frame, not left over from last frame.
}

void FileRogerArtProvider::markVacatedDirty(const Common::Rect &nativeRect) {
	if (!overlayShown() || !_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	// Exact rect + TTF pad. The compositor-overdraw ring beyond it is covered by
	// bitsRestore's exact erase rect (markNativeDirty in onErase). Phase 2
	// proved the gate cannot verify invalidation marks either way (layered redundancy:
	// _dirtyPrev loop + scene seed union) Ã¢â‚¬â€ invalidation changes are soak-verified.
	const Common::Rect dest = Roger::uiVacatedExtent(nativeRect, _lastGameRect);
	_compositor->addDirtyRect(dest);
	// This mark REMOVES previously-painted content. If it fires mid-cycle, the barrier
	// defers and the end-of-cycle fresh-frame present re-uses renderFrame's scratch with no
	// recompose; _dirtyCur alone is excluded from renderScene's seed union, so the removed
	// element's stale pixels would ghost for one cycle. Route it to the scene seed too.
	if (_inAnimateCycle)
		_compositor->addSceneDirtyRect(dest);
}

void FileRogerArtProvider::markNativeDirty(const Common::Rect &nativeRect) {
	// Ã‚Â§3.1 exact invalidation: SCI touched these native pixels. grow(1) native
	// absorbs integer-scaler rounding differences vs the sprite-path mapper.
	// O(1) accumulate; NEVER presents.
	if (!overlayShown() || !_compositor || nativeRect.isEmpty())
		return;
	// The status-bar strip (rows 0.._statusBarH) is TRANSPARENT in the Roger overlay
	// (computePictureRect reserves it for the native score to show through). Marking it
	// dirty is a no-op visually but inflates the dirty area on every bitsShow tick,
	// since SCI re-blits the native score row every cycle. Clip to the picture region.
	// Invariant: Roger overlay content in the status strip is signalled only via
	// markUiDirty (uiPushStatus banner) Ã¢â‚¬â€ a markNativeDirty rect wholly inside the
	// strip is safe to drop; violating that would silently lose marks here.
	Common::Rect pic = nativeRect;
	pic.clip(Common::Rect(0, _statusBarH, 320, 200));
	if (pic.isEmpty())
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	Common::Rect n = pic;
	n.grow(1);
	const Common::Rect dest = Roger::sciRectToDest(n, _lastGameRect);
	_compositor->addDirtyRect(dest);
	// Same deferred-mid-cycle ghost hazard as markVacatedDirty: bitsRestore's erase rect can
	// uncover previously-painted content mid-cycle, and the deferred fresh-frame present would
	// otherwise push it from stale scratch. Route to the scene seed union too (empty otherwise).
	if (_inAnimateCycle)
		_compositor->addSceneDirtyRect(dest);
}

void FileRogerArtProvider::markFullDirty() {
	_barrierDirty = true;
	_compositeCacheValid = false;
	if (_compositor)
		_compositor->forceFullPresent();
}

Common::Rect FileRogerArtProvider::cursorDstRect(const Common::Rect &gameRect) {
	if (!enabled || _useHwCursor || !_cursorVisible || gameRect.isEmpty())
		return Common::Rect();
	ensureCursor();
	if (!_cursorSurf)
		return Common::Rect();
	const Common::Point mp = g_system->getEventManager()->getMousePos();
	if (_cursorNativeSize.x > 0) {
		// SCI cursor: native game-px footprint mapped through the game rect —
		// the enhanced surface scales into it, so on-screen size == native size.
		return Roger::cursorOverlayRect(mp, gameRect, _cursorNativeSize, _cursorNativeHotspot);
	}
	// Fallback arrow: fixed pixel size (roger_cursor_size), blitted 1:1.
	const int ox = gameRect.left + mp.x * gameRect.width() / 320;
	const int oy = gameRect.top + mp.y * gameRect.height() / 200;
	return Common::Rect(ox - _cursorHotspot.x, oy - _cursorHotspot.y,
	                    ox - _cursorHotspot.x + _cursorSurf->w,
	                    oy - _cursorHotspot.y + _cursorSurf->h);
}

void FileRogerArtProvider::presentBarrier() {
	// The single gated present (spec Ã‚Â§3.2). Every skip path below is O(1).
	if (_inAnimateCycle)
		return; // mid-cycle marks accumulate; the end-of-cycle call flushes them
	if (_uiBatchDepth > 0)
		return; // batched UI re-push: marks accumulate; endBatch flushes once
	if (!overlayShown() || !_compositor || !_haveScene || !_sceneCache)
		return;
	if (_frameJustComposed && _scratchScene) {
		// Per-cycle present: renderFrame just composed scene+UI into _scratchScene
		// and refreshed the caches Ã¢â‚¬â€ present that frame directly. No recompose.
		_frameJustComposed = false;
		_barrierDirty = false;
		Graphics::ManagedSurface &scene = *_scratchScene;
		if (_mode == Roger::kModeSideBySide) {
			presentComparison();
		} else {
			compositeCursor(scene, _lastGameRect);
			_compositor->presentToOverlay(scene);
		}
		maybeScriptCapture(scene, _lastGameRect);
		if (_diffCheck)
			runDiffCheck();
		return;
	}
	const bool capture = _inputDriver && _inputDriver->capturePending();
	const bool cursorMoved = cursorDstRect(_lastGameRect) != _lastCursorDstRect;
	if (!_barrierDirty && !_compositor->hasPendingDirty() && !cursorMoved && !capture)
		return;
	_barrierDirty = false;
	if (_diag) {
		const uint32 t0 = g_system->getMillis();
		presentWithUi();
		const uint32 t1 = g_system->getMillis();
		_presentTelCount++;
		_presentTelMs += t1 - t0;
		if (_presentTelWindowStart == 0)
			_presentTelWindowStart = t1;
		if (t1 - _presentTelWindowStart >= 1000) {
			warning("ROGER-DIAG[present]: n=%u ms=%u window=%u",
			        _presentTelCount, _presentTelMs, t1 - _presentTelWindowStart);
			_presentTelWindowStart = t1;
			_presentTelCount = 0;
			_presentTelMs = 0;
		}
	} else {
		presentWithUi();
	}
}

void FileRogerArtProvider::presentComparison() {
	if (_mode != Roger::kModeSideBySide || !_compositor)
		return;
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0)
		return;

	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	// Dedicated buffer Ã¢â‚¬â€ NEVER the renderScene scratch. renderScene redraws only
	// its seed union each frame and relies on the scratch's other pixels
	// persisting from prior frames; composing the split layout into that same
	// buffer left SBS pixels outside the next frame's union, which a later full
	// _compositeCache copy then baked in Ã¢â‚¬â€ presentComparison scaled its OWN
	// previous output into the left panel (recursive nested split, seen in the
	// SQ3 pod room where idle frames use small bounded seeds).
	if (!_sbsScratch || _sbsScratch->w != OW || _sbsScratch->h != OH) {
		delete _sbsScratch;
		_sbsScratch = new Graphics::ManagedSurface(OW, OH, rgba);
	}
	Graphics::ManagedSurface &out = *_sbsScratch;
	out.clear(out.format.ARGBToColor(255, 0, 0, 0)); // opaque black letterbox

	Common::Rect leftF, rightF;
	Roger::comparePanelRects(OW, OH, leftF, rightF);

	// Left panel: the ENHANCED composite. _compositeCache holds scene + UI (no cursor),
	// rebuilt by renderFrame/presentWithUi Ã¢â‚¬â€ so dialogs/narration/banners show here too.
	// Fall back to _sceneCache (scene, no UI) when the composite cache isn't valid.
	Graphics::ManagedSurface *leftSrc = (_compositeCacheValid && _compositeCache) ? _compositeCache
	                                  : (_haveScene ? _sceneCache : nullptr);
	if (_diag)
		warning("ROGER-DIAG[sbsLeft]: src=%s frameJustComposed=%d",
		        leftSrc == _compositeCache ? "composite" : (leftSrc ? "sceneFallback" : "none"),
		        _frameJustComposed ? 1 : 0);
	if (leftSrc) {
		// The composite is overlay-sized and carries its own letterbox (aspect =
		// window aspect, not 8:5); scaling the WHOLE surface into the 8:5 panel
		// distorted the content vertically. Blit only the game-content subrect so
		// the left panel is exactly 8:5, matching the native right panel.
		Common::Rect srcGame = _lastGameRect;
		if (srcGame.isEmpty() || srcGame.right > leftSrc->w || srcGame.bottom > leftSrc->h)
			srcGame = Common::Rect(0, 0, (int16)leftSrc->w, (int16)leftSrc->h);
		Roger::scaleBlitNearest(*out.surfacePtr(), leftF, *leftSrc->surfacePtr(), srcGame);
	}

	// Right panel: the ORIGINAL native frame. Read the pre-erase snapshot (_nativeBaseline,
	// captured at kernelAnimate's snapshot point before restoreAndDelete) so the animating
	// cast (ego/moving views) is present Ã¢â‚¬â€ the live visual buffer has it erased by now.
	bool haveRight = false;
	if (_haveBaseline && g_sci && g_sci->_gfxScreen && g_sci->_gfxPalette16) {
		GfxScreen *screen = g_sci->_gfxScreen;
		const int sw = screen->getWidth(), sh = screen->getHeight();
		if ((int)_nativeBaseline.size() == sw * sh) {
			const Palette &pal = g_sci->_gfxPalette16->_sysPalette;
			Graphics::Surface nat;
			nat.create((int16)sw, (int16)sh, rgba);
			for (int y = 0; y < sh; y++)
				for (int x = 0; x < sw; x++) {
					const Color &c = pal.colors[_nativeBaseline[(uint)y * sw + x]];
					nat.setPixel(x, y, rgba.ARGBToColor(255, c.r, c.g, c.b));
				}
			Roger::scaleBlitNearest(*out.surfacePtr(), rightF, nat);
			nat.free();
			haveRight = true;
		}
	}
	if (!haveRight) {
		// First frame after entering side-by-side (no baseline yet): live buffer as fallback.
		Graphics::Surface *live = snapshotNativeRegion(Common::Rect(0, 0, 320, 200));
		if (live) {
			Roger::scaleBlitNearest(*out.surfacePtr(), rightF, *live);
			live->free();
			delete live;
		}
	}

	// The side-by-side window is fully SYNTHETIC: no native pixel may show
	// through. The enhanced composite copied into the left panel carries the
	// transparent status-bar strip (designed for enhanced mode, where it aligns
	// with the native score row) Ã¢â‚¬â€ but the backend still renders the native game
	// at the ENHANCED-mode rect, misaligned with both panels, so its pixels bled
	// through that strip as a stretched garbage band across the panel top.
	// Force the whole frame opaque before presenting.
	{
		const uint32 amask = out.format.ARGBToColor(255, 0, 0, 0);
		for (int y = 0; y < out.h; y++) {
			uint32 *px = (uint32 *)out.getBasePtr(0, y);
			for (int x = 0; x < out.w; x++)
				px[x] |= amask;
		}
	}

	// Thin divider down the center.
	const uint32 divider = out.format.ARGBToColor(255, 90, 90, 90);
	const int cx = OW / 2;
	for (int dx = -1; dx <= 1; dx++)
		out.surfacePtr()->drawLine(cx + dx, 0, cx + dx, OH - 1, divider);

	// Single cursor drawn at the physical pointer position (the backend HW cursor is invisible
	// over the overlay). getMousePos is a whole-window linear map, so this floats naturally
	// over whichever panel the pointer is on. onMouseMoved re-presents so it tracks smoothly.
	if (_cursorVisible) {
		ensureCursor();
		if (_cursorSurf) {
			const Common::Point mp = g_system->getEventManager()->getMousePos();
			const int ox = mp.x * OW / 320;
			const int oy = mp.y * OH / 200;
			const Common::Rect dst(ox - _cursorHotspot.x, oy - _cursorHotspot.y,
			                       ox - _cursorHotspot.x + _cursorSurf->w,
			                       oy - _cursorHotspot.y + _cursorSurf->h);
			// blendScaleBlitNearest (1:1 here), not blendBlitFrom: the latter drops the
			// whole blit once dst hangs off the right/bottom edge (see compositeCursor).
			Roger::blendScaleBlitNearest(out, *_cursorSurf, dst, false);
		}
	}

	_compositor->forceFullPresent();
	_compositor->presentToOverlay(out);
}

void FileRogerArtProvider::buildGlyphs(const char *text, int fontId, int penColor,
                                       Common::Array<Roger::UiGlyph> &out) {
	if (!text || !_assetGen)
		return;
	for (const char *p = text; *p; ++p) {
		const byte c = (byte)*p;
		if (c >= 0x20 && c < 0x7f)
			continue; // printable ASCII -> TTF handles it
		bool seen = false;
		for (uint i = 0; i < out.size(); i++)
			if (out[i].ch == c) { seen = true; break; }
		if (seen)
			continue;
		Graphics::Surface *g = _assetGen->generateTextSurface(Common::String(1, (char)c),
		                                                       fontId, (byte)(penColor >= 0 ? penColor : 0));
		if (g) {
			_uiIcons.push_back(g); // owned; freed on room change
			Roger::UiGlyph ug; ug.ch = c; ug.surf = g;
			out.push_back(ug);
		}
	}
}

void FileRogerArtProvider::onWindowOpen(const Common::Rect &r, uint16 wndStyle,
                                        int backColor, int penColor, const char *title,
                                        uint32 token) {
	// Menu exile (R5): the dropdown box is MODEL-owned — openDropdown resets the
	// retained rows and stores the box; the journal emit is menuRebuildDropdown at
	// endBatch. Never treated as a real window (no bracket, no immediate append).
	// The model ingestion runs UNCONDITIONALLY (matching the old menu.cpp
	// collection): a registered-but-disabled provider still receives onText row
	// pushes, so its openDropdown reset must fire too or _rows accumulates forever.
	// Rendering/journal work stays below the !enabled gate (rebuilds bail on !_plate).
	if (token == kGfxTokenMenuDropdown) {
		_menuModel.openDropdown(r);
		_batchTouchedDropdown = true;
		return;
	}
	if (!enabled)
		return;
	// Arm open->show attribution (R9): the window's content show follows immediately
	// (GfxPorts::drawWindow emits onWindowOpen, then bitsShow(dims) under _wmgrPort,
	// which self-derives owner 0 — see onShow). Only real windows (control namespace)
	// arm; the menu.cpp singletons (status strip / dropdown) never route a content
	// show this way. Armed even without a plate: attribution state, not rendering.
	if ((token & Roger::kTokenNamespaceMask) == Roger::kControlTokenNs) {
		_pendingShowOwner = token;
		// GfxPaint16::bitsShow even-rounds every shown rect (left &= 0xFFFE, right
		// rounded up to even; paint16.cpp:359-360). Framed windows always have an ODD
		// dims.left — GfxPorts::addWindow force-evens the content rect then grows it by
		// 1px for the frame — so the shown rect is 1px wider on the left than the armed
		// dims rect and inclusive Rect::contains() never matches. Normalize the armed
		// rect to bitsShow's rounding here or open->show attribution never fires for
		// framed dialogs (the pre-cmdbox-fix state, masked by later redundancy layers).
		_pendingShowRect = r;
		_pendingShowRect.left &= 0xFFFE;              // round down (mirror bitsShow)
		_pendingShowRect.right = (_pendingShowRect.right + 1) & 0xFFFE; // round up
	}
	if (!overlayShown() || !_plate) return; // no hires scene -> leave native UI visible
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiWindow; e.nativeRect = r;
	// Faithful fill: a window that is transparent (bit 0) or USER-backed (bit 7 = 0x80,
	// i.e. a picture-backed port whose content is drawn by scripts/controls directly onto
	// the game picture Ã¢â‚¬â€ the QFG1 character-creation sheet) must not paint an opaque box
	// over the hires plate. In SCI0, _styleUser = USER|TRANSPARENT (0x81); a USER-only
	// (0x80) window slips through the SCI0 drawWindow skip-guard and reaches this path.
	// A genuine opaque dialog (SQ3 message windows: style 0, no USER/TRANSPARENT bits)
	// keeps its fill. In SCI1_LATE+, USER-flagged windows are blocked at the drawWindow
	// level and never reach this path, so the new check is inert for those games.
	const bool pictureBackedOrTransparent = (wndStyle & 1 /*TRANSPARENT*/) || (wndStyle & 0x80 /*USER*/);
	e.backColor = pictureBackedOrTransparent ? -1 : backColor;
	e.penColor = penColor;
	e.hasFrame = !(wndStyle & 2 /*NOFRAME*/);
	e.token = token;
	if (_diag)
		warning("ROGER-DIAG[uiWindow]: rect=(%d,%d,%d,%d) wndStyle=0x%02x backColor=%d -> e.backColor=%d hasFrame=%d pictureBackedOrTransparent=%d token=0x%08x",
		        r.left, r.top, r.right, r.bottom, wndStyle, backColor, e.backColor,
		        (int)e.hasFrame, (int)pictureBackedOrTransparent, token);
	if ((token & Roger::kTokenNamespaceMask) == Roger::kControlTokenNs) {
		e.windowId = token & 0x0FFFFFFFu; // the window box op belongs to itself
		_journal->openBracket(e.windowId, r);
	}
	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
	// Folded-in titlebar text (was ports.cpp's title uiPushText): a titled window
	// (e.g. the inventory's "You are carrying:") draws its title in a titlebar strip
	// the window box above does not reproduce. Capture it so the hires overlay shows
	// the title too: a dark titlebar (grey for SCI0, black later) with centered white
	// text, matching the native bar. Role-sized (kRoleBody): the native font metrics
	// the old seam computed via StringWidth are deliberately dropped — title text is
	// short single-line, exactly what the role fallback renders.
	if (title && *title &&
	    (wndStyle & 4 /*SCI_WINDOWMGR_STYLE_TITLE*/) &&
	    (token & Roger::kTokenNamespaceMask) == Roger::kControlTokenNs) {
		Common::Rect titleRect(r.left, r.top, r.right, (int16)(r.top + 10));
		const int titleBack = (getSciVersion() <= SCI_VERSION_0_LATE) ? 8 : 0;
		Roger::UiElement t;
		t.type = Roger::kUiText; t.nativeRect = titleRect; t.text = title;
		t.penColor = 15 /*white (EGA)*/; t.backColor = titleBack; t.align = 1 /*center*/;
		t.token = token; t.textRole = Roger::kRoleBody;
		buildGlyphs(title, 0, 15, t.glyphs);
		journalAppend(t);
		markUiDirty(titleRect);
		presentBarrier();
	}
}

void FileRogerArtProvider::uiPushTextInternal(const Common::Rect &r, const char *text, int penColor,
                                              int backColor, int fontId, int align, uint32 token,
                                              int textRole, bool useAltFont,
                                              int nativeFontH, int nativeTextW) {
	if (!overlayShown() || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiText; e.nativeRect = r; e.text = text ? text : "";
	e.penColor = penColor; e.backColor = backColor; e.fontId = fontId;
	e.align = align; e.token = token;
	e.textRole = textRole; e.useAltFont = useAltFont;
	e.nativeFontH = nativeFontH; e.nativeTextW = nativeTextW;
	buildGlyphs(text, fontId, penColor, e.glyphs);
	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
}

// R7/R8: dispatch onControl by kind to the per-kind bodies below.
void FileRogerArtProvider::onControl(ControlKind kind, const Common::Rect &rect,
                                     const char *text, int fontId, int style, int cursorPos,
                                     uint32 token, int nativeFontH, int nativeTextW) {
	switch (kind) {
	case kControlButton:
		uiPushButtonInternal(rect, text, fontId, style, token, nativeFontH, nativeTextW);
		break;
	case kControlTextEdit:
		uiPushTextEditInternal(rect, text, fontId, style, cursorPos, token,
		                       nativeFontH, nativeTextW);
		break;
	default:
		break;
	}
}

void FileRogerArtProvider::onFrameBox(const Common::Rect &rect, int penColor) {
	uiPushFrameBoxInternal(rect, penColor); // change-gating (present-storm guard) is inside
}

// R17: relaxes the native kernelTexteditChange pixel-width keystroke cap. The hires
// field is rendered far wider than the native nsRect, so the native cap is wrong here;
// the script-side maxChars buffer bound (checked earlier in kernelTexteditChange) still
// applies, so nothing overflows. Returns the exact prior condition (the old inline C3
// gate was `enabled` only) — no behavior change: observer-null / disabled keeps the cap.
bool FileRogerArtProvider::wantsUnclampedTextEdit() const {
	return enabled;
}

void FileRogerArtProvider::uiPushButtonInternal(const Common::Rect &r, const char *text, int fontId,
                                                int style, uint32 token,
                                                int nativeFontH, int nativeTextW) {
	if (!overlayShown() || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiButton; e.nativeRect = r; e.text = text ? text : "";
	e.fontId = fontId; e.style = style; e.align = 1 /*center*/;
	e.backColor = 7 /*light gray*/; e.penColor = 0; e.hasFrame = true; e.token = token;
	e.nativeFontH = nativeFontH; e.nativeTextW = nativeTextW;
	buildGlyphs(text, fontId, e.penColor, e.glyphs);
	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushTextEditInternal(const Common::Rect &r, const char *text, int fontId,
                                                  int style, int cursorPos, uint32 token,
                                                  int nativeFontH, int nativeTextW) {
	if (!overlayShown() || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiTextEdit; e.nativeRect = r; e.text = text ? text : "";
	e.fontId = fontId; e.style = style; e.cursorPos = cursorPos; e.align = 0;
	e.backColor = 15 /*white*/; e.penColor = 0; e.hasFrame = true; e.token = token;
	e.textRole = Roger::kRoleBody; // body size, same as the dialog prompt above it
	e.vAlignTop = true;            // SCI draws edit text at the top of the field, not centred
	e.nativeFontH = nativeFontH; e.nativeTextW = nativeTextW;
	buildGlyphs(text, fontId, e.penColor, e.glyphs);
	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushIconInternal(const Common::Rect &r, int viewId, int loopNo, int celNo,
                                              uint32 token) {
	if (!overlayShown() || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiIcon; e.nativeRect = r; e.token = token;
	// Prefer the upscaled cel (views/<id>/view.<id>.loop.<loop>.png) so inventory item
	// images are hires; fall back to a rendered native cel. The hires cel is borrowed
	// from the ViewCache (do NOT free it); native cels are owned via _uiIcons.
	const Graphics::Surface *hi = _viewCache ? _viewCache->getCel(viewId, loopNo, celNo) : nullptr;
	if (hi) {
		e.iconSurface = hi;
	} else {
		Graphics::Surface *cel = renderNativeCel(viewId, loopNo, celNo);
		if (cel) { _uiIcons.push_back(cel); e.iconSurface = cel; }
	}
	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::onDrawCelInternal(const Common::Rect &r, int viewId, int loopNo, int celNo) {
	if (!overlayShown() || !_plate || !_viewCache) return;
	const Graphics::Surface *hi = _viewCache->getCel(viewId, loopNo, celNo);
	ensureUi();
	// NOTE: do NOT clearToken here. push() replaces by (type,token,rect), so distinct-rect
	// cels (e.g. the 13 stat graphics on the QFG1 char sheet) coexist in the layer, and a
	// redraw at the SAME rect replaces in place. Clearing the shared token at the start of
	// every call would erase the previous cel, leaving only the last one visible.
	// Lifetime: an icon dies when a native erase rect covers it (onErase) or on
	// room change Ã¢â‚¬â€ never via a blanket namespace clear.
	const uint32 tok = Roger::kDrawCelIconTokenNs;

	Roger::UiElement e;
	e.type = Roger::kUiIcon; e.nativeRect = r; e.token = tok;

	if (_diag) {
		warning("ROGER-DIAG[drawCelIcon]: view=%d loop=%d cel=%d rect=(%d,%d,%d,%d) hi=%s%dx%d",
		        viewId, loopNo, celNo, r.left, r.top, r.right, r.bottom,
		        hi ? "" : "none ", hi ? hi->w : 0, hi ? hi->h : 0);
		// One-shot cel-content dump: the decisive evidence when an icon renders cut or
		// wrong (QFG1 char-sheet selection frame). Written once per (view,loop,cel) per run.
		if (hi) {
			Common::String png = Common::String::format("%s/dbg-cel-%d-%d-%d.png",
				ConfMan.hasKey("screenshotpath") ? ConfMan.get("screenshotpath").c_str() : ".",
				viewId, loopNo, celNo);
			bool dumped = false;
			for (uint i = 0; i < _diagDumpedCels.size(); i++)
				if (_diagDumpedCels[i] == png) { dumped = true; break; }
			if (!dumped) {
				_diagDumpedCels.push_back(png);
				Roger::dumpSurfacePng(*hi, png);
			}
		}
	}

	if (hi) {
		e.iconSurface = hi; // borrowed from the ViewCache (hires path)
	} else {
		// No hires art: fall back to a rendered native cel so it stays visible under
		// the opaque overlay. beginSelfDraw suppressed bitsShow, so Feeder B won't
		// pick this up Ã¢â‚¬â€ we must inject it here. renderNativeCel already bakes mirroring.
		// Use a (viewId,loopNo,celNo)-keyed cache so the same cel drawn at N positions
		// renders once and is referenced N times (no per-call growth, no leak).
		Graphics::Surface *surf = nullptr;
		for (uint i = 0; i < _drawCelNativeCache.size(); i++) {
			const DrawCelNativeKey &k = _drawCelNativeCache[i];
			if (k.viewId == viewId && k.loopNo == loopNo && k.celNo == celNo) {
				surf = k.surf;
				break;
			}
		}
		if (!surf) {
			surf = renderNativeCel(viewId, loopNo, celNo);
			if (!surf) return; // render failed; silently skip
			_uiIcons.push_back(surf); // owned; freed on room change
			DrawCelNativeKey k;
			k.viewId = viewId; k.loopNo = loopNo; k.celNo = celNo; k.surf = surf;
			_drawCelNativeCache.push_back(k);
		}
		e.iconSurface = surf; // borrowed from cache; journal borrows
	}

	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushStatusInternal(const Common::Rect &r, const char *text, int fontId,
                                                int penColor, int backColor, uint32 token,
                                                int nativeFontH, int nativeTextW) {
	// Remember the banner so it can be re-applied on room load / F10 enable, even if
	// the overlay was not ready when the game first drew it.
	_haveStatus = true; _statusRect = r; _statusText = text ? text : "";
	_statusFont = fontId; _statusPen = penColor; _statusBack = backColor; _statusToken = token;
	_statusNativeFontH = nativeFontH; _statusNativeTextW = nativeTextW;
	if (!overlayShown() || !_plate) return;
	ensureUi();
	// The score banner and the menu bar share this token (top strip); drop whatever
	// is there (e.g. the menu bar's window + titles) before pushing the banner.
	_journal->clearToken(token);

	// Opaque bar (matches the native menu/status strip), no frame, full width.
	Roger::UiElement bar;
	bar.type = Roger::kUiWindow; bar.nativeRect = r; bar.backColor = backColor;
	bar.penColor = penColor; bar.style = 2; bar.token = token;
	journalAppend(bar);

	// The native renderer always fills the underline row below the bar black
	// (GfxPorts::_menuLine; menu.cpp drawBar/kernelDrawStatus). Mirror it so the
	// WHOLE reserved strip is overlay-owned: a strip band left to native
	// show-through can drift up to 1px against Roger's edge mapping depending on
	// how the backend samples its game blit (the 1px-narrow bar, 2026-07-09).
	const Common::Rect line = Roger::statusStripRemainder(r, _statusBarH);
	if (!line.isEmpty()) {
		Roger::UiElement ul;
		ul.type = Roger::kUiWindow; ul.nativeRect = line; ul.backColor = 0;
		ul.penColor = penColor; ul.style = 2; ul.token = token;
		journalAppend(ul);
		markUiDirty(line);
	}

	// Hybrid banner: crisp TTF for ASCII characters, game's own SCI font glyph spliced
	// inline for non-ASCII bytes (e.g. SQ3's stylized "III"). No whole-native path.
	Roger::UiElement e;
	e.type = Roger::kUiText; e.nativeRect = r;
	e.text = text ? text : "";              // full text; non-ASCII glyphs spliced from the game font
	e.penColor = penColor; e.backColor = backColor; e.align = 0;
	e.textRole = Roger::kRoleHeading;
	e.useAltFont = true;
	e.token = token;
	e.nativeFontH = nativeFontH; e.nativeTextW = nativeTextW;
	buildGlyphs(text, fontId, penColor, e.glyphs);
	journalAppend(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::reapplyStatus() {
	if (_haveStatus)
		uiPushStatusInternal(_statusRect, _statusText.c_str(), _statusFont, _statusPen, _statusBack, _statusToken,
		                     _statusNativeFontH, _statusNativeTextW);
}

// Generic text-out captures live in the 0x6------- namespace. The low bits carry the
// window/port id the text was drawn in (0x60000000 | port->id), so a window dispose
// (GfxPorts::removeWindow -> onWindowClose -> bracket close) drops exactly that window's
// text Ã¢â‚¬â€ the same lifetime controls16/menu text already has. Text drawn on the picture
// port (no dialog / char screen while open) uses that port's id, which is never disposed
// mid-room, so it persists until room change. kGenericTextTokenNs is the namespace base
// (matches picture-port id 0 fallback and is the value passed to the namespace helpers).
static inline bool isGenericTextToken(uint32 t) { return (t & Roger::kTokenNamespaceMask) == Roger::kGenericTextTokenNs; }

void FileRogerArtProvider::beginBatch() {
	_uiBatchDepth++;
	// Arm a bar reset: if this batch turns out to be a BAR re-push (its first
	// event is an onText(menuBar)), the model's titles rebuild from a clean set.
	// A dropdown batch (drawMenu) emits no menuBar text, so the retained bar
	// titles survive it — exactly the pre-exile bar-title lifetime.
	_barResetPending = true;
}

void FileRogerArtProvider::endBatch() {
	if (_uiBatchDepth > 0 && --_uiBatchDepth == 0) {
		// Rebuild only what THIS batch touched, then present ONCE (the
		// present-storm guard — every push above was suppressed by the batch
		// depth). A dropdown batch must not clear+rebuild the bar strip (token
		// kGfxTokenStatus) it never touched, and vice versa.
		if (_batchTouchedBar)
			menuRebuildBar();
		if (_batchTouchedDropdown)
			menuRebuildDropdown();
		_batchTouchedBar = _batchTouchedDropdown = false;
		presentBarrier();
	}
}

// Exiled GfxMenu bar-overlay emitter, reading the retained MenuModel.
// Journal output is IDENTICAL to the old emitter: clear kGfxTokenStatus, opaque
// white full-width bar (NOFRAME), black underline row, then one heading-role
// alt-font text op per printable-ASCII title — same token, rects, append order.
void FileRogerArtProvider::menuRebuildBar() {
	if (!overlayShown() || !_plate)
		return;
	ensureUi(); // may CREATE the journal if a menu is the first UI push
	if (!_journal)
		return;
	// The menu bar and the score/title banner share the top strip and are mutually
	// exclusive in time, so they use the SAME token (kGfxTokenStatus): rebuilding
	// the bar replaces the banner; the next kernelDrawStatus (onText source=status)
	// replaces the bar back (and a strip-covering bitsRestore reapplies the cached
	// banner — see onRestore).
	_journal->clearToken(kGfxTokenStatus);
	// The bar rect: the cached _statusRect IS the full _menuBarRect (the banner is
	// drawn on room load, before any menu can open, and both native fills cover the
	// same strip). Fallback: derive the row extent from the first captured title.
	Common::Rect barRect = _statusRect;
	if (!_haveStatus || barRect.isEmpty()) {
		if (_menuModel.barTitles().empty())
			return;
		barRect = Common::Rect(0, _menuModel.barTitles()[0].rect.top,
		                       320, _menuModel.barTitles()[0].rect.bottom);
	}
	// Opaque white bar (matches the native white menu bar), no frame, spanning the
	// FULL bar width (a transparent gap for a graphical-glyph title would let the
	// native bar bleed through and overlap the hires titles).
	Roger::UiElement barE;
	barE.type = Roger::kUiWindow; barE.nativeRect = barRect;
	barE.backColor = 15 /*EGA white*/; barE.penColor = 0; barE.style = 2 /*NOFRAME*/;
	barE.token = kGfxTokenStatus;
	journalAppend(barE);
	markUiDirty(barRect);
	// Mirror the black underline row below the bar (statusStripRemainder) so the
	// whole reserved strip stays overlay-owned (the 1px-narrow-bar seam fix,
	// 2026-07-09).
	const Common::Rect line = Roger::statusStripRemainder(barRect, _statusBarH);
	if (!line.isEmpty()) {
		Roger::UiElement ul;
		ul.type = Roger::kUiWindow; ul.nativeRect = line; ul.backColor = 0;
		ul.penColor = 0; ul.style = 2; ul.token = kGfxTokenStatus;
		journalAppend(ul);
		markUiDirty(line);
	}
	for (uint i = 0; i < _menuModel.barTitles().size(); i++) {
		const Roger::MenuBarTitle &t = _menuModel.barTitles()[i];
		if (!t.isText)
			continue; // graphical glyph (Sierra icon) -> leave the native bar showing
		Roger::UiElement e;
		e.type = Roger::kUiText; e.nativeRect = t.rect; e.text = t.text;
		e.penColor = 0; e.backColor = -1; e.align = 0 /*left*/;
		e.textRole = Roger::kRoleHeading; e.useAltFont = true; e.token = kGfxTokenStatus;
		e.nativeFontH = t.nativeFontH; e.nativeTextW = t.nativeTextW;
		buildGlyphs(t.text.c_str(), 0, 0, e.glyphs);
		journalAppend(e);
		markUiDirty(t.rect);
	}
}

// Exiled GfxMenu dropdown-overlay emitter, reading the retained MenuModel.
// Journal output is IDENTICAL to the old emitter: clear kGfxTokenMenuDropdown,
// framed opaque white box, then one body-role alt-font text op per row (selected
// row inverted white-on-black) — same token, rects, append order.
void FileRogerArtProvider::menuRebuildDropdown() {
	if (!overlayShown() || !_plate)
		return;
	ensureUi(); // may CREATE the journal if a menu is the first UI push
	if (!_journal)
		return;
	_journal->clearToken(kGfxTokenMenuDropdown); // single open dropdown at a time
	// Opaque white box with a frame (matches SCI's black-bordered white dropdown).
	Roger::UiElement box;
	box.type = Roger::kUiWindow; box.nativeRect = _menuModel.box();
	box.backColor = 15 /*EGA white*/; box.penColor = 0; box.hasFrame = true;
	box.token = kGfxTokenMenuDropdown;
	journalAppend(box);
	markUiDirty(box.nativeRect);
	for (uint i = 0; i < _menuModel.rows().size(); i++) {
		const Roger::MenuRow &r = _menuModel.rows()[i];
		const bool sel = r.selected(_menuModel.highlight());
		const int pen = sel ? 15 : 0;
		const int back = sel ? 0 : -1; // selected row drawn inverted (white on black)
		Roger::UiElement e;
		e.type = Roger::kUiText; e.nativeRect = r.rect; e.text = r.text;
		e.penColor = pen; e.backColor = back; e.align = 0 /*left*/;
		e.textRole = Roger::kRoleBody; e.useAltFont = true; e.token = kGfxTokenMenuDropdown;
		e.nativeFontH = r.nativeFontH; e.nativeTextW = r.nativeTextW;
		buildGlyphs(r.text.c_str(), 0, pen, e.glyphs);
		journalAppend(e);
		markUiDirty(r.rect);
	}
}

void FileRogerArtProvider::onMenuHighlight(uint16 itemId) {
	if (!enabled)
		return;
	// The dedup exiled from GfxMenu::invertMenuSelection lives in
	// setHighlight: a no-op highlight (repeat, or the itemId==0 old-row re-invert
	// interactiveWithMouse sends first) never re-pushes — the present-storm guard.
	// A real change re-composites the retained dropdown and presents ONCE (not
	// batched: menuRebuildDropdown emits no per-push presents itself).
	if (_menuModel.setHighlight(itemId)) {
		menuRebuildDropdown();
		presentBarrier();
	}
}

void FileRogerArtProvider::onWindowClose(uint32 token) {
	// Menu exile (R5): dropdown dispose (kernelSelect close). The dropdown is
	// drawn straight to the screen (no window, and this seam fires whether or not
	// its save-under restored), so this is a manual-invalidation case: clear the
	// model + journal, invalidate the retained box, present. Gated on an actual
	// open dropdown — kernelSelect fires this close on EVERY event it examines
	// (each keypress), so the no-dropdown call must stay O(1) with no present.
	// The MODEL reset (closeDropdown) runs UNCONDITIONALLY — a registered-but-disabled
	// provider still ingests onText rows via the ungated path, so its close reset must
	// fire too (enabled-gate symmetry). Journal/present work stays below the gate.
	if (token == kGfxTokenMenuDropdown) {
		const bool hadRows = !_menuModel.rows().empty();
		const Common::Rect box = _menuModel.box();
		_menuModel.closeDropdown();
		if (!enabled)
			return;
		const bool removed = _journal && _journal->clearToken(kGfxTokenMenuDropdown);
		if (hadRows || removed) {
			markVacatedDirty(box);
			presentBarrier();
		}
		return;
	}
	if (!enabled)
		return;
	_pendingShowOwner = 0; // a close cancels any armed open->show attribution
	// The window-bracket close (control namespace) drops the window box op AND every
	// op captured inside it (controls + generic text) — ONE signal subsuming the old
	// 0x40.. + 0x60.. clear pair (the 0x60.. clear was already a no-op once the
	// bracket closed). Singleton tokens (status strip 0x10.., dropdown 0x20..) from
	// menu.cpp route through the same clear path as before.
	clearWindowToken(token);
}

void FileRogerArtProvider::clearWindowToken(uint32 token) {
	// The save-under restore path (bitsRestore) now goes through onRestore
	// (checkpoint rollback) and no longer arrives here. Sole caller is the
	// onWindowClose dispatcher; tokens seen:
	//   - GfxPorts::removeWindow Ã¢â‚¬â€ bracket close (0x40000000|id + 0x60000000|id)
	//   - menu.cpp Ã¢â‚¬â€ status strip (0x10000000) and dropdown (0x20000000) singletons
	// All work below is gated on an actual removal Ã¢â‚¬â€ a no-op call must stay cheap
	// (no present, no dirty marks) to keep per-cycle cost trivial.
	Common::Array<Common::Rect> removedRects;
	bool removedUi = false;
	if (_journal) {
		const uint32 ns = token & Roger::kTokenNamespaceMask;
		if (ns == Roger::kControlTokenNs) {
			// removeWindow bracket (via onWindowClose): drops the window box op AND every
			// op captured inside it, whatever port drew it (the kGenericTextTokenNs|portId clear that
			// ports.cpp also sends becomes a no-op Ã¢â‚¬â€ brackets subsume it).
			removedUi = _journal->closeBracket(token & 0x0FFFFFFFu, &removedRects);
		} else if (ns == Roger::kGenericTextTokenNs) {
			removedUi = false; // lifetime is bracket/erase-based now
		} else {
			removedUi = _journal->clearToken(token, &removedRects); // explicit singletons (status strip, dropdown, frame box)
		}
	}

	// Window dispose also kills that window's Feeder B pixel captures (controls namespace
	// 0x40000000 | window id) Ã¢â‚¬â€ both the not-yet-processed pending regions (queued while a
	// blocking window froze the animate cycle; processing them after dispose would stamp the
	// restored native background over the plate) and the persistent stamps already created
	// (conversation portraits etc. must vanish with their window). Cheap when nothing is
	// tagged: bitsRestore's per-cycle handle tokens have small segments (top nibble 0), so
	// they don't enter this branch. Removed stamp rects join removedRects for the dirty pass.
	bool removedStamps = false;
	if ((token & Roger::kTokenNamespaceMask) == Roger::kControlTokenNs) {
		for (uint i = _foregroundRegions.size(); i-- > 0;)
			if (_foregroundRegions[i].owner == token)
				_foregroundRegions.remove_at(i);
		for (uint i = _textSprites.size(); i-- > 0;) {
			if (_textSprites[i].owner != token)
				continue;
			removedRects.push_back(_textSprites[i].celRect);
			if (_textSprites[i].celOverride && _textSprites[i].celOverrideOwned) {
				_textSprites[i].celOverride->free();
				delete _textSprites[i].celOverride;
			}
			_textSprites.remove_at(i);
			removedStamps = true;
		}
	}

	if (!removedUi && !removedStamps) {
		if (_diag && isGenericTextToken(token))
			warning("ROGER-DIAG[clearToken]: tok=0x%08x removed=0 (no match)", token);
		return;
	}
	if (_diag && isGenericTextToken(token))
		warning("ROGER-DIAG[clearToken]: tok=0x%08x removed=1", token);
	// RETAINED duty-3 exception (Phase 3): no-save-under / reanimate==false disposals
	// never fire bitsRestore, and Feeder B stamp rects can exceed the save-under rect Ã¢â‚¬â€
	// this is their only same-present invalidation (net blind while frozen; _dirtyPrev a frame late).
	for (uint i = 0; i < removedRects.size(); i++)
		markVacatedDirty(removedRects[i]);
	presentBarrier();
}

void FileRogerArtProvider::onErase(const Common::Rect &nativeRect) {
	if (!enabled)
		return;
	// Ã‚Â§3.1 exact invalidation: the restored save-under rect, straight from SCI.
	// This is what makes the SQ3 white-line class structurally dead Ã¢â‚¬â€ the region
	// is invalidated no matter what any element bookkeeping thought was there.
	markNativeDirty(nativeRect);
	if (!overlayShown() || !_plate || !_journal || nativeRect.isEmpty()) {
		presentBarrier(); // mid-cycle: defers; frozen-cycle: flushes the mark
		return;
	}
	// Remove persisted generic text and kDrawCel icon captures whose box lies within
	// the erased region: the restore just overwrote those native pixels.
	// No early-out on !removed Ã¢â‚¬â€ the barrier must always fire to flush the
	// markNativeDirty above (bitsRestore walking storm: barrier defers mid-cycle,
	// so no per-hook present; the deferral, not a token match, guards the cycle).
	const bool removed = _journal->eraseContained(nativeRect);
	if (removed && _diag)
		warning("ROGER-DIAG[eraseText]: rect=(%d,%d,%d,%d) remaining=%u",
		        nativeRect.left, nativeRect.top, nativeRect.right, nativeRect.bottom,
		        (unsigned)_journal->ops().size());
	presentBarrier();
}

void FileRogerArtProvider::journalAppend(const Roger::UiElement &e) {
	// Drop a reveal rect only when the new content EFFECTIVELY COVERS it Ã¢â‚¬â€ genuine
	// content drawn over a rolled-back region should not be suppressed, but a mere
	// overlap must not kill the reveal: a status-bar re-push overlapping a dropdown
	// reveal by one row would otherwise cancel it and let the residue stamp return
	// (Phase 2 final review, Minor #4 Ã¢â‚¬â€ timing-fragile any-intersection rule).
	for (uint i = _revealRects.size(); i-- > 0;) {
		if (Roger::rectCoverageFraction(_revealRects[i], e.nativeRect) >= Roger::kCoverageThresholdPct)
			_revealRects.remove_at(i);
	}
	_journal->append(e);
}

void FileRogerArtProvider::onSave(uint32 handleToken, const Common::Rect &rect) {
	if (!enabled || !_journal)
		return;
	_journal->checkpoint(handleToken, rect);
}

void FileRogerArtProvider::onFree(uint32 handleToken) {
	if (!enabled || !_journal)
		return;
	_journal->dropCheckpoint(handleToken);
}

void FileRogerArtProvider::onRestore(uint32 handleToken, const Common::Rect &rect) {
	if (!enabled)
		return;
	// Ã‚Â§3.1 invalidation first, exactly like onErase (the barrier defers
	// mid-cycle; a frozen cycle flushes the mark).
	markNativeDirty(rect);
	patchNativeBaseline(rect); // SBS: patch frozen-cycle draws into the native right-panel baseline
	if (!overlayShown() || !_plate || !_journal) { presentBarrier(); return; }
	Common::Array<Common::Rect> removed;
	bool did = _journal->rollback(handleToken, rect, &removed);
	if (!did)
		// unknown handle: old semantics, but spare the persistent singletons Ã¢â‚¬â€ nothing repaints them after a bare restore
		did = _journal->eraseContained(rect, &removed, true);
	// Stamps drawn since the checkpoint inside the rect die with the rollback
	// (menu-bug class: a dropdown's own stamps must not outlive it). Coverage-based
	// (>= 90%), NOT strict containment: the restore rect is byte-aligned and up to a
	// pixel narrower per side than the show rect that created the stamp (show
	// (60,9,214,59) vs restore (61,9,214,59)); strict rect.contains(celRect) misses
	// that 1px inset and the stamp is retained forever Ã¢â‚¬â€ the tracked non-enhanced menu
	// residue. This mirrors the >= 90% reveal-suppression at capture time so stamp
	// creation and rollback stay symmetric.
	for (uint i = _textSprites.size(); i-- > 0;) {
		if (Roger::restoreReclaimsStamp(rect, _textSprites[i].celRect, Roger::kCoverageThresholdPct) && _textSprites[i].seq > 0) {
			removed.push_back(_textSprites[i].celRect);
			if (_textSprites[i].celOverride && _textSprites[i].celOverrideOwned) {
				_textSprites[i].celOverride->free();
				delete _textSprites[i].celOverride;
			}
			_textSprites.remove_at(i);
			did = true;
		}
	}
	// Ã¢â‚¬Â¦and pending not-yet-processed regions the rect substantially covers are stale too
	// (same coverage rule as the stamps above Ã¢â‚¬â€ a byte-aligned restore must still reclaim
	// a 1px-wider pending show region).
	for (uint i = _foregroundRegions.size(); i-- > 0;) {
		if (Roger::restoreReclaimsStamp(rect, _foregroundRegions[i].rect, Roger::kCoverageThresholdPct))
			_foregroundRegions.remove_at(i);
	}
	for (uint i = 0; i < removed.size(); i++)
		markVacatedDirty(removed[i]);
	// Remember the reveal: the caller (or a later native op this cycle) will
	// bitsShow the restored pixels; that show is NOT content.
	_revealRects.push_back(rect);
	if (_diag)
		warning("ROGER-DIAG[restore]: tok=0x%08x rect=(%d,%d,%d,%d) rolledBack=%d removed=%u",
		        handleToken, rect.left, rect.top, rect.right, rect.bottom,
		        did ? 1 : 0, (unsigned)removed.size());
	// The top strip's score/title banner and the transient menu bar share the singleton
	// token 0x10000000 (menuRebuildBar / the status banner). On the MOUSE menu
	// path SCI closes the menu by bitsRestore(_barSaveHandle) of the full menu strip Ã¢â‚¬â€
	// which reverts the NATIVE pixels to the saved banner background Ã¢â‚¬â€ WITHOUT a following
	// kernelDrawStatus. rollback() deliberately spares the 0x10000000 op from removal
	// (isSaveUnderExemptSingleton), so the stale menu-titles op is neither rolled back nor
	// re-pushed as the banner: the enhanced strip stays stuck on "File Game Action ..."
	// while native shows the score banner (SBS-confirmed Roger defect). When a restore
	// reverts the whole status strip, re-apply the cached banner to mirror native Ã¢â‚¬â€ the
	// same seam the keyboard path reaches via a follow-up kernelDrawStatus. Gated on the
	// restore actually covering the strip (>= 90% of _statusRect), so a dropdown's own
	// narrower restore (top row 9, never touching the banner row 0) does not trigger it.
	if (_haveStatus && !_statusRect.isEmpty() &&
	    Roger::rectCoverageFraction(_statusRect, rect) >= Roger::kCoverageThresholdPct) {
		reapplyStatus(); // clears 0x10000000 titles, re-pushes the banner, presents
		return;
	}
	presentBarrier();
}

// Fixed token for the kGraphFrameBox selection highlight. Room-scoped: cleared by
// the room-change journal clear (onNativePicture / room entry). A single constant
// token means each new push calls clearToken() first, so the highlight tracks
// movement without accumulating stale elements even when the rect changes.
void FileRogerArtProvider::uiPushFrameBoxInternal(const Common::Rect &r, int penColor) {
	if (!overlayShown() || !_plate) return; // no hires scene Ã¢â‚¬â€ leave native highlight visible
	ensureUi();
	// Gate: if the frame element under Roger::kFrameBoxToken is already identical (same rect
	// + same color), skip the clear/push/invalidate/present cycle entirely. This prevents
	// a per-cycle present storm when kernelDrawText fires on every control redraw (TAB,
	// hover, any redraw) while the selection has not actually moved or changed color.
	// Per CLAUDE.md per-cycle discipline: only mutate + present when the frame changed.
	const Common::Array<Roger::UiElement> &elems = _journal->ops();
	Common::Rect oldFrameRect; // empty when no existing frame element
	for (uint i = 0; i < elems.size(); i++) {
		if (elems[i].token == Roger::kFrameBoxToken) {
			if (elems[i].nativeRect == r && elems[i].penColor == penColor)
				return; // identical Ã¢â‚¬â€ nothing to do
			oldFrameRect = elems[i].nativeRect;
			break; // found but different Ã¢â‚¬â€ fall through to update
		}
	}
	// Selection moved or color changed (or no existing element): update and present.
	// clearToken() removes the stale element so the rect/color change takes effect
	// (push() only deduplicates on type+token+rect, so changing rect without clearing
	// would accumulate stale elements as the user moves the selection).
	_journal->clearToken(Roger::kFrameBoxToken);
	Roger::UiElement e;
	e.type = Roger::kUiWindow; e.nativeRect = r;
	e.backColor = -1; // no fill Ã¢â‚¬â€ never paints over scene content
	e.penColor = penColor;
	e.hasFrame = true;
	e.token = Roger::kFrameBoxToken;
	journalAppend(e);
	// RETAINED duty-3 exception (Phase 3, clearWindowToken's twin): no SCI save-under exists
	// for the frame box, and the net can't see overlay-only draws Ã¢â‚¬â€ old position would ghost.
	if (!oldFrameRect.isEmpty()) markVacatedDirty(oldFrameRect);
	markUiDirty(r);
	presentBarrier();
}

Graphics::Surface *FileRogerArtProvider::renderNativeCel(int viewId, int loopNo, int celNo) const {
	if (!g_sci || !g_sci->_gfxCache)
		return nullptr;
	if (viewId < 0)
		return nullptr; // synthetic sprite (celOverride-only): no view resource; getView would assert

	GfxView *view = g_sci->_gfxCache->getView(viewId);
	if (!view)
		return nullptr;

	int16 w = view->getWidth((int16)loopNo, (int16)celNo);
	int16 h = view->getHeight((int16)loopNo, (int16)celNo);
	if (w <= 0 || h <= 0)
		return nullptr;

	const CelInfo *ci = view->getCelInfo((int16)loopNo, (int16)celNo);
	if (!ci)
		return nullptr;

	byte clearKey = ci->clearKey;

	// getBitmap() caches the unpacked palette-index bitmap in the CelInfo.
	// It handles mirroring and undithering for EGA, and is cheaper than
	// calling unpackCel ourselves.
	const SciSpan<const byte> &bitmap = view->getBitmap((int16)loopNo, (int16)celNo);
	if (bitmap.size() < (uint)(w * h))
		return nullptr;

	// The view's embedded palette (getPalette) is null for EGA (SCI0) cels, which
	// is exactly what SQ3/QFG1 EGA use. The colors SCI actually displays live in
	// the active SYSTEM palette: GfxPalette::setEGA() fills it with the 16 EGA
	// colors for EGA games, or it holds the loaded palette for VGA. Use it so both
	// EGA and VGA cels resolve to correct RGB (view->getPalette() returning null no
	// longer drops the sprite).
	if (!g_sci->_gfxPalette16)
		return nullptr;
	const Palette &pal = g_sci->_gfxPalette16->_sysPalette;

	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *surf = new Graphics::Surface();
	surf->create(w, h, rgba);

	for (int16 y = 0; y < h; y++) {
		for (int16 x = 0; x < w; x++) {
			const byte raw = bitmap[y * w + x];
			uint32 px;
			if (raw == clearKey) {
				px = rgba.ARGBToColor(0, 0, 0, 0);
			} else {
				// Re-expand ScummVM's undithered EGA cel bytes (see egaDeUndither).
				const byte idx = Roger::egaDeUndither(raw, x, y, clearKey);
				const Color &c = pal.colors[idx];
				px = rgba.ARGBToColor(255, c.r, c.g, c.b);
			}
			surf->setPixel(x, y, px);
		}
	}

	return surf;
}

Graphics::Surface *FileRogerArtProvider::snapshotNativeRegion(const Common::Rect &nativeRect) const {
	if (!g_sci || !g_sci->_gfxScreen || !g_sci->_gfxPalette16)
		return nullptr;
	GfxScreen *screen = g_sci->_gfxScreen;
	Common::Rect r = nativeRect;
	r.clip(Common::Rect(0, 0, screen->getWidth(), screen->getHeight()));
	if (r.isEmpty())
		return nullptr;
	const Palette &pal = g_sci->_gfxPalette16->_sysPalette;
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *surf = new Graphics::Surface();
	surf->create(r.width(), r.height(), rgba);
	for (int16 y = 0; y < r.height(); y++) {
		for (int16 x = 0; x < r.width(); x++) {
			const byte idx = screen->getVisual((int16)(r.left + x), (int16)(r.top + y));
			const Color &c = pal.colors[idx];
			surf->setPixel(x, y, rgba.ARGBToColor(255, c.r, c.g, c.b));
		}
	}
	return surf;
}

void FileRogerArtProvider::clearTextSprites() {
	for (uint i = 0; i < _textSprites.size(); i++) {
		if (_textSprites[i].celOverride && _textSprites[i].celOverrideOwned) {
			_textSprites[i].celOverride->free();
			delete _textSprites[i].celOverride;
		}
	}
	_textSprites.clear();
	_foregroundRegions.clear();
}

void FileRogerArtProvider::processForegroundCaptures(const Common::Array<Common::Rect> &liveSpriteRects) {
	if (_foregroundRegions.empty())
		return; // nothing newly shown this frame; existing _textSprites persist as-is

	// Captured crisp text: exclude only regions a text rect SUBSTANTIALLY covers (>=80%), so a
	// graphic merely edge-clipped by a wide/multi-line text rect survives (fixes lost portrait/bars).
	Common::Array<Common::Rect> textRects;
	if (_journal)
		Roger::collectUiTextRects(_journal->ops(), Roger::kGenericTextTokenNs, textRects);

	// Filter per region (the filters judge each rect independently) so each surviving
	// rect keeps its owning-window token through to the stamped sprite.
	Common::Array<FgRegion> pending = _foregroundRegions;
	_foregroundRegions.clear();
	for (uint i = 0; i < pending.size(); i++) {
		const Common::Rect &nr = pending[i].rect;
		// The window's own frame+fill draw (GfxPorts::drawWindow's bitsShow, tagged with
		// the window token) is already reproduced semantically as a kUiWindow element Ã¢â‚¬â€
		// pixel-stamping it puts the blocky native dialog under the enhanced one (the SQ3
		// death-message bug; visible whenever the game cycle keeps running under a
		// non-modal window). Match by token + near-equal rect so a graphic drawn INSIDE
		// the window (dialog icons) still stamps.
		if (_journal && Roger::regionIsCapturedWindowBody(_journal->ops(), pending[i].owner, nr, Roger::kCoverageThresholdPct))
			continue;
		// A region a bitsRestore this cycle just revealed is restored background, not
		// content Ã¢â‚¬â€ do NOT pixel-stamp it (structural replacement for the deleted
		// beginSelfDraw suppressions). The check must be HERE, at process time, not
		// only at capture time: a menu's dropdown show is captured during the FROZEN
		// menu-open cycle (kernelAnimate does not tick, so no reveal exists yet), then
		// this pending region survives to the resumed close cycle where the restore has
		// planted its reveal. Coverage-based (>=90%), not strict containment: SCI re-shows
		// the restored region through kGraphRedrawBox / bitsShow grown by the element's 1px
		// frame (dropdown restore rect (7,9,141,27) vs its show (6,9,142,27), 98.5% inside).
		bool revealed = false;
		for (uint r = 0; r < _revealRects.size(); r++) {
			if (Roger::rectCoverageFraction(nr, _revealRects[r]) >= Roger::kCoverageThresholdPct) { revealed = true; break; }
		}
		if (revealed)
			continue;
		Common::Array<Common::Rect> one, keep;
		one.push_back(nr);
		// Live cast: exclude on any intersection (moving actors must never be pixel-stamped).
		Roger::filterForegroundCaptureRegions(one, liveSpriteRects, keep);
		if (keep.empty())
			continue;
		Common::Array<Common::Rect> keep2;
		Roger::filterForegroundCaptureRegionsCovered(keep, textRects, 80, keep2);
		if (keep2.empty())
			continue;
		Graphics::Surface *snap = snapshotNativeRegion(nr);
		if (!snap)
			continue;
		// Insert or update by native rect (a redraw of the same region refreshes the cel).
		bool updated = false;
		for (uint j = 0; j < _textSprites.size(); j++) {
			if (_textSprites[j].celRect == nr) {
				if (_textSprites[j].celOverride && _textSprites[j].celOverrideOwned) {
					_textSprites[j].celOverride->free();
					delete _textSprites[j].celOverride;
				}
				_textSprites[j].celOverride = snap;
				_textSprites[j].celOverrideOwned = true;
				_textSprites[j].owner = pending[i].owner; // latest draw's window owns the stamp
				_textSprites[j].seq = ++_stampSeqCounter; // refresh seq: any rollback affecting this region will drop it
				updated = true;
				break;
			}
		}
		if (updated)
			continue;
		Roger::Sprite s;
		s.viewId = -1; s.loopNo = -1; s.celNo = -1; // synthetic: no real view; render reads celOverride
		s.celRect = nr;
		s.priority = 255;   // always-on-top UI: never occluded by the plate priority map (compositor:452)
		s.mirror = false;
		s.celOverride = snap;
		s.celOverrideOwned = true;
		s.owner = pending[i].owner; // window token: stamp dies with its window (onWindowClose)
		s.seq = ++_stampSeqCounter; // seq tag: rollback can remove this stamp if it postdates a checkpoint
		_textSprites.push_back(s);
		if (_diag)
			warning("ROGER-DIAG[fgCapture]: pic=%d rect=(%d,%d,%d,%d) owner=0x%08x now=%u",
			        _loadedPicId, nr.left, nr.top, nr.right, nr.bottom, pending[i].owner,
			        (unsigned)_textSprites.size());
	}
	if (_debugCapture)
		dumpCaptureDebug();
}

void FileRogerArtProvider::onAddToPicCelInternal(int viewId, int loopNo, int celNo,
                                                 const Common::Rect &celRect, int priority) {
	Roger::Sprite s;
	s.viewId = viewId;
	s.loopNo = loopNo;
	s.celNo = celNo;
	s.celRect = celRect;
	s.priority = priority;
	// Mirror is already baked into every cel source: GfxView::getBitmap() flips the
	// pixels for a mirrored loop, and both cel paths (renderNativeCel and the hires
	// ViewCache's generateViewCel) read through getBitmap. So the compositor must NOT
	// flip again Ã¢â‚¬â€ keep mirror false here.
	s.mirror = false;
	s.celOverride = nullptr;
	s.coverGrow = true; // game view cel -> drawn slightly larger to cover plate fringe
	_staticSprites.push_back(s);
	if (_diag)
		warning("ROGER-DIAG[addToPic]: pic=%d view=%d loop=%d cel=%d pri=%d rect=(%d,%d,%d,%d) nowHave=%u",
		        _loadedPicId, viewId, loopNo, celNo, priority,
		        celRect.left, celRect.top, celRect.right, celRect.bottom, (unsigned)_staticSprites.size());
}

void FileRogerArtProvider::onCel(const Common::Rect &rect, int viewId, int loopNo, int celNo,
                                 int priority, uint32 owner, CelSource source) {
	switch (source) {
	case kCelSourceInitBake:
		// owner-gated promotion (the _picNotValid trap) lives inside this body,
		// unchanged: a cel promotes only while its owner leaves the animate list.
		onInitCelInternal(viewId, loopNo, celNo, rect, priority, owner);
		break;
	case kCelSourceAddToPic:
		onAddToPicCelInternal(viewId, loopNo, celNo, rect, priority);
		break;
	case kCelSourceStandalone:
		onDrawCelInternal(rect, viewId, loopNo, celNo);
		break;
	case kCelSourceIcon:
		// `owner` carries the window UI token for the icon source (C9 decision):
		// read only here, never by the initBake promotion logic above.
		uiPushIconInternal(rect, viewId, loopNo, celNo, owner);
		break;
	case kCelSourceAnimate:
	default:
		// Live animate-cast cels are composited via onAnimateFrame, not per
		// draw — no per-cel work here (no seam emits kCelSourceAnimate yet).
		break;
	}
}

void FileRogerArtProvider::onInitCelInternal(int viewId, int loopNo, int celNo,
                                             const Common::Rect &celRect, int priority, uint32 owner) {
	if (owner != 0) {
		// One capture per animate object, latest draw wins Ã¢â‚¬â€ mirrors the native buffer,
		// which holds the object's most recent baked draw. Prevents an actor that moved
		// during room init from leaving a trail of stale copies.
		for (uint i = 0; i < _initCels.size(); i++) {
			Roger::Sprite &q = _initCels[i];
			if (q.owner == owner) {
				q.viewId = viewId; q.loopNo = loopNo; q.celNo = celNo;
				q.celRect = celRect; q.priority = priority;
				return;
			}
		}
	} else {
		// Ownerless (script kDrawCel) captures: dedup by view+loop+cel+rect (a room init
		// may redraw the same cel a few times).
		for (uint i = 0; i < _initCels.size(); i++) {
			const Roger::Sprite &q = _initCels[i];
			if (q.owner == 0 && q.viewId == viewId && q.loopNo == loopNo && q.celNo == celNo && q.celRect == celRect)
				return;
		}
	}
	Roger::Sprite s;
	s.viewId = viewId; s.loopNo = loopNo; s.celNo = celNo;
	s.celRect = celRect; s.priority = priority; s.mirror = false; s.celOverride = nullptr;
	s.owner = owner;
	s.coverGrow = true; // game view cel -> drawn slightly larger to cover plate fringe
	_initCels.push_back(s);
	if (_diag)
		warning("ROGER-DIAG[initCel]: pic=%d view=%d loop=%d cel=%d pri=%d rect=(%d,%d,%d,%d) owner=%08x now=%u",
		        _loadedPicId, viewId, loopNo, celNo, priority,
		        celRect.left, celRect.top, celRect.right, celRect.bottom, owner, (unsigned)_initCels.size());
}

// Self-draw depth (SciGfxObserver::beginSelfDraw/endSelfDraw). Bracketed sites:
// GfxPaint16::drawPicture, drawCelAndShow, drawHiresCelAndShow, kernelDisplay's
// two flush shows, GfxAnimate::updateScreen and reAnimate. While depth > 0 the
// generic onShow capture ignores shows (content already composited semantically).
// History: unbracketed kDisplay flush shows pixel-stamped doubles of every
// kDisplay line next to the TTF render (SQ3 intro doubled credits) — and
// bracketing Box itself depth-suppressed the per-line text capture; only the
// flush shows are bracketed. Deliberately NOT gated on `enabled`: the depth
// must stay balanced no matter what flags flip between begin and end.
void FileRogerArtProvider::beginSelfDraw() { _nativeDrawDepth++; }
void FileRogerArtProvider::endSelfDraw()   { if (_nativeDrawDepth > 0) _nativeDrawDepth--; }

void FileRogerArtProvider::onShow(const Common::Rect &screenRect, uint32 owner) {
	if (!enabled)
		return;
	// R9 open->show attribution: a show that self-derived owner 0 immediately after
	// an onWindowOpen (the drawWindow terminal show, which runs under _wmgrPort)
	// adopts the just-opened window's token when contained in its rect. Single-shot:
	// consumed by the first matching show; also reset at every frame boundary
	// (onFrameStart) and at onWindowClose. Ordinary port-derived shows (owner != 0)
	// pass through untouched.
	if (owner == 0 && _pendingShowOwner != 0 && _pendingShowRect.contains(screenRect)) {
		owner = _pendingShowOwner;
		_pendingShowOwner = 0;
	}
	onShowInternal(screenRect, owner);
}

void FileRogerArtProvider::onShowInternal(const Common::Rect &screenRect, uint32 ownerToken) {
	// Ã‚Â§3.1 exact invalidation: SCI showed these native pixels, so the overlay
	// region is stale regardless of any capture bookkeeping below. Deliberately
	// NOT gated on _nativeDrawDepth: invalidation is dumb and exact; only the
	// content capture below is scoped. O(1); never presents.
	markNativeDirty(screenRect);
	patchNativeBaseline(screenRect); // SBS: patch frozen-cycle draws into the native right-panel baseline
	if (!overlayShown() || _nativeDrawDepth > 0 || !_plate)
		return; // overlay off, inside a Roger-handled draw, or no hires plate
	if (screenRect.isEmpty())
		return;
	// The status/menu strip is the banner's business (uiPushStatus/reapplyStatus); the
	// overlay leaves it transparent so the native bar shows through. A strip-only show
	// (e.g. the menu bar's black underline row) must not become a picture stamp.
	if (screenRect.bottom <= (int16)_statusBarH)
		return;
	// A show inside a rect we just rolled back is SCI revealing restored
	// background Ã¢â‚¬â€ capture nothing (structural replacement for the hand-placed
	// beginSelfDraw suppressions on restore paths; kills the menu-close class).
	// Coverage-based (>=90%), not strict containment: after a bitsRestore SCI
	// re-shows the region through kGraphRedrawBox / bitsShow grown by the element's
	// 1px frame (the menu dropdown's restore rect is (7,9,141,27) but its follow-up
	// show is (6,9,142,27) Ã¢â‚¬â€ one border pixel wider per side). Strict contains()
	// missed that overhang and pixel-stamped the native dropdown residue.
	for (uint i = 0; i < _revealRects.size(); i++) {
		if (Roger::rectCoverageFraction(screenRect, _revealRects[i]) >= Roger::kCoverageThresholdPct)
			return;
	}
	if (_diag)
		warning("ROGER-DIAG[showRect]: pic=%d rect=(%d,%d,%d,%d) owner=0x%08x", _loadedPicId,
		        screenRect.left, screenRect.top, screenRect.right, screenRect.bottom, ownerToken);
	FgRegion r; r.rect = screenRect; r.owner = ownerToken;
	_foregroundRegions.push_back(r); // persistent foreground-sprite capture (was _genRegions)
}

void FileRogerArtProvider::onText(const Common::Rect &rect, const char *text, int fontId,
                                  int penColor, int backColor, int align,
                                  int nativeFontH, int nativeTextW, uint32 token,
                                  TextSource source, uint16 itemId) {
	switch (source) {
	case kTextSourceBox:
		// Box body forces backColor=-1 / role=body itself; the 8-param internal keeps that.
		onTextBoxInternal(rect, text, fontId, penColor, align, nativeFontH, nativeTextW, token);
		break;
	case kTextSourceStatus:
		uiPushStatusInternal(rect, text, fontId, penColor, backColor, token,
		                     nativeFontH, nativeTextW);
		break;
	case kTextSourceControl:
	case kTextSourceListRow:
	case kTextSourceFill:
		// Body role, no alt font. Fill is text="" -> the internal draws only the bg box.
		uiPushTextInternal(rect, text, penColor, backColor, fontId, align, token,
		                   Roger::kRoleBody, false, nativeFontH, nativeTextW);
		break;
	case kTextSourceMenuBar:
		// Menu exile (R5): accumulate into the retained model; the journal emit
		// happens ONCE at endBatch (menuRebuildBar). The bar reset armed by
		// beginBatch fires on the FIRST title only, so a dropdown batch (which
		// emits no menuBar text) never wipes the retained bar titles.
		if (_barResetPending) {
			_menuModel.beginBar();
			_barResetPending = false;
		}
		_menuModel.addBarTitle(rect, text ? text : "", nativeFontH, nativeTextW);
		_batchTouchedBar = true;
		break;
	case kTextSourceMenuRow:
		// Dropdown row: the SCI item id rides onText's dedicated itemId param
		// (selection is keyed by item id — separator rows skip ids, so ordinal
		// derivation is unsafe). Emit happens at endBatch (menuRebuildDropdown).
		_menuModel.addRow(rect, text ? text : "", itemId, nativeFontH, nativeTextW);
		_batchTouchedDropdown = true;
		break;
	default:
		uiPushTextInternal(rect, text, penColor, backColor, fontId, align, token,
		                   Roger::kRoleBody, false, nativeFontH, nativeTextW);
		break;
	}
}

void FileRogerArtProvider::onTextBoxInternal(const Common::Rect &nativeRect, const char *text,
                                             int fontId, int penColor, int align,
                                             int nativeFontH, int nativeTextW, uint32 winToken) {
	if (!overlayShown() || _nativeDrawDepth > 0 || !_plate)
		return; // overlay off, inside a Roger-handled draw, or no hires plate
	if (!text || !*text || nativeRect.isEmpty())
		return;
	if (_diag)
		warning("ROGER-DIAG[nativeText]: pic=%d rect=(%d,%d,%d,%d) font=%d pen=%d tok=%08x \"%s\"",
		        _loadedPicId, nativeRect.left, nativeRect.top, nativeRect.right, nativeRect.bottom,
		        fontId, penColor, winToken, text);
	Roger::UiElement e;
	e.type = Roger::kUiText;
	e.nativeRect = nativeRect;
	e.text = text;
	e.fontId = fontId;
	e.penColor = penColor;
	e.backColor = -1;          // no fill: drawn over the plate / window background
	e.align = align;
	// Scope to the drawing window (generic namespace | port id). Falls back to the
	// namespace base if the caller had no port, matching the picture-port persist case.
	e.token = isGenericTextToken(winToken) ? winToken : Roger::kGenericTextTokenNs;
	e.textRole = Roger::kRoleBody;   // same body size as dialog/control text
	e.nativeFontH = nativeFontH;     // native cell height -> renderer target size
	e.nativeTextW = nativeTextW;     // single-line width cap (0 = multi-line: no cap)
	_genTextPending.push_back(e);
	// Emit into _journal NOW, not deferred to the next animate cycle. A blocking message
	// (Print/kDisplay) draws its text here and then waits for a click WITHOUT ticking
	// kernelAnimate, so a deferred flush would only reach _journal after the message's
	// window is already disposed Ã¢â‚¬â€ missing its bracket close and leaving the text
	// tagged to a dead window (it then lingered until the NEXT window reused the id). Pushing
	// immediately means the text is in _journal under its live window bracket, so the window's
	// removeWindow closes it on dismiss. Safe: onNativeText fires on a real text draw, not
	// per-cycle. onAnimateFrame still calls flushGenericText (a no-op when empty).
	flushGenericText();
}

void FileRogerArtProvider::flushGenericText() {
	if (!overlayShown() || !_plate)
		{ _genTextPending.clear(); return; }
	ensureUi();
	// Emit this frame's generic captures PERSISTENTLY: append each into _journal where it
	// stays until room change. We do NOT clear prior generic text every frame, because SCI
	// draws static text (e.g. QFG1 stat labels) only once Ã¢â‚¬â€ clearing+relying-on-recapture
	// made it flash then vanish. append() supersedes an element with the same type and
	// containing rect, so a stat value redraw refreshes in place (live
	// updates) while untouched lines persist. Cleared wholesale on room change (clear).
	for (uint i = 0; i < _genTextPending.size(); i++) {
		Roger::UiElement e = _genTextPending[i];
		// Build glyphs for non-ASCII bytes using the cross-frame cache so each distinct
		// glyph surface (ch, fontId, penColor) is generated at most once per room.
		// Without this, every call would push new surfaces into _uiIcons each frame.
		for (const char *p = e.text.c_str(); *p; ++p) {
			const byte c = (byte)*p;
			if (c >= 0x20 && c < 0x7f)
				continue; // ASCII handled by TTF
			bool found = false;
			for (uint k = 0; k < _genericGlyphCache.size(); k++) {
				if (_genericGlyphCache[k].ch == c && _genericGlyphCache[k].fontId == e.fontId &&
				    _genericGlyphCache[k].penColor == e.penColor) {
					Roger::UiGlyph ug; ug.ch = c; ug.surf = _genericGlyphCache[k].surf;
					e.glyphs.push_back(ug);
					found = true;
					break;
				}
			}
			if (!found) {
				// First use of this glyph this room: generate it and cache it.
				bool alreadyInOut = false;
				for (uint k = 0; k < e.glyphs.size(); k++)
					if (e.glyphs[k].ch == c) { alreadyInOut = true; break; }
				if (!alreadyInOut) {
					Graphics::Surface *g = _assetGen ? _assetGen->generateTextSurface(
					    Common::String(1, (char)c), e.fontId,
					    (byte)(e.penColor >= 0 ? e.penColor : 0)) : nullptr;
					if (g) {
						_uiIcons.push_back(g); // owned; freed on room change
						Roger::UiGlyph ug; ug.ch = c; ug.surf = g;
						e.glyphs.push_back(ug);
						GenGlyphKey key; key.ch = c; key.fontId = e.fontId;
						key.penColor = e.penColor; key.surf = g;
						_genericGlyphCache.push_back(key);
					}
				}
			}
		}
		journalAppend(e);
	}
	_genTextPending.clear();
	// Drop any generic element a controls16/menu element already covers (no double render).
	_journal->dedupeGenericText(Roger::kGenericTextTokenNs);
	if (_debugCapture)
		dumpCaptureDebug();
}

void FileRogerArtProvider::onFrameEnd() {
	if (!enabled)
		return;
	// kernelAnimate is mid-cycle from this hook until onAnimateFrame runs.
	// While it is, blocking-seam barrier calls defer (the cycle's own tail call
	// flushes them) Ã¢â‚¬â€ this is what makes a bitsRestore storm structurally unable
	// to present per-hook (the bb65c56b75a class).
	_inAnimateCycle = true;
	// Ã¢â€â‚¬Ã¢â€â‚¬ Cycle-diff backstop net (spec Phase 2) Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
	// The visual buffer holds the WHOLE previous frame at this seam (see the
	// side-by-side comment below). Diff it against the previous cycle's copy and
	// mark the changed boxes dirty via markNativeDirty Ã¢â‚¬â€ which clips the status
	// strip, grows 1 native px, maps to overlay space, and (because _inAnimateCycle
	// is already set) routes to the scene seed union so the cycle-tail present
	// re-seeds clean background. Anything a hook missed heals here within one
	// cycle. Cost budget < 1.0 ms median: ROGER-NET sum32 under -CycleLog is the
	// measurement; the perf gate's busy+1ms threshold is the hard backstop.
	if (_diffNet && overlayShown() && g_sci && g_sci->_gfxScreen) {
		const uint32 netT0 = _cycleLog ? g_system->getMillis() : 0;
		GfxScreen *netScreen = g_sci->_gfxScreen;
		const int nw = netScreen->getWidth(), nh = netScreen->getHeight();
		_netCurVisual.resize((uint)nw * nh); // no-op after the first cycle
		for (int y = 0; y < nh; y++)
			for (int x = 0; x < nw; x++)
				_netCurVisual[(uint)y * nw + x] = netScreen->getVisual((int16)x, (int16)y);
		uint netBoxes = 0;
		if (_haveNetPrev && _netPrevVisual.size() == _netCurVisual.size()) {
			Common::Array<Common::Rect> changed;
			Roger::extractChangedBoxes(_netPrevVisual.begin(), _netCurVisual.begin(), nw, nh, changed);
			netBoxes = changed.size();
			for (uint i = 0; i < changed.size(); i++)
				markNativeDirty(changed[i]);
		}
		_netPrevVisual = _netCurVisual;
		_haveNetPrev = true;
		if (_cycleLog) {
			_netCostAccumMs += g_system->getMillis() - netT0;
			if ((++_netCycleCount & 31) == 0) {
				warning("ROGER-NET sum32=%ums boxes=%u", _netCostAccumMs, netBoxes);
				_netCostAccumMs = 0;
			}
		}
	}
	// Side-by-side compare mode also needs this snapshot: it is taken at the one moment
	// the native visual buffer holds the WHOLE frame (pic + addToPic + animate cast),
	// just before restoreAndDelete() erases the animating cast (ego/moving views). The
	// live buffer read later in presentComparison has that cast already erased.
	if (_mode != Roger::kModeSideBySide) return; // else skip the costly per-frame snapshot (SBS panel only)
	if (!overlayShown() || !g_sci || !g_sci->_gfxScreen)
		return;
	GfxScreen *screen = g_sci->_gfxScreen;
	const int sw = screen->getWidth(), sh = screen->getHeight();
	_nativeBaseline.resize((uint)sw * sh);
	for (int y = 0; y < sh; y++)
		for (int x = 0; x < sw; x++)
			_nativeBaseline[(uint)y * sw + x] = screen->getVisual((int16)x, (int16)y);
	_haveBaseline = true;
}

void FileRogerArtProvider::patchNativeBaseline(const Common::Rect &r) {
	// SBS right panel: _nativeBaseline is snapshotted once per animate cycle. A
	// blocking Print/Display/menu loop draws AFTER the last snapshot and then
	// freezes the cycle, so its window never reaches the panel. Patch exactly the
	// shown/restored rect from the live visual buffer. Gated on !_inAnimateCycle:
	// mid-cycle shows are the cast's own draw/erase churn, owned by the next
	// cycle's full snapshot - patching those would bake cast-erased background
	// into the baseline. O(rect), single pass.
	//
	// A frozen cycle has no later present (the last present fired mid-draw, before
	// the window's final bitsShow), so patched pixels sit unseen until some
	// unrelated present occurs. If real pixels changed, set _barrierDirty and call
	// presentBarrier() to flush the right panel immediately. Gating on real change
	// (not unconditional) keeps no-op re-blits (e.g. the per-cycle score-row
	// bitsShow that writes identical bytes) from presenting - the "gate the present
	// on real change" performance rule. presentBarrier() respects _uiBatchDepth and
	// _inAnimateCycle, so the call is safe here.
	if (_mode != Roger::kModeSideBySide || !_haveBaseline || _inAnimateCycle)
		return;
	if (!g_sci || !g_sci->_gfxScreen)
		return;
	GfxScreen *screen = g_sci->_gfxScreen;
	const int sw = screen->getWidth(), sh = screen->getHeight();
	if ((int)_nativeBaseline.size() != sw * sh)
		return;
	Common::Rect c = r;
	c.clip(Common::Rect(0, 0, (int16)sw, (int16)sh));
	bool changed = false;
	for (int y = c.top; y < c.bottom; y++) {
		for (int x = c.left; x < c.right; x++) {
			const byte v = screen->getVisual((int16)x, (int16)y);
			byte &stored = _nativeBaseline[(uint)y * sw + x];
			if (v != stored) {
				stored = v;
				changed = true;
			}
		}
	}
	if (changed) {
		_barrierDirty = true;
		presentBarrier();
	}
}

bool FileRogerArtProvider::drawGenericRegions(Graphics::ManagedSurface &scene,
                                              const Common::Rect &picRect) {
	if (!g_sci || !g_sci->_gfxScreen)
		return false;
	// Feeder B composites only the bitsShow-hook regions recorded this frame; bail cheaply.
	if (_genRegions.empty())
		return false;
	GfxScreen *screen = g_sci->_gfxScreen;
	const int sw = screen->getWidth();    // 320 (SCI0)
	const int sh = screen->getHeight();   // 200
	byte pal[256 * 3];
	g_system->getPaletteManager()->grabPalette(pal, 0, 256);

	// Snapshot the visual buffer once (getVisual is a per-pixel inline read).
	Common::Array<byte> vis;
	vis.resize((uint)sw * sh);
	for (int y = 0; y < sh; y++)
		for (int x = 0; x < sw; x++)
			vis[(uint)y * sw + x] = screen->getVisual((int16)x, (int16)y);

	bool drewAny = false;
	const int picScreenTop = _compositor->picScreenTop();
	for (uint i = 0; i < _genRegions.size(); i++) {
		Common::Rect nr = _genRegions[i];
		nr.clip(Common::Rect(0, 0, (int16)sw, (int16)sh));
		// Drop the menu-bar strip and anything above the picture window; the banner /
		// UI display-list owns that region.
		nr.top    = MAX<int16>(nr.top, (int16)picScreenTop);
		nr.bottom = MIN<int16>(nr.bottom, (int16)(picScreenTop + _compositor->picH()));
		if (nr.isEmpty())
			continue;
		Common::Rect dst = Roger::mapNativeRectToOverlay(nr, picRect,
			_compositor->picW(), _compositor->picH(), picScreenTop);
		dst.clip(picRect);
		if (dst.isEmpty())
			continue;
		Roger::upscaleNativeRegionNearest(*scene.surfacePtr(), dst,
			vis.begin(), sw, nr, pal);
		_compositor->addDirtyRect(dst); // ensure the region is pushed (and erased next frame)
		drewAny = true;
	}
	if (_diag && drewAny)
		warning("ROGER-DIAG[genRegions]: pic=%d stamped=%u native regions (blocky Feeder B)",
		        _loadedPicId, (unsigned)_genRegions.size());
	_genRegions.clear();
	return drewAny;
}

void FileRogerArtProvider::onAnimateFrame(const AnimateList &list) {
	if (!enabled)
		return;
	Common::Array<Roger::Sprite> sprites;
	Common::Array<Graphics::Surface *> nativeSurfaces;

	const bool dbg = _debugLog;

	flushGenericText(); // emit this frame's generic text captures into _journal (deduped)

	// Build the set of cels in the LIVE animate cast this frame (view+loop+cel), so the
	// init-captured static cels (_initCels) can exclude anything that is actively animated
	// (those are drawn live; only the never-animated init draws Ã¢â‚¬â€ the baked signs/props Ã¢â‚¬â€ stay).
	for (AnimateList::const_iterator it = list.begin(); it != list.end(); ++it) {
		if (it->signal & kSignalHidden)
			continue;
		Roger::Sprite s;
		s.viewId   = it->viewId;
		s.loopNo   = it->loopNo;
		s.celNo    = it->celNo;
		s.celRect  = it->celRect;
		s.priority = it->priority;
		s.coverGrow = true; // game view cel -> drawn slightly larger to cover plate fringe
		// Mirror is already baked in upstream: GfxView::getBitmap() flips a mirrored
		// loop's pixels, and BOTH cel sources read through it Ã¢â‚¬â€ renderNativeCel (native
		// fallback) and the hires ViewCache's generateViewCel. Flipping again in the
		// compositor double-flips (ego walks backwards), so keep mirror false.
		s.mirror = false;

		// Provide a native-cel fallback only for sprites that have no hires view art.
		// renderScene consults getCel() first and ignores celOverride when a hires cel
		// exists, so rendering the fallback in that case is wasted work Ã¢â‚¬â€ skip it. getCel
		// is cheap (cached, incl. cached known-missing).
		if (!_viewCache || !_viewCache->getCel(it->viewId, it->loopNo, it->celNo)) {
			Graphics::Surface *nativeSurf = renderNativeCel(it->viewId, it->loopNo, it->celNo);
			s.celOverride = nativeSurf; // borrowed by the sprite (freed below)
			if (nativeSurf)
				nativeSurfaces.push_back(nativeSurf);
		}

		sprites.push_back(s);
	}

	// Static props captured at room-init time (_initCels): cels drawn while _picNotValid was
	// set, i.e. during the room's first setup Ã¢â‚¬â€ that includes BOTH the baked decorations
	// (QFG1 first-visit signs: drawn once via the init-frame cast, baked into the picture,
	// then their objects dispose out of the animate list) AND live actors like the ego, whose
	// first draw happens on the same init frame. No view/loop/cel identity can tell them apart
	// (SCI0 rooms pack decorations and actors into one per-room view resource: QFG1 300 signs
	// = view 300 loop 2, live bard/goblin = loops 0/1/3, and BOTH are in the cast on frame 1).
	// The reliable discriminator is the OWNING OBJECT, tagged at capture time: promote an init
	// cel only while its owner is ABSENT from the animate list. A disposed-after-baking prop
	// promotes (its pixels persist natively); a live actor never does (it is drawn Ã¢â‚¬â€ or, when
	// hidden, natively erased Ã¢â‚¬â€ by the cast), which is why this scans the full list including
	// kSignalHidden entries. Suppression is per-frame, not a permanent prune: the signs are in
	// the cast on frame 1 and must still promote after their objects leave.
	Common::Array<uint32> liveOwners;
	for (AnimateList::const_iterator it = list.begin(); it != list.end(); ++it)
		// Token must match onCel(kCelSourceInitBake)'s owner (gfxOwnerToken in
		// sci_gfx_observer.h, segment<<16 | offset) — the promotion discriminator.
		liveOwners.push_back(gfxOwnerToken(it->object.getSegment(), it->object.getOffset()));

	// Promote the eligible init cels, deduped against addToPic.
	Common::Array<Roger::Sprite> statics = _staticSprites;
	for (uint i = 0; i < _initCels.size(); i++) {
		const Roger::Sprite &c = _initCels[i];
		if (c.owner != 0) {
			bool live = false;
			for (uint j = 0; j < liveOwners.size(); j++)
				if (liveOwners[j] == c.owner) { live = true; break; }
			if (live)
				continue;
		}
		bool dup = false;
		for (uint j = 0; j < statics.size(); j++)
			if (statics[j].viewId == c.viewId && statics[j].loopNo == c.loopNo &&
			    statics[j].celNo == c.celNo && statics[j].celRect == c.celRect) { dup = true; break; }
		if (!dup)
			statics.push_back(c);
	}

	// Capture native foreground (menu/stat labels, buttons, software cursor) into persistent
	// sprites, scoped against the live cast so moving actors are never re-captured.
	// COORDINATE SEAM: fg capture rects are screen-global (bitsShow globalizes via
	// offsetRect), while sprite celRects are picture-local Ã¢â‚¬â€ mixing them drew every
	// stamp picScreenTop rows too low (the status bar's black underline row stamped
	// as a dark line across the top of every scene). Compare in screen space here,
	// convert to picture-local at the statics hand-off below.
	const int picTop = _compositor ? _compositor->picScreenTop() : _statusBarH;
	if (!_foregroundRegions.empty()) {
		Common::Array<Common::Rect> liveRects;
		for (uint i = 0; i < sprites.size(); i++) {
			Common::Rect lr = sprites[i].celRect;
			lr.translate(0, (int16)picTop); // picture-local -> screen space
			liveRects.push_back(lr);
		}
		processForegroundCaptures(liveRects);
	}
	for (uint i = 0; i < _textSprites.size(); i++) {
		Roger::Sprite s = _textSprites[i];
		s.celRect.translate(0, (int16)-picTop); // screen -> picture-local (compositor space)
		statics.push_back(s);
	}

	// Merge captured static cels (addToPic + init-baked) with the animate cast, priority-sorted,
	// so static props occlude/are-occluded correctly against the ego.
	Common::Array<Roger::Sprite> merged;
	Roger::mergeSpritesByPriority(sprites, statics, merged);

	// Static cels need a native-cel fallback too (when no hires cel exists). Build them
	// for the merged entries that lack a celOverride and are not in the animate list.
	for (uint i = 0; i < merged.size(); i++) {
		if (merged[i].celOverride)
			continue;
		// Skip the native fallback when a hires cel exists (renderScene would ignore it).
		if (_viewCache && _viewCache->getCel(merged[i].viewId, merged[i].loopNo, merged[i].celNo))
			continue;
		Graphics::Surface *nativeSurf = renderNativeCel(merged[i].viewId, merged[i].loopNo, merged[i].celNo);
		if (nativeSurf) {
			merged[i].celOverride = nativeSurf;
			nativeSurfaces.push_back(nativeSurf);
		}
	}

	if (dbg)
		warning("ROGER: pic=%d sprites=%u (+%u static: %u addToPic +%u init) plate=%dx%d overlay=%s",
		        _loadedPicId, (unsigned)sprites.size(), (unsigned)statics.size(),
		        (unsigned)_staticSprites.size(), (unsigned)(statics.size() - _staticSprites.size()),
		        _plate ? _plate->w : -1, _plate ? _plate->h : -1, overlayShown() ? "on" : "off");

	renderFrame(merged);

	for (uint i = 0; i < nativeSurfaces.size(); i++) {
		nativeSurfaces[i]->free();
		delete nativeSurfaces[i];
	}
	_inAnimateCycle = false; // cycle draw complete Ã¢â‚¬â€ reopen the barrier
	_revealRects.clear();   // reveal suppressions expire at cycle end (bitsShow fired already)
	presentBarrier(); // spec Ã‚Â§3.2: the per-cycle present (fresh-frame branch Ã¢â‚¬â€ no recompose)

	// ROGER-CYCLE telemetry (rebuilt from the deleted animate.cpp seam block).
	// period = entry-to-entry (walking-speed), busy = entry to end-of-composite
	// (covers restoreAndDelete - where the 2.7x walking regression lived). The
	// LINE FORMAT is pinned by regression-lib.ps1's Get-CycleStats regex
	// 'ROGER-CYCLE period=(\d+) busy=(\d+)' - never change it.
	uint32 cyclePeriod = 0, cycleBusy = 0;
	if (_cycleTelemetry.frameRendered(g_system->getMillis(), cyclePeriod, cycleBusy) && _cycleLog)
		warning("ROGER-CYCLE period=%u busy=%u", cyclePeriod, cycleBusy);
}

void FileRogerArtProvider::onFrameStart() {
	// Cycle telemetry origin stamp (the old animate.cpp seam block, rebuilt
	// here). One unconditional getMillis per cycle - negligible, and keeps
	// `period` honest across -CycleLog toggles. Deliberately NOT gated on
	// `enabled`: the old seam telemetry ran regardless of the enable flag.
	_cycleTelemetry.frameStart(g_system->getMillis());
	// R9: a pending open->show attribution never survives a frame boundary — the
	// drawWindow content show is same-call, so a stale arm here is a bug, not a
	// feature. O(1).
	_pendingShowOwner = 0;
}

void FileRogerArtProvider::remapComparisonMouse(Common::Point &mousePos) {
	if (_mode != Roger::kModeSideBySide)
		return;
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0)
		return;
	// mousePos is a whole-window linear map (0..319/0..199). Each panel shows the full game,
	// so remap through whichever panel the pointer is over: a click at the visual center of
	// EITHER panel hits game (160,100). The cursor (drawn at the physical pointer position in
	// presentComparison) then lines up with the interaction on both panels.
	Common::Rect leftF, rightF;
	Roger::comparePanelRects(OW, OH, leftF, rightF);
	const int px = mousePos.x * OW / 320; // -> overlay px
	const int py = mousePos.y * OH / 200;
	const Common::Rect &f = (px < OW / 2) ? leftF : rightF;
	int gx = (px - f.left) * 320 / f.width();
	int gy = (py - f.top) * 200 / f.height();
	if (gx < 0) gx = 0; else if (gx > 319) gx = 319;
	if (gy < 0) gy = 0; else if (gy > 199) gy = 199;
	mousePos.x = (int16)gx;
	mousePos.y = (int16)gy;
}

void FileRogerArtProvider::toggleOverlay() {
	diagDumpState("toggle");
	const Roger::CompareDisplayMode prev = _mode;
	_mode = Roger::nextDisplayMode(_mode);
	applyNativeCursorVisibility();

	if (_mode == Roger::kModeOriginal) {
		// Enhanced -> Original: reveal the native 320x200 render underneath.
		g_system->hideOverlay();
	} else if (prev == Roger::kModeOriginal) {
		// Original -> SideBySide: overlay comes back. _nativeBaseline went stale while
		// the overlay was off (onFrameEnd early-returns when hidden); force a
		// fresh snapshot on the next kernelAnimate before the SBS native panel reads it.
		_haveBaseline = false;
		if (_haveScene) { markFullDirty(); presentBarrier(); }
		reapplyStatus();
	} else {
		// Enhanced <-> SideBySide: overlay already shown, but the layout changes wholesale.
		if (_haveScene) { markFullDirty(); presentBarrier(); }
	}

	const char *name = _mode == Roger::kModeEnhanced ? "ENHANCED (upscaled)"
	                 : _mode == Roger::kModeOriginal ? "ORIGINAL (native)"
	                 : "SIDE-BY-SIDE (enhanced|original)";
	warning("ROGER: display mode -> %s", name);
}

void FileRogerArtProvider::toggleDebugLog() {
	_debugLog = !_debugLog;
	warning("ROGER: debug logging %s", _debugLog ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Live enhance-pass tuning helpers (used by the F12 tune panel's Apply)
// ---------------------------------------------------------------------------

void FileRogerArtProvider::regenInPlace() {
	if (!_assetGen || _loadedPicId < 0)
		return;
	const int saved = _loadedPicId;
	// pushHiresBackground treats every call as a room ENTRY and clears the
	// per-room captured sprites (_staticSprites / _initCels / _textSprites).
	// Those are captured once, at the room's actual entry draws Ã¢â‚¬â€ nothing can
	// re-capture them mid-room, so a live-tuning regen must carry them across
	// the call or every addToPic/init-baked prop (QFG1 signs, seated NPCs)
	// vanishes until the next real room change.
	Common::Array<Roger::Sprite> keepStatics = _staticSprites;
	Common::Array<Roger::Sprite> keepInitCels = _initCels;
	Common::Array<Roger::Sprite> keepText = _textSprites;
	// The copies above are shallow Ã¢â‚¬â€ _textSprites entries OWN their celOverride
	// surfaces and clearTextSprites() (inside pushHiresBackground) frees them.
	// Empty the source first so the clear frees nothing.
	_textSprites.clear();
	_loadedPicId = -1; // invalidate early-return guard in pushHiresBackgroundInternal
	// Internal variant: a regen must keep the current pic STACK (the public
	// pushHiresBackground would reset it to {saved}, collapsing any addTo
	// overlays out of the regenerated plate).
	pushHiresBackgroundInternal(saved);
	_staticSprites = keepStatics;
	_initCels = keepInitCels;
	_textSprites = keepText;
	markFullDirty();
	presentBarrier();
}

// -- DEBUG TOOL: in-game quick-tune panel (spec 2026-07-05) ------------------

void FileRogerArtProvider::markTunePanelDirty() {
	if (!_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) {
		_compositor->forceFullPresent();
		return;
	}
	Common::Rect d = Roger::sciRectToDest(Roger::tunePanelRect(_tunePanel.leftSide), _lastGameRect);
	d.grow(2); // absorb mapping rounding vs the drawn border
	_compositor->addDirtyRect(d);
}

void FileRogerArtProvider::toggleTunePanel() {
	// Enhanced mode only: original/side-by-side have no place to draw it.
	if (!enabled || _mode != Roger::kModeEnhanced || !_assetGen)
		return;
	_tunePanel.open = !_tunePanel.open;
	if (_tunePanel.open) {
		_tunePanel.debugLog = _debugLog;
		// Map the asset gen's current view variant to a panel slot (the nearest
		// sentinel -> the last slot).
		const int vv = _assetGen->viewVariant();
		_tunePanel.viewMode = (vv < 0) ? Roger::viewScalerCount() : vv;
		// enhancePasses() is always concrete here (the ctor seeds it from config).
		_tunePanel.stagedPasses = _assetGen->enhancePasses();
		_tunePanel.appliedPasses = _tunePanel.stagedPasses;
		Roger::tuneSeedPicModes(_tunePanel);                            // registry modes (once per session)
		Roger::tuneSelectOrAddMode(_tunePanel, _tunePanel.stagedPasses); // reflect the applied config
		if (_assetGen->plateNearest()) // nearest survives a close/reopen
			_tunePanel.picModeSel = (int)_tunePanel.picModes.size();
		_tunePanel.hoverId = 0;
		Roger::buildTunePanel(_tunePanel, _tuneWidgets);
	}
	debug("ROGER tunePanel: %s", _tunePanel.open ? "open" : "closed");
	markTunePanelDirty();
	presentBarrier();
}

void FileRogerArtProvider::tuneApplyViewMode() {
	if (!_assetGen)
		return;
	// The last panel slot is the nearest sentinel; earlier slots are registry
	// view-scaler indices.
	const int variant = Roger::tuneViewModeIsNearest(_tunePanel.viewMode)
	                        ? Roger::kViewScalerNearest
	                        : _tunePanel.viewMode;
	if (variant == _assetGen->viewVariant())
		return;
	_assetGen->setViewVariant(variant);
	if (_viewCache)
		_viewCache->clear();
	debug("ROGER tunePanel: view enhance -> %s (variant %d)",
	      Roger::tuneViewModeIsNearest(_tunePanel.viewMode) ? "nearest" : Roger::viewScaler(variant).id,
	      variant);
	// Sprites re-pull cels through the ViewCache next animate cycle; a full
	// present then restyles everything on screen (event-driven, not per-cycle).
	markFullDirty();
}

void FileRogerArtProvider::tuneApplyPicMode() {
	if (!_assetGen)
		return;
	const bool nearest = Roger::tunePicModeIsNearest(_tunePanel);
	const bool wasNearest = _assetGen->plateNearest();
	// A pass-mode slot loads its sequence into the staged list (the nearest
	// slot has no pass list and leaves the builder alone).
	if (!nearest && _tunePanel.picModeSel >= 0 && _tunePanel.picModeSel < (int)_tunePanel.picModes.size())
		_tunePanel.stagedPasses = _tunePanel.picModes[_tunePanel.picModeSel];
	const bool passesChanged = Roger::tunePending(_tunePanel);
	if (nearest == wasNearest && !passesChanged)
		return; // nothing to regenerate
	_assetGen->setPlateNearest(nearest);
	if (passesChanged)
		_assetGen->setEnhancePasses(_tunePanel.stagedPasses);
	// Mode juggling: nearest or off-config passes generate in memory - never
	// churn the disk cache. Back at the launch config (and not nearest):
	// restore the pre-tuning mode so room loads return to cache speed.
	const Common::Array<int> configPasses =
		Roger::effectivePasses(ConfMan.hasKey("roger_omyac_passes"),
		                       ConfMan.hasKey("roger_omyac_passes") ? ConfMan.get("roger_omyac_passes") : "");
	const bool offConfig = nearest || !Roger::tunePassesEqual(_tunePanel.stagedPasses, configPasses);
	if (!offConfig) {
		if (_tuneModeRemembered) {
			_assetGen->setMode(_tunePreTuneMode);
			_tuneModeRemembered = false;
		}
	} else if (_assetGen->mode() != Roger::kGenMemory) {
		_tunePreTuneMode = _assetGen->mode();
		_tuneModeRemembered = true;
		_assetGen->setMode(Roger::kGenMemory);
	}
	const uint32 t0 = g_system->getMillis();
	regenInPlace();
	_tunePanel.lastGenMs = g_system->getMillis() - t0;
	_tunePanel.appliedPasses = _tunePanel.stagedPasses;
	debug("ROGER tunePanel: applied %s (%u passes) in %ums",
	      nearest ? "nearest" : "pass mode",
	      (unsigned)_tunePanel.appliedPasses.size(), _tunePanel.lastGenMs);
}

bool FileRogerArtProvider::tunePanelMouse(bool buttonDown, const Common::Point &gamePos) {
	if (!_tunePanel.open || _mode != Roger::kModeEnhanced)
		return false;
	if (!Roger::tunePanelRect(_tunePanel.leftSide).contains(gamePos))
		return false; // outside: game plays on
	if (!buttonDown)
		return true; // swallow ups / right-clicks over the panel, no action
	const uint32 id = Roger::hitTestWidgets(_tuneWidgets, gamePos.x, gamePos.y);
	switch (Roger::widKind(id)) {
	case Roger::kTuneClose:     _tunePanel.open = false; break;
	case Roger::kTuneDebugLog:  // F11 mirror
		toggleDebugLog();
		_tunePanel.debugLog = _debugLog;
		break;
	case Roger::kTuneSide:
		markTunePanelDirty(); // vacate the CURRENT side before flipping
		_tunePanel.leftSide = !_tunePanel.leftSide;
		break; // post-switch markTunePanelDirty covers the new side
	case Roger::kTuneViewEnhance: // cycle view-scaler modes (registry + nearest); apply
		_tunePanel.viewMode = (_tunePanel.viewMode + 1) % Roger::tuneViewModeCount();
		tuneApplyViewMode();
		break;
	case Roger::kTunePicEnhance:  // cycle pass modes + trailing nearest; apply
		_tunePanel.picModeSel = (_tunePanel.picModeSel + 1) % Roger::tunePicModeCount(_tunePanel);
		tuneApplyPicMode();
		break;
	case Roger::kTuneChipAddF:  _tunePanel.stagedPasses.push_back(2); break; // append fill
	case Roger::kTuneChipAddL:  _tunePanel.stagedPasses.push_back(1); break; // append line
	case Roger::kTuneChipAddA:  _tunePanel.stagedPasses.push_back(0); break; // append all
	case Roger::kTuneClear:     _tunePanel.stagedPasses.clear(); break;
	case Roger::kTuneAdd: // register the built sequence as a pic-enhance mode + apply
		Roger::tuneSelectOrAddMode(_tunePanel, _tunePanel.stagedPasses);
		tuneApplyPicMode();
		break;
	default: break; // display-only chip / panel background: consumed, no action
	}
	Roger::buildTunePanel(_tunePanel, _tuneWidgets);
	markTunePanelDirty();
	presentBarrier();
	return true;
}

void FileRogerArtProvider::diagDumpState(const char *where) {
	if (!_diag)
		return;
	warning("ROGER-DIAG[%s]: pic=%d enabled=%d overlayShown=%d plate=%s haveScene=%d "
	        "compCacheValid=%d haveBaseline=%d uiElems=%u uiIcons=%u staticSprites=%u",
	        where, _loadedPicId, enabled ? 1 : 0, overlayShown() ? 1 : 0,
	        _plate ? "yes" : "NULL", _haveScene ? 1 : 0, _compositeCacheValid ? 1 : 0,
	        _haveBaseline ? 1 : 0,
	        _journal ? (unsigned)_journal->ops().size() : 0u,
	        (unsigned)_uiIcons.size(), (unsigned)_staticSprites.size());
}

void FileRogerArtProvider::onNativePicture() {
	diagDumpState("nativePic");
	if (_compositor)
		_compositor->setRoom(nullptr, nullptr);
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	if (_journal) _journal->clear();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_genericGlyphCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_drawCelNativeCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_staticSprites.clear();
	_initCels.clear();
	clearTextSprites();
	_genTextPending.clear(); // discard any pending generic text from the departing room
	_debugDumpedPic = -1;   // allow a fresh debug-capture dump for the next room
	_diffCheckedPic = -1;   // allow a fresh diff-check run for the next room
	_revealRects.clear();  // stale reveal suppressions must not bleed into the new room
	_genRegions.clear(); // drop any stale Feeder B rects from the departing room (drawGenericRegions won't run if _plate is null)
	_haveScene = false;
	_loadedPicId = -1;
	g_system->hideOverlay();
}

void FileRogerArtProvider::onPicture(GuiResourceId picId, bool addToFlag) {
	// The pre-gate that used to live in GfxPaint16::drawPicture: replace only when
	// enabled and we actually have replacement art for this pic. hasBackground checks
	// enabled itself, but keep the explicit gate here so a disabled provider produces
	// identical behavior (native render already ran; we add nothing).
	if (!enabled)
		return;
	if (hasBackground(picId)) {
		// prefetch was a no-op for the filesystem provider; pushHiresBackground*
		// generates + presents the plate synchronously (cache-warm in the common case).
		if (addToFlag)
			pushHiresBackgroundAddTo(picId);
		else
			pushHiresBackground(picId);
	} else if (!addToFlag) {
		// Full-screen room with no replacement: drop any stale overlay from the
		// previous room (Hard Constraint 6). addToPic overlays must not evict it.
		onNativePicture();
	}
}

void FileRogerArtProvider::onPictureAbsent() {
	// Reserved companion to onPicture (spec §4.2): the caller emits it where a
	// full-screen pic with no observer replacement is drawn. drawPicture routes that
	// case through onPicture (!addToFlag && !hasBackground -> onNativePicture) so this
	// direct entry point is a no-op for the filesystem provider; kept for observers
	// that split the two signals.
	if (!enabled)
		return;
	onNativePicture();
}

void FileRogerArtProvider::onMouseMoved() {
	// DEBUG TOOL: tune-panel hover tracking (game-space hit test).
	if (_tunePanel.open && _mode == Roger::kModeEnhanced) {
		const Common::Point mp = g_system->getEventManager()->getMousePos();
		const uint32 h = Roger::hitTestWidgets(_tuneWidgets, mp.x, mp.y);
		if (h != _tunePanel.hoverId) {
			_tunePanel.hoverId = h;
			markTunePanelDirty();
		}
	}

	if (_mode == Roger::kModeSideBySide) {
		// Re-present the split layout so the single composited cursor tracks the pointer.
		if (_haveScene)
			presentComparison();
		return;
	}
	if (_useHwCursor)
		return; // hardware cursor moves itself; no recomposite needed
	// The barrier's bounded path IS the cursor fast path now: it restores the old
	// cursor region from the composite cache, repaints at the pointer, and pushes
	// only the two cursor-sized rects.
	presentBarrier();
}

void FileRogerArtProvider::composeRoomScene(Graphics::ManagedSurface &out) {
	Common::Array<Roger::Sprite> none;
	composeRoomScene(out, none);
}

void FileRogerArtProvider::composeRoomScene(Graphics::ManagedSurface &out,
                                            const Common::Array<Roger::Sprite> &sprites) {
	// Reuse the same geometry renderFrame uses.
	const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
	const Common::Rect gameRect = currentGameRect(OW, OH);
	const Common::Rect picRect = Roger::computePictureRect(gameRect, _statusBarH);
	_compositor->setPictureDest(picRect);
	_compositor->renderScene(out, sprites, gameRect);
}

void FileRogerArtProvider::onTransition(int sciType, const Common::Rect & /*picRect*/, int blackoutSciType) {
	if (!_transitionsEnabled || !overlayShown() || !_compositor || !_plate)
		return;
	diagDumpState("transition");
	const Roger::TransitionFamily fam = Roger::transitionFamilyFor(sciType);
	if (fam == Roger::kFxNone && blackoutSciType < 0)
		return; // instant cut: the deferred first-frame present (existing path) handles it
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0)
		return;
	// `from` = the previous room's last composed scene (still in _sceneCache). If there
	// is none (first room of the session), fade up from black.
	Graphics::ManagedSurface from(OW, OH, rgba);
	if (_haveScene && _sceneCache && _sceneCache->w == OW && _sceneCache->h == OH)
		from.copyFrom(*_sceneCache);
	else
		from.fillRect(Common::Rect(0, 0, (int16)OW, (int16)OH), rgba.ARGBToColor(255, 0, 0, 0));
	// `to` = the new room background + the frame-1 cast. Native kernelAnimate
	// draws the cast (drawCels) BEFORE animateShowPic runs the transition, so the
	// native reveal already contains every frame-1 draw Ã¢â‚¬â€ revealing a sprite-less
	// plate here is the enhanced-only room-entry flash. At this point in the same
	// cycle the Feeder-A hooks have captured exactly that set: _staticSprites
	// (addToPic) + _initCels (every cast draw during _picNotValid, live actors
	// included Ã¢â‚¬â€ the steady-state live-owner promotion filter deliberately does
	// NOT apply to this one transient frame). onAnimateFrame recomposes
	// from the real animate list on the very next frame.
	Common::Array<Roger::Sprite> frame1;
	Roger::buildInitFrameSpriteSet(_staticSprites, _initCels, frame1);
	Common::Array<Graphics::Surface *> nativeSurfaces;
	for (uint i = 0; i < frame1.size(); i++) {
		if (frame1[i].celOverride)
			continue;
		// Skip the native fallback when a hires cel exists (renderScene would ignore it).
		if (_viewCache && _viewCache->getCel(frame1[i].viewId, frame1[i].loopNo, frame1[i].celNo))
			continue;
		Graphics::Surface *nativeSurf = renderNativeCel(frame1[i].viewId, frame1[i].loopNo, frame1[i].celNo);
		if (nativeSurf) {
			frame1[i].celOverride = nativeSurf;
			nativeSurfaces.push_back(nativeSurf);
		}
	}
	Graphics::ManagedSurface to(OW, OH, rgba);
	composeRoomScene(to, frame1);
	for (uint i = 0; i < nativeSurfaces.size(); i++) {
		nativeSurfaces[i]->free();
		delete nativeSurfaces[i];
	}
	if (_diag)
		warning("ROGER-DIAG[transition]: type=%d blackoutType=%d fam=%d frame1Sprites=%u",
		        sciType, blackoutSciType, (int)fam, (unsigned)frame1.size());
	// composeRoomScene pre-validates _bgCache; ensure the first post-transition renderFrame
	// does a full _compositeCache copy so the software-cursor fast path has clean pixels.
	_compositeCacheValid = false;
	Graphics::ManagedSurface &scratch = *scratchScene(OW, OH);
	// Skip the FX animation in side-by-side (it would present the non-split full-overlay
	// layout); the room-change bookkeeping below still runs, and the next frame's
	// presentComparison shows the split with the new room.
	if (_mode != Roger::kModeSideBySide) {
		if (blackoutSciType >= 0) {
			// Blackout form (SCI0 raw IDs 11-17): the original animates old -> BLACK
			// with the mirror type from blackoutTransitionIDs, then black -> new with
			// the requested type. A direct old->new morph here read as a foreign
			// "soft wipe" Ã¢â‚¬â€ the black interstitial is what gives the original its
			// pop. A kFxNone phase is an instant cut to/from black, exactly native.
			Graphics::ManagedSurface black(OW, OH, rgba);
			black.fillRect(Common::Rect(0, 0, (int16)OW, (int16)OH), rgba.ARGBToColor(255, 0, 0, 0));
			const Roger::TransitionFamily boFam = Roger::transitionFamilyFor(blackoutSciType);
			if (boFam != Roger::kFxNone)
				_compositor->runTransition(from, black, scratch, boFam,
				                           Roger::defaultDurationMs(boFam), blackoutSciType);
			if (fam != Roger::kFxNone)
				_compositor->runTransition(black, to, scratch, fam,
				                           Roger::defaultDurationMs(fam), sciType);
		} else {
			_compositor->runTransition(from, to, scratch, fam, Roger::defaultDurationMs(fam), sciType);
		}
	}
	// Leave _sceneCache holding the new background so the next kAnimate frame's dirty
	// present builds on it correctly.
	if (!_sceneCache || _sceneCache->w != OW || _sceneCache->h != OH) {
		delete _sceneCache;
		_sceneCache = new Graphics::ManagedSurface(OW, OH, rgba);
	}
	_sceneCache->copyFrom(to);
	_haveScene = true;

	// composeRoomScene() above pre-warmed the compositor's static-bg cache, which would let
	// the first post-transition renderFrame take the bounded-seed + dirty-present path using
	// dirty-rect history left over from the PREVIOUS room Ã¢â‚¬â€ the QFG1 fresh-start town
	// breakage (a real transition into pic 300; save-load reaches it via an instant cut and
	// is fine). Reset the compositor to a clean first frame (full seed + full present, stale
	// dirty rects dropped) so a transition-entry matches a save-restore/instant-cut entry.
	_compositor->resetForRoomChange();
}

void FileRogerArtProvider::onShake(int shakeCount, int directions) {
	if (!_transitionsEnabled || !overlayShown() || _mode == Roger::kModeSideBySide ||
	        !_compositor || !_haveScene || !_sceneCache)
		return; // side-by-side: pure FX, would present the non-split layout Ã¢â‚¬â€ skip
	const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0 || _sceneCache->w != OW || _sceneCache->h != OH)
		return;
	// Native SCI shake is ~10px of 200 rows; scale into overlay space.
	const int mag = (10 * OH) / 200;
	Graphics::ManagedSurface &scratch = *scratchScene(OW, OH);
	_compositor->runShake(*_sceneCache, scratch, shakeCount, directions, mag);
}

FileRogerArtProvider::~FileRogerArtProvider() {
	CursorMan.showMouse((g_sci && g_sci->_gfxCursor) ? g_sci->_gfxCursor->isVisible() : true);
	if (_inputDriver) {
		g_system->getEventManager()->getEventDispatcher()->unregisterSource(_inputDriver);
		delete _inputDriver;
		_inputDriver = nullptr;
	}
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	delete _assetGen; _assetGen = nullptr;
	delete _viewCache; _viewCache = nullptr;
	delete _compositor; _compositor = nullptr;
	delete _journal; _journal = nullptr;
	delete _textRenderer; _textRenderer = nullptr;
	delete _altTextRenderer; _altTextRenderer = nullptr;
	if (_sceneCache) { delete _sceneCache; _sceneCache = nullptr; }
	if (_scratchScene) { delete _scratchScene; _scratchScene = nullptr; }
	if (_sbsScratch) { delete _sbsScratch; _sbsScratch = nullptr; }
	if (_compositeCache) { delete _compositeCache; _compositeCache = nullptr; }
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_genericGlyphCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_drawCelNativeCache.clear(); // surfaces were owned by _uiIcons (freed above)
}

} // namespace Sci
