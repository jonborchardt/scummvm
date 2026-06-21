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
#include "sci/roger/roger_compositor.h"
#include "sci/roger/roger_coords.h"
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
	Common::FSNode v(Common::Path(visualPath(pictureId)));
	Common::FSNode p(Common::Path(priorityPath(pictureId)));
	Common::FSNode c(Common::Path(controlPath(pictureId)));
	return v.exists() && p.exists() && c.exists();
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

	// New room: drop any dialogs/icons left from the previous room so they do not
	// bleed onto the new scene. _haveScene is rebuilt by the next renderFrame.
	if (_uiLayer) _uiLayer->clearAll();
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
	_haveScene = false;

	// Evict previous room.
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }

	_plate = Roger::loadSurfaceRGBA(visualPath(pictureId));
	if (!_plate) {
		// No hires bg -> native shows. Clear the compositor's borrowed plate pointer
		// so it does not retain the plate we just deleted above.
		if (_compositor)
			_compositor->setRoom(nullptr, nullptr);
		_loadedPicId = -1;
		return;
	}

	if (!_viewCache)
		_viewCache = new Roger::ViewCache(_basePath + "/views");
	if (!_compositor)
		_compositor = new Roger::RogerCompositor();
	_compositor->setRoom(_plate, _viewCache);

	// Load the real EGA-color priority map for per-pixel overlay occlusion and
	// decode it to a band-per-pixel buffer. It is authored in picture space
	// (320x190 for SCI0) aligned with the plate, so picScreenTop = 0.
	int prW = 0, prH = 0;
	_priorityMap.clear();
	if (loadPriorityBands(occlusionPriorityPath(pictureId), _priorityMap, prW, prH)) {
		_compositor->setPicture(320, prH, 0);
		_compositor->setPriorityMask(_priorityMap.begin(), prW, prH);
	} else {
		// No occlusion map -> sprites still draw, just without occlusion.
		warning("ROGER: no occlusion priority map at %s", occlusionPriorityPath(pictureId).c_str());
		_compositor->setPicture(320, 190, 0);
		_compositor->setPriorityMask(nullptr, 0, 0);
	}
	_loadedPicId = pictureId;
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

	Graphics::ManagedSurface scene(OW, OH, rgba);
	_compositor->renderScene(scene, sprites);

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
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, gameRect, _textRenderer);
	}
	_compositor->presentToOverlay(scene);

	// roger_autoshot (verification harness): dump once per room. Deterministic — no
	// keystrokes/focus needed. Output goes to the configured screenshotpath. Two PNGs:
	//   roger-<id>-overlay.png — Roger's composited layer alone (letterbox + status
	//                            strip are transparent, shown as black by a viewer)
	//   roger-<id>-preview.png — the true on-screen result: the native 320x200 game
	//                            scaled into gameRect with Roger's layer blended over
	//                            it (mirrors the backend draw order), so plate/native
	//                            alignment and the status bar showing through are visible.
	if (_autoshot && _autoshotPicId != _loadedPicId) {
		Common::String dir;
		if (ConfMan.hasKey("screenshotpath"))
			dir = ConfMan.getPath("screenshotpath").toString('/');
		if (!dir.empty() && dir.lastChar() != '/')
			dir += '/';
		const Common::String base = dir + Common::String::format("roger-%d", _loadedPicId);

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
		_autoshotPicId = _loadedPicId;
	}
}

void FileRogerArtProvider::ensureUi() {
	if (!_uiLayer)
		_uiLayer = new Roger::RogerUiLayer();
	if (!_textRenderer) {
		Common::String ttf = "FreeSans.ttf";
		if (ConfMan.hasKey("roger_ui_font"))
			ttf = ConfMan.get("roger_ui_font");
		// A ladder of pixel sizes for fit-to-box selection (cell mode, hires).
		Common::Array<int> sizes;
		sizes.push_back(18); sizes.push_back(24); sizes.push_back(32);
		sizes.push_back(42); sizes.push_back(56); sizes.push_back(72);
		_textRenderer = new Roger::RogerTextRenderer(ttf, sizes);
	}
}

void FileRogerArtProvider::presentWithUi() {
	if (!_overlayActive || !_compositor || !_haveScene || !_sceneCache)
		return;
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::ManagedSurface scene(_sceneCache->w, _sceneCache->h, rgba);
	scene.copyFrom(*_sceneCache);
	if (_uiLayer && !_uiLayer->empty() && _textRenderer) {
		byte pal[256 * 3];
		g_system->getPaletteManager()->grabPalette(pal, 0, 256);
		_compositor->renderUiLayer(scene, _uiLayer->elements(), pal, _lastGameRect, _textRenderer);
	}
	_compositor->presentToOverlay(scene);
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
                                      int backColor, int fontId, int align, uint32 token) {
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiText; e.nativeRect = r; e.text = text ? text : "";
	e.penColor = penColor; e.backColor = backColor; e.fontId = fontId;
	e.align = align; e.token = token;
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
	_uiLayer->push(e);
	presentWithUi();
}

void FileRogerArtProvider::uiPushIcon(const Common::Rect &r, int viewId, int loopNo, int celNo,
                                      uint32 token) {
	if (!_overlayActive || !_plate) return;
	ensureUi();
	Roger::UiElement e;
	e.type = Roger::kUiIcon; e.nativeRect = r; e.token = token;
	Graphics::Surface *cel = renderNativeCel(viewId, loopNo, celNo);
	if (cel) { _uiIcons.push_back(cel); e.iconSurface = cel; }
	_uiLayer->push(e);
	presentWithUi();
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

void FileRogerArtProvider::hideOverlayForUI() {
	g_system->hideOverlay();
}

void FileRogerArtProvider::toggleOverlay() {
	_overlayActive = !_overlayActive;
	if (!_overlayActive)
		g_system->hideOverlay(); // reveal the native 320x200 render underneath
	// When re-enabled, the next kernelAnimate frame re-composites and re-shows it.
	warning("ROGER: overlay %s", _overlayActive ? "ENABLED (upscaled)" : "DISABLED (original)");
}

void FileRogerArtProvider::toggleDebugLog() {
	_debugLog = !_debugLog;
	warning("ROGER: debug logging %s", _debugLog ? "ON" : "OFF");
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

FileRogerArtProvider::~FileRogerArtProvider() {
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	delete _viewCache; _viewCache = nullptr;
	delete _compositor; _compositor = nullptr;
	delete _uiLayer; _uiLayer = nullptr;
	delete _textRenderer; _textRenderer = nullptr;
	if (_sceneCache) { delete _sceneCache; _sceneCache = nullptr; }
	for (uint i = 0; i < _uiIcons.size(); i++) { _uiIcons[i]->free(); delete _uiIcons[i]; }
	_uiIcons.clear();
}

} // namespace Sci
