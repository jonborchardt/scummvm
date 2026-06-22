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

#include "sci/roger/roger_pic_native.h"
#include "common/queue.h"

namespace Sci {
namespace Roger {

// ─── default-palette.ts: 40-entry default palette ───────────────────────────
static const byte DEFAULT_PALETTE[40] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
	0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x88, 0x88, 0x01, 0x02, 0x03,
	0x04, 0x05, 0x06, 0x88, 0x88, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd,
	0xfe, 0xff, 0x08, 0x91, 0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x88
};

// ─── circles.ts: per-size brush bitmask table (verbatim) ────────────────────
// Variable-height rows (1,3,5,7,9,11,13,15); a max-width 15-col table holds
// each size's rows in [0 .. rows-1]. The brush loop indexes sprite[py-top].
static const uint16 CIRCLE_BITMAPS[8][15] = {
	{ 0x1 },
	{ 0x2, 0x7, 0x2 },
	{ 0x0e, 0x1f, 0x1f, 0x1f, 0x0e },
	{ 0x1c, 0x3e, 0x7f, 0x7f, 0x7f, 0x3e, 0x1c },
	{ 0x038, 0x0fe, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x0fe, 0x038 },
	{ 0x070, 0x1fc, 0x3fe, 0x3fe, 0x7ff, 0x7ff, 0x7ff, 0x3fe, 0x3fe, 0x1fc, 0x0f8 },
	{ 0x1f0, 0x7fc, 0xffe, 0xffe, 0x1fff, 0x1fff, 0x1fff, 0x1fff, 0x1fff, 0xffe, 0xffe, 0x7fc, 0x1f0 },
	{ 0x3e0, 0xff8, 0x1ffc, 0x3ffe, 0x3ffe, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x3ffe, 0x3ffe, 0x1ffc, 0xff8, 0x3e0 }
};

// ─── noise.ts: 256-entry boolean table + 120 offsets (verbatim) ─────────────
// The 32 source bytes are expanded MSB-first into 256 booleans.
static const byte NOISE_SRC[32] = {
	0x20, 0x94, 0x02, 0x24, 0x90, 0x82, 0xa4, 0xa2, 0x82, 0x09, 0x0a, 0x22, 0x12,
	0x10, 0x42, 0x14, 0x91, 0x4a, 0x91, 0x11, 0x08, 0x12, 0x25, 0x10, 0x22, 0xa8,
	0x14, 0x24, 0x00, 0x50, 0x24, 0x04
};

static const int NOISE_OFFSETS[120] = {
	0x00, 0x18, 0x30, 0xc4, 0xdc, 0x65, 0xeb, 0x48, 0x60, 0xbd, 0x89, 0x04, 0x0a,
	0xf4, 0x7d, 0x6d, 0x85, 0xb0, 0x8e, 0x95, 0x1f, 0x22, 0x0d, 0xdf, 0x2a, 0x78,
	0xd5, 0x73, 0x1c, 0xb4, 0x40, 0xa1, 0xb9, 0x3c, 0xca, 0x58, 0x92, 0x34, 0xcc,
	0xce, 0xd7, 0x42, 0x90, 0x0f, 0x8b, 0x7f, 0x32, 0xed, 0x5c, 0x9d, 0xc8, 0x99,
	0xad, 0x4e, 0x56, 0xa6, 0xf7, 0x68, 0xb7, 0x25, 0x82, 0x37, 0x3a, 0x51, 0x69,
	0x26, 0x38, 0x52, 0x9e, 0x9a, 0x4f, 0xa7, 0x43, 0x10, 0x80, 0xee, 0x3d, 0x59,
	0x35, 0xcf, 0x79, 0x74, 0xb5, 0xa2, 0xb1, 0x96, 0x23, 0xe0, 0xbe, 0x05, 0xf5,
	0x6e, 0x19, 0xc5, 0x66, 0x49, 0xf0, 0xd1, 0x54, 0xa9, 0x70, 0x4b, 0xa4, 0xe2,
	0xe6, 0xe5, 0xab, 0xe4, 0xd2, 0xaa, 0x4c, 0xe3, 0x06, 0x6f, 0xc6, 0x4a, 0x75,
	0xa3, 0x97, 0xe1
};

// NOISE[i] == true iff bit (7 - (i&7)) of NOISE_SRC[i>>3] is set.
static inline bool noiseAt(int i) {
	int idx = i & 0xff;
	int b = NOISE_SRC[idx >> 3];
	return ((b >> (7 - (idx & 7))) & 0x1) == 0x1;
}

// ─── screen-buffer.ts: layer buffers + low-level plotters ───────────────────
// Port of createBuffers at scale [1,1] (so plot==plotOne, no doubling), plus
// the omyac tracking wrapper (render-omyac-upscaler.ts trackPlot/isFillable).
struct Buffers {
	int width, height;
	Common::Array<byte> visible;   // init 0xff
	Common::Array<byte> priority;  // init 0x00
	Common::Array<byte> control;   // init 0x00
	byte pal[4][40];               // four palettes, copies of DEFAULT_PALETTE

