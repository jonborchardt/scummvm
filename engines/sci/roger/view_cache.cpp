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

#include "graphics/surface.h"
#include "sci/roger/view_cache.h"
#include "sci/roger/roger_asset_gen.h"

namespace Sci {
namespace Roger {

// viewsBasePath is retained in the constructor signature for API compatibility,
// but cels now come solely from the generator — no on-disk spritesheet layout is read.
ViewCache::ViewCache(const Common::String &viewsBasePath) {
	(void)viewsBasePath;
}

ViewCache::~ViewCache() {
	for (Common::HashMap<Common::String, Graphics::Surface *>::iterator it = _genCels.begin(); it != _genCels.end(); ++it) {
		if (it->_value) {
			it->_value->free();
			delete it->_value;
		}
	}
	_genCels.clear();
}

const Graphics::Surface *ViewCache::getCel(int viewId, int loopNo, int celNo) {
	if (_gen && _gen->mode() != Roger::kGenPrebuilt) {
		Common::String key = Common::String::format("%d/%d/%d", viewId, loopNo, celNo);
		if (_genCels.contains(key))
			return _genCels[key];                 // cached (incl. cached nullptr = known-missing)
		uint32 ms = 0;
		Graphics::Surface *gen = _gen->generateViewCel(viewId, loopNo, celNo, ms);
		_genCels[key] = gen;
		return gen;
	}
	return nullptr;                               // prebuilt mode / no generator: no generated source
}

} // namespace Roger
} // namespace Sci
