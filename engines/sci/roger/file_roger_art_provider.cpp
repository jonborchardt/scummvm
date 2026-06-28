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

#include "sci/roger/file_roger_art_provider.h"
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

	// roger_visual_variant / roger_priority_variant selected prebuilt PNG files (the
	// hires visual and the EGA-color overlay-occlusion map). Under in-engine
	// generation both the visual and the occlusion bands are produced from the SCI
	// resource, so these knobs are obsolete and are no longer read.

	// roger_autoshot: a verification-harness flag (off by default). When set, the
	// first composited frame of each room is dumped to a PNG (see renderFrame).
	// This is how the dev loop captures the hires overlay deterministically without
	// keystrokes/focus — injected Alt+s/F10 never reach SDL (Win32 menu keys).
	_autoshot = ConfMan.hasKey("roger_autoshot") && ConfMan.getBool("roger_autoshot");
	// roger_diff_backstop: Feeder B per-frame full-buffer pixel diff (default off). When off
	// snapshotNativeBaseline() returns immediately, keeping _haveBaseline false so the costly
	// 320x200 buffer read + 64K diff never runs. The bitsShow-hook path (onNativeShowRect) and
	// addToPic capture (Feeder A) remain on regardless.
	_diffBackstop = ConfMan.hasKey("roger_diff_backstop") && ConfMan.getBool("roger_diff_backstop");
	// roger_debug: per-frame + per-UI-element diagnostic logging (also toggled in-game
	// with Ctrl+Shift+L). Read it here so the documented config knob actually works.
	_debugLog = ConfMan.hasKey("roger_debug") && ConfMan.getBool("roger_debug");

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
	return _overlayActive;
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

	// Roger supports EGA games only. Warn and disable for VGA.
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
	// Roger supports EGA SCI games only. The launcher blocks VGA games at add-time, but a
	// target configured another way (manual ConfMan / normal ScummVM launcher) can still
	// reach here. Feeding VGA pics through the EGA omyac pipeline renders garbage, so on
	// the first non-EGA picture we disable the overlay with a message instead. Setting
	// enabled = false makes hasBackground() return false from here on, so this fires once.
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

	// New room: drop the previous room's captured addToPic cels (Feeder A).
	_staticSprites.clear();

	uint32 tEnter = g_system->getMillis();

	// New room: drop any dialogs/icons left from the previous room so they do not
	// bleed onto the new scene. _haveScene is rebuilt by the next renderFrame.
	if (_uiLayer) _uiLayer->clearAll();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
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
	// roger_dirty_present (default on): convert+push only changed regions each frame.
	bool dirtyPresent = true;
	if (ConfMan.hasKey("roger_dirty_present"))
		dirtyPresent = ConfMan.getBool("roger_dirty_present");
	_compositor->setDirtyPresent(dirtyPresent);
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
	if (!_overlayActive || !_compositor || !_plate)
		return;
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
	// Snapshot scene+UI (no cursor) — cursor-only onMouseMoved restores from here.
	// Only the software-cursor onMouseMoved fast path reads _compositeCache; under the
	// hardware cursor that path early-returns and compositeCursor no-ops, so the cache
	// has no reader. Skip the ~22 MB copy entirely in the hw-cursor case.
	if (!_useHwCursor) {
		// _compositeCacheValid coming in tells us a full cursor-free snapshot from a prior
		// frame is intact; ensureCompositeCache clears it on (re)alloc. The onMouseMoved fast
		// path reads arbitrary cursor-position rects from this cache (not just the seed union),
		// so a bounded copy is only safe when that full prior snapshot exists AND this frame
		// touched only the union; otherwise (realloc, full-seed, generic regions, or a prior
		// invalidation) do a full copy. Copies into the existing allocation.
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
	}
	compositeCursor(scene, gameRect);
	_compositor->presentToOverlay(scene);

	// roger_autoshot (verification harness): dump once per room. Deterministic — no
	// keystrokes/focus needed.
	if (_autoshot && _autoshotPicId != _loadedPicId) {
		dumpAutoshot(scene, gameRect, "");
		_autoshotPicId = _loadedPicId;
	}
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
	// arrow into the overlay scene at the mouse position. The mouse is in game space
	// (320x200) while the overlay is shown; map it into the on-screen game rect.
	if (!enabled)
		return;
	if (_useHwCursor)
		return; // native hardware cursor is shown over the overlay instead (preferred)
	if (!_cursorVisible)
		return; // game hid the cursor; do not draw anything
	ensureCursor();
	if (!_cursorSurf)
		return;
	const Common::Point mp = g_system->getEventManager()->getMousePos();
	const int ox = gameRect.left + mp.x * gameRect.width() / 320;
	const int oy = gameRect.top + mp.y * gameRect.height() / 200;
	const Common::Rect dst(ox - _cursorHotspot.x, oy - _cursorHotspot.y,
	                       ox - _cursorHotspot.x + _cursorSurf->w,
	                       oy - _cursorHotspot.y + _cursorSurf->h);
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
	if (!_overlayActive || !_compositor || !_haveScene || !_sceneCache)
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
	Graphics::ManagedSurface &scene = *scratchScene(_sceneCache->w, _sceneCache->h);
	scene.copyFrom(*_sceneCache); // fully overwrites the scratch buffer
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		// Diagnostic dump of the UI element rects (roger_debug), throttled to one dump per
		// distinct dialog (signature over token/rect/type) so it does not spam per frame.
		if (_debugLog) {
			const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
			static uint32 lastDiagSig = 0;
			uint32 dsig = 2166136261u;
			for (uint i = 0; i < els.size(); i++) {
				dsig = (dsig ^ (uint32)els[i].token) * 16777619u;
				dsig = (dsig ^ (uint32)(els[i].nativeRect.left * 31 + els[i].nativeRect.top)) * 16777619u;
				dsig = (dsig ^ (uint32)(els[i].type * 7 + els[i].textRole)) * 16777619u;
			}
			if (dsig != lastDiagSig) {
				lastDiagSig = dsig;
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
	ensureCompositeCache(OW, OH);
	_compositeCache->copyFrom(scene);
	_compositeCacheValid = true;
	compositeCursor(scene, _lastGameRect);
	_compositor->presentToOverlay(scene);

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
	_compositeCacheValid = false;
	if (!_overlayActive || !_plate) return; // no hires scene -> leave native UI visible
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiWindow; e.nativeRect = r;
	e.backColor = (wndStyle & 1 /*TRANSPARENT*/) ? -1 : backColor;
	e.penColor = penColor;
	e.hasFrame = !(wndStyle & 2 /*NOFRAME*/);
	e.token = token;
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushText(const Common::Rect &r, const char *text, int penColor,
                                      int backColor, int fontId, int align, uint32 token,
                                      int textRole, bool useAltFont,
                                      int nativeFontH, int nativeTextW) {
	_compositeCacheValid = false;
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiText; e.nativeRect = r; e.text = text ? text : "";
	e.penColor = penColor; e.backColor = backColor; e.fontId = fontId;
	e.align = align; e.token = token;
	e.textRole = textRole; e.useAltFont = useAltFont;
	e.nativeFontH = nativeFontH; e.nativeTextW = nativeTextW;
	buildGlyphs(text, fontId, penColor, e.glyphs);
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushButton(const Common::Rect &r, const char *text, int fontId,
                                        int style, uint32 token,
                                        int nativeFontH, int nativeTextW) {
	_compositeCacheValid = false;
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiButton; e.nativeRect = r; e.text = text ? text : "";
	e.fontId = fontId; e.style = style; e.align = 1 /*center*/;
	e.backColor = 7 /*light gray*/; e.penColor = 0; e.hasFrame = true; e.token = token;
	e.nativeFontH = nativeFontH; e.nativeTextW = nativeTextW;
	buildGlyphs(text, fontId, e.penColor, e.glyphs);
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushTextEdit(const Common::Rect &r, const char *text, int fontId,
                                          int style, int cursorPos, uint32 token,
                                          int nativeFontH, int nativeTextW) {
	_compositeCacheValid = false;
	if (!_overlayActive || !_plate) return;
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
	presentWithUi();
}

void FileRogerArtProvider::uiPushIcon(const Common::Rect &r, int viewId, int loopNo, int celNo,
                                      uint32 token) {
	_compositeCacheValid = false;
	if (!_overlayActive || !_plate) return;
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
	presentWithUi();
}

void FileRogerArtProvider::onDrawCel(const Common::Rect &r, int viewId, int loopNo, int celNo) {
	if (!_overlayActive || !_plate || !_viewCache) return;
	const Graphics::Surface *hi = _viewCache->getCel(viewId, loopNo, celNo);
	if (!hi) return; // no upscaled art for this cel -> leave the native draw showing
	ensureUi();
	const uint32 tok = 0x50000000u; // standalone hires cel (e.g. inventory close-up)
	_uiLayer->clearToken(tok);      // keep only the latest standalone cel
	Roger::UiElement e;
	e.type = Roger::kUiIcon; e.nativeRect = r; e.token = tok;
	e.iconSurface = hi; // borrowed from the ViewCache
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushStatus(const Common::Rect &r, const char *text, int fontId,
                                        int penColor, int backColor, uint32 token,
                                        int nativeFontH, int nativeTextW) {
	_compositeCacheValid = false;
	// Remember the banner so it can be re-applied on room load / F10 enable, even if
	// the overlay was not ready when the game first drew it.
	_haveStatus = true; _statusRect = r; _statusText = text ? text : "";
	_statusFont = fontId; _statusPen = penColor; _statusBack = backColor; _statusToken = token;
	_statusNativeFontH = nativeFontH; _statusNativeTextW = nativeTextW;
	if (!_overlayActive || !_plate) return;
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
	presentWithUi();
}

void FileRogerArtProvider::reapplyStatus() {
	if (_haveStatus)
		uiPushStatus(_statusRect, _statusText.c_str(), _statusFont, _statusPen, _statusBack, _statusToken,
		             _statusNativeFontH, _statusNativeTextW);
}

void FileRogerArtProvider::uiClearToken(uint32 token) {
	_compositeCacheValid = false;
	if (_uiLayer) _uiLayer->clearToken(token);
	if (_overlayActive && _plate) presentWithUi();
}

void FileRogerArtProvider::uiClearAll() {
	_compositeCacheValid = false;
	if (_uiLayer) _uiLayer->clearAll();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	if (_overlayActive && _plate) presentWithUi();
}

Graphics::Surface *FileRogerArtProvider::renderNativeCel(int viewId, int loopNo, int celNo) const {
	if (!g_sci || !g_sci->_gfxCache)
		return nullptr;

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

void FileRogerArtProvider::onAddToPicCel(int viewId, int loopNo, int celNo,
                                         const Common::Rect &celRect, int priority) {
	Roger::Sprite s;
	s.viewId = viewId;
	s.loopNo = loopNo;
	s.celNo = celNo;
	s.celRect = celRect;
	s.priority = priority;
	s.mirror = false;
	s.celOverride = nullptr;
	_staticSprites.push_back(s);
}

void FileRogerArtProvider::beginNativeDraw() { _nativeDrawDepth++; }
void FileRogerArtProvider::endNativeDraw()   { if (_nativeDrawDepth > 0) _nativeDrawDepth--; }

void FileRogerArtProvider::onNativeShowRect(const Common::Rect &screenRect) {
	if (!_overlayActive || _nativeDrawDepth > 0 || !_plate)
		return; // overlay off, inside a Roger-handled draw, or no hires plate (plate-less rooms never call drawGenericRegions)
	if (screenRect.isEmpty())
		return;
	_genRegions.push_back(screenRect);
}

void FileRogerArtProvider::snapshotNativeBaseline() {
	if (!_diffBackstop) return; // backstop off (default): skip the costly per-frame snapshot
	if (!_overlayActive || !g_sci || !g_sci->_gfxScreen)
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
	_genRegions.clear();
	return drewAny;
}

void FileRogerArtProvider::renderFromAnimateList(const AnimateList &list) {
	Common::Array<Roger::Sprite> sprites;
	Common::Array<Graphics::Surface *> nativeSurfaces;

	const bool dbg = _debugLog;

	for (AnimateList::const_iterator it = list.begin(); it != list.end(); ++it) {
		if (it->signal & kSignalHidden)
			continue;
		Roger::Sprite s;
		s.viewId   = it->viewId;
		s.loopNo   = it->loopNo;
		s.celNo    = it->celNo;
		s.celRect  = it->celRect;
		s.priority = it->priority;
		s.mirror   = false; // mirror refinement deferred to a later task

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

	// Merge captured addToPic cels (Feeder A) with the animate cast, priority-sorted,
	// so static props occlude/are-occluded correctly against the ego.
	Common::Array<Roger::Sprite> merged;
	Roger::mergeSpritesByPriority(sprites, _staticSprites, merged);

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
		warning("ROGER: pic=%d sprites=%u (+%u addToPic) plate=%dx%d overlay=%s",
		        _loadedPicId, (unsigned)sprites.size(), (unsigned)_staticSprites.size(),
		        _plate ? _plate->w : -1, _plate ? _plate->h : -1, _overlayActive ? "on" : "off");

	renderFrame(merged);

	for (uint i = 0; i < nativeSurfaces.size(); i++) {
		nativeSurfaces[i]->free();
		delete nativeSurfaces[i];
	}
}

void FileRogerArtProvider::toggleOverlay() {
	_overlayActive = !_overlayActive;
	if (!_overlayActive) {
		g_system->hideOverlay(); // reveal the native 320x200 render underneath
	} else {
		// _nativeBaseline went stale while the overlay was off (snapshotNativeBaseline
		// early-returns when _overlayActive is false).  Force a fresh snapshot on the
		// next kernelAnimate before any Feeder-B diff runs.
		_haveBaseline = false;
		// Re-show immediately (do not wait for the next kAnimate) and restore the banner.
		if (_haveScene) presentWithUi();
		reapplyStatus();
	}
	warning("ROGER: overlay %s", _overlayActive ? "ENABLED (upscaled)" : "DISABLED (original)");
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
	if (_haveScene) presentWithUi();
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
	presentWithUi();
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

void FileRogerArtProvider::onNativePicture() {
	if (_compositor)
		_compositor->setRoom(nullptr, nullptr);
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	if (_uiLayer) _uiLayer->clearAll();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_staticSprites.clear();
	_genRegions.clear(); // drop any stale Feeder B rects from the departing room (drawGenericRegions won't run if _plate is null)
	_haveScene = false;
	_loadedPicId = -1;
	g_system->hideOverlay();
}

void FileRogerArtProvider::onMouseMoved() {
	if (_useHwCursor)
		return; // hardware cursor moves itself; no recomposite needed

	// Fast path: composite cache is valid — patch _scratchScene in-place rather than
	// doing a full 22 MB copyFrom(_sceneCache) + re-render UI on every mouse event.
	if (_compositeCache && _compositeCacheValid && _scratchScene &&
	        _compositor && _haveScene && !_lastGameRect.isEmpty()) {
		// Restore the old cursor region from the cursor-free composite cache.
		if (!_lastCursorDstRect.isEmpty()) {
			_compositor->addDirtyRect(_lastCursorDstRect);
			_scratchScene->blitFrom(*_compositeCache, _lastCursorDstRect, _lastCursorDstRect);
		}
		// Paint cursor at new position (updates _lastCursorDstRect, adds new dirty rect).
		compositeCursor(*_scratchScene, _lastGameRect);
		// Present only the changed regions (two small cursor-sized rects).
		_compositor->presentToOverlay(*_scratchScene);
		return;
	}

	// Slow fallback: full rebuild — handles first present, resize, or stale cache.
	presentWithUi();
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
	if (!_transitionsEnabled || !_overlayActive || !_compositor || !_plate)
		return;
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
	Graphics::ManagedSurface &scratch = *scratchScene(OW, OH);
	_compositor->runTransition(from, to, scratch, fam, Roger::defaultDurationMs(fam), sciType);
	// Leave _sceneCache holding the new background so the next kAnimate frame's dirty
	// present builds on it correctly.
	if (!_sceneCache || _sceneCache->w != OW || _sceneCache->h != OH) {
		delete _sceneCache;
		_sceneCache = new Graphics::ManagedSurface(OW, OH, rgba);
	}
	_sceneCache->copyFrom(to);
	_haveScene = true;
}

void FileRogerArtProvider::onShake(int shakeCount, int directions) {
	if (!_transitionsEnabled || !_overlayActive || !_compositor || !_haveScene || !_sceneCache)
		return;
	const int OW = g_system->getOverlayWidth(), OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0 || _sceneCache->w != OW || _sceneCache->h != OH)
		return;
	// Native SCI shake is ~10px of 200 rows; scale into overlay space.
	const int mag = (10 * OH) / 200;
	Graphics::ManagedSurface &scratch = *scratchScene(OW, OH);
	_compositor->runShake(*_sceneCache, scratch, shakeCount, directions, mag);
}

FileRogerArtProvider::~FileRogerArtProvider() {
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
}

} // namespace Sci
