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

// Port of the MMPX reference implementation (MIT license):
//   Copyright 2020 Morgan McGuire & Mara Gagiu.
//   https://casual-effects.com/research/McGuire2021PixelArt/index.html
// Rule bodies and their order are kept verbatim; only the pixel type
// (EGA palette index instead of ABGR32) and output addressing differ.

#include "sci/roger/roger_mmpx.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

namespace {

// Standard 16-colour EGA palette (r,g,b per index) — fixed and engine-free
// so luma comparisons are deterministic in unit tests.
static const byte EGA_RGB[16][3] = {
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0xAA}, {0x00, 0xAA, 0x00}, {0x00, 0xAA, 0xAA},
	{0xAA, 0x00, 0x00}, {0xAA, 0x00, 0xAA}, {0xAA, 0x55, 0x00}, {0xAA, 0xAA, 0xAA},
	{0x55, 0x55, 0x55}, {0x55, 0x55, 0xFF}, {0x55, 0xFF, 0x55}, {0x55, 0xFF, 0xFF},
	{0xFF, 0x55, 0x55}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0x55}, {0xFF, 0xFF, 0xFF}
};

// Reference luma: (r+g+b+1) * (256 - alpha). Opaque -> multiplier 1; the
// clearKey -> alpha 0 -> multiplier 256, exactly as in the ARGB reference.
struct LumaLut {
	uint32 v[256];
	explicit LumaLut(byte clearKey) {
		for (int i = 0; i < 256; i++) {
			const byte *rgb = EGA_RGB[i & 0x0f];
			const uint32 base = (uint32)rgb[0] + rgb[1] + rgb[2] + 1;
			v[i] = base * ((i == clearKey) ? 256 : 1);
		}
	}
};

inline bool allEq2(byte b, byte a0, byte a1) { return b == a0 && b == a1; }
inline bool allEq3(byte b, byte a0, byte a1, byte a2) { return b == a0 && b == a1 && b == a2; }
inline bool allEq4(byte b, byte a0, byte a1, byte a2, byte a3) { return b == a0 && b == a1 && b == a2 && b == a3; }
inline bool anyEq3(byte b, byte a0, byte a1, byte a2) { return b == a0 || b == a1 || b == a2; }
inline bool noneEq2(byte b, byte a0, byte a1) { return b != a0 && b != a1; }
inline bool noneEq4(byte b, byte a0, byte a1, byte a2, byte a3) { return b != a0 && b != a1 && b != a2 && b != a3; }

struct SrcView {
	const byte *p;
	int w, h;
	byte at(int x, int y) const {
		x = CLIP<int>(x, 0, w - 1);
		y = CLIP<int>(y, 0, h - 1);
		return p[y * w + x];
	}
};

} // end of anonymous namespace

IndexImage mmpx2x(const IndexImage &in, byte clearKey) {
	IndexImage out;
	out.w = in.w * 2;
	out.h = in.h * 2;
	out.pixels.resize((size_t)out.w * out.h);
	if (in.w <= 0 || in.h <= 0)
		return out;

	const LumaLut lut(clearKey);
	const SrcView s = { in.pixels.begin(), in.w, in.h };

	for (int y = 0; y < in.h; y++) {
		for (int x = 0; x < in.w; x++) {
			const byte A = s.at(x - 1, y - 1), B = s.at(x, y - 1), C = s.at(x + 1, y - 1);
			const byte D = s.at(x - 1, y),     E = s.at(x, y),     F = s.at(x + 1, y);
			const byte G = s.at(x - 1, y + 1), H = s.at(x, y + 1), I = s.at(x + 1, y + 1);
			byte J = E, K = E, L = E, M = E;

			if (A != E || B != E || C != E || D != E || F != E || G != E || H != E || I != E) {
				const byte Q = s.at(x - 2, y), R = s.at(x + 2, y);
				const byte P = s.at(x, y - 2), S = s.at(x, y + 2);
				const uint32 Bl = lut.v[B], Dl = lut.v[D], El = lut.v[E], Fl = lut.v[F], Hl = lut.v[H];

				// 1:1 slope rules
				if ((D == B && D != H && D != F) && (El >= Dl || E == A) && anyEq3(E, A, C, G) && ((El < Dl) || A != D || E != P || E != Q)) J = D;
				if ((B == F && B != D && B != H) && (El >= Bl || E == C) && anyEq3(E, A, C, I) && ((El < Bl) || C != B || E != P || E != R)) K = B;
				if ((H == D && H != F && H != B) && (El >= Hl || E == G) && anyEq3(E, A, G, I) && ((El < Hl) || G != H || E != S || E != Q)) L = H;
				if ((F == H && F != B && F != D) && (El >= Fl || E == I) && anyEq3(E, C, G, I) && ((El < Fl) || I != H || E != R || E != S)) M = F;

				// Intersection rules
				if ((E != F && allEq4(E, C, I, D, Q) && allEq2(F, B, H)) && (F != s.at(x + 3, y))) K = M = F;
				if ((E != D && allEq4(E, A, G, F, R) && allEq2(D, B, H)) && (D != s.at(x - 3, y))) J = L = D;
				if ((E != H && allEq4(E, G, I, B, P) && allEq2(H, D, F)) && (H != s.at(x, y + 3))) L = M = H;
				if ((E != B && allEq4(E, A, C, H, S) && allEq2(B, D, F)) && (B != s.at(x, y - 3))) J = K = B;
				if (Bl < El && allEq4(E, G, H, I, S) && noneEq4(E, A, D, C, F)) J = K = B;
				if (Hl < El && allEq4(E, A, B, C, P) && noneEq4(E, D, G, I, F)) L = M = H;
				if (Fl < El && allEq4(E, A, D, G, Q) && noneEq4(E, B, C, I, H)) K = M = F;
				if (Dl < El && allEq4(E, C, F, I, R) && noneEq4(E, B, A, G, H)) J = L = D;

				// 2:1 slope rules
				if (H != B) {
					if (H != A && H != E && H != C) {
						if (allEq3(H, G, F, R) && noneEq2(H, D, s.at(x + 2, y - 1))) L = M;
						if (allEq3(H, I, D, Q) && noneEq2(H, F, s.at(x - 2, y - 1))) M = L;
					}
					if (B != I && B != G && B != E) {
						if (allEq3(B, A, F, R) && noneEq2(B, D, s.at(x + 2, y + 1))) J = K;
						if (allEq3(B, C, D, Q) && noneEq2(B, F, s.at(x - 2, y + 1))) K = J;
					}
				}
				if (F != D) {
					if (D != I && D != E && D != C) {
						if (allEq3(D, A, H, S) && noneEq2(D, B, s.at(x + 1, y + 2))) J = L;
						if (allEq3(D, G, B, P) && noneEq2(D, H, s.at(x + 1, y - 2))) L = J;
					}
					if (F != E && F != A && F != G) {
						if (allEq3(F, C, H, S) && noneEq2(F, B, s.at(x - 1, y + 2))) K = M;
						if (allEq3(F, I, B, P) && noneEq2(F, H, s.at(x - 1, y - 2))) M = K;
					}
				}
			}

			byte *row0 = out.pixels.begin() + (size_t)(y * 2) * out.w + x * 2;
			byte *row1 = row0 + out.w;
			row0[0] = J; row0[1] = K;
			row1[0] = L; row1[1] = M;
		}
	}
	return out;
}

} // End of namespace Roger
} // End of namespace Sci
