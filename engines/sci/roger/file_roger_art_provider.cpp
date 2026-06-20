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
#include "common/fs.h"
#include "common/str.h"

namespace Sci {

FileRogerArtProvider::FileRogerArtProvider(const Common::String &gameId,
                                            const Common::String &gamePath) {
	// Build path: gamePath/../<gameId>-roger/
	Common::FSNode gameNode(Common::Path(gamePath));
	Common::FSNode parentNode = gameNode.getParent();
	Common::FSNode rogerNode = parentNode.getChild(gameId + "-roger");
	_basePath = rogerNode.getPath().toString('/');
}

Common::String FileRogerArtProvider::picDir(GuiResourceId id) const {
	return _basePath + "/pics/" + Common::String::format("%d", id) + "/";
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

bool FileRogerArtProvider::loadBuffers(GuiResourceId, GfxScreen *) {
	// Implemented in Task 6
	return false;
}

void FileRogerArtProvider::pushHiresBackground(GuiResourceId) {
	// Implemented in Task 8 (Emscripten canvas bridge)
}

} // namespace Sci
