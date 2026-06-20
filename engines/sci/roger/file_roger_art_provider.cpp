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
#include "graphics/managed_surface.h"
#include "common/array.h"
#include "common/path.h"
#include "common/fs.h"
#include "common/config-manager.h"
#include "common/system.h"

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

void FileRogerArtProvider::renderFromAnimateList(const AnimateList &list) {
	Common::Array<Roger::Sprite> sprites;
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
		sprites.push_back(s);
	}
	renderFrame(sprites);
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
