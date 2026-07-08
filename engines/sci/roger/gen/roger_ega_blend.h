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

#ifndef SCI_ROGER_EGA_BLEND_H
#define SCI_ROGER_EGA_BLEND_H

#include "common/array.h"
#include "common/types.h"
#include "graphics/surface.h"

namespace Sci {
namespace Roger {

// BLEND_TABLE[byte] packed 0xAABBGGRR (matches sci.js ImageData ABGR layout).
// 256-entry precomputed okLab-mixed EGA color table.
// Decode: r = v & 0xff; g = (v>>8)&0xff; b = (v>>16)&0xff; alpha = 255.
extern const uint32 BLEND_TABLE[256];

// Create a new RGBA32 Graphics::Surface from a doubled-nibble pixel buffer.
// Each byte in `pixels` is used as an index into BLEND_TABLE.
// The surface is in the same format as loadSurfaceRGBA() returns.
// Caller owns the result: call ->free() then delete. Returns nullptr on failure.
Graphics::Surface *blendToSurface(const Common::Array<byte> &pixels, int w, int h);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_EGA_BLEND_H
