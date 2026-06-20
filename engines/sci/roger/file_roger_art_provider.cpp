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
#include "graphics/managed_surface.h"
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

Common::String FileRogerArtProvider::slicedDir(GuiResourceId id) const {
	return _basePath + "/pics/" + Common::String::format("%d", id) + "/sliced";
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

	// Evict previous room.
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	delete _slices; _slices = nullptr;

	_plate = Roger::loadSurfaceRGBA(visualPath(pictureId));
	if (!_plate) {
		// No hires bg -> native shows. Clear the compositor's borrowed pointers so
		// it does not retain the slice/plate we just deleted above.
		if (_compositor)
			_compositor->setRoom(nullptr, nullptr, nullptr);
		_loadedPicId = -1;
		return;
	}

	_slices = new Roger::SliceSet(slicedDir(pictureId));
	_slices->load(); // ok if it returns false (no slices -> no occlusion)

	if (!_viewCache)
		_viewCache = new Roger::ViewCache(_basePath + "/views");
	if (!_compositor)
		_compositor = new Roger::RogerCompositor();
	_compositor->setRoom(_plate, _slices, _viewCache);
	_loadedPicId = pictureId;
}

void FileRogerArtProvider::renderFrame(const Common::Array<Roger::Sprite> &sprites) {
	if (!_compositor || !_plate)
		return;
	Graphics::ManagedSurface scene(g_system->getOverlayWidth(),
	                               g_system->getOverlayHeight(),
	                               g_system->getOverlayFormat());
	_compositor->renderScene(scene, sprites);
	_compositor->presentToOverlay(scene);
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

	Palette *pal = view->getPalette();
	if (!pal)
		return nullptr;

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
				const Color &c = pal->colors[idx];
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

void FileRogerArtProvider::onNativePicture() {
	if (_compositor)
		_compositor->setRoom(nullptr, nullptr, nullptr);
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	delete _slices; _slices = nullptr;
	_loadedPicId = -1;
	g_system->hideOverlay();
}

FileRogerArtProvider::~FileRogerArtProvider() {
	if (_plate) { _plate->free(); delete _plate; _plate = nullptr; }
	delete _slices; _slices = nullptr;
	delete _viewCache; _viewCache = nullptr;
	delete _compositor; _compositor = nullptr;
}

} // namespace Sci
