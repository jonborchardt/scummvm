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

// Faithful, function-for-function port of OldManYellsAtCode1's `agi-up`
// hybrid upscaler, via sci.js render-omyac-upscaler.ts. Pipeline:
//   build anchors -> detect line endings -> connect line/fill anchors ->
//   hybrid 6x Bresenham -> enhance passes -> null-fill.
// nativePreRender (Step 1) lives in roger_pic_native.{h,cpp} (Task 4); this
// file ports Steps 2-8 and the top-level entry point.

#include "sci/roger/gen/roger_omyac.h"
#include "sci/roger/gen/roger_passes.h"

#include <math.h>

#include "common/util.h"
#include "sci/roger/gen/roger_ega_blend.h"

namespace Sci {
namespace Roger {

// --- Constants ---
// CMD_NONE/CMD_LINE/CMD_FILL and the OMYAC_* dimensions come from
// roger_pic_native.h.

// 8-direction lookup: N=0 NE=1 E=2 SE=3 S=4 SW=5 W=6 NW=7
static const int DIR_DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int DIR_DY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

// Bit position per direction in an anchor's `connects` field.
static const int CON_BIT[8] = {
	1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7
};

// [diag, cardA, cardB] for the diagonal-flanking suppression rule.
static const int DIAGONALS[4][3] = {
	{ 1, 0, 2 },
	{ 3, 4, 2 },
	{ 5, 4, 6 },
	{ 7, 0, 6 }
};

// Sentinel for enhance(): no winning colour found (outside 0x00-0xff).
static const int ENHANCE_NO_RESULT = -1;

// --- Internal data shapes ---
// Anchor positions in upscaled coords + 8-direction connection bitfield.
struct Anchor {
	int screenX;
	int screenY;
	bool isEndpoint;
	int connects;
};

// --- Step 2: Build anchors ---
// projectOntoSegment returns (qx, qy) via out params and the squared distance.
// Math.round on non-negative coords == (int)floorf(x + 0.5f).
static float projectOntoSegment(float cx, float cy, float sx, float sy,
                                float ex, float ey, int &outQx, int &outQy) {
	float dx = ex - sx;
	float dy = ey - sy;
	float len2 = dx * dx + dy * dy;
	float t = 0;
	if (len2 > 0) {
		t = ((cx - sx) * dx + (cy - sy) * dy) / len2;
		if (t < 0)
			t = 0;
		if (t > 1)
			t = 1;
	}
	float qx = sx + t * dx;
	float qy = sy + t * dy;
	float ddx = cx - qx;
	float ddy = cy - qy;
	outQx = (int)floorf(qx + 0.5f);
	outQy = (int)floorf(qy + 0.5f);
	return ddx * ddx + ddy * ddy;
}

static void buildAnchors(const NativeRef &ref, Common::Array<Anchor> &anchors) {
	anchors.resize(OMYAC_NATIVE_W * OMYAC_NATIVE_H);
	for (int y = 0; y < OMYAC_NATIVE_H; y++) {
		for (int x = 0; x < OMYAC_NATIVE_W; x++) {
			int idx = y * OMYAC_NATIVE_W + x;
			Anchor &a = anchors[idx];
			a.screenX = (int)floorf((float)(x * OMYAC_SCALE) + (float)OMYAC_SCALE / 2.0f);
			a.screenY = (int)floorf((float)(y * OMYAC_SCALE) + (float)OMYAC_SCALE / 2.0f);
			a.isEndpoint = false;
			a.connects = 0;
		}
	}

	Common::Array<float> bestDist2;
	bestDist2.resize(OMYAC_NATIVE_W * OMYAC_NATIVE_H);
	for (uint i = 0; i < bestDist2.size(); i++)
		bestDist2[i] = (float)INFINITY;

	for (uint cmdIdx = 0; cmdIdx < ref.segments.size(); cmdIdx++) {
		const Common::Array<int> &segs = ref.segments[cmdIdx];
		// Flat [x0,y0,x1,y1,...] groups of 4. Guard malformed lengths.
		for (uint s = 0; s + 3 < segs.size(); s += 4) {
			int x0 = segs[s], y0 = segs[s + 1], x1 = segs[s + 2], y1 = segs[s + 3];
			float svx = x0 * OMYAC_SCALE + (float)OMYAC_SCALE / 2.0f;
			float svy = y0 * OMYAC_SCALE + (float)OMYAC_SCALE / 2.0f;
			float evx = x1 * OMYAC_SCALE + (float)OMYAC_SCALE / 2.0f;
			float evy = y1 * OMYAC_SCALE + (float)OMYAC_SCALE / 2.0f;
			int minX = MIN(x0, x1);
			int maxX = MAX(x0, x1);
			int minY = MIN(y0, y1);
			int maxY = MAX(y0, y1);
			for (int cy = minY; cy <= maxY; cy++) {
				for (int cx = minX; cx <= maxX; cx++) {
					if (cx < 0 || cx >= OMYAC_NATIVE_W || cy < 0 || cy >= OMYAC_NATIVE_H)
						continue;
					int idx = cy * OMYAC_NATIVE_W + cx;
					if (ref.refCmd[idx] != (int16)cmdIdx)
						continue;
					float ccx = cx * OMYAC_SCALE + (float)OMYAC_SCALE / 2.0f;
					float ccy = cy * OMYAC_SCALE + (float)OMYAC_SCALE / 2.0f;
					int qx, qy;
					float dist2 = projectOntoSegment(ccx, ccy, svx, svy, evx, evy, qx, qy);
					if (dist2 < bestDist2[idx]) {
						bestDist2[idx] = dist2;
						anchors[idx].screenX = qx;
						anchors[idx].screenY = qy;
					}
				}
			}
		}
	}
}

// --- Step 3: Detect line endpoints ---
static void detectLineEndings(const NativeRef &ref, Common::Array<Anchor> &anchors,
                              int endpointMaxSame) {
	for (int y = 0; y < OMYAC_NATIVE_H; y++) {
		for (int x = 0; x < OMYAC_NATIVE_W; x++) {
			int idx = y * OMYAC_NATIVE_W + x;
			if (ref.cmdType[idx] != CMD_LINE)
				continue;
			int cmdId = ref.refCmd[idx];
			int same = 0;
			for (int dir = 0; dir < 8; dir++) {
				int nx = x + DIR_DX[dir];
				int ny = y + DIR_DY[dir];
				if (nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H)
					continue;
				if (ref.refCmd[ny * OMYAC_NATIVE_W + nx] == cmdId)
					same++;
			}
			anchors[idx].isEndpoint = same < endpointMaxSame;
		}
	}
}

// --- Step 4: Connect line anchors ---
static bool isLineCmd(const NativeRef &ref, int idx) {
	return ref.cmdType[idx] == CMD_LINE;
}

// cmdType-aware "same colour" test (see TS comment): cells are only same-colour
// if both are unfilled OR both plotted with the same byte value.
static bool sameColor(const NativeRef &ref, int a, int b) {
	byte tA = ref.cmdType[a];
	byte tB = ref.cmdType[b];
	if (tA == CMD_NONE || tB == CMD_NONE)
		return tA == tB;
	return ref.refPixel[a] == ref.refPixel[b];
}

static void connectLineAnchors(const NativeRef &ref, Common::Array<Anchor> &anchors) {
	for (int y = 0; y < OMYAC_NATIVE_H; y++) {
		for (int x = 0; x < OMYAC_NATIVE_W; x++) {
			int idx = y * OMYAC_NATIVE_W + x;
			if (ref.cmdType[idx] != CMD_LINE)
				continue;
			int cmdId = ref.refCmd[idx];
			Anchor &anchor = anchors[idx];
			byte color = ref.refPixel[idx];

			for (int dir = 0; dir < 8; dir++) {
				int nx = x + DIR_DX[dir];
				int ny = y + DIR_DY[dir];
				if (nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H)
					continue;
				int nidx = ny * OMYAC_NATIVE_W + nx;
				if (ref.refCmd[nidx] == cmdId)
					anchor.connects |= CON_BIT[dir];
			}

			if (!anchor.isEndpoint)
				continue;

			Common::Array<int> pickedCmd;
			Common::Array<int> pickedDir;
			Common::Array<bool> pickedHasOpp;
			bool hasForward = false;

			for (int dir = 0; dir < 8; dir++) {
				int nx = x + DIR_DX[dir];
				int ny = y + DIR_DY[dir];
				if (nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H)
					continue;
				int nidx = ny * OMYAC_NATIVE_W + nx;
				if (ref.refPixel[nidx] != color)
					continue;
				if (!isLineCmd(ref, nidx))
					continue;
				int cmdN = ref.refCmd[nidx];
				if (cmdN == cmdId)
					continue;

				int opp = (dir + 4) % 8;
				int ox = x + DIR_DX[opp];
				int oy = y + DIR_DY[opp];
				bool hasOpp = ox >= 0 && ox < OMYAC_NATIVE_W && oy >= 0 && oy < OMYAC_NATIVE_H &&
				              ref.refCmd[oy * OMYAC_NATIVE_W + ox] == cmdId;

				int slot = -1;
				for (uint k = 0; k < pickedCmd.size(); k++) {
					if (pickedCmd[k] == cmdN) {
						slot = (int)k;
						break;
					}
				}
				if (slot < 0) {
					pickedCmd.push_back(cmdN);
					pickedDir.push_back(dir);
					pickedHasOpp.push_back(hasOpp);
				} else if (hasOpp && !pickedHasOpp[slot]) {
					pickedDir[slot] = dir;
					pickedHasOpp[slot] = true;
				}
			}

			for (uint k = 0; k < pickedCmd.size(); k++) {
				anchor.connects |= CON_BIT[pickedDir[k]];
				hasForward = true;
			}

			if (hasForward)
				continue;

			for (int dir = 0; dir < 8; dir++) {
				int nx = x + DIR_DX[dir];
				int ny = y + DIR_DY[dir];
				if (nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H)
					continue;
				if (ref.refCmd[ny * OMYAC_NATIVE_W + nx] != cmdId)
					continue;
				int fwd = (dir + 4) % 8;
				int fx = x + DIR_DX[fwd];
				int fy = y + DIR_DY[fwd];
				bool fIn = fx >= 0 && fx < OMYAC_NATIVE_W && fy >= 0 && fy < OMYAC_NATIVE_H;
				if (fIn && isLineCmd(ref, fy * OMYAC_NATIVE_W + fx))
					anchor.connects |= CON_BIT[fwd];
				if (!fIn)
					anchor.connects |= CON_BIT[fwd];
				break;
			}
		}
	}
}

// --- Step 5: Connect fill anchors ---
static void connectFillAnchors(const NativeRef &ref, Common::Array<Anchor> &anchors,
                               bool diagFlankSuppress) {
	for (int y = 0; y < OMYAC_NATIVE_H; y++) {
		for (int x = 0; x < OMYAC_NATIVE_W; x++) {
			int idx = y * OMYAC_NATIVE_W + x;
			if (ref.cmdType[idx] == CMD_LINE)
				continue;

			Anchor &anchor = anchors[idx];

			for (int dir = 0; dir < 8; dir += 2) {
				int nx = x + DIR_DX[dir];
				int ny = y + DIR_DY[dir];
				bool oob = nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H;
				if (!oob) {
					int nidx = ny * OMYAC_NATIVE_W + nx;
					if (!sameColor(ref, idx, nidx))
						continue;
					if (isLineCmd(ref, nidx))
						continue;
				}
				anchor.connects |= CON_BIT[dir];
			}

			for (int d = 0; d < 4; d++) {
				int diag = DIAGONALS[d][0];
				int cardA = DIAGONALS[d][1];
				int cardB = DIAGONALS[d][2];
				int dnx = x + DIR_DX[diag];
				int dny = y + DIR_DY[diag];
				if (dnx < 0 || dnx >= OMYAC_NATIVE_W || dny < 0 || dny >= OMYAC_NATIVE_H)
					continue;
				int dnidx = dny * OMYAC_NATIVE_W + dnx;
				if (!sameColor(ref, idx, dnidx))
					continue;
				if (isLineCmd(ref, dnidx))
					continue;

				int ax = x + DIR_DX[cardA];
				int ay = y + DIR_DY[cardA];
				int bx = x + DIR_DX[cardB];
				int by = y + DIR_DY[cardB];
				bool aIn = ax >= 0 && ax < OMYAC_NATIVE_W && ay >= 0 && ay < OMYAC_NATIVE_H;
				bool bIn = bx >= 0 && bx < OMYAC_NATIVE_W && by >= 0 && by < OMYAC_NATIVE_H;
				if (diagFlankSuppress && aIn && bIn) {
					if (isLineCmd(ref, ay * OMYAC_NATIVE_W + ax) &&
					    isLineCmd(ref, by * OMYAC_NATIVE_W + bx))
						continue;
				}
				anchor.connects |= CON_BIT[diag];
			}
		}
	}
}

// --- Step 6: Hybrid render ---
// Half-A/half-B Bresenham used by the hybrid render pass.
static void drawHybridLine(Common::Array<byte> &buf, Common::Array<byte> &typeBuf,
                           int x0, int y0, int x1, int y1,
                           byte colorA, byte colorB, byte pixType) {
	int dx = ABS(x1 - x0);
	int dy = ABS(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int total = MAX(dx, dy) + 1;
	float half = (float)total / 2.0f;
	int err = dx - dy;
	int step = 0;
	int cx = x0;
	int cy = y0;
	while (true) {
		if (cx >= 0 && cx < OMYAC_HYBRID_W && cy >= 0 && cy < OMYAC_HYBRID_H) {
			int idx = cy * OMYAC_HYBRID_W + cx;
			buf[idx] = step < half ? colorA : colorB;
			typeBuf[idx] = pixType;
		}
		if (cx == x1 && cy == y1)
			break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			cx += sx;
		}
		if (e2 < dx) {
			err += dx;
			cy += sy;
		}
		step++;
	}
}

static void hybridRender(const NativeRef &ref, Common::Array<Anchor> &anchors,
                         Common::Array<byte> &buf, Common::Array<byte> &typeBuf) {
	buf.resize(OMYAC_HYBRID_W * OMYAC_HYBRID_H);
	typeBuf.resize(OMYAC_HYBRID_W * OMYAC_HYBRID_H);
	for (uint i = 0; i < buf.size(); i++) {
		buf[i] = 0xff;
		typeBuf[i] = 0;
	}

	// [ddx, ddy, dir, outBit, inBit]; dir is unused (named _dir in the TS).
	const int PAIRS[4][5] = {
		{ 1, 0, 2, CON_BIT[2], CON_BIT[6] },
		{ 1, 1, 3, CON_BIT[3], CON_BIT[7] },
		{ 0, 1, 4, CON_BIT[4], CON_BIT[0] },
		{ -1, 1, 5, CON_BIT[5], CON_BIT[1] }
	};

	for (int y = 0; y < OMYAC_NATIVE_H; y++) {
		for (int x = 0; x < OMYAC_NATIVE_W; x++) {
			int idx = y * OMYAC_NATIVE_W + x;
			Anchor &a = anchors[idx];
			byte colA = ref.refPixel[idx];
			byte pixTypeA = ref.cmdType[idx];
			for (int p = 0; p < 4; p++) {
				int ddx = PAIRS[p][0];
				int ddy = PAIRS[p][1];
				int outBit = PAIRS[p][3];
				int inBit = PAIRS[p][4];
				int nx = x + ddx;
				int ny = y + ddy;
				if (nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H)
					continue;
				int nidx = ny * OMYAC_NATIVE_W + nx;
				Anchor &b = anchors[nidx];
				if (!(a.connects & outBit) && !(b.connects & inBit))
					continue;
				byte colB = ref.refPixel[nidx];
				drawHybridLine(buf, typeBuf, a.screenX, a.screenY, b.screenX, b.screenY,
				               colA, colB, pixTypeA);
			}
		}
	}

	for (int y = 0; y < OMYAC_NATIVE_H; y++) {
		for (int x = 0; x < OMYAC_NATIVE_W; x++) {
			int idx = y * OMYAC_NATIVE_W + x;
			Anchor &a = anchors[idx];
			if (a.connects == 0)
				continue;
			byte col = ref.refPixel[idx];
			byte pixType = ref.cmdType[idx];
			for (int dir = 0; dir < 8; dir++) {
				int nx = x + DIR_DX[dir];
				int ny = y + DIR_DY[dir];
				bool oob = nx < 0 || nx >= OMYAC_NATIVE_W || ny < 0 || ny >= OMYAC_NATIVE_H;
				if (!oob)
					continue;
				if (!(a.connects & CON_BIT[dir]))
					continue;
				int steps = OMYAC_HYBRID_W + OMYAC_HYBRID_H;
				if (DIR_DX[dir] > 0)
					steps = MIN(steps, OMYAC_HYBRID_W - 1 - a.screenX);
				if (DIR_DX[dir] < 0)
					steps = MIN(steps, a.screenX);
				if (DIR_DY[dir] > 0)
					steps = MIN(steps, OMYAC_HYBRID_H - 1 - a.screenY);
				if (DIR_DY[dir] < 0)
					steps = MIN(steps, a.screenY);
				int bx = a.screenX + DIR_DX[dir] * steps;
				int by = a.screenY + DIR_DY[dir] * steps;
				drawHybridLine(buf, typeBuf, a.screenX, a.screenY, bx, by, col, col, pixType);
			}
		}
	}

	for (int i = 0; i < OMYAC_NATIVE_W * OMYAC_NATIVE_H; i++) {
		Anchor &a = anchors[i];
		if (a.screenX < 0 || a.screenX >= OMYAC_HYBRID_W ||
		    a.screenY < 0 || a.screenY >= OMYAC_HYBRID_H)
			continue;
		if (ref.cmdType[i] == CMD_NONE)
			continue;
		int dotIdx = a.screenY * OMYAC_HYBRID_W + a.screenX;
		buf[dotIdx] = ref.refPixel[i];
		typeBuf[dotIdx] = ref.cmdType[i];
	}
}

// --- Step 7: enhance() kernel ---
static bool enhanceEligible(int pixType, int mode) {
	if (mode == 1)
		return pixType == CMD_LINE;
	if (mode == 2)
		return pixType == CMD_FILL || pixType == CMD_NONE;
	return true;
}

static int enhanceResultType(int mode, int lineCount, int fillCount) {
	if (mode == 1)
		return CMD_LINE;
	if (mode == 2)
		return CMD_FILL;
	return lineCount >= fillCount ? CMD_LINE : CMD_FILL;
}

static void enhance(Common::Array<byte> &buf, Common::Array<byte> &typeBuf, int mode,
                    const OmyacParams &params) {
	Common::Array<byte> src(buf);
	Common::Array<byte> srcType(typeBuf);
	int minVotes = mode == 1 ? params.minVotesLine : params.minVotesFillAll;
	// Vote on the full doubled-nibble byte space (256 buckets).
	int count[256];
	int tied[256];

	for (int y = 0; y < OMYAC_HYBRID_H; y++) {
		for (int x = 0; x < OMYAC_HYBRID_W; x++) {
			int idx = y * OMYAC_HYBRID_W + x;
			bool isBackground = srcType[idx] == CMD_NONE;
			bool isFillPixel = srcType[idx] == CMD_FILL;
			bool overrideable = mode == 1 && isFillPixel;
			if (!isBackground && !overrideable)
				continue;

			int lineNeighbours = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (!dx && !dy)
						continue;
					int nx = x + dx;
					int ny = y + dy;
					if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
						continue;
					int nidx = ny * OMYAC_HYBRID_W + nx;
					if (srcType[nidx] == CMD_NONE)
						continue;
					if (srcType[nidx] == CMD_LINE)
						lineNeighbours++;
				}
			}
			bool suppressFill = lineNeighbours >= params.fillSuppressLineNeighbours;

			for (int c = 0; c < 256; c++)
				count[c] = 0;
			int lineCount = 0;
			int fillCount = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (!dx && !dy)
						continue;
					int nx = x + dx;
					int ny = y + dy;
					if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
						continue;
					int nidx = ny * OMYAC_HYBRID_W + nx;
					if (srcType[nidx] == CMD_NONE)
						continue;
					int nt = srcType[nidx];
					if (suppressFill && nt != CMD_LINE)
						continue;
					if (!enhanceEligible(nt, mode))
						continue;
					count[src[nidx]]++;
					if (nt == CMD_LINE)
						lineCount++;
					else
						fillCount++;
				}
			}

