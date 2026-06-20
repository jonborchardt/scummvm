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

#ifndef SCI_ROGER_FILE_ROGER_ART_PROVIDER_H
#define SCI_ROGER_FILE_ROGER_ART_PROVIDER_H

#include "sci/roger/roger_art_provider.h"
#include "sci/roger/null_roger_art_provider.h"
#include "common/str.h"
#include "common/path.h"

namespace Sci {

class FileRogerArtProvider : public RogerArtProvider {
public:
	// gameId: ScummVM game ID string (e.g. "sq3", "qfg1")
	// gamePath: path to the game directory, as a Common::Path so native
	//   separators are parsed correctly (use ConfMan.getPath("path"), NOT
	//   ConfMan.get("path") — the latter is a raw string with backslashes on
	//   Windows that Common::Path's '/' separator cannot split).
	FileRogerArtProvider(const Common::String &gameId, const Common::Path &gamePath);

	bool hasBackground(GuiResourceId pictureId) const override;
	bool loadBuffers(GuiResourceId pictureId, GfxScreen *screen) override;
	void pushHiresBackground(GuiResourceId pictureId) override;

	// Test-only accessors — expose private path helpers for white-box testing
	Common::String testVisualPath(GuiResourceId id) const { return visualPath(id); }
	Common::String testPriorityPath(GuiResourceId id) const { return priorityPath(id); }
	Common::String testControlPath(GuiResourceId id) const { return controlPath(id); }

private:
	Common::String _basePath;       // absolute path to <gameid>-roger/ directory
	Common::String _visualVariant;  // hires visual variant, e.g. "omyac-upscaler" ("" = plain pic.<id>.png)

	Common::String picDir(GuiResourceId id) const;
	Common::String visualPath(GuiResourceId id) const;
	Common::String priorityPath(GuiResourceId id) const;
	Common::String controlPath(GuiResourceId id) const;
};

} // namespace Sci

#endif // SCI_ROGER_FILE_ROGER_ART_PROVIDER_H
