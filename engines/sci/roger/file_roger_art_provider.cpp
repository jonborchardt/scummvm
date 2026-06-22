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
#include "sci/roger/png_loader.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_compositor.h"
#include "sci/roger/roger_coords.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_ui_layer.h"
#include "sci/roger/roger_text.h"
#include "sci/roger/view_cache.h"
#include "sci/roger/slice_set.h"
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

	// The art pipeline emits the low-res original as "pic.<id>.png" and the
	// upscaled hires visual as "pic.<variant>.<id>.png". Default to the
	// upscaler; override with the config key "roger_visual_variant" (empty
	// string selects the plain "pic.<id>.png").
	_visualVariant = "omyac-upscaler";
	if (ConfMan.hasKey("roger_visual_variant"))
		_visualVariant = ConfMan.get("roger_visual_variant");

	// Overlay occlusion samples a real EGA-color-encoded priority map (each pixel's
	// color = its SCI priority band). The pipeline emits it as
	// "<variant>.<id>_p.png"; default to the native-resolution one. (pic.<id>_p.png,
	// used by loadBuffers for SCI's own buffer, may be a placeholder.)
	_priorityVariant = "baseline-native";
	if (ConfMan.hasKey("roger_priority_variant"))
		_priorityVariant = ConfMan.get("roger_priority_variant");

	// roger_autoshot: a verification-harness flag (off by default). When set, the
	// first composited frame of each room is dumped to a PNG (see renderFrame).
	// This is how the dev loop captures the hires overlay deterministically without
	// keystrokes/focus — injected Alt+s/F10 never reach SDL (Win32 menu keys).
	_autoshot = ConfMan.hasKey("roger_autoshot") && ConfMan.getBool("roger_autoshot");
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

	// roger_gen_mode: controls on-the-fly plate generation. Default "prebuilt" =>
	// _assetGen->generatePlate returns nullptr => existing loadSurfaceRGBA path.
	// Other modes: "cache", "memory", "always".
	Roger::GenMode genMode = Roger::kGenPrebuilt;
	if (ConfMan.hasKey("roger_gen_mode")) {
		const Common::String modeStr = ConfMan.get("roger_gen_mode");
		if (modeStr == "cache")
			genMode = Roger::kGenCache;
		else if (modeStr == "memory")
			genMode = Roger::kGenMemory;
		else if (modeStr == "always")
			genMode = Roger::kGenAlways;
		// else: unrecognized => keep kGenPrebuilt (safe default)
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

Common::String FileRogerArtProvider::picDir(GuiResourceId id) const {
	return _basePath + "/pics/" + Common::String::format("%d", id) + "/source/";
}

Common::String FileRogerArtProvider::visualPath(GuiResourceId id) const {
	// "pic.<variant>.<id>.png" (hires), or "pic.<id>.png" when variant is empty.
	const Common::String idStr = Common::String::format("%d", id);
	if (_visualVariant.empty())
		return picDir(id) + "pic." + idStr + ".png";
	return picDir(id) + "pic." + _visualVariant + "." + idStr + ".png";
}

// Roger uses TWO priority maps per room, by design (not duplicates):
//   priorityPath          pic.<id>_p.png            grayscale 8-bit, 320x200 — fills
//                                                   SCI's NATIVE priority buffer
//                                                   (loadBuffers) for walkability +
//                                                   native occlusion.
//   occlusionPriorityPath <variant>.<id>_p.png      EGA-color-encoded (band-per-pixel)
//                         (default baseline-native) — the OVERLAY compositor's
//                                                   per-pixel occlusion source.
// controlPath (pic.<id>_c.png) is the grayscale control map for loadBuffers.
Common::String FileRogerArtProvider::priorityPath(GuiResourceId id) const {
	return picDir(id) + "pic." + Common::String::format("%d", id) + "_p.png";
}

Common::String FileRogerArtProvider::controlPath(GuiResourceId id) const {
	return picDir(id) + "pic." + Common::String::format("%d", id) + "_c.png";
}

Common::String FileRogerArtProvider::occlusionPriorityPath(GuiResourceId id) const {
	const Common::String idStr = Common::String::format("%d", id);
	if (_priorityVariant.empty())
		return priorityPath(id);
	return picDir(id) + _priorityVariant + "." + idStr + "_p.png";
}

bool FileRogerArtProvider::loadPriorityBands(const Common::String &path,
                                             Common::Array<byte> &outBands, int &outW, int &outH) const {
	Graphics::Surface *s = Roger::loadSurfaceRGBA(path);
	if (!s)
		return false;
	outW = s->w;
	outH = s->h;
	outBands.resize(outW * outH);
	for (int y = 0; y < outH; y++) {
		for (int x = 0; x < outW; x++) {
			uint8 a, r, g, b;
			s->format.colorToARGB(s->getPixel(x, y), a, r, g, b);
			outBands[y * outW + x] = (byte)Roger::bandForRGB(r, g, b);
		}
	}
	s->free();
	delete s;
	return true;
}

bool FileRogerArtProvider::hasBackground(GuiResourceId pictureId) const {
	if (!enabled)
		return false;
	// Priority + control maps are always required: the drawPicture hook skips
	// SCI's native render and fills the 320x200 priority/control buffers from
	// these (walkability + occlusion). They are kept prebuilt by design.
	Common::FSNode p(Common::Path(priorityPath(pictureId)));
	Common::FSNode c(Common::Path(controlPath(pictureId)));
	if (!p.exists() || !c.exists())
		return false;
	// In a generating mode the hires visual is produced in-engine, so the
	// prebuilt visual PNG is NOT required (requiring it would needlessly cap
	// coverage to pre-authored rooms). In prebuilt mode the PNG is the plate
	// source, so it must exist.
	if (_assetGen && _assetGen->mode() != Roger::kGenPrebuilt)
		return true;
	Common::FSNode v(Common::Path(visualPath(pictureId)));
	return v.exists();
}

void FileRogerArtProvider::precacheAll() {
	// One-time startup warm-up. Only runs when roger_precache is set AND a
	// generating mode is active (prebuilt mode has nothing to cache).
	if (!_assetGen || _assetGen->mode() == Roger::kGenPrebuilt)
		return;
	if (!ConfMan.hasKey("roger_precache") || !ConfMan.getBool("roger_precache"))
		return;
	if (!g_sci)
		return;
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan)
		return;

	// Enumerate every pic resource the game has; precache the ones that have a
	// full prebuilt art set (visual+priority+control), since only those activate
	// the overlay. Generic: no per-game table — we ask the live engine for its pics.
	Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
	int total = 0;
	for (Common::List<ResourceId>::const_iterator it = pics.begin(); it != pics.end(); ++it)
		if (hasBackground((GuiResourceId)it->getNumber()))
			++total;
	if (total == 0)
		return;

	warning("ROGER precache: warming cache for %d art-backed pics (mode=%d)...", total, (int)_assetGen->mode());
	uint32 t0 = g_system->getMillis();
	int done = 0;
	for (Common::List<ResourceId>::const_iterator it = pics.begin(); it != pics.end(); ++it) {
		GuiResourceId id = (GuiResourceId)it->getNumber();
		if (!hasBackground(id))
			continue;
		uint32 ms = 0;
		Graphics::Surface *s = _assetGen->generatePlate(id, ms); // cache mode writes the PNG
		if (s) { s->free(); delete s; }                          // we only wanted it on disk
		++done;
		warning("ROGER precache: pic %d (%d/%d) %u ms", id, done, total, ms);
	}
	warning("ROGER precache: %d plates warmed in %u ms total", done, g_system->getMillis() - t0);
}

