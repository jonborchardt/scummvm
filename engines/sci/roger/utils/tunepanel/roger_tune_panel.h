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

#ifndef SCI_ROGER_UTILS_TUNEPANEL_ROGER_TUNE_PANEL_H
#define SCI_ROGER_UTILS_TUNEPANEL_ROGER_TUNE_PANEL_H

// TUNE PANEL (kept dev utility, quarantined 2026-07-07) — the in-game
// quick-tune debug dialog (F12 / Ctrl+Shift+T), spec
// docs/superpowers/specs/2026-07-05-roger-tune-panel-design.md. Session-only
// staged omyac pass edits behind Apply, plus view-scaler module selection
// (registry-driven; a single module — the shipping 6x — is registered today,
// so exactly one variant row shows). The MMPX judging this panel was built
// for concluded 2026-07-06 (s2>s3 won); the panel stays as the pass-tuning
// debug tool. Everything here is engine-free and unit-testable.
//
// QUARANTINE CONTRACT — see utils/tunepanel/README.md. The only permitted
// references to utils/tunepanel/ are: the F12 integration block in
// file_roger_art_provider.{h,cpp} (state member + draw/toggle/mouse glue),
// engines/sci/module.mk, and build_tests.ps1's test registration
// (test/sci/roger/test_tune_panel.h). Everything else — including
// event.cpp's key/mouse routing — must go through the plain virtuals on the
// abstract provider (roger_art_provider.h), which name no tunepanel types.
// This module may only consume stable SCI-free roger seams
// (roger_widgets.h, roger_passes.h, roger_view_scaler.h, roger_coords.h) —
// never provider/compositor internals, never SCI engine state, and never
// anything under utils/studio/ (each utils/ tool is quarantined on its own).
//
// All layout/hit-testing is in GAME space (320x200): the panel rect is fixed
// so .rin scripts can click widgets at window-size-independent coordinates.
// Drawing (drawTunePanel) maps each rect to overlay space via sciRectToDest.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_widgets.h" // PanelWidget, widId, hitTestWidgets

namespace Graphics { class ManagedSurface; }

namespace Sci {
namespace Roger {

enum TuneWidKind {
	kTuneNone = 0,
	kTuneClose,
	kTuneVariantRow, // index = viewScaler() registry index
	kTuneChip,       // index = chip position in stagedPasses
	kTuneChipX, kTuneChipLeft, kTuneChipRight,
	kTuneChipAddF, kTuneChipAddL, kTuneChipAddA,
	kTuneClear, kTuneReset, kTuneApply,
	kTuneSide,       // send the panel to the other screen side
	kTunePreset      // index = goodPassPattern() registry index (one-click swap)
};

struct TunePanelState {
	bool open = false;
	bool leftSide = false;            // panel docks right by default; kTuneSide flips
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

// Fixed GAME-space panel rect (below the status strip). Docks on the right by
// default; `leftSide` mirrors it to the left edge (same size). The RIGHT-side
// coordinates are LOCKED by test_script_geometry_lock + tune-panel-smoke.rin.
inline Common::Rect tunePanelRect(bool leftSide = false) {
	return leftSide ? Common::Rect(2, 12, 92, 196) : Common::Rect(228, 12, 318, 196);
}

// Pure layout: fills `out` (cleared first) with globally-positioned
// game-space widgets. Variant rows flow from the top; the chip-op /
// clear-reset-apply rows are BOTTOM-ANCHORED so their coordinates do not
// move as chips are added (script stability). Chips render in rows of 7,
// capped to the space between (extra staged passes still exist, just not
// clickable — acceptable for a debug tool).
void buildTunePanel(const TunePanelState &st, Common::Array<PanelWidget> &out);

// "fla * 812ms": pass stamp (omyacPassStamp), pending marker, last gen time.
Common::String tuneStatusLine(const TunePanelState &st);

// Draw the panel into `scene` (overlay-sized RGBA). gameRect = the game
// picture placement (provider's _lastGameRect); each game-space rect maps
// through sciRectToDest. Labels use the GUI big font. Task 5 implements.
void drawTunePanel(Graphics::ManagedSurface &scene, const Common::Rect &gameRect,
                   const TunePanelState &st, const Common::Array<PanelWidget> &widgets);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_UTILS_TUNEPANEL_ROGER_TUNE_PANEL_H
