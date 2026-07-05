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

#ifndef SCI_ROGER_ROGER_STUDIO_RENDER_H
#define SCI_ROGER_ROGER_STUDIO_RENDER_H

// SCI-free pure helpers for the Roger Studio debug tool (see
// docs/superpowers/specs/2026-07-02-roger-studio-design.md). Everything here
// is unit-testable without a running engine.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_scale.h"

namespace Sci {
namespace Roger {

// ── OmyacParams registry: index-addressable fields for the studio HUD ────────
struct OmyacParamDesc {
	const char *name;
	int minV;
	int maxV;
	int step;
	bool isBool;
	const char *help;  // one-sentence plain-English description (~70 chars max)
};

int omyacParamCount();
OmyacParamDesc omyacParamDesc(int i);
int omyacParamGet(const OmyacParams &p, int i);
void omyacParamSet(OmyacParams &p, int i, int value); // clamps to [minV, maxV]

// ── Export filename stamps ───────────────────────────────────────────────────
Common::String omyacPassStamp(const Common::Array<int> &passes); // "ffla" / "none"
Common::String omyacParamStamp(const OmyacParams &p);            // "default" / "mvl3-iso0"
Common::String studioExportName(const char *kind, int id, const Common::String &detail);

// ── Studio v2: per-game startup defaults ─────────────────────────────────────
struct StudioDefaults {
	int picId;   // -1 = first available
	int viewId;  // -1 = first available
	int loopNo;
	int celNo;
	int celX;    // native coords, cel bottom-centre anchor
	int celY;
};

StudioDefaults studioDefaultsForGame(const Common::String &gameId);

// ── Studio v2: pass-list edit ops (pure; unit-tested) ────────────────────────
void passInsertAfter(Common::Array<int> &passes, int &selected, int passVal);
void passRemoveAt(Common::Array<int> &passes, int &selected);
bool passMove(Common::Array<int> &passes, int &selected, int dir); // dir in {-1,+1}

// ── Studio v2: export names ──────────────────────────────────────────────────
Common::String studioSceneExportName(int picId, char slot, const Common::String &detail);
Common::String studioCompareExportName(int picId, bool diff,
                                       const Common::String &stampA,
                                       const Common::String &stampB);

// ── Studio v2: widget layer - kinds, id encoding, panel layout, hit-testing ────
enum WidKind {
	kWidNone = 0,
	kWidPicPrev, kWidPicNext, kWidViewPrev, kWidViewNext,
	kWidLoopPrev, kWidLoopNext, kWidCelPrev, kWidCelNext,
	kWidVariantCycle, kWidPlateMode, kWidShowView, kWidFit,
	kWidTabA, kWidTabB, kWidShowA, kWidShowB, kWidSplit, kWidDiff,
	kWidCopyAB, kWidExport,
	kWidParamMinus, kWidParamPlus, kWidParamToggle,   // indexed by param
	kWidChip, kWidChipX,                              // indexed by chip
	kWidChipLeft, kWidChipRight,
	kWidChipAddF, kWidChipAddL, kWidChipAddA, kWidChipReset,
	kWidChipClear,
	kWidShowBackfill, kWidShowGrid,  // shared scene toggles (pink / pixel grid)
	kWidGrid6,                       // 6-pipeline comparison grid display mode
	kWidAnimPlay, kWidAnimSlower, kWidAnimFaster   // global cel playback
};

uint32 widId(int kind, int index = 0);   // (kind << 16) | (index & 0xffff)
int widKind(uint32 id);
int widIndex(uint32 id);

struct StudioWidget {
	Common::Rect rect;      // panel-local small coords
	uint32 id;
	Common::String label;
	bool on;                // toggled/active state (drawn highlighted)
	bool enabled;
};

struct StudioPanelState {
	int picId, viewId, loopNo, celNo;
	int celX, celY;          // native coords for position readout @(x,y)
	const char *variantName;
	bool plateNearest;      // active slot's plate mode
	bool showView;
	int activeSlot;         // 0 = A, 1 = B
	int displayMode;        // 0 ShowA, 1 ShowB, 2 Split, 3 Diff
	int selectedChip;       // -1 = none
	bool showBackfill;      // recolour fillNullPixels ("unfilled") pixels hot pink
	bool showGrid;          // draw light plate-pixel grid when zoomed in
	bool animPlaying = false;  // global cel playback running
	int animMs = 150;          // current playback period (ms per cel)
	Common::Array<int> passes;      // active slot's
	Common::Array<int> paramValues; // active slot's, omyacParamCount() entries
};

// Lays out every widget for the control panel into `out` (cleared first).
// panel = panel-local small rect, i.e. (0, 0, smallW, smallH).
// Pure and deterministic: same state -> same rects. Uses kCharW/kRowH below.
void buildStudioPanel(const Common::Rect &panel, const StudioPanelState &st,
                      Common::Array<StudioWidget> &out);

// First widget whose rect contains (x, y) and is enabled; kWidNone if none.
uint32 hitTestWidgets(const Common::Array<StudioWidget> &widgets, int x, int y);

static const int kStudioCharW = 7;   // layout char width (small px)
static const int kStudioRowH = 24;   // layout row height (small px)

// ── Studio v2: shift diagnosis (pure; unit-tested) ───────────────────────────
// Per-pixel |a-b| map, white-on-black: out = (m,m,m,255) with m = max channel
// delta of the first 3 bytes. All buffers w*h*4 bytes, same layout.
void diffMapRGBA(const byte *a, const byte *b, int w, int h, byte *out);

// Best global alignment of a against b over (dx,dy) in [-radius,+radius]^2,
// SAD over the first 3 bytes/pixel, normalized by overlap area. Result is the
// displacement applied to a that minimizes SAD: dx=+1 means b's content sits
// 1 px right of a's. Ties are broken in order: (1) smaller |dx|+|dy|; (2) on
// equal distance, smaller |dy|; (3) on equal |dy|, smaller |dx|. Returns false
// if the image is too small (w or h <= 2*radius).
bool estimateOffsetSAD(const byte *a, const byte *b, int w, int h, int radius,
                       int &outDx, int &outDy);

// ── Grid mode + animation helpers (pure; unit-tested) ───────────────────────

// The 6 comparison pipelines, in tile order (row-major 2x3). Returns the
// roger_view_scaler registry preset index for tile 0..5, or -1 if the preset
// id is missing (registry drift — the caller should skip the tile).
int gridPresetSlot(int tile);
int gridTileCount(); // 6

// Tile rect for the 2x3 grid inside `area`, 2 px gutters, row-major.
Common::Rect gridTileRect(const Common::Rect &area, int tile);

// Playback speed table {300,200,150,100,66} ms per cel; idx clamped.
int animSpeedMs(int idx);
int animSpeedStep(int idx, int dir); // idx+dir clamped to the table range

// Native-px rect of a cel anchored at (anchorX, anchorY), SCI's own placement
// (GfxView::getCelRect): left = ax + dx - (w>>1); bottom = ay + dy + 1.
// Keeps walk cycles foot-planted across differently-sized cels.
Common::Rect celAnchorRect(int w, int h, int displaceX, int displaceY,
                           int anchorX, int anchorY);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_STUDIO_RENDER_H
