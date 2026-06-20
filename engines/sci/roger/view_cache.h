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

#ifndef SCI_ROGER_VIEW_CACHE_H
#define SCI_ROGER_VIEW_CACHE_H

#include "common/str.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/array.h"

namespace Graphics { struct Surface; }

namespace Sci {
namespace Roger {

class ViewCache {
public:
	explicit ViewCache(const Common::String &viewsBasePath);
	~ViewCache();

	// Borrowed pointer owned by the cache; nullptr if asset missing.
	const Graphics::Surface *getCel(int viewId, int loopNo, int celNo);

private:
	struct Loop {
		Graphics::Surface *sheet;            // full spritesheet (owned)
		Common::Array<Graphics::Surface *> cels; // sub-surfaces (owned)
	};
	Common::String _base;
	Common::HashMap<Common::String, Loop> _loops;  // key "viewId/loopNo"
	Loop *loadLoop(int viewId, int loopNo);
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_VIEW_CACHE_H
