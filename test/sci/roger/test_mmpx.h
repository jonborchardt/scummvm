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

#include <cxxtest/TestSuite.h>
#include "engines/sci/roger/roger_mmpx.h"
#include "engines/sci/roger/roger_scale.h"

using Sci::Roger::IndexImage;
using Sci::Roger::mmpx2x;
using Sci::Roger::scaleNearest;

namespace {

IndexImage makeImg(int w, int h, const byte *px) {
	IndexImage img;
	img.w = w; img.h = h;
	img.pixels.resize((size_t)w * h);
	for (int i = 0; i < w * h; i++)
		img.pixels[i] = px[i];
	return img;
}

bool sameImage(const IndexImage &a, const IndexImage &b) {
	if (a.w != b.w || a.h != b.h)
		return false;
	for (uint i = 0; i < a.pixels.size(); i++)
		if (a.pixels[i] != b.pixels[i])
			return false;
	return true;
}

} // anonymous namespace

class MmpxTestSuite : public CxxTest::TestSuite {
public:
	// A flat image hits the early-out and replicates exactly (2x dims).
	void test_flat_image_replicates() {
		const byte px[6] = { 7, 7, 7, 7, 7, 7 };
		IndexImage in = makeImg(3, 2, px);
		IndexImage out = mmpx2x(in, 0xFF);
		TS_ASSERT_EQUALS(out.w, 6);
		TS_ASSERT_EQUALS(out.h, 4);
		TS_ASSERT(sameImage(out, scaleNearest(in, 2)));
	}

	// Checkerboard: INTERIOR pixels replicate (no MMPX rule fires when the
	// full read neighbourhood — reads reach +/-3 — is unclamped: 1:1 rules
	// need D != H, intersection/luma rules need allEq4 across mixed
	// diagonals, 2:1 rules need H != B or F != D; all false on a
	// checkerboard). Border pixels are NOT asserted: edge clamping
	// duplicates neighbours and can legitimately fire rules there (e.g.
	// rule 4 sets M=F at the (0,0) corner) — the reference behaves the
	// same way.
	void test_checkerboard_interior_replicates() {
		const int W = 10, H = 10;
		byte px[W * H];
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				px[y * W + x] = ((x ^ y) & 1) ? 2 : 1;
		IndexImage in = makeImg(W, H, px);
		IndexImage out = mmpx2x(in, 0xFF);
		for (int y = 3; y < H - 3; y++)
			for (int x = 3; x < W - 3; x++)
				for (int dy = 0; dy < 2; dy++)
					for (int dx = 0; dx < 2; dx++)
						TS_ASSERT_EQUALS(out.pixels[(size_t)(y * 2 + dy) * (W * 2) + (x * 2 + dx)],
						                 in.pixels[(size_t)y * W + x]);
	}

	// Axis-aligned vertical edge: replication (no diagonal to smooth).
	void test_vertical_edge_replicates() {
		const byte px[16] = { 3, 3, 9, 9,  3, 3, 9, 9,  3, 3, 9, 9,  3, 3, 9, 9 };
		IndexImage in = makeImg(4, 4, px);
		TS_ASSERT(sameImage(mmpx2x(in, 0xFF), scaleNearest(in, 2)));
	}

	// Style preservation: an isolated opaque pixel on transparency survives
	// as an exact 2x2 block (MMPX's headline property vs EPX).
	void test_isolated_pixel_preserved() {
		byte px[25];
		for (int i = 0; i < 25; i++)
			px[i] = 5;               // 5 = clearKey (transparent)
		px[2 * 5 + 2] = 4;           // single opaque pixel at (2,2)
		IndexImage in = makeImg(5, 5, px);
		TS_ASSERT(sameImage(mmpx2x(in, 5), scaleNearest(in, 2)));
	}

	// The dual: a 1-px transparent pinhole in an opaque field survives.
	void test_pinhole_preserved() {
		byte px[25];
		for (int i = 0; i < 25; i++)
			px[i] = 4;
		px[2 * 5 + 2] = 5;           // transparent pinhole, clearKey 5
		IndexImage in = makeImg(5, 5, px);
		TS_ASSERT(sameImage(mmpx2x(in, 5), scaleNearest(in, 2)));
	}

	// Corner smoothing engages: 3x3 two-tone 1:1 staircase, X=0 (black),
	// Y=15 (white). Hand-trace of the centre pixel E=(1,1)=Y:
	//   A=X B=X C=Y / D=X E=Y F=Y / G=Y H=Y I=Y, Q=X P=X.
	//   1:1 rule 1: D==B, D!=H, D!=F; El(766) >= Dl(1); anyEq3(E,A,C,G) via
	//   C; (A!=D false, E!=P true) -> J = D = X. Rules for K, L, M all fail.
	//   Centre output block = { X, Y / Y, Y }.
	void test_diagonal_corner_smooths() {
		const byte X = 0, Y = 15;
		const byte px[9] = { X, X, Y,  X, Y, Y,  Y, Y, Y };
		IndexImage in = makeImg(3, 3, px);
		IndexImage out = mmpx2x(in, 0xFF);
		TS_ASSERT_EQUALS(out.w, 6);
		TS_ASSERT_EQUALS(out.pixels[2 * 6 + 2], X); // J: smoothed corner
		TS_ASSERT_EQUALS(out.pixels[2 * 6 + 3], Y); // K
		TS_ASSERT_EQUALS(out.pixels[3 * 6 + 2], Y); // L
		TS_ASSERT_EQUALS(out.pixels[3 * 6 + 3], Y); // M
	}

	// MMPX never invents colours: output bytes are a subset of input bytes.
	void test_palette_subset() {
		byte px[64];
		for (int i = 0; i < 64; i++)
			px[i] = (byte)((i * 7 + i / 8 * 3) % 16);
		IndexImage in = makeImg(8, 8, px);
		IndexImage out = mmpx2x(in, 0xFF);
		bool seen[256] = { false };
		for (int i = 0; i < 64; i++)
			seen[px[i]] = true;
		for (uint i = 0; i < out.pixels.size(); i++)
			TS_ASSERT(seen[out.pixels[i]]);
	}
};