bool FileRogerArtProvider::loadBuffers(GuiResourceId pictureId, GfxScreen *screen) {
	Common::Array<byte> priority = Roger::loadGrayscale8(priorityPath(pictureId));
	Common::Array<byte> control  = Roger::loadGrayscale8(controlPath(pictureId));

	if (priority.empty() || control.empty())
		return false;

	const uint16 w = screen->getWidth();
	const uint16 h = screen->getHeight();

	if (priority.size() != (uint)(w * h) || control.size() != (uint)(w * h))
		return false;

	for (int16 y = 0; y < (int16)h; y++) {
		for (int16 x = 0; x < (int16)w; x++) {
			const byte p = priority[y * w + x];
			const byte c = control[y * w + x];
			screen->putPixel(x, y,
				GFX_SCREEN_MASK_PRIORITY | GFX_SCREEN_MASK_CONTROL,
				0, p, c);
		}
	}
	return true;
}

void FileRogerArtProvider::pushHiresBackground(GuiResourceId pictureId) {
	if (_loadedPicId == pictureId && _plate)
		return; // already loaded for this room

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
	const char *plateSrc = "prebuilt-load";
	uint32 tAcq0 = g_system->getMillis();
	if (_assetGen && _assetGen->mode() != Roger::kGenPrebuilt) {
		_plate = _assetGen->generatePlate(pictureId, genMs);
		if (_plate)
			plateSrc = genMs ? "generated(miss)" : "cache-hit";
	}
	if (!_plate)                                   // prebuilt mode, or generation failed
		_plate = Roger::loadSurfaceRGBA(visualPath(pictureId));   // unchanged fallback
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
	_compositor->setRoom(_plate, _viewCache);

	// Load the real EGA-color priority map for per-pixel overlay occlusion and
	// decode it to a band-per-pixel buffer. It is authored in picture space
	// (320x190 for SCI0) aligned with the plate, so picScreenTop = 0.
	int prW = 0, prH = 0;
	_priorityMap.clear();
	uint32 tOcc0 = g_system->getMillis();
	bool haveOcc = loadPriorityBands(occlusionPriorityPath(pictureId), _priorityMap, prW, prH);
	uint32 occMs = g_system->getMillis() - tOcc0;
	if (haveOcc) {
		_compositor->setPicture(320, prH, 0);
		_compositor->setPriorityMask(_priorityMap.begin(), prW, prH);
	} else {
		// No occlusion map -> sprites still draw, just without occlusion.
		warning("ROGER: no occlusion priority map at %s", occlusionPriorityPath(pictureId).c_str());
		_compositor->setPicture(320, 190, 0);
		_compositor->setPriorityMask(nullptr, 0, 0);
	}
	_loadedPicId = pictureId;

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

void FileRogerArtProvider::renderFrame(const Common::Array<Roger::Sprite> &sprites) {
	if (!_overlayActive || !_compositor || !_plate)
		return;
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

	// Cache the composed room+sprite scene so a UI-only change can be re-presented
	// cheaply (blocking dialogs do not tick kernelAnimate).
	if (!_sceneCache || _sceneCache->w != OW || _sceneCache->h != OH) {
		delete _sceneCache;
		_sceneCache = new Graphics::ManagedSurface(OW, OH, rgba);
	}
	_sceneCache->copyFrom(scene);
	_haveScene = true;
	_lastGameRect = gameRect;

	// If a dialog is already up, re-blend it on top of the freshly composed scene.
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, gameRect, _textRenderer, _altTextRenderer);
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
	ensureCursor();
	if (!_cursorSurf)
		return;
	const Common::Point mp = g_system->getEventManager()->getMousePos();
	const int ox = gameRect.left + mp.x * gameRect.width() / 320;
	const int oy = gameRect.top + mp.y * gameRect.height() / 200;
	const Common::Rect dst(ox - 2, oy - 2, ox - 2 + _cursorSurf->w, oy - 2 + _cursorSurf->h);
	scene.blendBlitFrom(*_cursorSurf, Common::Rect(0, 0, _cursorSurf->w, _cursorSurf->h), dst);
}

