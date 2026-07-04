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
// ROGER_CYCLE_LOG) uses getenv() — same pattern as sci.cpp's ROGER_NO_LAUNCHER.
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sci/roger/file_roger_art_provider.h"
#include "sci/roger/roger_input.h"
#include "sci/roger/roger_selftest.h"
#include "sci/roger/roger_cursor.h"
#include "sci/roger/png_loader.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_palette_remap.h"
#include "sci/roger/roger_compositor.h"
#include "sci/roger/roger_coords.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_ui_layer.h"
#include "sci/roger/roger_text.h"
#include "sci/roger/view_cache.h"
#include "sci/roger/slice_set.h"
#include "sci/roger/roger_pic_parser.h"
// animate.h references these SCI engine types in GfxAnimate's interface but does
// not declare them itself. This translation unit includes animate.h (to iterate
// the AnimateList in renderFromAnimateList) without first pulling in the full
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
#include "graphics/managed_surface.h"
#include "graphics/paletteman.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"
#include "common/array.h"
#include "common/file.h"
#include "common/path.h"
#include "common/fs.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"

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

	// roger_autoshot: a verification-harness flag (off by default). When set, the
	// first composited frame of each room is dumped to a PNG (see renderFrame).
	// This is how the dev loop captures the hires overlay deterministically without
	// keystrokes/focus — injected Alt+s/F10 never reach SDL (Win32 menu keys).
	_autoshot = ConfMan.hasKey("roger_autoshot") && ConfMan.getBool("roger_autoshot");
	_selfTest = ConfMan.hasKey("roger_selftest") && ConfMan.getBool("roger_selftest");
	// roger_diff_backstop: Feeder B per-frame full-buffer pixel diff (default off). When off
	// snapshotNativeBaseline() returns immediately, keeping _haveBaseline false so the costly
	// 320x200 buffer read + 64K diff never runs. The bitsShow-hook path (onNativeShowRect) and
	// addToPic capture (Feeder A) remain on regardless.
	_diffBackstop = ConfMan.hasKey("roger_diff_backstop") && ConfMan.getBool("roger_diff_backstop");
	// roger_debug: per-frame + per-UI-element diagnostic logging (also toggled in-game
	// with Ctrl+Shift+L). Read it here so the documented config knob actually works.
	_debugLog = ConfMan.hasKey("roger_debug") && ConfMan.getBool("roger_debug");
	// roger_diag: revertible overlay-state trace at room/present/transition seams (off by default).
	// Env-first (ROGER_DIAG=1) so build_and_run.ps1 -Diag arms a single launch without editing
	// scummvm.ini — ini edits race against a running instance's config rewrite-on-exit; the ini
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
		if (!inputScript.empty() && !_inputDriver->loadScriptFile(inputScript))
			warning("ROGER-SCRIPT: script not loaded, running without: %s", inputScript.c_str());
		if (!inputLive.empty())
			_inputDriver->setLiveFile(inputLive);
		// Registered as a backend event source: due events flow through the normal
		// pollEvent path (dispatch drains sources on every poll — blocking dialogs
		// included). Not autoFree: we own it and unregister in the destructor.
		g_system->getEventManager()->getEventDispatcher()->registerSource(_inputDriver, false);
	}

	// Initial display mode (default Enhanced; F10 still cycles from wherever this
	// starts). Env-first so build_and_run.ps1 -Mode can pin a single launch for
	// evidence capture — e.g. -Mode sbs boots straight into Side-by-Side for an
	// enhanced-vs-native comparison shot with no F10 keypress choreography — without
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

	// Cursor: the native hardware cursor is NOT usefully visible over the in-game
	// OSystem overlay (verified in live play — it disappears), which is the original
	// reason Roger composites its own arrow into the overlay scene. So default to the
	// composited cursor. roger_hw_cursor=true opts back into the (currently invisible)
	// hardware cursor for experimentation. Default false.
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

	// roger_omyac_passes: three-state semantics —
	//   unset           => default sequence (defaultPasses())
	//   set to ""       => wireframe (empty array = zero passes)
	//   set to tokens   => parsed list (fill/f=2, line/l=1, all/a=0)
	_assetGen->setEnhancePasses(
		parseOmyacPasses(ConfMan.hasKey("roger_omyac_passes"),
		                 ConfMan.hasKey("roger_omyac_passes") ? ConfMan.get("roger_omyac_passes") : "")
	);

}

bool FileRogerArtProvider::isOverlayVisible() const {
	return overlayShown();
}

bool FileRogerArtProvider::hasBackground(GuiResourceId pictureId) const {
	if (!enabled)
		return false;
	// No per-frame view-type check here — it stays off the hot render path. A non-EGA
	// game that slips past the launcher's add-time VGA block is caught once on its first
	// pushHiresBackground(), which disables the overlay (enabled=false) so we never reach
	// here again for it.
	return _assetGen && _assetGen->mode() != Roger::kGenPrebuilt;
}

void FileRogerArtProvider::precacheAll() {
	// One-time startup warm-up. Only runs in a generating mode (prebuilt mode has
	// nothing to cache). roger_precache selects the scope: all|pics|views|off
	// (default all when the key is unset). Generic: no per-game table — we ask the
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

	// EGA SCI0 only — by design, permanently. VGA/SCI1 is out of scope (not deferred);
	// the omyac pipeline is EGA-specific. Reject cleanly and fall back to native render.
	if (resMan->getViewType() != kViewEga) {
		warning("ROGER: VGA game detected — Roger art replacement supports EGA games only. Overlay disabled.");
		enabled = false;
		return;
	}

	uint32 t0 = g_system->getMillis();

	if (doPics) {
		Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
		const int total = (int)pics.size();
		int done = 0, skipped = 0;
		const bool isEga = (resMan->getViewType() == kViewEga);
		warning("ROGER precache: warming %d pic plates (mode=%d)...", total, (int)_assetGen->mode());
		for (Common::List<ResourceId>::const_iterator it = pics.begin(); it != pics.end(); ++it) {
			GuiResourceId id = (GuiResourceId)it->getNumber();

			// Skip non-EGA pics — Roger only processes EGA pics via omyac.
			Resource *picRes = resMan->findResource(ResourceId(kResourceTypePic, (uint16)id), false);
			if (picRes && picRes->size() >= 2) {
				const Roger::PicFormat picFmt = Roger::picResourceFormat(
					picRes->data(), (uint32)picRes->size(), isEga);
				if (picFmt != Roger::kPicSci0Ega) {
					++skipped;
					continue;
				}
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
		warning("ROGER precache: %d pic plates warmed, %d non-EGA skipped", done, skipped);
	}

	if (doViews && g_sci->_gfxCache) {
		Common::List<ResourceId> views = resMan->listResources(kResourceTypeView);
		int warmed = 0;
		for (Common::List<ResourceId>::const_iterator it = views.begin(); it != views.end(); ++it) {
			const int viewId = it->getNumber();
			GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
			if (!view)
				continue; // missing/malformed view -> skip (Hard Constraint 6)
			// Snapshot loop/cel counts NOW, while 'view' is valid. generateViewCel()
			// below calls GfxCache::getView(), which purges the WHOLE view cache when
			// it is full (cache.cpp) — that frees this 'view' pointer. Dereferencing
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
					uint32 ms = 0;
					Graphics::Surface *s = _assetGen->generateViewCel(viewId, lp, cl, ms);
					if (s) { s->free(); delete s; } // cache mode wrote it; discard the surface
					++warmed;
				}
			}
		}
		warning("ROGER precache: %d view cels warmed", warmed);
	}

	warning("ROGER precache: done in %u ms total", g_system->getMillis() - t0);
}

