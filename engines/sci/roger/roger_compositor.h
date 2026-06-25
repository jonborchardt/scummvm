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
#include "sci/roger/roger_ui_layer.h"

namespace Graphics { struct Surface; class ManagedSurface; }

namespace Sci {
namespace Roger {

class ViewCache;
class RogerTextRenderer;

struct Sprite {
	int viewId, loopNo, celNo;
	Common::Rect celRect;  // 320x200 space
	int priority;          // SCI band 0..15
	bool mirror;
	const Graphics::Surface *celOverride = nullptr; // optional pre-rendered native cel (RGBA), borrowed; used when no hires cel
};

// Clamp each rect in `in` to `bounds`, drop empties, and merge any that intersect into
// their bounding union (repeated until no two output rects intersect). Output rects are
// all inside `bounds`. Used to turn a frame's collected dynamic rects into the minimal
// set of regions to convert+push.
void coalesceDirtyRects(const Common::Array<Common::Rect> &in, const Common::Rect &bounds,
                        Common::Array<Common::Rect> &out);

class RogerCompositor {
public:
	RogerCompositor() : _plate(nullptr), _views(nullptr),
		_picW(320), _picH(190), _priority(nullptr), _priorityW(0), _priorityH(0), _picScreenTop(0),
		_bgCache(nullptr), _bgPlate(nullptr), _overlayConv(nullptr) {}
	~RogerCompositor();

	// Borrowed pointers; lifetime managed by the caller (the provider).
	void setRoom(Graphics::Surface *cleanPlate, ViewCache *views);

	// Logical SCI picture dimensions (cel rects are in this space — 320x190 for
	// SCI0) and the screen row where the picture begins (the menu-bar offset, used
	// to index the screen-space priority map). The plate encodes this picture.
	void setPicture(int picW, int picH, int picScreenTop);

	// Per-pixel occlusion source: SCI's screen-space priority map (one byte per
	// pixel = SCI priority band 0..15). Borrowed; lifetime managed by the caller.
	// A sprite pixel is hidden (the plate's baked-in foreground shows) wherever the
	// priority there exceeds the sprite's priority — exactly SCI's own occlusion.
	void setPriorityMask(const byte *priority, int priW, int priH);

	// Overlay-space rect where the picture (plate + sprites) is drawn. The caller
	// computes this (roger_coords::computeGameRect → computePictureRect) so it
	// coincides with the native game's on-screen picture region (below the status
	// bar), keeping the alpha-blended overlay aligned with the native game. When
	// left unset (empty), renderScene falls back to the full destination surface.
	void setPictureDest(const Common::Rect &r) { _pictureDest = r; }

	// Dirty-rect present: record a dest-space UI/cursor rect that changed this present.
	// renderUiLayer (UI) calls this internally; the provider calls it for the composited
	// cursor. (Sprite rects are recorded separately by renderScene into the scene-granularity
	// set — see _sceneDirtyCur.) presentToOverlay pushes the dirtyUnion of all of these.
	void addDirtyRect(const Common::Rect &destRect) { if (!destRect.isEmpty()) _dirtyCur.push_back(destRect); }
	// Enable/disable dirty present (roger_dirty_present knob). When off, presentToOverlay
	// always does a full region push (the pre-dirty behavior).
	void setDirtyPresent(bool enabled) { _dirtyPresent = enabled; }

	// Coalesced union (clamped to bounds) of every dynamic dirty rect that may need
	// repainting this present: UI/cursor rects at PRESENT granularity (_dirtyCur this
	// present + _dirtyPrev last present) plus sprite rects at SCENE/renderScene granularity
	// (_sceneDirtyCur + _sceneDirtyPrev). presentToOverlay's dirty path pushes exactly this.
	// Sprite rects are tracked separately because a UI-only present (presentWithUi: cursor/
	// dialog, NO renderScene) must not roll the sprite history away, or a sprite that moves
	// across that present leaves a "shadow of old animation frames." Pure (no g_system);
	// exposed so tests can assert a vacated position is covered.
	void dirtyUnion(const Common::Rect &bounds, Common::Array<Common::Rect> &out) const;
	// Advance present-granularity bookkeeping: roll this present's UI/cursor rects (_dirtyCur)
	// into _dirtyPrev and clear _dirtyCur. Sprite rects roll separately, in renderScene.
	// presentToOverlay calls this once per present; exposed so tests can simulate presents.
	void rollPresentDirty();

