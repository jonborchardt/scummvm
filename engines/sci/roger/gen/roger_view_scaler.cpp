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

#include "sci/roger/gen/roger_view_scaler.h"
#include "common/util.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

// Entry 0's module function: the shipping scale6x. clearKey is unused  -- 
// scale2x/scale3x carry the transparent index through like any other pixel.
static IndexImage scale6xModule(const IndexImage &in, byte) {
	return scale6x(in);
}

// The module registry. Append new scalers here (see the header for the
// two-step recipe); entry 0 must remain the shipping pipeline.
static const ViewScaler SCALERS[] = {
	{ "s2-s3", "6x (s2>s3)", 6, scale6xModule },
};

int viewScalerCount() {
	return ARRAYSIZE(SCALERS);
}

const ViewScaler &viewScaler(int i) {
	return SCALERS[CLIP<int>(i, 0, ARRAYSIZE(SCALERS) - 1)];
}

int viewScalerIndexById(const char *id) {
	if (!id)
		return -1;
	for (int i = 0; i < ARRAYSIZE(SCALERS); i++)
		if (!strcmp(SCALERS[i].id, id))
			return i;
	return -1;
}

IndexImage applyViewScaler(int i, const IndexImage &in, byte clearKey) {
	return viewScaler(i).scale(in, clearKey);
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

IndexImage applyViewScalerTo6x(int i, const IndexImage &in, byte clearKey) {
	IndexImage out = applyViewScaler(i, in, clearKey);
	if (viewScaler(i).factor != 6)
		out = resampleNearestExact(out, in.w * 6, in.h * 6);
	return out;
}

// -- View-enhance modes (registry + trailing nearest) ---

int viewEnhanceModeCount() {
	return viewScalerCount() + 1;
}

bool viewEnhanceModeIsNearest(int m) {
	return m >= viewScalerCount();
}

const char *viewEnhanceModeLabel(int m) {
	return viewEnhanceModeIsNearest(m) ? "nearest" : viewScaler(m).label;
}

const char *viewEnhanceModeId(int m) {
	return viewEnhanceModeIsNearest(m) ? "nearest" : viewScaler(m).id;
}

IndexImage applyViewEnhanceMode6x(int m, const IndexImage &in, byte clearKey) {
	if (viewEnhanceModeIsNearest(m))
		return resampleNearestExact(in, in.w * 6, in.h * 6);
	return applyViewScalerTo6x(m, in, clearKey);
}

} // End of namespace Roger
} // End of namespace Sci
