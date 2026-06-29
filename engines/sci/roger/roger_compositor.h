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
#include "sci/roger/roger_capabilities.h"
#include "sci/roger/roger_ui_layer.h"
#include "sci/roger/roger_effects.h"

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

// Compare two w*h byte (EGA index) buffers; emit coalesced bounding boxes covering the
// changed pixels (Feeder B diff backstop). Empty `out` when the buffers are identical.
void extractChangedBoxes(const byte *prev, const byte *cur, int w, int h,
                         Common::Array<Common::Rect> &out);

// Drop capture rects that overlap any live animate-cast sprite rect (native/320x200
// coordinates). Used to scope native-foreground capture so moving actors are never
// re-captured as static images (they are drawn by the sprite path). Appends to `out`.
void filterForegroundCaptureRegions(const Common::Array<Common::Rect> &captured,
                                    const Common::Array<Common::Rect> &liveSpriteRects,
                                    Common::Array<Common::Rect> &out);

// Merge addToPic (static) and animate sprites into one back-to-front draw list:
// static first, then animate, then a STABLE sort by ascending priority. Stable ⇒ at
// equal priority addToPic draws before animate (native bakes addToPic into the pic
// first). n is small (a handful of sprites), so insertion sort is fine.
void mergeSpritesByPriority(const Common::Array<Sprite> &animate,
                            const Common::Array<Sprite> &staticSprites,
                            Common::Array<Sprite> &out);

// Map a native SCI screen-space rect (320x200; picture window starts at row
// picScreenTop, dims picW x picH) into overlay space, using the SAME integer scaler
// renderScene uses for cel rects, so a captured native region lines up with the plate.
Common::Rect mapNativeRectToOverlay(const Common::Rect &nativeRect,
                                    const Common::Rect &picRect,
                                    int picW, int picH, int picScreenTop);

// Nearest-neighbour upscale of a native EGA region (visual indices, expanded through a
// 256*3 RGB palette) into dest's overlayRect (RGBA32, opaque). Used by Feeder B to
// composite native draws Roger has no vector source for.
void upscaleNativeRegionNearest(Graphics::Surface &dest, const Common::Rect &overlayRect,
                                const byte *visual, int visualPitch,
                                const Common::Rect &nativeRect, const byte *palette);

class RogerCompositor {
public:
	RogerCompositor() : _plate(nullptr), _views(nullptr),
		_picW(320), _picH(190), _priority(nullptr), _priorityW(0), _priorityH(0), _picScreenTop(0),
		_bgCache(nullptr), _bgPlate(nullptr), _overlayConv(nullptr) {}
	~RogerCompositor();

	// Store probed game capabilities (called once per room load by the provider).
	void setCapabilities(const RogerCapabilities &caps) { _caps = caps; }

	// Native (SCI rows) -> overlay px, using the probed screen height. Pure.
	int nativeRowsToOverlay(int nativeRows, int overlayH) const {
		const int rows = _caps.screenRows > 0 ? _caps.screenRows : 200;
		return nativeRows * overlayH / rows;
	}

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

	// roger_diag: revertible seed/present trace (off by default). See file_roger_art_provider.
	void setDiag(bool on) { _diag = on; }

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

	// Time-boxed full-screen transition between two composed RGBA32 scenes (full-overlay
	// sized). Renders into `scratch`, full-presents each step, returns after presenting
	// `to`. Runs synchronously (blocks, like SCI's own GfxTransitions::doit). durationMs<=0
	// => instant swap (present `to` once). sciTypeHint is the raw SCI transition type
	// (transitions.h enum) used to pick wipe direction when fam==kFxWipe; 0 is safe.
	void runTransition(Graphics::ManagedSurface &from, Graphics::ManagedSurface &to,
	                   Graphics::ManagedSurface &scratch, TransitionFamily fam, int durationMs,
	                   int sciTypeHint = 0);
	// Offset-present `scene` for `shakeCount` jolts (directions: bit0=vertical, bit1=horizontal),
	// magnitudePx in overlay pixels. Restores `scene` at rest before returning.
	void runShake(Graphics::ManagedSurface &scene, Graphics::ManagedSurface &scratch,
	              int shakeCount, int directions, int magnitudePx);
	// Geometry accessors (used by FileRogerArtProvider::drawGenericRegions for Feeder B).
	int picW() const { return _picW; }
	int picH() const { return _picH; }
	int picScreenTop() const { return _picScreenTop; }

