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

#include "sci/roger/gen/roger_pic_parser.h"
#include "sci/roger/gen/roger_byte_reader.h"

namespace Sci {
namespace Roger {

PicFormat picResourceFormat(const byte *data, uint32 size, bool isEga) {
	if (size < 2)
		return kPicSci0Ega; // too short; safe fallback
	// SCI1.1 VGA cel pics: first 2 bytes are LE word 0x0026 (38).
	const uint16 header = (uint16)(data[0] | (data[1] << 8));
	if (header == 0x0026)
		return kPicSci11VgaCel;
	// If the engine is in VGA (non-EGA) mode and the resource is vector-format
	// (first byte is an opcode >= 0xF0 or data byte < 0xF0), it's SCI1.0 VGA vector.
	if (!isEga)
		return kPicSci1VgaVector;
	return kPicSci0Ega;
}

// op-codes.ts (== ScummVM PIC_OP_*)
enum {
	OP_SET_VISUAL    = 0xf0,
	OP_CLEAR_VISUAL  = 0xf1,
	OP_SET_PRIORITY  = 0xf2,
	OP_CLEAR_PRIORITY = 0xf3,
	OP_SHORT_BRUSHES = 0xf4,
	OP_MEDIUM_LINES  = 0xf5,
	OP_LONG_LINES    = 0xf6,
	OP_SHORT_LINES   = 0xf7,
	OP_FILLS         = 0xf8,
	OP_SET_PATTERN   = 0xf9,
	OP_LONG_BRUSHES  = 0xfa,
	OP_SET_CONTROL   = 0xfb,
	OP_CLEAR_CONTROL = 0xfc,
	OP_MEDIUM_BRUSHES = 0xfd,
	OP_XOP           = 0xfe,
	OP_DONE          = 0xff
};

// points.ts: getPoint24  --  reads 24 bits as absolute (x,y).
// bits 0-3:   high nibble of x
// bits 4-7:   high nibble of y
// bits 8-15:  low byte of x
// bits 16-23: low byte of y
static Point getPoint24(ByteReader &r) {
	uint32 code = r.read24();
	Point p;
	p.x = (int16)(((code & 0xf00000) >> 12) | ((code & 0xff00) >> 8));
	p.y = (int16)(((code & 0x0f0000) >> 8)  | (code & 0x00ff));
	return p;
}

// points.ts: getPoint16  --  reads 16-bit delta (y first, then x).
static Point getPoint16(ByteReader &r, Point ref) {
	int y = r.read8();
	int absY = y & 0x7f;
	int dy = (y & 0x80) ? -absY : absY;
	int x = r.read8();
	int dx = (x > 0x7f) ? x - 256 : x;
	Point p;
	p.x = (int16)(ref.x + dx);
	p.y = (int16)(ref.y + dy);
	return p;
}

// points.ts: getPoint8  --  reads 8-bit delta (4 bits x, 4 bits y with sign flags).
static Point getPoint8(ByteReader &r, Point ref) {
	int code = r.read8();
	bool xSign = ((code >> 4) & 8) != 0;
	bool ySign = (code & 8) != 0;
	int dx = (code >> 4) & 7;
	int dy = code & 7;
	Point p;
	p.x = (int16)(ref.x + (xSign ? -dx : dx));
	p.y = (int16)(ref.y + (ySign ? -dy : dy));
	return p;
}

// Returns true if next byte is a data byte (< 0xf0), false at EOF or opcode.
static bool more(ByteReader &r) {
	int p = r.peek8();
	return p >= 0 && p < 0xf0;
}

// Build a DrawCommand with the current draw state filled in.
static DrawCommand makeCmd(CommandKind kind, int drawMode, const int drawCodes[3]) {
	DrawCommand c;
	c.kind = kind;
	c.drawMode = drawMode;
	c.drawCodes[0] = drawCodes[0];
	c.drawCodes[1] = drawCodes[1];
	c.drawCodes[2] = drawCodes[2];
	return c;
}

Common::Array<DrawCommand> parsePic(const byte *data, uint32 size) {
	Common::Array<DrawCommand> out;
	ByteReader r(data, size);

	// pic-state.ts: createPicState initial values.
	int drawMode = kDrawVisual | kDrawPriority; // DrawMode.Visual | DrawMode.Priority = 3
	int drawCodes[3] = { 0, 0, 0 };            // [visual, priority, control]
	int pSize = 0;
	bool pRect = false, pSpray = false;

	while (!r.eof()) {
		int op = r.read8();
		if (op == OP_DONE)
			break;

		switch (op) {
		// --- Color / mode setters ---
		case OP_SET_VISUAL:
			drawCodes[0] = r.read8();
			drawMode |= kDrawVisual;
			break;
		case OP_CLEAR_VISUAL:
			drawMode &= ~kDrawVisual;
			break;
		case OP_SET_PRIORITY:
			drawCodes[1] = r.read8();
			drawMode |= kDrawPriority;
			break;
		case OP_CLEAR_PRIORITY:
			drawMode &= ~kDrawPriority;
			break;
		case OP_SET_CONTROL:
			drawCodes[2] = r.read8();
			drawMode |= kDrawControl;
			break;
		case OP_CLEAR_CONTROL:
			drawMode &= ~kDrawControl;
			break;

		// --- Lines ---
		// handlers.ts: ShortLines, MediumLines, LongLines
		// First point is always getPoint24; subsequent are delta per variant.
		case OP_SHORT_LINES:
		case OP_MEDIUM_LINES:
		case OP_LONG_LINES: {
			DrawCommand c = makeCmd(kCmdPline, drawMode, drawCodes);
			Point prev = getPoint24(r);
			c.points.push_back(prev);
			while (more(r)) {
				Point next;
				if (op == OP_LONG_LINES)
					next = getPoint24(r);
				else if (op == OP_MEDIUM_LINES)
					next = getPoint16(r, prev);
				else
					next = getPoint8(r, prev);
				c.points.push_back(next);
				prev = next;
			}
			out.push_back(c);
			break;
		}

		// --- Fills ---
		// handlers.ts: Fills  --  while peek < 0xf0, read one getPoint24 per fill.
		case OP_FILLS: {
			while (more(r)) {
				DrawCommand c = makeCmd(kCmdFill, drawMode, drawCodes);
				c.points.push_back(getPoint24(r));
				out.push_back(c);
			}
			break;
		}

		// --- Set Pattern ---
		// handlers.ts: SetPattern  --  reads one byte, extracts size/rect/spray flags.
		case OP_SET_PATTERN: {
			int code = r.read8();
			pSize  = code & 0x07;
			pRect  = (code & 0x10) != 0;
			pSpray = (code & 0x20) != 0;
			break;
		}

		// --- Brushes ---
		// handlers.ts structure (faithfully ported):
		//   ShortBrushes/MediumBrushes: unconditional first point (getPoint24),
		//     then while(peek < 0xf0) for delta points.
		//   LongBrushes: while(peek < 0xf0) from the start (no unconditional first).
		// Texture byte (read8()>>1) is read BEFORE the point, only when pSpray.
		case OP_SHORT_BRUSHES: {
			// First brush: texture (if any) then getPoint24  --  unconditional.
			{
				int texture = pSpray ? (r.read8() >> 1) : 0;
				Point pos = getPoint24(r);
				DrawCommand c = makeCmd(kCmdBrush, drawMode, drawCodes);
				c.patternSize = pSize; c.patternRect = pRect; c.patternSpray = pSpray;
				c.textureCode = texture;
				c.points.push_back(pos);
				out.push_back(c);
				// prev for delta decode
				Point prev = pos;
				// Subsequent brushes while data bytes remain.
				while (more(r)) {
					texture = pSpray ? (r.read8() >> 1) : 0;
					Point next = getPoint8(r, prev);
					DrawCommand c2 = makeCmd(kCmdBrush, drawMode, drawCodes);
					c2.patternSize = pSize; c2.patternRect = pRect; c2.patternSpray = pSpray;
					c2.textureCode = texture;
					c2.points.push_back(next);
					out.push_back(c2);
					prev = next;
				}
			}
			break;
		}
		case OP_MEDIUM_BRUSHES: {
			// First brush: texture (if any) then getPoint24  --  unconditional.
			{
				int texture = pSpray ? (r.read8() >> 1) : 0;
				Point pos = getPoint24(r);
				DrawCommand c = makeCmd(kCmdBrush, drawMode, drawCodes);
				c.patternSize = pSize; c.patternRect = pRect; c.patternSpray = pSpray;
				c.textureCode = texture;
				c.points.push_back(pos);
				out.push_back(c);
				Point prev = pos;
				while (more(r)) {
					texture = pSpray ? (r.read8() >> 1) : 0;
					Point next = getPoint16(r, prev);
					DrawCommand c2 = makeCmd(kCmdBrush, drawMode, drawCodes);
					c2.patternSize = pSize; c2.patternRect = pRect; c2.patternSpray = pSpray;
					c2.textureCode = texture;
					c2.points.push_back(next);
					out.push_back(c2);
					prev = next;
				}
			}
			break;
		}
		case OP_LONG_BRUSHES: {
			// LongBrushes: starts with while(peek < 0xf0)  --  no unconditional first.
			while (more(r)) {
				int texture = pSpray ? (r.read8() >> 1) : 0;
				Point pos = getPoint24(r);
				DrawCommand c = makeCmd(kCmdBrush, drawMode, drawCodes);
				c.patternSize = pSize; c.patternRect = pRect; c.patternSpray = pSpray;
				c.textureCode = texture;
				c.points.push_back(pos);
				out.push_back(c);
			}
			break;
		}

		// --- Extended opcodes (0xfe) ---
		case OP_XOP: {
			int opx = r.read8();
			switch (opx) {
			case 0x00: {
				// ExtendedOpCode.UpdatePalette: while peek < 0xf0 read entry+color triplets.
				DrawCommand c;
				c.kind = kCmdUpdatePalette;
				c.drawMode = 0;
				while (more(r)) {
					int entry = r.read8();
					int pal   = entry / 40;
					int idx   = entry % 40;
					int color = r.read8();
					c.paletteEntries.push_back(pal);
					c.paletteEntries.push_back(idx);
					c.paletteEntries.push_back(color);
				}
				out.push_back(c);
				break;
			}
			case 0x01: {
				// ExtendedOpCode.SetPalette: read pal byte, then 40 color bytes.
				DrawCommand c;
				c.kind = kCmdSetPalette;
				c.drawMode = 0;
				c.palIdx = r.read8();
				c.paletteColors.resize(40);
				for (int i = 0; i < 40; i++)
					c.paletteColors[i] = (byte)r.read8();
				out.push_back(c);
				break;
			}
			case 0x02:
				// ExtendedOpCode.x02: skip 1 byte + 40 bytes (palette-like chunk).
				r.skip(1 + 40);
				break;
			case 0x03:
				// ExtendedOpCode.x03: skip 1 byte.
				r.skip(1);
				break;
			case 0x04:
				// ExtendedOpCode.x04: noop.
				break;
			case 0x05:
				// ExtendedOpCode.x05: skip 1 byte.
				r.skip(1);
				break;
			case 0x06:
				// ExtendedOpCode.x06: noop.
				break;
			case 0x07: {
				// ExtendedOpCode.x07: embedded cel.
				// handlers.ts:215-227 + parse-cel.ts
				DrawCommand c;
				c.kind = kCmdCel;
				c.drawMode = drawMode;
				c.drawCodes[0] = drawCodes[0];
				c.drawCodes[1] = drawCodes[1];
				c.drawCodes[2] = drawCodes[2];
				c.points.push_back(getPoint24(r));

				// Little-endian 16-bit cel size.
				int celSize = r.read8() | (r.read8() << 8);

				// parse-cel.ts: width u16le, height u16le, dx i8, dy i8, keyColor u8.
				int w = r.read8() | (r.read8() << 8);
				int h = r.read8() | (r.read8() << 8);
				c.cel.width    = (int16)w;
				c.cel.height   = (int16)h;
				c.cel.dx       = (int8)r.read8();
				c.cel.dy       = (int8)r.read8();
				c.cel.keyColor = (uint8)r.read8();

				// RLE from byte 7 onward: data=u8; color=data&0xf; repeat=data>>4.
				int total = w * h;
				c.cel.pixels.resize(total);
				int i = 0;
				int consumed = 7; // bytes consumed from the cel buffer so far (header)
				while (i < total && consumed < celSize) {
					int d = r.read8();
					consumed++;
					int color  = d & 0xf;
					int repeat = d >> 4;
					for (int k = 0; k < repeat && i < total; k++)
						c.cel.pixels[i++] = (byte)color;
				}
				out.push_back(c);
				break;
			}
			case 0x08:
				// ExtendedOpCode.x08: skip 14 bytes.
				r.skip(14);
				break;
			default:
				// Unknown xop: stop cleanly, never throw (Hard Constraint 6).
				return out;
			}
			break;
		}

		default:
			// Unknown opcode: stop cleanly, never throw (Hard Constraint 6).
			return out;
		}
	}
	return out;
}

} // namespace Roger
} // namespace Sci
