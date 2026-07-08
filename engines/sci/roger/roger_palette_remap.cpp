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

#include "sci/roger/roger_palette_remap.h"
#include "sci/roger/gen/roger_ega_blend.h"
#include "graphics/surface.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

// Per-channel gain term: when snap>0 use live/snap; when snap==0 it's undefined
// multiplicatively, so fall back to identity (1.0) and let the additive nudge handle it.
// To keep black-base entries able to brighten, we accumulate live into addResidual when snap==0.
static inline float gainTerm(int snap, int live, float &addResidual) {
	if (snap > 0)
		return (float)live / (float)snap;
	addResidual += (float)live; // snap==0: contribute live as an additive nudge
	return 1.0f;
}

void buildLivePaletteTable(const byte snapEga[48], const byte liveEga[48], uint32 outTable[256]) {
	for (int idx = 0; idx < 256; idx++) {
		const int hi = idx >> 4, lo = idx & 0x0f;
		const uint32 baked = BLEND_TABLE[idx];
		const int br = baked & 0xff, bg = (baked >> 8) & 0xff, bb = (baked >> 16) & 0xff;
		float addR = 0, addG = 0, addB = 0;
		const float gr = (gainTerm(snapEga[hi * 3 + 0], liveEga[hi * 3 + 0], addR) +
		                  gainTerm(snapEga[lo * 3 + 0], liveEga[lo * 3 + 0], addR)) * 0.5f;
		const float gg = (gainTerm(snapEga[hi * 3 + 1], liveEga[hi * 3 + 1], addG) +
		                  gainTerm(snapEga[lo * 3 + 1], liveEga[lo * 3 + 1], addG)) * 0.5f;
		const float gb = (gainTerm(snapEga[hi * 3 + 2], liveEga[hi * 3 + 2], addB) +
		                  gainTerm(snapEga[lo * 3 + 2], liveEga[lo * 3 + 2], addB)) * 0.5f;
		const int r = CLIP((int)(br * gr + addR * 0.5f + 0.5f), 0, 255);
		const int g = CLIP((int)(bg * gg + addG * 0.5f + 0.5f), 0, 255);
		const int b = CLIP((int)(bb * gb + addB * 0.5f + 0.5f), 0, 255);
		outTable[idx] = 0xff000000u | ((uint32)b << 16) | ((uint32)g << 8) | (uint32)r;
	}
}

int paletteDiffMask(const byte snapEga[48], const byte liveEga[48], bool changed[16]) {
	int n = 0;
	for (int i = 0; i < 16; i++) {
		const bool diff = snapEga[i * 3] != liveEga[i * 3] ||
		                  snapEga[i * 3 + 1] != liveEga[i * 3 + 1] ||
		                  snapEga[i * 3 + 2] != liveEga[i * 3 + 2];
		changed[i] = diff;
		if (diff) n++;
	}
	return n;
}

void reblendChangedPixels(const byte *indexMap, int w, int h, const uint32 table[256],
                          const bool changed[16], Graphics::Surface &plate, Common::Rect &outDirty) {
	if (!indexMap || w <= 0 || h <= 0 || plate.w != w || plate.h != h)
		return;
	int minX = w, minY = h, maxX = -1, maxY = -1;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			const byte b = indexMap[y * w + x];
			if (!changed[b >> 4] && !changed[b & 0x0f])
				continue;
			const uint32 v = table[b];
			plate.setPixel(x, y, plate.format.ARGBToColor(255, v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff));
			if (x < minX) minX = x; if (x > maxX) maxX = x;
			if (y < minY) minY = y; if (y > maxY) maxY = y;
		}
	}
	if (maxX >= 0)
		outDirty = Common::Rect((int16)minX, (int16)minY, (int16)(maxX + 1), (int16)(maxY + 1));
	else
		outDirty = Common::Rect();
}

} // namespace Roger
} // namespace Sci