	// Compose dest = plate + sprites (back-to-front) with per-pixel priority occlusion.
	// gameRect (overlay-space, from computeGameRect) bounds the displayed game: the area
	// OUTSIDE it (the letterbox) is filled opaque black so the native render/cursor cannot
	// leak through, while the reserved status strip inside it stays transparent (native
	// Sierra menu icon shows through). Empty gameRect (tests) => whole surface opaque black.
	void renderScene(Graphics::ManagedSurface &dest, const Common::Array<Sprite> &sprites,
	                 const Common::Rect &gameRect = Common::Rect());

	// Push scene to the OSystem overlay and make it visible.
	void presentToOverlay(Graphics::ManagedSurface &scene);

	// Render the UI display-list on top of an already-composed scene. palette is
	// 256*3 RGB (may be null -> fills skipped). gameRect is the 320x200 on-screen
	// placement (computeGameRect). text may be null (text/caret skipped) for tests.
	// altText renders elements flagged useAltFont (header/menu font); null => use text.
	void renderUiLayer(Graphics::ManagedSurface &dest, const Common::Array<UiElement> &elems,
	                   const byte *palette, const Common::Rect &gameRect,
	                   const RogerTextRenderer *text, const RogerTextRenderer *altText = nullptr);

private:
	Graphics::Surface *_plate;
	ViewCache *_views;
	int _picW, _picH;          // logical SCI picture size (cel-rect coordinate space)
	const byte *_priority;     // screen-space priority map (borrowed), or nullptr
	int _priorityW, _priorityH;
	int _picScreenTop;         // screen row where the picture starts (menu-bar offset)
	Common::Rect _pictureDest; // overlay-space picture rect (set by the caller; empty => full surface)

	// Static-background cache. The compose+present runs synchronously inside SCI's
	// kAnimate cycle, so re-scaling the 1920x1140 plate into the game rect every frame
	// throttles the game clock. The letterbox + scaled plate never change within a room,
	// so build them once into _bgCache and seed each frame with a straight copy.
	// Invalidated on room change (setRoom) and on any geometry/size change.
	Graphics::ManagedSurface *_bgCache;
	Graphics::Surface *_bgPlate;   // plate the cache was built from (re-validate within a room)
	Common::Rect _bgPicRect, _bgGameRect;
	// Set when renderScene (re)draws the static background this frame (room/geometry
	// change). presentToOverlay then pushes the FULL overlay to lay down the letterbox;
	// otherwise it pushes only the game region (everything dynamic — sprites, dialogs,
	// cursor — is inside gameRect, so the static black letterbox needn't be re-converted
	// and re-pushed every frame).
	bool _bgRebuilt = false;
	// Persistent overlay-format buffer for presentToOverlay's RGBA32->overlay conversion,
	// so we don't allocate+free a full-overlay surface every frame (convertTo did).
	Graphics::Surface *_overlayConv;

	// Dirty-rect present accumulators (dest/overlay space). _dirtyCur is filled each frame
	// as the scene is composited; presentToOverlay pushes _dirtyCur ∪ _dirtyPrev (last
	// frame's, so a moved sprite/cursor repaints the clean background it vacated), then
	// rolls _dirtyCur into _dirtyPrev and clears _dirtyCur. Off by default until wired.
	Common::Array<Common::Rect> _dirtyCur, _dirtyPrev;
	bool _dirtyPresent = false;
	int _framesSinceFullPresent = 0; // periodic full-present heal counter
	// Sprite dirty rects, tracked at renderScene granularity (NOT present granularity): rolled
	// cur->prev at the TOP of renderScene, so an intervening UI-only present (presentWithUi,
	// no renderScene) cannot discard the previous sprite positions. dirtyUnion adds these, so
	// a sprite that moves between two renderScene calls repaints the clean background it
	// vacated even if several UI-only presents happened in between. Without this split, a
	// mouse-move during an animation clobbered _dirtyPrev and left a shadow of old frames.
	Common::Array<Common::Rect> _sceneDirtyCur, _sceneDirtyPrev;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_COMPOSITOR_H