	// Tracking state (set per command by nativePreRender).
	NativeRef *ref;
	int currentCmdIdx;
	int currentType;

	Buffers(int w, int h) : width(w), height(h), ref(nullptr), currentCmdIdx(-1), currentType(CMD_NONE) {
		visible.resize(w * h);
		priority.resize(w * h);
		control.resize(w * h);
		for (int i = 0; i < w * h; i++) {
			visible[i] = 0xff;
			priority[i] = 0x00;
			control[i] = 0x00;
		}
		for (int p = 0; p < 4; p++)
			for (int i = 0; i < 40; i++)
				pal[p][i] = DEFAULT_PALETTE[i];
	}

	// setPixel: doubled-nibble write to the visible buffer (used by the blitter).
	void setPixel(int x, int y, int color) {
		int idx = width * y + x;
		visible[idx] = (byte)((color & 0xf) | (color << 4));
	}

	// plotOne: writes to visible/priority/control depending on drawMode.
	void plotOne(int x, int y, int drawMode, const int *drawCodes) {
		if (x < 0 || x >= width || y < 0 || y >= height)
			return;

		int idx = width * y + x;

		if (isVisual(drawMode)) {
			int pIdx = (drawCodes[0] / 40);          // forcePal is undefined -> derive
			int palIndex = (drawCodes[0] % 40);
			// TS would throw on pIdx > 3 (undefined palette); we have no
			// exceptions, so guard to avoid an OOB read. Valid SCI0 visual
			// color codes keep pIdx in 0..3.
			if (pIdx >= 0 && pIdx < 4 && palIndex >= 0 && palIndex < 40)
				visible[idx] = pal[pIdx][palIndex];
		}

		if (isPriority(drawMode))
			priority[idx] = (byte)drawCodes[1];

		if (isControl(drawMode))
			control[idx] = (byte)drawCodes[2];
	}

	// trackPlot: plotOne + per-pixel command tracking (omyac wrapper).
	void trackPlot(int x, int y, int drawMode, const int *drawCodes) {
		plotOne(x, y, drawMode, drawCodes);
		if (currentType == CMD_NONE)
			return;
		if (x < 0 || x >= width || y < 0 || y >= height)
			return;
		if (!isVisual(drawMode))
			return;
		int i = y * width + x;
		ref->refCmd[i] = (int16)currentCmdIdx;
		ref->cmdType[i] = (byte)currentType;
		ref->refPixel[i] = visible[i];
	}