void FileRogerArtProvider::ensureUi() {
	if (!_uiLayer)
		_uiLayer = new Roger::RogerUiLayer();
	if (!_textRenderer) {
		// Default to a TTF that actually ships in ScummVM's fonts.dat (FreeSans was
		// replaced by the Liberation family). Override with roger_ui_font.
		Common::String ttf = "LiberationSans-Regular.ttf";
		if (ConfMan.hasKey("roger_ui_font"))
			ttf = ConfMan.get("roger_ui_font");
		// A ladder of pixel sizes for fit-to-box selection (cell mode, hires).
		Common::Array<int> sizes;
		sizes.push_back(18); sizes.push_back(24); sizes.push_back(32);
		sizes.push_back(42); sizes.push_back(56); sizes.push_back(72);
		sizes.push_back(96); sizes.push_back(120); sizes.push_back(160);
		_textRenderer = new Roger::RogerTextRenderer(ttf, sizes);
		// roger_ui_font_scale: global size multiplier (percent) on the role type scale.
		// 100 = the role's baseline cell height; larger = bigger text everywhere. Text
		// word-wraps and is capped to each box, so a larger scale grows text (and wraps)
		// rather than clipping. Default 150 (readable hires dialogs).
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
	}
	Graphics::ManagedSurface &scene = *scratchScene(_sceneCache->w, _sceneCache->h);
	scene.copyFrom(*_sceneCache); // fully overwrites the scratch buffer
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		if (_debugLog) {
			const Common::Array<Roger::UiElement> &els = _uiLayer->elements();
			warning("ROGER-UI: gameRect=(%d,%d,%d,%d)", _lastGameRect.left, _lastGameRect.top,
			        _lastGameRect.right, _lastGameRect.bottom);
			for (uint i = 0; i < els.size(); i++) {
				const Roger::UiElement &e = els[i];
				const Common::Rect d = Roger::sciRectToDest(e.nativeRect, _lastGameRect);
				warning("ROGER-UI: [%u] type=%d tok=%08x native=(%d,%d,%d,%d) dest=(%d,%d,%d,%d) text='%.24s'",
				        i, (int)e.type, e.token, e.nativeRect.left, e.nativeRect.top,
				        e.nativeRect.right, e.nativeRect.bottom, d.left, d.top, d.right, d.bottom,
				        e.text.c_str());
			}
		}
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, _lastGameRect, _textRenderer, _altTextRenderer);
	}
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