bool FileRogerArtProvider::precacheOnePic(GuiResourceId picId, uint32 &ms) {
	if (!_assetGen || _assetGen->mode() == Roger::kGenPrebuilt) return false;
	// Skip non-EGA pics — only EGA pics have the omyac path.
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
			uint32 ms = 0;
			Graphics::Surface *s = _assetGen->generateViewCel(viewId, lp, cl, ms);
			if (s) { s->free(); delete s; }
		}
	}
	return true;
}

void FileRogerArtProvider::pushHiresBackground(GuiResourceId pictureId) {
	// EGA SCI0 only — by design, permanently. VGA/SCI1 is out of scope (not deferred);
	// the omyac pipeline is EGA-specific. Reject cleanly and fall back to native render.
	// The launcher blocks VGA games at add-time, but a target configured another way
	// (manual ConfMan / normal ScummVM launcher) can still reach here. Setting enabled = false
	// makes hasBackground() return false from here on, so this fires once per engine instance.
	if (g_sci && g_sci->getResMan() && g_sci->getResMan()->getViewType() != kViewEga) {
		warning("ROGER: not an EGA SCI game - Roger art replacement supports EGA games only. "
		        "Disabling the hires overlay.");
		enabled = false;
		if (_compositor)
			_compositor->setRoom(nullptr, nullptr);
		_loadedPicId = -1;
		return;
	}

	if (_loadedPicId == pictureId && _plate)
		return; // already loaded for this room

	diagDumpState("pushBG-enter");

	// New room: drop the previous room's captured addToPic cels (Feeder A) + init-baked cels.
	_staticSprites.clear();
	_initCels.clear();
	clearTextSprites();
	_genTextPending.clear(); // discard any pending generic text from the departing room
	_debugDumpedPic = -1;   // allow a fresh debug-capture dump for this room
	_diffCheckedPic = -1;   // allow a fresh diff-check run for this room
	// New room must fully refresh the cursor-restore cache; the transition path pre-validates
	// _bgCache via composeRoomScene so the first renderFrame may not be a full-seed.
	_compositeCacheValid = false;

	uint32 tEnter = g_system->getMillis();

	// New room: drop any dialogs/icons left from the previous room so they do not
	// bleed onto the new scene. _haveScene is rebuilt by the next renderFrame.
	if (_uiLayer) _uiLayer->clearAll();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_genericGlyphCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_drawCelNativeCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_haveScene = false;

	// Evict previous room.
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }

	uint32 genMs = 0;
	_plate = nullptr;
	const char *plateSrc = "none";
	uint32 tAcq0 = g_system->getMillis();
	if (_assetGen && _assetGen->mode() != Roger::kGenPrebuilt) {
		_plateIndex.clear();
		_plate = _assetGen->generatePlateWithIndex(pictureId, _plateIndex, genMs);
		if (_plate)
			plateSrc = genMs ? "generated(miss)" : "cache-hit";
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
	int prW = 0, prH = 0;
	_priorityMap.clear();
	uint32 occGenMs = 0;
	uint32 tOcc0 = g_system->getMillis();
	bool haveOcc = (_assetGen && _assetGen->mode() != Roger::kGenPrebuilt &&
	                _assetGen->generatePriorityMap(pictureId, _priorityMap, prW, prH, occGenMs));
	uint32 occMs = g_system->getMillis() - tOcc0;

	// _picW x _picH is the SCI picture window (320x190); sprite cel rects live in that
	// space, so picH stays 190 regardless of the priority map's hires resolution. (The
	// old code passed prH here only because native priorityBands also returned 190.)
	_compositor->setPicture(320, 190, 0);
	if (haveOcc)
		_compositor->setPriorityMask(_priorityMap.begin(), prW, prH); // hires bands (1920x1140)
	else
		_compositor->setPriorityMask(nullptr, 0, 0); // no bands -> sprites draw without occlusion
	_loadedPicId = pictureId;

	// Snapshot the room-load EGA palette for live re-apply. The first 16 OSystem palette
	// entries are the EGA base colors in SCI0 (GfxPalette16::setEGA fills them at indices
	// 0..15). grabPalette(buf, start, count) fills count*3 RGB bytes.
	g_system->getPaletteManager()->grabPalette(_palSnapshot, 0, 16); // 16 colors = 48 bytes
	_haveSnapshot = true;

	// Re-push the cached score/title banner into the UI layer so it is enhanced again
	// after the room change (the game only redraws status on score/text change). The
	// present is deferred to the first kAnimate frame (presentWithUi no-ops until the
	// scene is composited) — presenting the sprite-less plate here flashed a wrong
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
			const bool aspect = g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection);
			const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
			const Common::Rect gameRect = Roger::computeGameRect(OW, OH, aspect);
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
	// mis-recolors them — on a large palette change the whole plate turns to garbage.
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
	// centered, aspect-preserving sub-rect. Replicate that placement so the overlay
	// lines up 1:1 (no shift when toggled), reserving the top status-bar strip so the
	// native "Score:" bar shows through the (transparent) overlay there.
	const bool aspectCorrected = g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection);
	const Common::Rect gameRect = Roger::computeGameRect(OW, OH, aspectCorrected);
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
	// full-seed frame (rebuild/transition/shake/heal — lastSceneWasFull), and when generic
	// regions (inventory/close-up upscales) were drawn outside the sprite union — so the cache
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
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, gameRect, _textRenderer, _altTextRenderer);
	}
	// Snapshot scene+UI (no cursor) — the barrier's bounded path patches and
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
	_frameJustComposed = true; // presentBarrier() (the renderFromAnimateList tail) presents this frame
}

void FileRogerArtProvider::maybeScriptCapture(Graphics::ManagedSurface &scene,
                                              const Common::Rect &gameRect) {
	// Scripted `capture <label>`: one-shot dump at the next present after the
	// command's due time. Reuses the autoshot writer (same screenshotpath +
	// naming: roger-<pic>-<label>-{overlay,preview}.png). O(1) when idle.
	if (!_inputDriver)
		return;
	Common::String label;
	if (_inputDriver->takeCaptureRequest(label))
		dumpAutoshot(scene, gameRect, ("-" + label).c_str());
}

