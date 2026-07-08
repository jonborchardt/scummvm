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

#ifndef SCI_ROGER_ROGER_VIEW_SCALER_H
#define SCI_ROGER_ROGER_VIEW_SCALER_H

#include "sci/roger/gen/roger_scale.h"

namespace Sci {
namespace Roger {

// Registry of view-cel upscaler modules. Entry 0 is the shipping scaler â€”
// scale6x() == scale3x(scale2x(in)) â€” locked byte-identical by unit test
// (test_view_scaler.h). It is the only registered module today, so every
// selection UI (tune panel variant rows, Studio variant button / grid)
// shows exactly one option: 6x.
//
// To add a new scaler module:
//   1. Implement `IndexImage myScale(const IndexImage &in, byte clearKey);`
//      in its own source file (clearKey = the cel's transparent palette
//      index, for scalers that need a luma/clear channel).
//   2. Append one row to SCALERS[] in roger_view_scaler.cpp: a stable,
//      filename-safe id (it becomes the disk-cache transform name if the
//      module is ever promoted to the shipping path), a UI label, the
//      integer upscale factor, and the function pointer.
// Non-6x factors are normalized onto the 6x plate grid by
// applyViewScalerTo6x, so any integer factor is selectable.
struct ViewScaler {
	const char *id;     // filename-safe, unique, stable
	const char *label;  // UI label (tune panel row / Studio variant button)
	int factor;         // integer upscale factor of scale()
	IndexImage (*scale)(const IndexImage &in, byte clearKey);
};

int viewScalerCount();                   // >= 1; entry 0 = shipping module
const ViewScaler &viewScaler(int i);     // i clamped to the valid range
int viewScalerIndexById(const char *id); // -1 when unknown (or id == null)
IndexImage applyViewScaler(int i, const IndexImage &in, byte clearKey);

// Apply module i, then bring the result onto the 6x plate grid when the
// module's factor is not 6 (exact-rational nearest resample below; keeps
// cel geometry plate-aligned).
IndexImage applyViewScalerTo6x(int i, const IndexImage &in, byte clearKey);

// Exact-rational nearest resample: out(x,y) = in(x*in.w/outW, y*in.h/outH).
// Brings a non-6x module result onto the 6x plate grid without the
// truncated 8.8 fixed-point drift of ManagedSurface's blit scaler (the
// resolved occlusion-misalignment bug class â€” never scale plate-aligned
// content through blitFrom/blendBlitFrom).
IndexImage resampleNearestExact(const IndexImage &in, int outW, int outH);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_VIEW_SCALER_H
