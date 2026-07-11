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

#ifndef SCI_ROGER_ROGER_EFFECTS_H
#define SCI_ROGER_ROGER_EFFECTS_H

#include "graphics/surface.h"

namespace Sci {
namespace Roger {

enum TransitionFamily {
	kFxNone,
	kFxFade,
	kFxDissolve,
	kFxWipe,
	kFxScroll,
	kFxSplitV,    // two vertical strips expand/contract from/to horizontal center
	kFxSplitH,    // two horizontal bands expand/contract from/to vertical center
	kFxDiagonal   // Linf corner-curtain expands from / contracts to center
};

// Map an SCI transition type (transitions.h enum values) to an overlay family.
// Unknown values default to kFxFade (a safe, always-correct effect).
TransitionFamily transitionFamilyFor(int sciType);

// Identity: each family renders faithfully (no Phase-1 collapse).
TransitionFamily effectiveFamily(TransitionFamily f);

// Default per-family effect duration in milliseconds (tuned from SCI's timing).
int defaultDurationMs(TransitionFamily f);

// Wipe direction from a raw SCI transition type. Returns 0-3:
//   0 = reveal from right  (new content appears from the right edge)
//   1 = reveal from left
//   2 = reveal from bottom
//   3 = reveal from top
// Used by runTransition when fam == kFxWipe.
int wipeDirectionFor(int sciType);

// True for FromCenter SCI types (0, 1, 7); false for ToCenter (300, 301, 6).
// Used by runTransition to pass the direction parameter to split/diagonal blends.
bool splitFromCenter(int sciType);

// Returns the dissolve block size in overlay pixels for the given SCI type.
// PIXELATION (9) -> 8 px (~90K blocks); all others -> 24 px (~10K blocks).
int blockPxForSciType(int sciType);

// Blend functions: cross-fade two RGBA32 surfaces.
// All surfaces must share dimensions and RGBA32 format; no-op on mismatch.

// Fade through black: t<0.5 fades from->black, t>=0.5 black->to.
void blendFadeThroughBlack(const Graphics::Surface &from, const Graphics::Surface &to,
                           Graphics::Surface &out, float t);

// Hash-ordered dissolve: each blockPxxblockPx cell reveals 'to' at a pseudo-random
// threshold, giving an organic mosaic feel matching the original SCI LFSR ordering.
// t=0->all from, t=1->all to.
void blendDissolve(const Graphics::Surface &from, const Graphics::Surface &to,
                   Graphics::Surface &out, float t, int blockPx);

// Directional wipe: reveals 'to' over 'from' as t goes 0->1.
// direction: 0=from right, 1=from left, 2=from bottom, 3=from top.
void blendWipe(const Graphics::Surface &from, const Graphics::Surface &to,
               Graphics::Surface &out, float t, int direction);

// Slide-scroll: old frame slides off in `direction`, new frame enters from the opposite edge.
// direction uses the same 0-3 convention as blendWipe (the edge the NEW scene enters from).
void blendScroll(const Graphics::Surface &from, const Graphics::Surface &to,
                 Graphics::Surface &out, float t, int direction);

// Map a scroll SCI transition type to a scroll direction (0=right,1=left,2=bottom,3=top).
int scrollDirectionFor(int sciType);

// Split-vertical curtain: reveals 'to' from the horizontal center outward (fromCenter=true)
// or from the edges inward (fromCenter=false).
// threshold per pixel: fromCenter=abs(2*x/W-1), toCenter=1-abs(2*x/W-1).
void blendSplitVertical(const Graphics::Surface &from, const Graphics::Surface &to,
                        Graphics::Surface &out, float t, bool fromCenter);

// Split-horizontal curtain: same formula on the Y axis.
void blendSplitHorizontal(const Graphics::Surface &from, const Graphics::Surface &to,
                          Graphics::Surface &out, float t, bool fromCenter);

// Corner-curtain diagonal: Linf norm from center as threshold.
//   fromCenter=true:  center (Linf=0) reveals first, corners (Linf=1) reveal last.
//   fromCenter=false: corners reveal first, center reveals last.
void blendDiagonal(const Graphics::Surface &from, const Graphics::Surface &to,
                   Graphics::Surface &out, float t, bool fromCenter);

} // namespace Roger
} // namespace Sci
#endif