void FileRogerArtProvider::dumpAutoshot(Graphics::ManagedSurface &scene,
                                        const Common::Rect &gameRect, const char *suffix) {
	// Output goes to the configured screenshotpath. Two PNGs:
	//   roger-<id><suffix>-overlay.png — Roger's composited layer alone (letterbox +
	//                            status strip are transparent, shown as black by a viewer)
	//   roger-<id><suffix>-preview.png — the true on-screen result: the native 320x200
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
		warning("ROGER: autoshot wrote %s-overlay.png", base.c_str());

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
				warning("ROGER: autoshot wrote %s-preview.png", base.c_str());
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
		if (_uiLayer) {
			const Common::Array<Roger::UiElement> &es = _uiLayer->elements();
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
	// the same helper used by roger_autoshot via dumpAutoshot).
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
			// RGBA32 format: aShift=0 → alpha is in bits 7..0 (lowest byte).
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
	// CLUT8 (index 0 transparent, 1 black, 2 white) — the same proven cursor path the
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

	// RGBA with straight alpha — Roger composites this into its own overlay scene
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

void FileRogerArtProvider::buildCursorForShape(int cursorId) {
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	_cursorHotspot = Common::Point(2, 2); // fallback: arrow hotspot if resource missing

	if (!g_sci || !g_sci->getResMan() || cursorId < 0)
		return;

	Resource *res = g_sci->getResMan()->findResource(
		ResourceId(kResourceTypeCursor, (uint16)cursorId), false);
	if (!res || (int)res->size() != 68)
		return;

	Common::Point hs;
	_cursorSurf = Roger::decodeSci0Cursor(res->data(), (int)res->size(), hs);
	_cursorHotspot = hs;
	_compositeCacheValid = false; // cursor surface changed -> next present is full rebuild
}

void FileRogerArtProvider::buildCursorFromView(int viewId, int loopNo, int celNo) {
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	_cursorHotspot = Common::Point(0, 0);

	if (!g_sci || !g_sci->_gfxCache) return;
	GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
	if (!view) return;

	const CelInfo *ci = view->getCelInfo((int16)loopNo, (int16)celNo);
	if (!ci) return;
	const int16 w = ci->width, h = ci->height;
	const int16 dx = ci->displaceX, dy = ci->displaceY;

	Graphics::Surface *native = renderNativeCel(viewId, loopNo, celNo);
	if (!native) return;

	const int kScale = 5;
	const int W = native->w * kScale, H = native->h * kScale;
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
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
	// Hotspot from VIEW cel metadata (matches GfxCursor::kernelSetView formula), scaled.
	_cursorHotspot = Common::Point(
		(int)(w / 2 - dx) * kScale,
		(int)(h - dy - 1) * kScale
	);
	_compositeCacheValid = false;

	native->free(); delete native;
}

void FileRogerArtProvider::compositeCursor(Graphics::ManagedSurface &scene,
                                           const Common::Rect &gameRect) {
	// The native OS cursor is invisible over the OSystem overlay, so draw our own
	// arrow into the overlay scene at the mouse position.
	const Common::Rect dst = cursorDstRect(gameRect);
	if (dst.isEmpty())
		return;
	scene.blendBlitFrom(*_cursorSurf, Common::Rect(0, 0, _cursorSurf->w, _cursorSurf->h), dst);
	if (_compositor)
		_compositor->addDirtyRect(dst); // cursor moved here this frame (dirty-rect present)
	_lastCursorDstRect = dst; // fast path uses this to restore the old cursor region
}

void FileRogerArtProvider::ensureUi() {
	if (!_uiLayer)
		_uiLayer = new Roger::RogerUiLayer();
	if (!_textRenderer) {
		// Default to a monospace TTF that ships in ScummVM's fonts.dat: the fixed-width
		// DOS/terminal look matches SCI0's native bitmap font far better than a
		// proportional sans (judged in-game via the Ctrl+Shift+F cycle). Override with
		// roger_ui_font; per-game targets can each set their own.
		Common::String ttf = "GoMono-Regular.ttf";
		if (ConfMan.hasKey("roger_ui_font"))
			ttf = ConfMan.get("roger_ui_font");
		// A ladder of pixel sizes for fit-to-box selection (cell mode, hires).
		Common::Array<int> sizes;
		sizes.push_back(18); sizes.push_back(24); sizes.push_back(32);
		sizes.push_back(42); sizes.push_back(56); sizes.push_back(72);
		sizes.push_back(96); sizes.push_back(120); sizes.push_back(160);
		_textRenderer = new Roger::RogerTextRenderer(ttf, sizes);
		// roger_ui_font_scale: global size multiplier (percent) applied to each element's
		// target cell height. Wrapping text fits its box by height, so a larger scale grows
		// (and re-wraps) the text rather than clipping. Default 150.
		int scale = 150;
		if (ConfMan.hasKey("roger_ui_font_scale"))
			scale = ConfMan.getInt("roger_ui_font_scale");
		_textRenderer->setGlobalScale(scale);
		if (!_textRenderer->ttfLoaded())
			warning("ROGER: dialog font '%s' did NOT load from fonts.dat — using bitmap fallback", ttf.c_str());
	}
	if (!_altTextRenderer) {
		// Header (score/title banner) + menus use a distinct, more modern font.
		Common::String headerTtf = "NotoSans-Regular.ttf";
		if (ConfMan.hasKey("roger_ui_header_font"))
			headerTtf = ConfMan.get("roger_ui_header_font");
		Common::Array<int> sizes;
		sizes.push_back(18); sizes.push_back(24); sizes.push_back(32);
		sizes.push_back(42); sizes.push_back(56); sizes.push_back(72);
		sizes.push_back(96); sizes.push_back(120); sizes.push_back(160);
		_altTextRenderer = new Roger::RogerTextRenderer(headerTtf, sizes);
		// Same global size multiplier so headings scale with the body text.
		int scale = 150;
		if (ConfMan.hasKey("roger_ui_font_scale"))
			scale = ConfMan.getInt("roger_ui_font_scale");
		_altTextRenderer->setGlobalScale(scale);
		if (!_altTextRenderer->ttfLoaded())
			warning("ROGER: header font '%s' did NOT load from fonts.dat — using bitmap fallback", headerTtf.c_str());
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
	// cache — and the OSystem overlay was reallocated to the new size on resize.
	// Pushing the stale (now over-sized) cache to the smaller overlay asserts in the
	// backend (copyRectToTexture bounds check) → crash. Rescale the cached scene to the
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
		const bool aspect = g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection);
		_lastGameRect = Roger::computeGameRect(OW, OH, aspect);
		_compositeCacheValid = false;
		_lastCursorDstRect = Common::Rect(); // position was in old overlay space; invalid
	}
	byte pal[256 * 3];
	g_system->getPaletteManager()->grabPalette(pal, 0, 256);
	ensureCompositeCache(OW, OH); // invalidates _compositeCacheValid on (re)alloc
	Graphics::ManagedSurface &scene = *scratchScene(_sceneCache->w, _sceneCache->h);
	const Common::Rect fullR(0, 0, (int16)OW, (int16)OH);

	// The .rin capture and the -ui autoshot dump read the WHOLE present source,
	// so those presents need a fully composed frame — and so does a present that
	// presentToOverlay will decide to push FULL (heal frame / dirty-present off /
	// bg rebuild): the bounded path only makes the pushed regions valid.
	const bool needFullSource = (_inputDriver && _inputDriver->capturePending()) || _autoshot ||
	                            _compositor->nextPresentIsFull();

	if (_compositeCacheValid && !needFullSource && _mode != Roger::kModeSideBySide) {
		// §3.3 region-bounded recompose: patch the composite cache only inside the
		// dirty union, then source the present from it. No full-frame copy, no
		// full UI re-render — this is the latency win at dialog time.
		Common::Array<Common::Rect> regions;
		_compositor->dirtyUnion(fullR, regions);
		if (!regions.empty()) {
			if (_uiLayer && !_uiLayer->empty() && _textRenderer)
				_compositor->patchCompositeRegions(*_compositeCache, *_sceneCache,
				                                   _uiLayer->elements(), regions, pal,
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
		// re-rendered elements were unchanged there) — pushing `regions` suffices.
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
		compositeCursor(scene, _lastGameRect); // paints + addDirtyRect + _lastCursorDstRect
		_compositor->presentToOverlay(scene);
		maybeScriptCapture(scene, _lastGameRect); // guaranteed no-op (needFullSource)
		return;
	}

	// Legacy full path: rebuild scene+UI wholesale. Runs on room/geometry/F10/font
	// changes, resize, sbs mode, hw-cursor-invalidated caches, capture/autoshot.
	scene.copyFrom(*_sceneCache); // fully overwrites the scratch buffer
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		// Diagnostic dump of the UI element rects (roger_debug), throttled to one dump per
		// distinct dialog (signature over token/rect/type) so it does not spam per frame.
		if (_debugLog) {
			const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
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
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, _lastGameRect, _textRenderer, _altTextRenderer);
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

	// Verification harness: when a dialog is composited, also dump a -ui snapshot.
	// Throttled to one dump per distinct UI state (a cheap signature over the layer)
	// so a banner/dialog that re-presents every frame doesn't rewrite the PNG in a
	// tight loop. The -ui preview overlays the native dialog under the hires one.
	if (_autoshot && _uiLayer && !_uiLayer->empty()) {
		uint32 sig = 2166136261u; // FNV-1a over the element fields that affect the image
		const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
		for (uint i = 0; i < els.size(); i++) {
			const Roger::UiElement &e = els[i];
			sig = (sig ^ (uint32)e.token) * 16777619u;
			sig = (sig ^ (uint32)(e.nativeRect.left * 31 + e.nativeRect.top)) * 16777619u;
			sig = (sig ^ (uint32)(e.type * 7 + e.textRole)) * 16777619u;
			for (uint c = 0; c < e.text.size(); c++)
				sig = (sig ^ (byte)e.text[c]) * 16777619u;
		}
		if (sig != _lastUiSig) {
			_lastUiSig = sig;
			dumpAutoshot(scene, _lastGameRect, "-ui");
		}
	}
}

void FileRogerArtProvider::markUiDirty(const Common::Rect &nativeRect) {
	if (!overlayShown() || !_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	_compositor->addDirtyRect(Roger::uiPaintExtent(nativeRect, _lastGameRect));
}

void FileRogerArtProvider::markVacatedDirty(const Common::Rect &nativeRect) {
	if (!overlayShown() || !_compositor)
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	// Exact rect + TTF pad. The compositor-overdraw ring beyond it is covered by
	// bitsRestore's exact erase rect (markNativeDirty in onNativeEraseRect) — the
	// spec's §3.1 claim; the gate proves it (see the fallback note in the plan).
	_compositor->addDirtyRect(Roger::uiVacatedExtent(nativeRect, _lastGameRect));
}

void FileRogerArtProvider::markNativeDirty(const Common::Rect &nativeRect) {
	// §3.1 exact invalidation: SCI touched these native pixels. grow(1) native
	// absorbs integer-scaler rounding differences vs the sprite-path mapper.
	// O(1) accumulate; NEVER presents.
	if (!overlayShown() || !_compositor || nativeRect.isEmpty())
		return;
	// The status-bar strip (rows 0.._statusBarH) is TRANSPARENT in the Roger overlay
	// (computePictureRect reserves it for the native score to show through). Marking it
	// dirty is a no-op visually but inflates the dirty area on every bitsShow tick,
	// since SCI re-blits the native score row every cycle. Clip to the picture region.
	// Invariant: Roger overlay content in the status strip is signalled only via
	// markUiDirty (uiPushStatus banner) — a markNativeDirty rect wholly inside the
	// strip is safe to drop; violating that would silently lose marks here.
	Common::Rect pic = nativeRect;
	pic.clip(Common::Rect(0, _statusBarH, 320, 200));
	if (pic.isEmpty())
		return;
	_barrierDirty = true;
	if (_lastGameRect.isEmpty()) { _compositor->forceFullPresent(); return; }
	Common::Rect n = pic;
	n.grow(1);
	_compositor->addDirtyRect(Roger::sciRectToDest(n, _lastGameRect));
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
	const int ox = gameRect.left + mp.x * gameRect.width() / 320;
	const int oy = gameRect.top + mp.y * gameRect.height() / 200;
	return Common::Rect(ox - _cursorHotspot.x, oy - _cursorHotspot.y,
	                    ox - _cursorHotspot.x + _cursorSurf->w,
	                    oy - _cursorHotspot.y + _cursorSurf->h);
}

void FileRogerArtProvider::presentBarrier() {
	// The single gated present (spec §3.2). Every skip path below is O(1).
	if (_inAnimateCycle)
		return; // mid-cycle marks accumulate; the end-of-cycle call flushes them
	if (!overlayShown() || !_compositor || !_haveScene || !_sceneCache)
		return;
	if (_frameJustComposed && _scratchScene) {
		// Per-cycle present: renderFrame just composed scene+UI into _scratchScene
		// and refreshed the caches — present that frame directly. No recompose.
		_frameJustComposed = false;
		_barrierDirty = false;
		Graphics::ManagedSurface &scene = *_scratchScene;
		if (_mode == Roger::kModeSideBySide) {
			presentComparison();
		} else {
			compositeCursor(scene, _lastGameRect);
			_compositor->presentToOverlay(scene);
		}
		if (_autoshot && _autoshotPicId != _loadedPicId) {
			dumpAutoshot(scene, _lastGameRect, "");
			_autoshotPicId = _loadedPicId;
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
	presentWithUi();
}

void FileRogerArtProvider::presentComparison() {
	if (_mode != Roger::kModeSideBySide || !_compositor)
		return;
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0)
		return;

	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::ManagedSurface &out = *scratchScene(OW, OH);
	out.clear(out.format.ARGBToColor(255, 0, 0, 0)); // opaque black letterbox

	Common::Rect leftF, rightF;
	Roger::comparePanelRects(OW, OH, leftF, rightF);

	// Left panel: the ENHANCED composite. _compositeCache holds scene + UI (no cursor),
	// rebuilt by renderFrame/presentWithUi — so dialogs/narration/banners show here too.
	// Fall back to _sceneCache (scene, no UI) when the composite cache isn't valid.
	Graphics::ManagedSurface *leftSrc = (_compositeCacheValid && _compositeCache) ? _compositeCache
	                                  : (_haveScene ? _sceneCache : nullptr);
	if (leftSrc)
		Roger::scaleBlitNearest(*out.surfacePtr(), leftF, *leftSrc->surfacePtr());

	// Right panel: the ORIGINAL native frame. Read the pre-erase snapshot (_nativeBaseline,
	// captured at kernelAnimate's snapshot point before restoreAndDelete) so the animating
	// cast (ego/moving views) is present — the live visual buffer has it erased by now.
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
			out.blendBlitFrom(*_cursorSurf, Common::Rect(0, 0, _cursorSurf->w, _cursorSurf->h), dst);
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
			_uiIcons.push_back(g); // owned; freed on room change / uiClearAll
			Roger::UiGlyph ug; ug.ch = c; ug.surf = g;
			out.push_back(ug);
		}
	}
}

void FileRogerArtProvider::uiPushWindow(const Common::Rect &r, int backColor, int penColor,
                                        uint16 wndStyle, uint32 token) {
	if (!overlayShown() || !_plate) return; // no hires scene -> leave native UI visible
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiWindow; e.nativeRect = r;
	// Faithful fill: a window that is transparent (bit 0) or USER-backed (bit 7 = 0x80,
	// i.e. a picture-backed port whose content is drawn by scripts/controls directly onto
	// the game picture — the QFG1 character-creation sheet) must not paint an opaque box
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
	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushText(const Common::Rect &r, const char *text, int penColor,
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
	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushButton(const Common::Rect &r, const char *text, int fontId,
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
	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushTextEdit(const Common::Rect &r, const char *text, int fontId,
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
	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushIcon(const Common::Rect &r, int viewId, int loopNo, int celNo,
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
	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::onDrawCel(const Common::Rect &r, int viewId, int loopNo, int celNo) {
	if (!overlayShown() || !_plate || !_viewCache) return;
	const Graphics::Surface *hi = _viewCache->getCel(viewId, loopNo, celNo);
	ensureUi();
	// NOTE: do NOT clearToken here. push() replaces by (type,token,rect), so distinct-rect
	// cels (e.g. the 13 stat graphics on the QFG1 char sheet) coexist in the layer, and a
	// redraw at the SAME rect replaces in place. Clearing the shared token at the start of
	// every call would erase the previous cel, leaving only the last one visible.
	const uint32 tok = 0x50000000u;

	Roger::UiElement e;
	e.type = Roger::kUiIcon; e.nativeRect = r; e.token = tok;

	if (hi) {
		e.iconSurface = hi; // borrowed from the ViewCache (hires path)
	} else {
		// No hires art: fall back to a rendered native cel so it stays visible under
		// the opaque overlay. beginNativeDraw suppressed bitsShow, so Feeder B won't
		// pick this up — we must inject it here. renderNativeCel already bakes mirroring.
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
		e.iconSurface = surf; // borrowed from cache; UiLayer borrows
	}

	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::uiPushStatus(const Common::Rect &r, const char *text, int fontId,
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
	_uiLayer->clearToken(token);

	// Opaque bar (matches the native menu/status strip), no frame, full width.
	Roger::UiElement bar;
	bar.type = Roger::kUiWindow; bar.nativeRect = r; bar.backColor = backColor;
	bar.penColor = penColor; bar.style = 2; bar.token = token;
	_uiLayer->push(bar);

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
	_uiLayer->push(e);
	markUiDirty(r);
	presentBarrier();
}

void FileRogerArtProvider::reapplyStatus() {
	if (_haveStatus)
		uiPushStatus(_statusRect, _statusText.c_str(), _statusFont, _statusPen, _statusBack, _statusToken,
		             _statusNativeFontH, _statusNativeTextW);
}

// Generic text-out captures live in the 0x6------- namespace. The low bits carry the
// window/port id the text was drawn in (0x60000000 | port->id), so a window dispose
// (GfxPorts::removeWindow -> uiClearToken(0x60000000 | id)) drops exactly that window's
// text — the same lifetime controls16/menu text already has. Text drawn on the picture
// port (no dialog / char screen while open) uses that port's id, which is never disposed
// mid-room, so it persists until room change. GENERIC_TEXT_TOKEN is the namespace base
// (matches picture-port id 0 fallback and is the value passed to the namespace helpers).
static const uint32 GENERIC_TEXT_TOKEN = 0x60000000u;
static const uint32 GENERIC_TEXT_MASK = 0xF0000000u;
static inline bool isGenericTextToken(uint32 t) { return (t & GENERIC_TEXT_MASK) == GENERIC_TEXT_TOKEN; }

void FileRogerArtProvider::uiClearToken(uint32 token) {
	// SCI calls this from bitsRestore for every save-under region it restores — which, while
	// walking, is ~2× per updated sprite EVERY game cycle, almost always for a token that matches
	// no UI element. presentWithUi() is a full-overlay re-present (copy + convert + copyRectToOverlay)
	// and the cache-invalidate forces a full recompose next frame, so an unconditional present here
	// dominated the cycle (~196 ms — the game ran ~2.7× slow). Only invalidate + present when an
	// element was actually removed (e.g. a dialog/look-at dismissal); otherwise this is a no-op.
	Common::Array<Common::Rect> removedRects;
	const bool removedUi = _uiLayer && _uiLayer->clearToken(token, &removedRects);

	// Window dispose also kills that window's Feeder B pixel captures (controls namespace
	// 0x40000000 | window id) — both the not-yet-processed pending regions (queued while a
	// blocking window froze the animate cycle; processing them after dispose would stamp the
	// restored native background over the plate) and the persistent stamps already created
	// (conversation portraits etc. must vanish with their window). Cheap when nothing is
	// tagged: bitsRestore's per-cycle handle tokens have small segments (top nibble 0), so
	// they don't enter this branch. Removed stamp rects join removedRects for the dirty pass.
	bool removedStamps = false;
	if ((token & 0xF0000000u) == 0x40000000u) {
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
		if (_diag && (token & GENERIC_TEXT_MASK) == GENERIC_TEXT_TOKEN)
			warning("ROGER-DIAG[clearToken]: tok=0x%08x removed=0 (no match)", token);
		return;
	}
	if (_diag && (token & GENERIC_TEXT_MASK) == GENERIC_TEXT_TOKEN)
		warning("ROGER-DIAG[clearToken]: tok=0x%08x removed=1", token);
	// Dirty the overlay regions the removed elements occupied so the barrier's
	// present repaints them with clean background (else they ghost until another
	// draw touches them). markVacatedDirty centralizes the extent math.
	for (uint i = 0; i < removedRects.size(); i++)
		markVacatedDirty(removedRects[i]);
	presentBarrier();
}

void FileRogerArtProvider::onNativeEraseRect(const Common::Rect &nativeRect) {
	// §3.1 exact invalidation: the restored save-under rect, straight from SCI.
	// This is what makes the SQ3 white-line class structurally dead — the region
	// is invalidated no matter what any element bookkeeping thought was there.
	markNativeDirty(nativeRect);
	if (!overlayShown() || !_plate || !_uiLayer || nativeRect.isEmpty()) {
		presentBarrier(); // mid-cycle: defers; frozen-cycle: flushes the mark
		return;
	}
	// Remove persisted generic text whose box lies within the erased region.
	// No early-out on !removed — the barrier must always fire to flush the
	// markNativeDirty above (bitsRestore walking storm: barrier defers mid-cycle,
	// so no per-hook present; the deferral, not a token match, guards the cycle).
	bool removed = false;
	const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
	Common::Array<Roger::UiElement> kept;
	Common::Array<Common::Rect> droppedRects;
	for (uint i = 0; i < els.size(); i++) {
		if (isGenericTextToken(els[i].token) && nativeRect.contains(els[i].nativeRect))
			{ removed = true; droppedRects.push_back(els[i].nativeRect); continue; }
		kept.push_back(els[i]);
	}
	if (removed) {
		_uiLayer->clearAll();
		for (uint i = 0; i < kept.size(); i++)
			_uiLayer->push(kept[i]);
		for (uint i = 0; i < droppedRects.size(); i++)
			markVacatedDirty(droppedRects[i]);
		if (_diag)
			warning("ROGER-DIAG[eraseText]: rect=(%d,%d,%d,%d) remaining=%u",
			        nativeRect.left, nativeRect.top, nativeRect.right, nativeRect.bottom, (unsigned)kept.size());
	}
	presentBarrier();
}

void FileRogerArtProvider::uiClearAll() {
	if (_uiLayer) _uiLayer->clearAll();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_genericGlyphCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_drawCelNativeCache.clear(); // surfaces were owned by _uiIcons (freed above)
	if (overlayShown() && _plate) { markFullDirty(); presentBarrier(); }
}

// Fixed token for the kGraphFrameBox selection highlight. Room-scoped: cleared by
// uiClearAll (called on every room change and onNativePicture). A single constant
// token means each new push calls clearToken() first, so the highlight tracks
// movement without accumulating stale elements even when the rect changes.
static const uint32 FRAME_BOX_TOKEN = 0x70000000u;

void FileRogerArtProvider::uiPushFrameBox(const Common::Rect &r, int penColor) {
	if (!overlayShown() || !_plate) return; // no hires scene — leave native highlight visible
	ensureUi();
	// Gate: if the frame element under FRAME_BOX_TOKEN is already identical (same rect
	// + same color), skip the clear/push/invalidate/present cycle entirely. This prevents
	// a per-cycle present storm when kernelDrawText fires on every control redraw (TAB,
	// hover, any redraw) while the selection has not actually moved or changed color.
	// Per CLAUDE.md per-cycle discipline: only mutate + present when the frame changed.
	const Common::Array<Roger::UiElement> &elems = _uiLayer->elements();
	Common::Rect oldFrameRect; // empty when no existing frame element
	for (uint i = 0; i < elems.size(); i++) {
		if (elems[i].token == FRAME_BOX_TOKEN) {
			if (elems[i].nativeRect == r && elems[i].penColor == penColor)
				return; // identical — nothing to do
			oldFrameRect = elems[i].nativeRect;
			break; // found but different — fall through to update
		}
	}
	// Selection moved or color changed (or no existing element): update and present.
	// clearToken() removes the stale element so the rect/color change takes effect
	// (push() only deduplicates on type+token+rect, so changing rect without clearing
	// would accumulate stale elements as the user moves the selection).
	_uiLayer->clearToken(FRAME_BOX_TOKEN);
	Roger::UiElement e;
	e.type = Roger::kUiWindow; e.nativeRect = r;
	e.backColor = -1; // no fill — never paints over scene content
	e.penColor = penColor;
	e.hasFrame = true;
	e.token = FRAME_BOX_TOKEN;
	_uiLayer->push(e);
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
	if (_uiLayer)
		Roger::collectUiTextRects(_uiLayer->elements(), GENERIC_TEXT_TOKEN, textRects);

	// Filter per region (the filters judge each rect independently) so each surviving
	// rect keeps its owning-window token through to the stamped sprite.
	Common::Array<FgRegion> pending = _foregroundRegions;
	_foregroundRegions.clear();
	for (uint i = 0; i < pending.size(); i++) {
		const Common::Rect &nr = pending[i].rect;
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
		s.owner = pending[i].owner; // window token: stamp dies with its window (uiClearToken)
		_textSprites.push_back(s);
		if (_diag)
			warning("ROGER-DIAG[fgCapture]: pic=%d rect=(%d,%d,%d,%d) owner=0x%08x now=%u",
			        _loadedPicId, nr.left, nr.top, nr.right, nr.bottom, pending[i].owner,
			        (unsigned)_textSprites.size());
	}
	if (_debugCapture)
		dumpCaptureDebug();
}

void FileRogerArtProvider::onAddToPicCel(int viewId, int loopNo, int celNo,
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
	// flip again — keep mirror false here.
	s.mirror = false;
	s.celOverride = nullptr;
	_staticSprites.push_back(s);
	if (_diag)
		warning("ROGER-DIAG[addToPic]: pic=%d view=%d loop=%d cel=%d pri=%d rect=(%d,%d,%d,%d) nowHave=%u",
		        _loadedPicId, viewId, loopNo, celNo, priority,
		        celRect.left, celRect.top, celRect.right, celRect.bottom, (unsigned)_staticSprites.size());
}

void FileRogerArtProvider::onInitCel(int viewId, int loopNo, int celNo,
                                     const Common::Rect &celRect, int priority, uint32 owner) {
	if (owner != 0) {
		// One capture per animate object, latest draw wins — mirrors the native buffer,
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
	_initCels.push_back(s);
	if (_diag)
		warning("ROGER-DIAG[initCel]: pic=%d view=%d loop=%d cel=%d pri=%d rect=(%d,%d,%d,%d) owner=%08x now=%u",
		        _loadedPicId, viewId, loopNo, celNo, priority,
		        celRect.left, celRect.top, celRect.right, celRect.bottom, owner, (unsigned)_initCels.size());
}

void FileRogerArtProvider::beginNativeDraw() { _nativeDrawDepth++; }
void FileRogerArtProvider::endNativeDraw()   { if (_nativeDrawDepth > 0) _nativeDrawDepth--; }

void FileRogerArtProvider::onNativeShowRect(const Common::Rect &screenRect, uint32 ownerToken) {
	// §3.1 exact invalidation: SCI showed these native pixels, so the overlay
	// region is stale regardless of any capture bookkeeping below. Deliberately
	// NOT gated on _nativeDrawDepth: invalidation is dumb and exact; only the
	// content capture below is scoped. O(1); never presents.
	markNativeDirty(screenRect);
	if (!overlayShown() || _nativeDrawDepth > 0 || !_plate)
		return; // overlay off, inside a Roger-handled draw, or no hires plate
	if (screenRect.isEmpty())
		return;
	if (_diag)
		warning("ROGER-DIAG[showRect]: pic=%d rect=(%d,%d,%d,%d) owner=0x%08x", _loadedPicId,
		        screenRect.left, screenRect.top, screenRect.right, screenRect.bottom, ownerToken);
	FgRegion r; r.rect = screenRect; r.owner = ownerToken;
	_foregroundRegions.push_back(r); // persistent foreground-sprite capture (was _genRegions)
}

void FileRogerArtProvider::onNativeText(const Common::Rect &nativeRect, const char *text,
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
	e.token = isGenericTextToken(winToken) ? winToken : GENERIC_TEXT_TOKEN;
	e.textRole = Roger::kRoleBody;   // same body size as dialog/control text
	e.nativeFontH = nativeFontH;     // native cell height -> renderer target size
	e.nativeTextW = nativeTextW;     // single-line width cap (0 = multi-line: no cap)
	_genTextPending.push_back(e);
	// Emit into _uiLayer NOW, not deferred to the next animate cycle. A blocking message
	// (Print/kDisplay) draws its text here and then waits for a click WITHOUT ticking
	// kernelAnimate, so a deferred flush would only reach _uiLayer after the message's
	// window is already disposed — missing its removeWindow clear and leaving the text
	// tagged to a dead window (it then lingered until the NEXT window reused the id). Pushing
	// immediately means the text is in _uiLayer under its live window token, so the window's
	// removeWindow clears it on dismiss. Safe: onNativeText fires on a real text draw, not
	// per-cycle. renderFromAnimateList still calls flushGenericText (a no-op when empty).
	flushGenericText();
}

void FileRogerArtProvider::flushGenericText() {
	if (!overlayShown() || !_plate)
		{ _genTextPending.clear(); return; }
	ensureUi();
	// Emit this frame's generic captures PERSISTENTLY: push each into _uiLayer where it
	// stays until room change. We do NOT clear prior generic text every frame, because SCI
	// draws static text (e.g. QFG1 stat labels) only once — clearing+relying-on-recapture
	// made it flash then vanish. _uiLayer->push replaces an element with the same
	// type+token+rect, so a stat value redraw at the same rect refreshes in place (live
	// updates) while untouched lines persist. Cleared wholesale on room change (clearAll).
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
		_uiLayer->push(e);
	}
	_genTextPending.clear();
	// Drop any generic element a controls16/menu element already covers (no double render).
	_uiLayer->dedupeGenericText(GENERIC_TEXT_TOKEN);
	if (_debugCapture)
		dumpCaptureDebug();
}

void FileRogerArtProvider::snapshotNativeBaseline() {
	// kernelAnimate is mid-cycle from this hook until renderFromAnimateList runs.
	// While it is, blocking-seam barrier calls defer (the cycle's own tail call
	// flushes them) — this is what makes a bitsRestore storm structurally unable
	// to present per-hook (the bb65c56b75a class).
	_inAnimateCycle = true;
	// Side-by-side compare mode also needs this snapshot: it is taken at the one moment
	// the native visual buffer holds the WHOLE frame (pic + addToPic + animate cast),
	// just before restoreAndDelete() erases the animating cast (ego/moving views). The
	// live buffer read later in presentComparison has that cast already erased.
	if (!_diffBackstop && _mode != Roger::kModeSideBySide) return; // else skip the costly per-frame snapshot
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

bool FileRogerArtProvider::drawGenericRegions(Graphics::ManagedSurface &scene,
                                              const Common::Rect &picRect) {
	if (!g_sci || !g_sci->_gfxScreen)
		return false;
	// Need either hook-recorded regions or a baseline to diff against; bail cheaply.
	if (_genRegions.empty() && !_haveBaseline) {
		return false;
	}
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

	// Diff backstop: any native pixels that differ from the last known baseline and were
	// not already recorded by a bitsShow hook this frame are captured too.
	// Belt-and-suspenders: _diffBackstop must be on (snapshotNativeBaseline also guards it,
	// keeping _haveBaseline false when the knob is off, but guard explicitly here too).
	if (_diffBackstop && _haveBaseline && _nativeBaseline.size() == vis.size()) {
		Common::Array<Common::Rect> changed;
		Roger::extractChangedBoxes(_nativeBaseline.begin(), vis.begin(), sw, sh, changed);
		for (uint i = 0; i < changed.size(); i++)
			_genRegions.push_back(changed[i]);
	}

	if (_genRegions.empty())
		return false;

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

void FileRogerArtProvider::renderFromAnimateList(const AnimateList &list) {
	Common::Array<Roger::Sprite> sprites;
	Common::Array<Graphics::Surface *> nativeSurfaces;

	const bool dbg = _debugLog;

	flushGenericText(); // emit this frame's generic text captures into _uiLayer (deduped)

	// Build the set of cels in the LIVE animate cast this frame (view+loop+cel), so the
	// init-captured static cels (_initCels) can exclude anything that is actively animated
	// (those are drawn live; only the never-animated init draws — the baked signs/props — stay).
	for (AnimateList::const_iterator it = list.begin(); it != list.end(); ++it) {
		if (it->signal & kSignalHidden)
			continue;
		Roger::Sprite s;
		s.viewId   = it->viewId;
		s.loopNo   = it->loopNo;
		s.celNo    = it->celNo;
		s.celRect  = it->celRect;
		s.priority = it->priority;
		// Mirror is already baked in upstream: GfxView::getBitmap() flips a mirrored
		// loop's pixels, and BOTH cel sources read through it — renderNativeCel (native
		// fallback) and the hires ViewCache's generateViewCel. Flipping again in the
		// compositor double-flips (ego walks backwards), so keep mirror false.
		s.mirror = false;

		// Provide a native-cel fallback only for sprites that have no hires view art.
		// renderScene consults getCel() first and ignores celOverride when a hires cel
		// exists, so rendering the fallback in that case is wasted work — skip it. getCel
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
	// set, i.e. during the room's first setup — that includes BOTH the baked decorations
	// (QFG1 first-visit signs: drawn once via the init-frame cast, baked into the picture,
	// then their objects dispose out of the animate list) AND live actors like the ego, whose
	// first draw happens on the same init frame. No view/loop/cel identity can tell them apart
	// (SCI0 rooms pack decorations and actors into one per-room view resource: QFG1 300 signs
	// = view 300 loop 2, live bard/goblin = loops 0/1/3, and BOTH are in the cast on frame 1).
	// The reliable discriminator is the OWNING OBJECT, tagged at capture time: promote an init
	// cel only while its owner is ABSENT from the animate list. A disposed-after-baking prop
	// promotes (its pixels persist natively); a live actor never does (it is drawn — or, when
	// hidden, natively erased — by the cast), which is why this scans the full list including
	// kSignalHidden entries. Suppression is per-frame, not a permanent prune: the signs are in
	// the cast on frame 1 and must still promote after their objects leave.
	Common::Array<uint32> liveOwners;
	for (AnimateList::const_iterator it = list.begin(); it != list.end(); ++it)
		// Token must match rogerOwnerToken() in graphics/animate.cpp (segment<<16 | offset).
		liveOwners.push_back(((uint32)it->object.getSegment() << 16) | (uint32)(it->object.getOffset() & 0xFFFF));

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
	if (!_foregroundRegions.empty()) {
		Common::Array<Common::Rect> liveRects;
		for (uint i = 0; i < sprites.size(); i++)
			liveRects.push_back(sprites[i].celRect);
		processForegroundCaptures(liveRects);
	}
	for (uint i = 0; i < _textSprites.size(); i++)
		statics.push_back(_textSprites[i]);

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
	_inAnimateCycle = false; // cycle draw complete — reopen the barrier
	presentBarrier(); // spec §3.2: the per-cycle present (fresh-frame branch — no recompose)
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

	if (_mode == Roger::kModeOriginal) {
		// Enhanced -> Original: reveal the native 320x200 render underneath.
		g_system->hideOverlay();
	} else if (prev == Roger::kModeOriginal) {
		// Original -> SideBySide: overlay comes back. _nativeBaseline went stale while
		// the overlay was off (snapshotNativeBaseline early-returns when hidden); force a
		// fresh snapshot on the next kernelAnimate before any Feeder-B diff runs.
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

// Period-appropriate body fonts, all shipping in ScummVM's fonts.dat. Ctrl+Shift+F
// rotates through these live so candidates can be judged in-game; the header/menu
// font stays config-only (roger_ui_header_font). See docs/roger.md.
static const char *const kBodyFontShortlist[] = {
	"ms_sans_serif.ttf",            // clean Win9x UI sans (period feel)
	"LiberationSans-Regular.ttf",   // neutral sans
	"NotoSans-Regular.ttf",         // neutral sans
	"LiberationSerif-Regular.ttf",  // storybook / manual feel
	"GoMono-Regular.ttf",           // DOS/terminal monospace (current default)
	"LiberationMono-Regular.ttf",   // DOS/terminal monospace (Courier-metric)
	"SourceCodeVariable-Roman.ttf", // monospace
};
static const int kBodyFontShortlistLen =
	(int)(sizeof(kBodyFontShortlist) / sizeof(kBodyFontShortlist[0]));

void FileRogerArtProvider::cycleBodyFont() {
	_bodyFontIdx = (_bodyFontIdx + 1) % kBodyFontShortlistLen;
	const char *next = kBodyFontShortlist[_bodyFontIdx];

	// Same size ladder as ensureUi().
	Common::Array<int> sizes;
	sizes.push_back(18); sizes.push_back(24); sizes.push_back(32);
	sizes.push_back(42); sizes.push_back(56); sizes.push_back(72);
	sizes.push_back(96); sizes.push_back(120); sizes.push_back(160);

	int scale = 150;
	if (ConfMan.hasKey("roger_ui_font_scale"))
		scale = ConfMan.getInt("roger_ui_font_scale");

	Roger::RogerTextRenderer *rebuilt = new Roger::RogerTextRenderer(Common::String(next), sizes);
	rebuilt->setGlobalScale(scale);
	delete _textRenderer;
	_textRenderer = rebuilt;

	warning("ROGER: body font -> '%s' (%d/%d)%s", next, _bodyFontIdx + 1,
	        kBodyFontShortlistLen, _textRenderer->ttfLoaded() ? "" : " [FAILED -> bitmap fallback]");

	// Redraw any open dialog/list with the new font, and restore the banner.
	if (_haveScene) { markFullDirty(); presentBarrier(); }
	reapplyStatus();
}

// ---------------------------------------------------------------------------
// Live enhance-pass tuning helpers
// ---------------------------------------------------------------------------

Common::Array<int> FileRogerArtProvider::parseOmyacPasses(bool hasKey, const Common::String &passStr) const {
	if (!hasKey)
		return Roger::defaultPasses();
	// hasKey + empty string = wireframe (zero passes).
	Common::Array<int> passes;
	Common::String tok;
	for (uint i = 0; i <= passStr.size(); ++i) {
		const char c = (i < passStr.size()) ? passStr[i] : '\0';
		if (c == ',' || c == ' ' || c == '\t' || c == '\0') {
			if (!tok.empty()) {
				if (tok == "fill" || tok == "f")
					passes.push_back(2);
				else if (tok == "line" || tok == "l")
					passes.push_back(1);
				else if (tok == "all" || tok == "a")
					passes.push_back(0);
				// unrecognized tokens silently skipped
				tok.clear();
			}
		} else {
			tok += c;
		}
	}
	return passes;
}

void FileRogerArtProvider::regenInPlace() {
	if (!_assetGen || _loadedPicId < 0)
		return;
	const int saved = _loadedPicId;
	_loadedPicId = -1; // invalidate early-return guard in pushHiresBackground
	pushHiresBackground(saved);
	markFullDirty();
	presentBarrier();
}

void FileRogerArtProvider::tuneEnhancePasses(int delta, int which) {
	if (!_assetGen)
		return;

	// Map which → pass int: 0(fill)→2, 1(line)→1, 2(all)→0
	const int passType = (which == 0) ? 2 : (which == 1) ? 1 : 0;

	// Count current passes by type.
	const Common::Array<int> &cur = _assetGen->enhancePasses();
	int fillCount = 0, lineCount = 0, allCount = 0;
	for (uint i = 0; i < cur.size(); ++i) {
		if (cur[i] == 2) fillCount++;
		else if (cur[i] == 1) lineCount++;
		else if (cur[i] == 0) allCount++;
	}

	// Apply delta to the targeted type, clamped to >= 0.
	if (passType == 2) fillCount = MAX(0, fillCount + delta);
	else if (passType == 1) lineCount = MAX(0, lineCount + delta);
	else if (passType == 0) allCount  = MAX(0, allCount  + delta);

	// Rebuild in canonical grouped order: fill (2), line (1), all (0).
	Common::Array<int> newPasses;
	for (int i = 0; i < fillCount; ++i) newPasses.push_back(2);
	for (int i = 0; i < lineCount; ++i) newPasses.push_back(1);
	for (int i = 0; i < allCount;  ++i) newPasses.push_back(0);
	_assetGen->setEnhancePasses(newPasses);

	// Tuning must generate in memory — avoid disk-cache churn. Switch out of
	// prebuilt/cache mode if needed (the user can re-set roger_gen_mode to
	// restore their preferred mode or call reloadGenConfig() to persist the
	// chosen sequence).
	if (_assetGen->mode() != Roger::kGenMemory)
		_assetGen->setMode(Roger::kGenMemory); // tune in memory; never churn the disk cache (incl. kGenAlways)

	// Log the active sequence unconditionally so tuning feedback is always visible
	// (not gated on _debugLog).
	debug("ROGER tuneEnhancePasses: fill=%d line=%d all=%d  (fill=pass2, line=pass1, all=pass0)",
	      fillCount, lineCount, allCount);

	regenInPlace();
}

void FileRogerArtProvider::reloadGenConfig() {
	if (!_assetGen)
		return;

	// Re-parse roger_omyac_passes from ConfMan using the same three-state logic
	// as the constructor. The user edits the config file and presses Ctrl+Shift+R.
	_assetGen->setEnhancePasses(
		parseOmyacPasses(ConfMan.hasKey("roger_omyac_passes"),
		                 ConfMan.hasKey("roger_omyac_passes") ? ConfMan.get("roger_omyac_passes") : "")
	);

	// Keep in a generating mode so the reload actually produces a new plate.
	if (_assetGen->mode() != Roger::kGenMemory)
		_assetGen->setMode(Roger::kGenMemory); // tune in memory; never churn the disk cache (incl. kGenAlways)

	debug("ROGER reloadGenConfig: roger_omyac_passes re-read; passes count=%u",
	      (unsigned)_assetGen->enhancePasses().size());

	regenInPlace();
}

void FileRogerArtProvider::diagDumpState(const char *where) {
	if (!_diag)
		return;
	warning("ROGER-DIAG[%s]: pic=%d enabled=%d overlayShown=%d plate=%s haveScene=%d "
	        "compCacheValid=%d haveBaseline=%d uiElems=%u uiIcons=%u staticSprites=%u",
	        where, _loadedPicId, enabled ? 1 : 0, overlayShown() ? 1 : 0,
	        _plate ? "yes" : "NULL", _haveScene ? 1 : 0, _compositeCacheValid ? 1 : 0,
	        _haveBaseline ? 1 : 0,
	        _uiLayer ? (unsigned)_uiLayer->elements().size() : 0u,
	        (unsigned)_uiIcons.size(), (unsigned)_staticSprites.size());
}

void FileRogerArtProvider::onNativePicture() {
	diagDumpState("nativePic");
	if (_compositor)
		_compositor->setRoom(nullptr, nullptr);
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	if (_uiLayer) _uiLayer->clearAll();
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
	_genRegions.clear(); // drop any stale Feeder B rects from the departing room (drawGenericRegions won't run if _plate is null)
	_haveScene = false;
	_loadedPicId = -1;
	g_system->hideOverlay();
}

void FileRogerArtProvider::onMouseMoved() {
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
	// Reuse the same geometry renderFrame uses, with an empty sprite list.
	const bool aspect = g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection);
	const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
	const Common::Rect gameRect = Roger::computeGameRect(OW, OH, aspect);
	const Common::Rect picRect = Roger::computePictureRect(gameRect, _statusBarH);
	_compositor->setPictureDest(picRect);
	Common::Array<Roger::Sprite> none;
	_compositor->renderScene(out, none, gameRect);
}

void FileRogerArtProvider::onTransition(int sciType, const Common::Rect & /*picRect*/) {
	if (!_transitionsEnabled || !overlayShown() || !_compositor || !_plate)
		return;
	diagDumpState("transition");
	const Roger::TransitionFamily fam = Roger::transitionFamilyFor(sciType);
	if (fam == Roger::kFxNone)
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
	// `to` = the new room background (no sprites yet).
	Graphics::ManagedSurface to(OW, OH, rgba);
	composeRoomScene(to);
	// composeRoomScene pre-validates _bgCache; ensure the first post-transition renderFrame
	// does a full _compositeCache copy so the software-cursor fast path has clean pixels.
	_compositeCacheValid = false;
	Graphics::ManagedSurface &scratch = *scratchScene(OW, OH);
	// Skip the FX animation in side-by-side (it would present the non-split full-overlay
	// layout); the room-change bookkeeping below still runs, and the next frame's
	// presentComparison shows the split with the new room.
	if (_mode != Roger::kModeSideBySide)
		_compositor->runTransition(from, to, scratch, fam, Roger::defaultDurationMs(fam), sciType);
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
	// dirty-rect history left over from the PREVIOUS room — the QFG1 fresh-start town
	// breakage (a real transition into pic 300; save-load reaches it via an instant cut and
	// is fine). Reset the compositor to a clean first frame (full seed + full present, stale
	// dirty rects dropped) so a transition-entry matches a save-restore/instant-cut entry.
	_compositor->resetForRoomChange();
}

void FileRogerArtProvider::onShake(int shakeCount, int directions) {
	if (!_transitionsEnabled || !overlayShown() || _mode == Roger::kModeSideBySide ||
	        !_compositor || !_haveScene || !_sceneCache)
		return; // side-by-side: pure FX, would present the non-split layout — skip
	const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0 || _sceneCache->w != OW || _sceneCache->h != OH)
		return;
	// Native SCI shake is ~10px of 200 rows; scale into overlay space.
	const int mag = (10 * OH) / 200;
	Graphics::ManagedSurface &scratch = *scratchScene(OW, OH);
	_compositor->runShake(*_sceneCache, scratch, shakeCount, directions, mag);
}

FileRogerArtProvider::~FileRogerArtProvider() {
	if (_inputDriver) {
		g_system->getEventManager()->getEventDispatcher()->unregisterSource(_inputDriver);
		delete _inputDriver;
		_inputDriver = nullptr;
	}
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	delete _assetGen; _assetGen = nullptr;
	delete _viewCache; _viewCache = nullptr;
	delete _compositor; _compositor = nullptr;
	delete _uiLayer; _uiLayer = nullptr;
	delete _textRenderer; _textRenderer = nullptr;
	delete _altTextRenderer; _altTextRenderer = nullptr;
	if (_sceneCache) { delete _sceneCache; _sceneCache = nullptr; }
	if (_scratchScene) { delete _scratchScene; _scratchScene = nullptr; }
	if (_compositeCache) { delete _compositeCache; _compositeCache = nullptr; }
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_genericGlyphCache.clear(); // surfaces were owned by _uiIcons (freed above)
	_drawCelNativeCache.clear(); // surfaces were owned by _uiIcons (freed above)
}

} // namespace Sci
