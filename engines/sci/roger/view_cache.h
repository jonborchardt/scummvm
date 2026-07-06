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

namespace Graphics { struct Surface; }

namespace Sci {
namespace Roger {
class RogerAssetGen;

class ViewCache {
public:
	explicit ViewCache(const Common::String &viewsBasePath);
	~ViewCache();
	ViewCache(const ViewCache &) = delete;
	ViewCache &operator=(const ViewCache &) = delete;

	// Borrowed pointer owned by the cache; nullptr if generation is unavailable
	// (no generator / prebuilt mode) or the cel could not be generated.
	const Graphics::Surface *getCel(int viewId, int loopNo, int celNo);

	// Generator source: when set and mode != kGenPrebuilt, getCel generates each
	// cel on first use and caches it. This is the only cel source.
	void setGenerator(RogerAssetGen *g) { _gen = g; }

	// TEMPORARY DEBUG TOOL (tune panel, spec 2026-07-05) — delete with the
	// panel unless another caller has adopted it by then. Frees every cached
	// cel (including cached-nullptr misses); next getCel regenerates.
	void clear();

	// White-box test shims (test_view_cache.h only).
	uint genCelCount() const { return _genCels.size(); }
	void insertForTest(int viewId, int loopNo, int celNo, Graphics::Surface *s);

private:
	RogerAssetGen *_gen = nullptr;
	// Generated cels (owned). Key "viewId/loopNo/celNo". nullptr entries are
	// cached to avoid re-attempting a known-missing cel.
	Common::HashMap<Common::String, Graphics::Surface *> _genCels;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_VIEW_CACHE_H
