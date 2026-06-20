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
#include "sci/graphics/screen.h"
#include "common/path.h"
#include "common/fs.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace Sci {

FileRogerArtProvider::FileRogerArtProvider(const Common::String &gameId,
                                            const Common::Path &gamePath) {
	// Build path: gamePath/../<gameId>-roger/ using pure path ops (no FS access).
	// gamePath is already a Common::Path (native separators parsed), so
	// getParent() works on Windows backslash paths too.
	Common::Path rogerPath = gamePath.getParent().appendComponent(gameId + "-roger");
	_basePath = rogerPath.toString('/');
}

Common::String FileRogerArtProvider::picDir(GuiResourceId id) const {
	return _basePath + "/pics/" + Common::String::format("%d", id) + "/source/";
}

Common::String FileRogerArtProvider::visualPath(GuiResourceId id) const {
	return picDir(id) + "pic." + Common::String::format("%d", id) + ".png";
}

Common::String FileRogerArtProvider::priorityPath(GuiResourceId id) const {
	return picDir(id) + "pic." + Common::String::format("%d", id) + "_p.png";
}

Common::String FileRogerArtProvider::controlPath(GuiResourceId id) const {
	return picDir(id) + "pic." + Common::String::format("%d", id) + "_c.png";
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
#ifdef __EMSCRIPTEN__
	// The Emscripten port serves all game data over HTTP from DATA_PATH ("/data")
	// via ScummVM's HTTP filesystem — it is NOT preloaded into the MEMFS. So the
	// hires PNG cannot be read with FS.readFile(); instead point an <img> at its
	// HTTP URL and let the browser fetch it. visualPath() already yields the
	// server-absolute URL (e.g. "/data/games/sq3-roger/pics/2/source/pic.2.png"):
	// the same path string the HTTP filesystem maps a node's _url to.
	// (The priority/control maps are loaded transparently over the same HTTP
	// filesystem in loadBuffers(), so no explicit fetch is needed for those.)
	Common::String url = visualPath(pictureId);
	EM_ASM({
		var url = UTF8ToString($0);
		var canvas = document.getElementById('roger-canvas');
		if (!canvas) return;
		var img = new Image();
		img.onload = function() {
			canvas.width  = img.naturalWidth;
			canvas.height = img.naturalHeight;
			canvas.getContext('2d').drawImage(img, 0, 0);
		};
		img.onerror = function() {
			console.warn('roger: failed to load hires background', url);
		};
		img.src = url;
	}, url.c_str());
#endif
}

} // namespace Sci

#ifdef __EMSCRIPTEN__
extern "C" {
	EMSCRIPTEN_KEEPALIVE void roger_set_enabled(int enabled) {
		if (Sci::g_sciRogerProvider)
			Sci::g_sciRogerProvider->enabled = (enabled != 0);
		EM_ASM({
			var canvas = document.getElementById('roger-canvas');
			if (canvas) canvas.style.opacity = $0 ? '1' : '0';
		}, enabled);
	}

	EMSCRIPTEN_KEEPALIVE void roger_set_opacity(float opacity) {
		EM_ASM({
			var canvas = document.getElementById('roger-canvas');
			if (canvas) canvas.style.opacity = $0;
		}, opacity);
	}
}
#endif
