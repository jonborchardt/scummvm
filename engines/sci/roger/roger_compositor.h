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
	const Graphics::Surface *celOverride = nullptr; // optional pre-rendered native cel (RGBA), borrowed; used when no hires cel
};

class RogerCompositor {
public:
	RogerCompositor() : _plate(nullptr), _slices(nullptr), _views(nullptr),
		_picW(320), _picH(190), _priority(nullptr), _priorityW(0), _priorityH(0), _picScreenTop(0) {}

	// Borrowed pointers; lifetime managed by the caller (the provider).
	// (slices is retained for API compatibility but no longer used — occlusion is
	// now per-pixel via the priority mask, see setPriorityMask.)
	void setRoom(Graphics::Surface *cleanPlate, SliceSet *slices, ViewCache *views);

	// Logical SCI picture dimensions (cel rects are in this space — 320x190 for
	// SCI0) and the screen row where the picture begins (the menu-bar offset, used
	// to index the screen-space priority map). The plate encodes this picture.
	void setPicture(int picW, int picH, int picScreenTop);

	// Per-pixel occlusion source: SCI's screen-space priority map (one byte per
	// pixel = SCI priority band 0..15). Borrowed; lifetime managed by the caller.
	// A sprite pixel is hidden (the plate's baked-in foreground shows) wherever the
	// priority there exceeds the sprite's priority — exactly SCI's own occlusion.
	void setPriorityMask(const byte *priority, int priW, int priH);

	// Compose dest = plate + sprites (back-to-front) with per-pixel priority occlusion.
	void renderScene(Graphics::ManagedSurface &dest, const Common::Array<Sprite> &sprites);

	// Push scene to the OSystem overlay and make it visible.
	void presentToOverlay(Graphics::ManagedSurface &scene);

private:
	Graphics::Surface *_plate;
	SliceSet *_slices;
	ViewCache *_views;
	int _picW, _picH;          // logical SCI picture size (cel-rect coordinate space)
	const byte *_priority;     // screen-space priority map (borrowed), or nullptr
	int _priorityW, _priorityH;
	int _picScreenTop;         // screen row where the picture starts (menu-bar offset)
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_COMPOSITOR_H