			int bestCount = minVotes;
			int ntied = 0;
			for (int c = 0; c < 256; c++) {
				if (count[c] > bestCount) {
					bestCount = count[c];
					ntied = 0;
					tied[ntied++] = c;
				} else if (count[c] == bestCount && bestCount > minVotes) {
					tied[ntied++] = c;
				}
			}

			int resultColor = ENHANCE_NO_RESULT;
			if (ntied == 1) {
				resultColor = tied[0];
			} else if (ntied > 1) {
				if (!params.tieBreakBlend) {
					resultColor = tied[0];
				} else {
					// Tie-break: average the BLEND_TABLE (okLab-mixed) RGB of each
					// tied byte, pick the byte whose blended colour is nearest the
					// average over all 256 doubled-nibble bytes.
					float sumR = 0;
					float sumG = 0;
					float sumB = 0;
					for (int k = 0; k < ntied; k++) {
						uint32 blend = BLEND_TABLE[tied[k]];
						sumR += blend & 0xff;
						sumG += (blend >> 8) & 0xff;
						sumB += (blend >> 16) & 0xff;
					}
					float avgR = sumR / ntied;
					float avgG = sumG / ntied;
					float avgB = sumB / ntied;
					int nearest = tied[0];
					float nearestDist = (float)INFINITY;
					for (int c = 0; c < 256; c++) {
						uint32 blend = BLEND_TABLE[c];
						float dr = avgR - (blend & 0xff);
						float dg = avgG - ((blend >> 8) & 0xff);
						float db = avgB - ((blend >> 16) & 0xff);
						float d = dr * dr + dg * dg + db * db;
						if (d < nearestDist) {
							nearestDist = d;
							nearest = c;
						}
					}
					resultColor = nearest;
				}
			}