void FileRogerArtProvider::uiPushWindow(const Common::Rect &r, int backColor, int penColor,
                                        uint16 wndStyle, uint32 token) {
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
                                      int textRole, bool useAltFont) {
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiText; e.nativeRect = r; e.text = text ? text : "";
	e.penColor = penColor; e.backColor = backColor; e.fontId = fontId;
	e.align = align; e.token = token;
	e.textRole = textRole; e.useAltFont = useAltFont;
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushButton(const Common::Rect &r, const char *text, int fontId,
                                        int style, uint32 token) {
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiButton; e.nativeRect = r; e.text = text ? text : "";
	e.fontId = fontId; e.style = style; e.align = 1 /*center*/;
	e.backColor = 7 /*light gray*/; e.penColor = 0; e.hasFrame = true; e.token = token;
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushTextEdit(const Common::Rect &r, const char *text, int fontId,
                                          int style, int cursorPos, uint32 token) {
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiTextEdit; e.nativeRect = r; e.text = text ? text : "";
	e.fontId = fontId; e.style = style; e.cursorPos = cursorPos; e.align = 0;
	e.backColor = 15 /*white*/; e.penColor = 0; e.hasFrame = true; e.token = token;
	e.textRole = Roger::kRoleBody; // body size, same as the dialog prompt above it
	e.vAlignTop = true;            // SCI draws edit text at the top of the field, not centred
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushIcon(const Common::Rect &r, int viewId, int loopNo, int celNo,
                                      uint32 token) {
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

void FileRogerArtProvider::uiPushStatus(const Common::Rect &r, const char *text, int penColor,
                                        int backColor, uint32 token) {
	// Remember the banner so it can be re-applied on room load / F10 enable, even if
	// the overlay was not ready when the game first drew it.
	_haveStatus = true; _statusRect = r; _statusText = text ? text : "";
	_statusPen = penColor; _statusBack = backColor; _statusToken = token;
	if (!_overlayActive || !_plate) return;
	ensureUi();
	// The score banner and the menu bar share this token (top strip); drop whatever
	// is there (e.g. the menu bar's window + titles) before pushing the banner text.
	_uiLayer->clearToken(token);
	Roger::UiElement e;
	e.type = Roger::kUiText; e.nativeRect = r;
	// Strip SCI's stylized/high-bit glyphs the TTF lacks (e.g. a "III" title glyph)
	// so they do not render as tofu on the opaque status strip; ASCII is unchanged.
	e.text = Roger::stripUnrenderable(text ? text : "");
	e.penColor = penColor; e.backColor = backColor; e.align = 0;
	e.textRole = Roger::kRoleHeading; // banner is a heading; capped to the strip height
	e.useAltFont = true;              // header uses the updated font
	e.token = token;
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::reapplyStatus() {
	if (_haveStatus)
		uiPushStatus(_statusRect, _statusText.c_str(), _statusPen, _statusBack, _statusToken);
}

void FileRogerArtProvider::uiClearToken(uint32 token) {
	if (_uiLayer) _uiLayer->clearToken(token);
	if (_overlayActive && _plate) presentWithUi();
}

void FileRogerArtProvider::uiClearAll() {
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
			byte idx = bitmap[y * w + x];
			uint32 px;
			if (idx == clearKey) {
				px = rgba.ARGBToColor(0, 0, 0, 0);
			} else {
				const Color &c = pal.colors[idx];
				px = rgba.ARGBToColor(255, c.r, c.g, c.b);
			}
			surf->setPixel(x, y, px);
		}
	}

	return surf;
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

		// Provide a native-cel fallback for sprites that have no hires view art.
		// The compositor will use it only when getCel() returns nullptr.
		Graphics::Surface *nativeSurf = renderNativeCel(it->viewId, it->loopNo, it->celNo);
		s.celOverride = nativeSurf; // borrowed by the sprite (freed below)
		if (nativeSurf)
			nativeSurfaces.push_back(nativeSurf);

		sprites.push_back(s);
	}

	// One concise per-frame line when diagnostics are on (Ctrl+Shift+L, or
	// roger_debug=true). Per-sprite spam was removed; this is the heartbeat.
	if (dbg)
		warning("ROGER: pic=%d sprites=%u plate=%dx%d overlay=%s prioBytes=%u",
		        _loadedPicId, (unsigned)sprites.size(), _plate ? _plate->w : -1,
		        _plate ? _plate->h : -1, _overlayActive ? "on" : "off", _priorityMap.size());

	// renderFrame composites synchronously; free native surfaces after it returns.
	renderFrame(sprites);

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
	_haveScene = false;
	_loadedPicId = -1;
	g_system->hideOverlay();
}

void FileRogerArtProvider::onMouseMoved() {
	if (_useHwCursor)
		return; // the hardware cursor moves itself smoothly; no per-move recomposite
	// Fallback path only: re-present the cached scene (+ any UI) so the composited
	// cursor follows the pointer. Cheap when idle (a memcpy + overlay push); only
	// fires when the mouse moved. presentWithUi no-ops if there is no scene/overlay.
	presentWithUi();
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
	if (_cursorSurf) { _cursorSurf->free(); delete _cursorSurf; _cursorSurf = nullptr; }
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
}

} // namespace Sci
