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

/**
 * EPX-family pixel-art scalers operating on 8-bit palette-index buffers.
 *
 * Ported verbatim from the sci.js TypeScript sources:
 *   libs/resize-filters/src/scale2x.ts  (lines 18-43)
 *   libs/resize-filters/src/scale3x.ts  (lines 18-46)
 *   libs/resize-filters/src/epx.ts      (epx9, lines 3-42)
 *   libs/resize-filters/src/s9.ts       (s9, lines 8-37)
 *
 * Neighbour naming follows the original exactly:
 *   a b c d   (for scale2x: a=up, b=right, c=left, d=down)
 *   A B C
 *   D E F     (for scale3x / epx9: 3x3 kernel)
 *   G H I
 *
 * Edge clamping: out-of-bounds neighbours replicate the centre pixel (s9.ts:26-34).
 */

#include "sci/roger/gen/roger_scale.h"

namespace Sci {
namespace Roger {

// ---
// scale2x
// Verbatim port of scale2x.ts lines 18-43.
//
// Neighbour layout used in scale2x.ts (not the 3x3 grid):
//     a
//   c p b
//     d
//
// Output 2x2 block written as:
//   p1 p2
//   p3 p4
// ---
IndexImage scale2x(const IndexImage &in) {
	IndexImage out;
	if (in.w <= 0 || in.h <= 0 || in.pixels.empty()) {
		out.w = 0;
		out.h = 0;
		return out;
	}

	out.w = in.w * 2;
	out.h = in.h * 2;
	out.pixels.resize(out.w * out.h, 0);

	const int iStride = in.w;
	const int oStride = out.w;
	const byte *src = in.pixels.data();
	byte *dst = out.pixels.data();

	for (int iy = 0; iy < in.h; iy++) {
		for (int ix = 0; ix < in.w; ix++) {
			const int p = iy * iStride + ix;

			// Clamped neighbour indices (scale2x.ts:22-25)
			const int a = (iy - 1 >= 0)   ? p - iStride : p;
			const int b = (ix + 1 < in.w) ? p + 1       : p;
			const int c = (ix - 1 >= 0)   ? p - 1       : p;
			const int d = (iy + 1 < in.h) ? p + iStride : p;

			// EPX 2x rule (scale2x.ts:27-35)
			int p1, p2, p3, p4;
			if (src[a] != src[d] && src[b] != src[c]) {
				p1 = (src[a] == src[c]) ? a : p;
				p2 = (src[a] == src[b]) ? b : p;
				p3 = (src[c] == src[d]) ? c : p;
				p4 = (src[b] == src[d]) ? d : p;
			} else {
				p1 = p; p2 = p; p3 = p; p4 = p;
			}

			// Write 2x2 output block (scale2x.ts:37-42)
			const int oOffset = (ix << 1) + (iy << 1) * oStride;
			dst[oOffset]               = src[p1];
			dst[oOffset + 1]           = src[p2];
			dst[oOffset + oStride]     = src[p3];
			dst[oOffset + 1 + oStride] = src[p4];
		}
	}

	return out;
}

// ---
// s9 (file-private helper)
// Verbatim port of s9.ts lines 8-37.
//
// Computes 9 clamped source indices for the 3x3 neighbourhood of (ix, iy).
// dest layout (matches s9.ts array positions):
//   0 1 2
//   3 4 5
//   6 7 8
//
// Any position that falls outside [0,width) x [0,height) is clamped to the
// centre index e, i.e. the centre pixel value is replicated at the border.
// ---
static void s9(int width, int height, int ix, int iy, int dest[9]) {
	const int e = iy * width + ix;

	const bool uOk = (iy - 1 >= 0);
	const bool rOk = (ix + 1 < width);
	const bool lOk = (ix - 1 >= 0);
	const bool dOk = (iy + 1 < height);

	dest[4] = e;

	dest[1] = uOk ? (iy - 1) * width + ix       : e;
	dest[3] = lOk ? iy       * width + (ix - 1) : e;
	dest[5] = rOk ? iy       * width + (ix + 1) : e;
	dest[7] = dOk ? (iy + 1) * width + ix       : e;

	dest[0] = (uOk && lOk) ? (iy - 1) * width + (ix - 1) : e;
	dest[2] = (uOk && rOk) ? (iy - 1) * width + (ix + 1) : e;
	dest[6] = (dOk && lOk) ? (iy + 1) * width + (ix - 1) : e;
	dest[8] = (dOk && rOk) ? (iy + 1) * width + (ix + 1) : e;
}

// ---
// epx9 (file-private helper)
// Verbatim port of epx.ts lines 3-42.
//
// src     -- the full source pixel array
// s9idx   -- 9 clamped source indices from s9(); layout:
//              A=0 B=1 C=2
//              D=3 E=4 F=5
//              G=6 H=7 I=8
// out9    -- receives 9 output values in the same 3x3 layout
// ---
static void epx9(const byte *src, const int s9idx[9], byte out9[9]) {
	// Read source values at each neighbour position (epx.ts:9-17)
	const byte A = src[s9idx[0]];
	const byte B = src[s9idx[1]];
	const byte C = src[s9idx[2]];
	const byte D = src[s9idx[3]];
	const byte E = src[s9idx[4]];
	const byte F = src[s9idx[5]];
	const byte G = src[s9idx[6]];
	const byte H = src[s9idx[7]];
	const byte I = src[s9idx[8]];

	// EPX-9 rule (epx.ts:19-39)
	if (B != H && D != F) {
		out9[0] = (D == B)                                         ? D : E;
		out9[1] = ((D == B && E != C) || (B == F && E != A))      ? B : E;
		out9[2] = (B == F)                                         ? F : E;
		out9[3] = ((D == B && E != G) || (D == H && E != A))      ? D : E;
		out9[4] = E;
		out9[5] = ((B == F && E != I) || (H == F && E != C))      ? F : E;
		out9[6] = (D == H)                                         ? D : E;
		out9[7] = ((D == H && E != I) || (H == F && E != G))      ? H : E;
		out9[8] = (H == F)                                         ? F : E;
	} else {
		out9[0] = E; out9[1] = E; out9[2] = E;
		out9[3] = E; out9[4] = E; out9[5] = E;
		out9[6] = E; out9[7] = E; out9[8] = E;
	}
}

// ---
// scale3x
// Verbatim port of scale3x.ts lines 18-46.
// ---
IndexImage scale3x(const IndexImage &in) {
	IndexImage out;
	if (in.w <= 0 || in.h <= 0 || in.pixels.empty()) {
		out.w = 0;
		out.h = 0;
		return out;
	}

	out.w = in.w * 3;
	out.h = in.h * 3;
	out.pixels.resize(out.w * out.h, 0);

	const int oStride = out.w;
	const byte *src = in.pixels.data();
	byte *dst = out.pixels.data();

	int neighbours[9];
	byte block[9];

	for (int iy = 0; iy < in.h; iy++) {
		for (int ix = 0; ix < in.w; ix++) {
			const int oOffset = ix * 3 + iy * 3 * oStride;

			s9(in.w, in.h, ix, iy, neighbours);
			epx9(src, neighbours, block);

			// Write 3x3 output block (scale3x.ts:34-43)
			dst[oOffset]                   = block[0]; // A
			dst[oOffset + 1]               = block[1]; // B
			dst[oOffset + 2]               = block[2]; // C
			dst[oOffset + oStride]         = block[3]; // D
			dst[oOffset + oStride + 1]     = block[4]; // E
			dst[oOffset + oStride + 2]     = block[5]; // F
			dst[oOffset + oStride * 2]     = block[6]; // G
			dst[oOffset + oStride * 2 + 1] = block[7]; // H
			dst[oOffset + oStride * 2 + 2] = block[8]; // I
		}
	}

	return out;
}

// ---
// scale6x = scale3x(scale2x(in))
// Matches create-pic-pipeline.ts lines 60-63.
// ---
IndexImage scale6x(const IndexImage &in) {
	return scale3x(scale2x(in));
}

// ---
// scaleNearest -- integer nearest-neighbour (pixel replication) by `factor`.
// ---
IndexImage scaleNearest(const IndexImage &in, int factor) {
	IndexImage out;
	if (in.w <= 0 || in.h <= 0 || in.pixels.empty() || factor <= 0) {
		out.w = 0; out.h = 0;
		return out;
	}
	out.w = in.w * factor;
	out.h = in.h * factor;
	out.pixels.resize((uint32)(out.w * out.h), 0);
	const byte *src = in.pixels.data();
	byte *dst = out.pixels.data();
	for (int iy = 0; iy < in.h; iy++) {
		for (int ix = 0; ix < in.w; ix++) {
			const byte v = src[iy * in.w + ix];
			for (int dy = 0; dy < factor; dy++) {
				byte *row = dst + ((iy * factor + dy) * out.w) + ix * factor;
				for (int dx = 0; dx < factor; dx++)
					row[dx] = v;
			}
		}
	}
	return out;
}

} // namespace Roger
} // namespace Sci
