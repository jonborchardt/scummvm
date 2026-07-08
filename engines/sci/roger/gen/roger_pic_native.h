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

#ifndef SCI_ROGER_ROGER_PIC_NATIVE_H
#define SCI_ROGER_ROGER_PIC_NATIVE_H
#include "common/array.h"
#include "common/scummsys.h"
#include "sci/roger/gen/roger_draw_command.h"
namespace Sci {
namespace Roger {

// Native pre-render dimensions / scale. Faithful port of
// render-omyac-upscaler.ts constants.
static const int OMYAC_NATIVE_W = 320, OMYAC_NATIVE_H = 190, OMYAC_SCALE = 6;
static const int OMYAC_HYBRID_W = 1920, OMYAC_HYBRID_H = 1140;

// Per-pixel command-type tracking. 0=untouched, 1=line, 2=fill.
enum { CMD_NONE = 0, CMD_LINE = 1, CMD_FILL = 2 };

// Output of nativePreRender â€” per-pixel ownership tracking over a 320x190
// native SCI0 render (port of render-omyac-upscaler.ts NativeRef).
struct NativeRef {
	Common::Array<byte> refPixel;  // 320*190 doubled-nibble bytes (init 0xff)
	Common::Array<int16> refCmd;   // 320*190 owning command index (init -1)
	Common::Array<byte> cmdType;   // 320*190 CMD_NONE/LINE/FILL
	Common::Array<byte> priority;  // 320*190 SCI priority band per pixel (0..15), init 0
	// segments[cmdIdx] = flat [x0,y0,x1,y1,...] for PLINE commands; empty otherwise.
	Common::Array<Common::Array<int> > segments;
};

// Replay a parsed pic command list into a native 320x190 render, tracking
// which command owns each pixel and whether it's a line or fill pixel.
// Verbatim port of render-omyac-upscaler.ts nativePreRender (lines 146-248).
//
// trackLayer selects which SCI screen drives the omyac geometry (refPixel/cmdType/
// refCmd/segments): kDrawVisual (default) records the visual colour exactly as
// before; kDrawPriority records the priority screen, with each priority code 0..15
// encoded as its solid EGA colour byte (doubled nibble 0xNN) so the SAME omyac
// pipeline upscales the priority screen into a colour picture just like the visual.
NativeRef nativePreRender(const Common::Array<DrawCommand> &cmds, int trackLayer = kDrawVisual);

} // namespace Roger
} // namespace Sci
#endif