			if (resultColor != ENHANCE_NO_RESULT) {
				buf[idx] = (byte)resultColor;
				typeBuf[idx] = (byte)enhanceResultType(mode, lineCount, fillCount);
			}
		}
	}

	// Isolated-pixel pass.
	if (!params.isolatedPixelPass)
		return;
	for (int y = 0; y < OMYAC_HYBRID_H; y++) {
		for (int x = 0; x < OMYAC_HYBRID_W; x++) {
			int idx = y * OMYAC_HYBRID_W + x;
			byte color = src[idx];
			int pixType = srcType[idx];
			if (pixType == CMD_NONE)
				continue;
			if (!enhanceEligible(pixType, mode))
				continue;

			bool hasSame = false;
			for (int dy = -1; dy <= 1 && !hasSame; dy++) {
				for (int dx = -1; dx <= 1 && !hasSame; dx++) {
					if (!dx && !dy)
						continue;
					int nx = x + dx;
					int ny = y + dy;
					if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
						continue;
					int nidx = ny * OMYAC_HYBRID_W + nx;
					if (srcType[nidx] != CMD_NONE && src[nidx] == color &&
					    enhanceEligible(srcType[nidx], mode)) {
						hasSame = true;
					}
				}
			}
			if (hasSame)
				continue;

			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (!dx && !dy)
						continue;
					int nx = x + dx;
					int ny = y + dy;
					if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
						continue;
					int nidx = ny * OMYAC_HYBRID_W + nx;
					if (srcType[nidx] == CMD_NONE) {
						buf[nidx] = color;
						typeBuf[nidx] = (byte)pixType;
					}
				}
			}
		}
	}
}

