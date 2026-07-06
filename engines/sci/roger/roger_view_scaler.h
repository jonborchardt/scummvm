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

#include "sci/roger/roger_scale.h"

namespace Sci {
namespace Roger {

// Composable view-cel upscaler pipelines. A pipeline is an ordered list of
// primitive integer-factor kernels applied LEFT TO RIGHT; its total factor is
// the product of the kernel factors. Preset ids are stable, filename-safe
// strings intended to become disk-cache transform names if a pipeline is
// ever promoted to the shipping path (generateViewCel's "scale6x" key slot).
// The shipping path does NOT consume this registry yet — Roger Studio is the
// only caller. Preset 0 is locked byte-identical to scale6x() by unit test.

enum ScaleKernel {
	kKernScale2x = 0, // EPX/Scale2x
	kKernScale3x,     // Scale3x (EPX-9)
	kKernNearest2,    // nearest-neighbour x2 (blocky reference)
	kKernNearest3,    // nearest-neighbour x3 (blocky reference)
	kKernMMPX,        // MMPX 2x (style-preserving; needs clearKey for luma)
	kKernCount
};

int kernelFactor(int k);       // 2 or 3
const char *kernelCode(int k); // "s2" "s3" "n2" "n3" "mx"
IndexImage applyKernel(int k, const IndexImage &in, byte clearKey);

struct ViewScalerPreset {
	const char *id;    // kernel codes joined by '-', e.g. "mx-mx-mx"
	const char *label; // Studio panel label, e.g. "mmpx^3 8x"
	int kernels[4];    // applied left to right; entries past kernelCount unused
	int kernelCount;
};

int viewScalerPresetCount();
const ViewScalerPreset &viewScalerPreset(int i); // i clamped to valid range
int viewScalerPresetFactor(int i);
IndexImage applyViewScalerPreset(int i, const IndexImage &in, byte clearKey);
int viewScalerPresetIndexById(const char *id);   // -1 when unknown

// Exact-rational nearest resample: out(x,y) = in(x*in.w/outW, y*in.h/outH).
// Brings a non-6x pipeline result onto the 6x plate grid without the
// truncated 8.8 fixed-point drift of ManagedSurface's blit scaler (the
// resolved occlusion-misalignment bug class — never scale plate-aligned
// content through blitFrom/blendBlitFrom).
IndexImage resampleNearestExact(const IndexImage &in, int outW, int outH);

// TEMPORARY DEBUG TOOL (tune panel, spec 2026-07-05) — delete with the panel.
// Apply preset i, then resample onto the 6x plate grid when the preset's
// factor is not 6 (the Studio's normalization; keeps cel geometry
// plate-aligned). Preset 0 is byte-identical to scale6x().
IndexImage applyViewScalerPresetTo6x(int i, const IndexImage &in, byte clearKey);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_VIEW_SCALER_H
