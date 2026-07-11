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

#ifndef SCI_ROGER_ROGER_PIC_PARSER_H
#define SCI_ROGER_ROGER_PIC_PARSER_H
#include "common/array.h"
#include "sci/roger/gen/roger_draw_command.h"
namespace Sci {
namespace Roger {
// Format of a SCI picture resource, detected from the first 2 bytes.
// kPicSci0Ega    --  SCI0 EGA vector data (Roger omyac pipeline).
// kPicSci11VgaCel  --  SCI1.1 VGA: LE header word 0x0026, cel background + vector overlay.
// kPicSci1VgaVector  --  SCI1.0 VGA vector (same opcodes as EGA but 256-color palette);
//                     currently unsupported (returns nullptr from generate*).
enum PicFormat { kPicSci0Ega = 0, kPicSci11VgaCel = 1, kPicSci1VgaVector = 2 };

// Detect the format from the raw resource bytes. Never reads past `size` bytes.
// Returns kPicSci0Ega for unknown/short resources (safe fallback).
// Note: kPicSci1VgaVector cannot be detected from bytes alone  --  caller must pass
// isEga=false to distinguish it from kPicSci0Ega when the engine is in VGA mode.
PicFormat picResourceFormat(const byte *data, uint32 size, bool isEga = true);

// Port of sci.js parse-pic.ts. Returns commands; stops cleanly on unknown
// opcode or EOF (never throws, never crashes  --  Hard Constraint 6).
Common::Array<DrawCommand> parsePic(const byte *data, uint32 size);
} // namespace Roger
} // namespace Sci
#endif