	// isFillable: omyac override (render-omyac-upscaler.ts:179-194).
	bool isFillable(int x, int y, int drawMode) {
		int idx = x + y * width;
		if (isVisual(drawMode)) {
			byte val = visible[idx];
			int high = val >> 4;
			int low = val & 0xf;
			int dither = (x & 1) ^ (y & 1);
			return 0xf == (dither ? high : low);
		}
		return (isPriority(drawMode) && priority[idx] == 0x00) ||
		       (isControl(drawMode) && control[idx] == 0x00);
	}
};

// ─── create-line.ts: integer Bresenham (verbatim) ───────────────────────────
static void drawLine(Buffers &b, int x0, int y0, int x1, int y1, int drawMode, const int *drawCodes) {
	if (x0 == x1) {
		int yMin = (y0 < y1) ? y0 : y1;
		int yMax = (y0 > y1) ? y0 : y1;
		for (int y = yMin; y <= yMax; y++)
			b.trackPlot(x0, y, drawMode, drawCodes);
		return;
	}

	if (y0 == y1) {
		int xMin = (x0 < x1) ? x0 : x1;
		int xMax = (x0 > x1) ? x0 : x1;
		for (int x = xMin; x <= xMax; x++)
			b.trackPlot(x, y0, drawMode, drawCodes);
		return;
	}

	b.trackPlot(x0, y0, drawMode, drawCodes);
	b.trackPlot(x1, y1, drawMode, drawCodes);

	int dx = x1 - x0;
	int dy = y1 - y0;
	int adx = (dx < 0) ? -dx : dx;
	int ady = (dy < 0) ? -dy : dy;
	int sx = dx > 0 ? 1 : -1;
	int sy = dy > 0 ? 1 : -1;

	int eps = 0;

	if (adx > ady) {
		for (int x = x0, y = y0; sx < 0 ? x >= x1 : x <= x1; x += sx) {
			b.trackPlot(x, y, drawMode, drawCodes);
			eps += ady;
			if ((eps << 1) >= adx) {
				y += sy;
				eps -= adx;
			}
		}
	} else {
		for (int x = x0, y = y0; sy < 0 ? y >= y1 : y <= y1; y += sy) {
			b.trackPlot(x, y, drawMode, drawCodes);
			eps += adx;
			if ((eps << 1) >= ady) {
				x += sx;
				eps -= ady;
			}
		}
	}
}

// ─── create-flood-fill.ts: scanline flood fill (verbatim seed order) ─────────
static void floodFill(Buffers &b, int ix, int iy, int drawMode, const int *drawCodes) {
	const int width = b.width;
	const int height = b.height;

	Common::Array<byte> visited;
	visited.resize(width * height);
	for (uint i = 0; i < visited.size(); i++)
		visited[i] = 0;

	Common::Queue<uint32> stack;

	int start = iy * width + ix;
	stack.push((uint32)start);

	do {
		int i = (int)stack.pop();

		if (visited[i])
			continue;
		visited[i] = 0xff;

		int x = i % width;
		int y = i / width;

		if (!b.isFillable(x, y, drawMode))
			continue;
		b.trackPlot(x, y, drawMode, drawCodes);

		bool initialAbove =
			y - 1 >= 0 && !visited[i - width] && b.isFillable(x, y - 1, drawMode);
		bool initialBelow =
			y + 1 < height && !visited[i + width] && b.isFillable(x, y + 1, drawMode);

		if (initialAbove)
			stack.push((uint32)(i - width));
		if (initialBelow)
			stack.push((uint32)(i + width));

		// scan right and left
		for (int dir = 1; dir >= -1; dir -= 2) {
			bool visitedAbove = initialAbove;
			bool visitedBelow = initialBelow;

			for (int sx = x + dir;
			     (dir > 0 ? sx < width : sx >= 0) && b.isFillable(sx, y, drawMode);
			     sx += dir) {
				int idx = i + (sx - x);
				visited[idx] = 0xff;
				b.trackPlot(sx, y, drawMode, drawCodes);

				if (y - 1 >= 0 && !visited[idx - width]) {
					if (b.isFillable(sx, y - 1, drawMode)) {
						if (!visitedAbove)
							stack.push((uint32)(idx - width));
						visitedAbove = true;
					} else {
						visitedAbove = false;
					}
				}

				if (y + 1 < height && !visited[idx + width]) {
					if (b.isFillable(sx, y + 1, drawMode)) {
						if (!visitedBelow)
							stack.push((uint32)(idx + width));
						visitedBelow = true;
					} else {
						visitedBelow = false;
					}
				}
			}
		}
	} while (!stack.empty());
}

// ─── create-brush.ts (+ circles/noise): brush stamp ─────────────────────────
static void drawBrush(Buffers &b, int cx, int cy, int drawMode, const int *drawCodes,
                      int size, bool isRect, bool isSpray, int textureCode) {
	const int stageWidth = b.width;
	const int stageHeight = b.height;

	int baseWidth = isRect ? 2 : 1;
	int width = baseWidth + size * 2;
	int height = 1 + size * 2;

	int top = (0 > cy - size) ? 0 : (cy - size);
	int left = (0 > cx - size) ? 0 : (cx - size);
	int bottom = top + height;
	int right = left + width;
	if (right >= stageWidth)
		left = stageWidth - width;
	if (bottom >= stageHeight)
		top = stageHeight - height;

	// textureCode is (read8()>>1) so it can reach 127, but NOISE_OFFSETS has only
	// 120 entries. JS reads undefined here (-> NaN -> 0 after &0xff in the loop);
	// mirror that effective behavior safely rather than over-read (Hard Constraint 6).
	int noiseIdx = (textureCode >= 0 && textureCode < 120) ? NOISE_OFFSETS[textureCode] : 0;

	// SCI0 brushes use size 0..7; guard the CIRCLE_BITMAPS[8] lookup against a
	// malformed patternSize (Hard Constraint 6).
	const uint16 *circleSprite = (!isRect && size >= 0 && size < 8) ? CIRCLE_BITMAPS[size] : nullptr;

	for (int py = top; py < bottom; py += 1)
		for (int px = left; px < right; px += 1) {
			if (!isRect) {
				if (!circleSprite)
					continue;
				const uint16 *sprite = circleSprite;
				uint16 row = sprite[py - top];
				int shift = width - (px - left) - 1;
				if (((row >> shift) & 0x1) == 0)
					continue;
			}
			if (isSpray && !noiseAt(noiseIdx++))
				continue;
			b.trackPlot(px, py, drawMode, drawCodes);
		}
}

// ─── create-blitter.ts: embedded cel blit (verbatim) ────────────────────────
static void blitCel(Buffers &b, int x0, int y0, int drawMode, const EmbeddedCel &cel) {
	const int stageWidth = b.width;
	const int stageHeight = b.height;

	// CEL on Control/Priority is unhandled (TS warns); only visual blits.
	if (!isVisual(drawMode))
		return;

	const Common::Array<byte> &data = cel.pixels;
	for (int y = y0; y < y0 + cel.height; y++)
		for (int x = x0; x < x0 + cel.width; x++) {
			if (x >= stageWidth || y >= stageHeight)
				continue;

			// Verbatim port: TS indexes data[x + y*cel.width] with absolute
			// screen coords (a quirk preserved for fidelity).
			int di = x + y * cel.width;
			if (di < 0 || di >= (int)data.size())
				continue;
			byte color = data[di];
			if (color == cel.keyColor)
				continue;
			b.setPixel(x, y, color);
		}
}

// ─── pic-step.ts: command dispatch at scale [1,1] ───────────────────────────
static void picStep(Buffers &b, const DrawCommand &cmd) {
	switch (cmd.kind) {
	case kCmdSetPalette: {
		// Valid SCI0 pics use palIdx 0..3; guard the write so a malformed
		// resource cannot smash past pal[4][40] (Hard Constraint 6). TS would
		// index undefined here and silently no-op via .set().
		if (cmd.palIdx >= 4)
			break;
		for (uint i = 0; i < cmd.paletteColors.size() && i < 40; i++)
			b.pal[cmd.palIdx][i] = cmd.paletteColors[i];
		break;
	}
	case kCmdUpdatePalette: {
		for (uint e = 0; e + 2 < cmd.paletteEntries.size(); e += 3) {
			int pal = cmd.paletteEntries[e];
			int idx = cmd.paletteEntries[e + 1];
			int color = cmd.paletteEntries[e + 2];
			if (pal < 0 || pal >= 4 || idx < 0 || idx >= 40)
				continue; // ignore malformed entries (Hard Constraint 6)
			b.pal[pal][idx] = (byte)color;
		}
		break;
	}
	case kCmdFill: {
		// scale [1,1]: Math.round(x*1) == x.
		int x = cmd.points[0].x;
		int y = cmd.points[0].y;
		floodFill(b, x, y, cmd.drawMode, cmd.drawCodes);
		break;
	}
	case kCmdPline: {
		for (uint p = 0; p + 1 < cmd.points.size(); p++)
			drawLine(b, cmd.points[p].x, cmd.points[p].y,
			         cmd.points[p + 1].x, cmd.points[p + 1].y,
			         cmd.drawMode, cmd.drawCodes);
		break;
	}
	case kCmdBrush: {
		int cx = cmd.points[0].x;
		int cy = cmd.points[0].y;
		drawBrush(b, cx, cy, cmd.drawMode, cmd.drawCodes,
		          cmd.patternSize, cmd.patternRect, cmd.patternSpray, cmd.textureCode);
		break;
	}
	case kCmdCel: {
		int x = cmd.points[0].x;
		int y = cmd.points[0].y;
		blitCel(b, x, y, cmd.drawMode, cmd.cel);
		break;
	}
	default:
		break;
	}
}

// ─── render-omyac-upscaler.ts: nativePreRender (lines 146-248) ──────────────
NativeRef nativePreRender(const Common::Array<DrawCommand> &cmds) {
	const int N = OMYAC_NATIVE_W * OMYAC_NATIVE_H;

	NativeRef ref;
	ref.refPixel.resize(N);
	ref.refCmd.resize(N);
	ref.cmdType.resize(N);
	for (int i = 0; i < N; i++) {
		ref.refPixel[i] = 0xff;
		ref.refCmd[i] = -1;
		ref.cmdType[i] = CMD_NONE;
	}
	// segments grows to cover the highest PLINE command index.
	ref.segments.resize(cmds.size());

	Buffers b(OMYAC_NATIVE_W, OMYAC_NATIVE_H);
	b.ref = &ref;

	int step = 0;
	for (uint c = 0; c < cmds.size(); c++) {
		const DrawCommand &cmd = cmds[c];
		b.currentCmdIdx = step;

		if (cmd.kind == kCmdPline) {
			b.currentType = CMD_LINE;
			Common::Array<int> segs;
			for (uint p = 0; p + 1 < cmd.points.size(); p++) {
				segs.push_back(cmd.points[p].x);
				segs.push_back(cmd.points[p].y);
				segs.push_back(cmd.points[p + 1].x);
				segs.push_back(cmd.points[p + 1].y);
			}
			ref.segments[step] = segs;
		} else if (cmd.kind == kCmdFill) {
			b.currentType = CMD_FILL;
		} else if (cmd.kind == kCmdBrush || cmd.kind == kCmdCel) {
			b.currentType = CMD_FILL;
		} else {
			b.currentType = CMD_NONE;
		}

		picStep(b, cmd);
		step++;
	}

	// White-background promotion: any still-CMD_NONE cell becomes a CMD_FILL
	// anchor with refPixel = visible (refCmd stays -1). Lines 241-245.
	for (int i = 0; i < N; i++) {
		if (ref.cmdType[i] != CMD_NONE)
			continue;
		ref.cmdType[i] = CMD_FILL;
		ref.refPixel[i] = b.visible[i];
	}

	return ref;
}

} // namespace Roger
} // namespace Sci