	// Invalidate the static-background cache so the next renderScene rebuilds it from
	// the (now-mutated) plate. Call after any in-place plate pixel mutation (e.g. live
	// palette re-blend) so the stale pre-mutation copy isn't re-used next frame.
	void invalidateBackgroundCache() { _bgPlate = nullptr; }

	// Force the next frame to be a CLEAN first frame after a room change that was reached
	// via a real transition. onTransition()'s composeRoomScene() pre-warms _bgCache, which
	// would otherwise let the first post-transition renderFrame take the bounded-seed +
	// dirty-present path using the PREVIOUS room's stale dirty-rect history (never cleared
	// on room change) — the QFG1 fresh-start town breakage (stale/missing regions, native
	// bleed-through). This nulls _bgPlate (=> next renderScene full-seeds + rebuilds the bg),
	// sets _bgRebuilt (=> next present is full), and drops all leftover dirty rects. An
	// instant-cut / save-restore entry already gets this clean first frame for free
	// (composeRoomScene doesn't run, so _bgPlate stays null from setRoom); this makes a
	// transition-entry behave identically. O(1), called once per room entry — no per-cycle cost.
	void resetForRoomChange() {
		_bgPlate = nullptr;
		_bgRebuilt = true;
		_dirtyCur.clear();
		_dirtyPrev.clear();
		_sceneDirtyCur.clear();
		_sceneDirtyPrev.clear();
		_lastSeedUnion.clear();
		_framesSinceFullPresent = 0;
		_framesSinceFullSeed = 0;
	}

	// Coalesced union (clamped to _bgGameRect) of every sprite's current and just-vacated
	// dest-rect that renderScene re-seeded the background over this frame. Equal to
	// coalesce(_sceneDirtyCur ∪ _sceneDirtyPrev). The provider uses this to bound its own
	// per-frame scene-cache copies to the regions that actually changed.
	const Common::Array<Common::Rect> &lastSeedUnion() const { return _lastSeedUnion; }
	// True when the last renderScene re-seeded the WHOLE game region (rebuild, empty
	// gameRect, or a periodic heal) — in that case lastSeedUnion() is not authoritative and
	// the caller must do a full-region copy. False => only lastSeedUnion() changed.
	bool lastSceneWasFull() const { return _lastSceneFull; }

private:
	RogerCapabilities _caps;   // set by setCapabilities(); read by render methods (Task 3+)
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
	bool _diag = false; // roger_diag seed/present trace (revertible instrumentation)
	int _framesSinceFullPresent = 0; // periodic full-present heal counter
	// Sprite dirty rects, tracked at renderScene granularity (NOT present granularity): rolled
	// cur->prev at the TOP of renderScene, so an intervening UI-only present (presentWithUi,
	// no renderScene) cannot discard the previous sprite positions. dirtyUnion adds these, so
	// a sprite that moves between two renderScene calls repaints the clean background it
	// vacated even if several UI-only presents happened in between. Without this split, a
	// mouse-move during an animation clobbered _dirtyPrev and left a shadow of old frames.
	Common::Array<Common::Rect> _sceneDirtyCur, _sceneDirtyPrev;

	// Bounded background-seed bookkeeping. renderScene re-seeds _bgCache->dest only over the
	// coalesced union of this frame's + last frame's sprite rects (everything else in the
	// persistent scratch surface is still correct), instead of the whole ~22MB game region.
	// _lastSeedUnion is that union; _lastSceneFull records whether a full-region seed ran
	// instead (rebuild/empty/heal). _framesSinceFullSeed drives a periodic full-seed heal,
	// independent of presentToOverlay's own present heal (each layer self-heals).
	Common::Array<Common::Rect> _lastSeedUnion;
	bool _lastSceneFull = true;
	int _framesSinceFullSeed = 0;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_COMPOSITOR_H
