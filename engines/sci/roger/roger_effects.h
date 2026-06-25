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

enum TransitionFamily { kFxNone, kFxFade, kFxDissolve, kFxWipe, kFxScroll };

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

// Blend functions: cross-fade two RGBA32 surfaces.
// All three surfaces must share dimensions and RGBA32 format; no-op on mismatch.

// Fade through black: t<0.5 fades from->black, t>=0.5 black->to.
void blendFadeThroughBlack(const Graphics::Surface &from, const Graphics::Surface &to,
                           Graphics::Surface &out, float t);

// Ordered (Bayer) dissolve: each blockPx×blockPx cell shows 'to' once threshold ≤ t, else 'from'.
void blendDissolve(const Graphics::Surface &from, const Graphics::Surface &to,
                   Graphics::Surface &out, float t, int blockPx);

// Directional wipe: reveals 'to' over 'from' as t goes 0->1.
// direction: 0=from right, 1=from left, 2=from bottom, 3=from top.
void blendWipe(const Graphics::Surface &from, const Graphics::Surface &to,
               Graphics::Surface &out, float t, int direction);

} // namespace Roger
} // namespace Sci
#endif
