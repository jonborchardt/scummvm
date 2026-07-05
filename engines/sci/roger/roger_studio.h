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

#ifndef SCI_ROGER_ROGER_STUDIO_H
#define SCI_ROGER_ROGER_STUDIO_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_studio_render.h"
#include "sci/roger/roger_view_scaler.h"

namespace Graphics { class ManagedSurface; struct Surface; }
namespace Common { struct Event; }

namespace Sci {
namespace Roger {

// Debug-only interactive tuning environment (ROGER_STUDIO=1 /
// build_and_run.ps1 -Studio). Owns the overlay; never touches the disk cache
// (own RogerAssetGen in kGenMemory with an empty cache dir). See
// docs/superpowers/specs/2026-07-02-roger-studio-design.md.
class RogerStudio {
public:
	explicit RogerStudio(const Common::String &gameId);
	~RogerStudio();

	// Blocking loop; returns when the user quits (Esc / window close).
	void run();

private:
	enum PlateMode { kPlateOmyac = 0, kPlateNearestRef };
	enum Display { kShowA = 0, kShowB, kShowSplit, kShowDiff, kShowGrid6 };

	struct Slot {
		OmyacParams        params;
		Common::Array<int> passes;               // starts = defaultPasses()
		int                variant = 0;          // viewScalerPreset() index; 0 = shipping s2-s3
		PlateMode          plateMode = kPlateOmyac;
		Graphics::Surface *render = nullptr;     // cached scene render (1920x1140 RGBA)
		Graphics::Surface *plateCache = nullptr; // cached plate (no cel); reused while !plateStale
		Common::Array<byte> backfillMask;        // fillNullPixels mask for plateCache; empty = no pink (nearest-ref)
		bool               stale = true;
		bool               plateStale = true;
		uint32             renderMs = 0;
	};

	// Frame / input
	void handleEvent(const Common::Event &ev);
	void drawFrame();                // compose scene area + panel into _display, push
	void drawCursor();               // composite the crosshair pointer at (_mouseX,_mouseY)
	// Scaled+panned blit of one render into a scene sub-area (Split/Diff/single share it).
	void blitRender(const Graphics::Surface &render, const Common::Rect &subArea);
	// Light 1-screen-px grid at plate-pixel boundaries (only when _viewScale >= 3).
	void drawPixelGrid(const Common::Rect &subArea);
	void drawGrid(const Common::Rect &area);   // 6-pipeline cel comparison tiles
	void ensureGridCels();                     // (re)build _gcSurf for the current cel
	void freeGridCels();
	void stepAnimCel();                        // advance shared cel index (wraps)
	void drawPanel();                // Task 6
	void dispatchWidget(uint32 id);  // Task 6
	void markDirty() { _dirty = true; }

	// Rendering
	void renderSlot(Slot &slot);     // plate (+ cel) -> slot.render
	Slot &activeSlot() { return _slots[_activeSlot]; }
	// Full invalidation: plate genuinely changes (pic/variant/plateMode/params/passes).
	void invalidateScene() {
		_slots[0].stale = _slots[0].plateStale = true;
		_slots[1].stale = _slots[1].plateStale = true;
		_diffStale = true;
		markDirty();
	}
	void invalidateActive() { activeSlot().stale = activeSlot().plateStale = true; _diffStale = true; markDirty(); }
	// Cel-only invalidation: plate is unchanged, so recomposite the cel over the
	// cached plate (loop/cel/view/showView/place/drag).
	void invalidateCelOnly() { _slots[0].stale = _slots[1].stale = true; _diffStale = true; markDirty(); }
	void ensureFresh(Slot &slot) { if (slot.stale) renderSlot(slot); }
	Common::String slotStamp(const Slot &slot) const; // "default-ffflffaaaa-6x[-nref]"
	void ensureDiff();               // Task 8: (re)build _diffSurf + SAD readout when stale
	void exportShown();              // Task 8 extends for split/diff

	// Scene-area geometry (Task 7 fills the interaction)
	Common::Rect sceneArea() const;  // _display minus the panel strip
	void fitView();                  // zoom/pan so the plate fits sceneArea
	bool displayToNative(int mx, int my, int &nx, int &ny) const;

	static const int kPanelH = 560;  // overlay px (drawn 2x from a small surface)

	RogerAssetGen        _gen;       // kGenMemory, empty cache dir
	Graphics::ManagedSurface *_display = nullptr;

	bool  _dirty = true;
	bool  _quit = false;

	Slot _slots[2];
	int  _activeSlot = 0;            // 0 = A, 1 = B
	int  _displayMode = kShowA;      // Display

	// Shared scene state
	Common::Array<int> _picIds;  int _picIdx = 0;
	Common::Array<int> _viewIds; int _viewIdx = 0;
	int _loopNo = 0, _celNo = 0;
	int _celX = 160, _celY = 150;    // native coords, cel BOTTOM-CENTRE anchor
	bool _showView = true;
	bool _showBackfill = false;      // recolour "unfilled" (fillNullPixels) pixels hot pink
	bool _showGrid = false;          // light plate-pixel grid when zoomed in (>= 3x)

	// Global cel playback (all display modes).
	bool   _animPlaying = false;
	int    _animSpeedIdx = 2;        // index into animSpeedMs table (150 ms)
	uint32 _lastAnimTick = 0;

	// Grid-mode per-cel cache: the current cel rendered through the six grid
	// presets, each at its preset's own factor. Keyed by (view, loop, cel);
	// any mismatch rebuilds. Surfaces owned.
	int _gcView = -1, _gcLoop = -1, _gcCel = -1;
	Graphics::Surface *_gcSurf[6] = {};
	int _gcFactor[6] = {};
	int _gcW = 0, _gcH = 0;          // native cel dims
	int _gcDx = 0, _gcDy = 0;        // native displaceX/Y

	// Mouse pointer in overlay (_display) coords. Real SDL events already arrive in
	// overlay space when the overlay is shown (see handleEvent); we composite our own
	// crosshair here because the native hardware cursor is invisible over the overlay
	// (same reason FileRogerArtProvider draws its own arrow).
	int _mouseX = 0, _mouseY = 0;

	// View transform (scene area only)
	float _viewScale = 1.0f;         // set by fitView()
	float _fitScale = 1.0f;
	int _panX = 0, _panY = 0;        // overlay px offset of plate origin in sceneArea
	bool _draggingCel = false;
	bool _panning = false;
	int _dragLastX = 0, _dragLastY = 0;

	// Panel (Task 6)
	Common::Array<StudioWidget> _widgets;   // panel-local small coords
	int _selectedChip = -1;
	uint32 _hoverWid = 0;

	Common::String _status;
	Common::String _offsetReadout;   // Task 8: SAD readout line (Diff only)

	// Task 8: cached diff map (A vs B), rebuilt only when a slot changes.
	Graphics::Surface *_diffSurf = nullptr; // 1920x1140 RGBA white-on-black diff
	bool _diffStale = true;

	bool _hudFontWarned = false;    // warn once per studio session on missing HUD font
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_STUDIO_H
