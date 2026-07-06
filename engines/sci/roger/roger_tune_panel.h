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

#ifndef SCI_ROGER_ROGER_TUNE_PANEL_H
#define SCI_ROGER_ROGER_TUNE_PANEL_H

// TEMPORARY DEBUG TOOL — the in-game quick-tune panel (F12), spec
// docs/superpowers/specs/2026-07-05-roger-tune-panel-design.md. Session-only
// view-scaler variant selection + staged omyac pass edits behind Apply.
// DELETE this module (and its provider/event.cpp seams) when the MMPX
// judging is done. Everything here is engine-free and unit-testable.
//
// All layout/hit-testing is in GAME space (320x200): the panel rect is fixed
// so .rin scripts can click widgets at window-size-independent coordinates.
// Drawing (drawTunePanel) maps each rect to overlay space via sciRectToDest.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_studio_render.h" // StudioWidget, widId, hitTestWidgets, pass helpers

namespace Graphics { class ManagedSurface; }

namespace Sci {
namespace Roger {

enum TuneWidKind {
	kTuneNone = 0,
	kTuneClose,
	kTuneVariantRow, // index = viewScalerPreset() index
	kTuneChip,       // index = chip position in stagedPasses
	kTuneChipX, kTuneChipLeft, kTuneChipRight,
	kTuneChipAddF, kTuneChipAddL, kTuneChipAddA,
	kTuneClear, kTuneReset, kTuneApply
};

struct TunePanelState {
	bool open = false;
	int variant = 0;                  // applied view-scaler preset index
	Common::Array<int> stagedPasses;  // chip edits accumulate here (NOT applied)
	Common::Array<int> appliedPasses; // last applied — pending marker compares
	int selectedChip = -1;
	uint32 hoverId = 0;               // widId under the mouse (0 = none)
	uint32 lastGenMs = 0;             // last Apply's regen wall-clock (ms)
};

bool tunePassesEqual(const Common::Array<int> &a, const Common::Array<int> &b);
inline bool tunePending(const TunePanelState &st) {
	return !tunePassesEqual(st.stagedPasses, st.appliedPasses);
}

// Fixed GAME-space panel rect (right side, below the status strip).
// LOCKED by test_script_geometry_lock + the tune-panel-smoke.rin script.
inline Common::Rect tunePanelRect() { return Common::Rect(228, 12, 318, 196); }

// Pure layout: fills `out` (cleared first) with globally-positioned
// game-space widgets. Variant rows flow from the top; the chip-op /
// clear-reset-apply rows are BOTTOM-ANCHORED so their coordinates do not
// move as chips are added (script stability). Chips render in rows of 7,
// capped to the space between (extra staged passes still exist, just not
// clickable — acceptable for a debug tool).
void buildTunePanel(const TunePanelState &st, Common::Array<StudioWidget> &out);

// "fla * 812ms": pass stamp (omyacPassStamp), pending marker, last gen time.
Common::String tuneStatusLine(const TunePanelState &st);

// Draw the panel into `scene` (overlay-sized RGBA). gameRect = the game
// picture placement (provider's _lastGameRect); each game-space rect maps
// through sciRectToDest. Labels use the GUI big font. Task 5 implements.
void drawTunePanel(Graphics::ManagedSurface &scene, const Common::Rect &gameRect,
                   const TunePanelState &st, const Common::Array<StudioWidget> &widgets);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_TUNE_PANEL_H