// --- Step 8: Final null-pixel mode fill ---
static void fillNullPixels(const NativeRef &ref, Common::Array<byte> &buf,
                           Common::Array<byte> &typeBuf, Common::Array<byte> &backfilled,
                           const OmyacParams &params) {
	// Diagnostic mask: 1 wherever this pass paints a pixel.
	backfilled.resize(OMYAC_HYBRID_W * OMYAC_HYBRID_H);
	for (uint i = 0; i < backfilled.size(); i++)
		backfilled[i] = 0;

	if (params.backfillOwnCell) {
		// Bounded, direction-neutral backfill (shipping default). Phase 1: up to
		// backfillFloodRounds breadth-first majority rounds. Each round votes on a
		// FROZEN copy of the buffers, so claimed colours advance exactly 1 px per
		// round and opposing fronts meet symmetrically  --  unlike the legacy path
		// below, whose in-place scan-order vote let a foreign colour cascade
		// arbitrarily far down-right (the pod-door "cyan through the transparent
		// corner" artifact). Interior gaps between hybrid strokes are <= ~3 px, so
		// the smooth flood-rasterized look survives. Phase 2: anything still
		// unclaimed takes its OWN native cell's colour (native-faithful).
		int count[256];
		for (int round = 0; round < params.backfillFloodRounds; round++) {
			Common::Array<byte> srcBuf(buf);
			Common::Array<byte> srcType(typeBuf);
			bool any = false;
			for (int y = 0; y < OMYAC_HYBRID_H; y++) {
				for (int x = 0; x < OMYAC_HYBRID_W; x++) {
					int idx = y * OMYAC_HYBRID_W + x;
					if (srcType[idx] != CMD_NONE)
						continue;
					for (int c = 0; c < 256; c++)
						count[c] = 0;
					byte bestColor = 0x00;
					int bestCount = 0;
					for (int dy = -1; dy <= 1; dy++) {
						for (int dx = -1; dx <= 1; dx++) {
							if (!dx && !dy)
								continue;
							int nx = x + dx;
							int ny = y + dy;
							if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
								continue;
							int nidx = ny * OMYAC_HYBRID_W + nx;
							if (srcType[nidx] == CMD_NONE)
								continue;
							byte c = srcBuf[nidx];
							count[c]++;
							if (count[c] > bestCount) {
								bestCount = count[c];
								bestColor = c;
							}
						}
					}
					if (bestCount > 0) {
						buf[idx] = bestColor;
						typeBuf[idx] = CMD_FILL;
						backfilled[idx] = 1;
						any = true;
					}
				}
			}
			if (!any)
				break;
		}
		// Phase 2: native-faithful terminal fill for everything the bounded flood
		// never reached.
		for (int y = 0; y < OMYAC_HYBRID_H; y++) {
			for (int x = 0; x < OMYAC_HYBRID_W; x++) {
				int idx = y * OMYAC_HYBRID_W + x;
				if (typeBuf[idx] != CMD_NONE)
					continue;
				backfilled[idx] = 1;
				int cell = (y / OMYAC_SCALE) * OMYAC_NATIVE_W + (x / OMYAC_SCALE);
				buf[idx] = ref.refPixel[cell];
				typeBuf[idx] = CMD_FILL;
			}
		}
		return;
	}

	// Legacy TS-port behaviour (backfillOwnCell == false): 8-neighbour majority in
	// scan order on the buffer being mutated.
	int count[256];
	for (int y = 0; y < OMYAC_HYBRID_H; y++) {
		for (int x = 0; x < OMYAC_HYBRID_W; x++) {
			int idx = y * OMYAC_HYBRID_W + x;
			if (typeBuf[idx] != CMD_NONE)
				continue;
			backfilled[idx] = 1;

			for (int c = 0; c < 256; c++)
				count[c] = 0;
			byte bestColor = 0x00;
			int bestCount = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (!dx && !dy)
						continue;
					int nx = x + dx;
					int ny = y + dy;
					if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
						continue;
					int nidx = ny * OMYAC_HYBRID_W + nx;
					if (typeBuf[nidx] == CMD_NONE)
						continue;
					byte c = buf[nidx];
					count[c]++;
					if (count[c] > bestCount) {
						bestCount = count[c];
						bestColor = c;
					}
				}
			}
			buf[idx] = bestColor;
			typeBuf[idx] = CMD_FILL;
		}
	}
}

// --- Top-level entry point ---
Common::Array<int> defaultPasses() {
	return parsePassString(kDefaultPassString);
}

OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes,
                        const OmyacParams &params) {
	Common::Array<Anchor> anchors;
	buildAnchors(ref, anchors);
	detectLineEndings(ref, anchors, params.endpointMaxSame);
	connectLineAnchors(ref, anchors);
	connectFillAnchors(ref, anchors, params.diagFlankSuppress);

	OmyacResult out;
	hybridRender(ref, anchors, out.pixels, out.cmdType);

	for (uint i = 0; i < passes.size(); i++)
		enhance(out.pixels, out.cmdType, passes[i], params);

	fillNullPixels(ref, out.pixels, out.cmdType, out.backfilled, params);

	return out;
}

OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes) {
	return renderOmyac(ref, passes, OmyacParams());
}

} // namespace Roger
} // namespace Sci
