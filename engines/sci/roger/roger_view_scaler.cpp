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

#include "sci/roger/roger_view_scaler.h"
#include "sci/roger/roger_mmpx.h"
#include "common/util.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

int kernelFactor(int k) {
	switch (k) {
	case kKernScale3x:
	case kKernNearest3:
		return 3;
	default:
		return 2;
	}
}

const char *kernelCode(int k) {
	switch (k) {
	case kKernScale2x:  return "s2";
	case kKernScale3x:  return "s3";
	case kKernNearest2: return "n2";
	case kKernNearest3: return "n3";
	case kKernMMPX:     return "mx";
	default:            return "??";
	}
}

IndexImage applyKernel(int k, const IndexImage &in, byte clearKey) {
	switch (k) {
	case kKernScale2x:  return scale2x(in);
	case kKernScale3x:  return scale3x(in);
	case kKernNearest2: return scaleNearest(in, 2);
	case kKernNearest3: return scaleNearest(in, 3);
	case kKernMMPX:     return mmpx2x(in, clearKey);
	default:            return in;
	}
}

// The comparison presets. Order is the Studio cycle order; preset 0 is the
// shipping-identical pipeline (scale6x == scale3x(scale2x(in)), i.e. scale2x
// applied FIRST — locked byte-identical by test_view_scaler.h).
static const ViewScalerPreset PRESETS[] = {
	{ "s2-s3",    "s2>s3 6x (ship)", { kKernScale2x,  kKernScale3x,  0, 0 }, 2 },
	{ "s3-s2",    "s3>s2 6x",        { kKernScale3x,  kKernScale2x,  0, 0 }, 2 },
	{ "s3-mx",    "s3>mmpx 6x",      { kKernScale3x,  kKernMMPX,     0, 0 }, 2 },
	{ "mx-s3",    "mmpx>s3 6x",      { kKernMMPX,     kKernScale3x,  0, 0 }, 2 },
	{ "mx-mx-mx", "mmpx^3 8x",       { kKernMMPX,     kKernMMPX,     kKernMMPX, 0 }, 3 },
	{ "mx-s2-s2", "mmpx>s2>s2 8x",   { kKernMMPX,     kKernScale2x,  kKernScale2x, 0 }, 3 },
	{ "s3-s3",    "s3>s3 9x",        { kKernScale3x,  kKernScale3x,  0, 0 }, 2 },
	{ "n2-n3",    "nearest 6x",      { kKernNearest2, kKernNearest3, 0, 0 }, 2 },
};

int viewScalerPresetCount() {
	return ARRAYSIZE(PRESETS);
}

const ViewScalerPreset &viewScalerPreset(int i) {
	return PRESETS[CLIP<int>(i, 0, ARRAYSIZE(PRESETS) - 1)];
}

int viewScalerPresetFactor(int i) {
	const ViewScalerPreset &p = viewScalerPreset(i);
	int f = 1;
	for (int k = 0; k < p.kernelCount; k++)
		f *= kernelFactor(p.kernels[k]);
	return f;
}

IndexImage applyViewScalerPreset(int i, const IndexImage &in, byte clearKey) {
	const ViewScalerPreset &p = viewScalerPreset(i);
	IndexImage img = in;
	for (int k = 0; k < p.kernelCount; k++)
		img = applyKernel(p.kernels[k], img, clearKey);
	return img;
}

int viewScalerPresetIndexById(const char *id) {
	if (!id)
		return -1;
	for (int i = 0; i < ARRAYSIZE(PRESETS); i++)
		if (!strcmp(PRESETS[i].id, id))
			return i;
	return -1;
}

IndexImage resampleNearestExact(const IndexImage &in, int outW, int outH) {
	IndexImage out;
	out.w = outW;
	out.h = outH;
	if (outW <= 0 || outH <= 0 || in.w <= 0 || in.h <= 0) {
		out.w = out.h = 0;
		return out;
	}
	out.pixels.resize((size_t)outW * outH);
	for (int y = 0; y < outH; y++) {
		const byte *srow = in.pixels.begin() + (size_t)((int64)y * in.h / outH) * in.w;
		byte *drow = out.pixels.begin() + (size_t)y * outW;
		for (int x = 0; x < outW; x++)
			drow[x] = srow[(int)((int64)x * in.w / outW)];
	}
	return out;
}

IndexImage applyViewScalerPresetTo6x(int i, const IndexImage &in, byte clearKey) {
	IndexImage out = applyViewScalerPreset(i, in, clearKey);
	if (viewScalerPresetFactor(i) != 6)
		out = resampleNearestExact(out, in.w * 6, in.h * 6);
	return out;
}

} // End of namespace Roger
} // End of namespace Sci
