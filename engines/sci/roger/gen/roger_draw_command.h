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

#ifndef SCI_ROGER_ROGER_DRAW_COMMAND_H
#define SCI_ROGER_ROGER_DRAW_COMMAND_H
#include "common/array.h"
#include "common/scummsys.h"
namespace Sci {
namespace Roger {

// draw-mode.ts: Visual=1, Priority=2, Control=4 (bitmask).
enum DrawMode { kDrawVisual = 1, kDrawPriority = 2, kDrawControl = 4 };
inline bool isVisual(int m)   { return (m & kDrawVisual) == kDrawVisual; }
inline bool isPriority(int m) { return (m & kDrawPriority) == kDrawPriority; }
inline bool isControl(int m)  { return (m & kDrawControl) == kDrawControl; }

enum CommandKind {
	kCmdSetPalette, kCmdUpdatePalette, kCmdBrush, kCmdFill, kCmdPline, kCmdCel
};

struct Point { int16 x; int16 y; };

// Decoded embedded cel (parse-cel.ts): indexed pixels + offsets + key color.
struct EmbeddedCel {
	int16 width = 0, height = 0;
	int8 dx = 0, dy = 0;
	uint8 keyColor = 0;
	Common::Array<byte> pixels; // width*height palette indices
};

struct DrawCommand {
	CommandKind kind;
	int drawMode = 0;
	int drawCodes[3] = { 0, 0, 0 };   // [visual, priority, control]
	// BRUSH pattern: size (0..7), isRect, isSpray, textureCode.
	int patternSize = 0; bool patternRect = false; bool patternSpray = false; int textureCode = 0;
	Common::Array<Point> points;       // PLINE points, or [pos] for FILL/BRUSH/CEL
	// SET_PALETTE: palIdx + 40 colors. UPDATE_PALETTE: triplets in paletteEntries.
	int palIdx = 0;
	Common::Array<byte> paletteColors;            // 40 entries for SET_PALETTE
	Common::Array<int> paletteEntries;            // flat [pal,idx,color, ...] for UPDATE_PALETTE
	EmbeddedCel cel;                              // valid when kind==kCmdCel
};

} // namespace Roger
} // namespace Sci
#endif
