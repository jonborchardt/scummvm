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

#ifndef SCI_ROGER_ROGER_COMPOSITOR_H
#define SCI_ROGER_ROGER_COMPOSITOR_H

#include "common/array.h"
#include "common/rect.h"

namespace Graphics { struct Surface; class ManagedSurface; }

namespace Sci {
namespace Roger {

class ViewCache;
class SliceSet;

struct Sprite {
	int viewId, loopNo, celNo;
	Common::Rect celRect;  // 320x200 space
	int priority;          // SCI band 0..15
	bool mirror;
};

class RogerCompositor {
public:
	RogerCompositor() : _plate(nullptr), _slices(nullptr), _views(nullptr) {}

	// Borrowed pointers; lifetime managed by the caller (the provider).
	void setRoom(Graphics::Surface *cleanPlate, SliceSet *slices, ViewCache *views);

	// Compose dest = plate + sprites (assumed back-to-front) + occluding slices.
	void renderScene(Graphics::ManagedSurface &dest, const Common::Array<Sprite> &sprites);

	// Push scene to the OSystem overlay and make it visible.
	void presentToOverlay(Graphics::ManagedSurface &scene);

private:
	Graphics::Surface *_plate;
	SliceSet *_slices;
	ViewCache *_views;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_COMPOSITOR_H
