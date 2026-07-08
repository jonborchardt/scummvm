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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCI_ROGER_ROGER_CURSOR_H
#define SCI_ROGER_ROGER_CURSOR_H

#include "common/rect.h"
#include "graphics/surface.h"

namespace Sci {
namespace Roger {

// Decode a raw SCI0 cursor resource (exactly 68 bytes) into a new RGBA32 surface
// scaled 5x nearest-neighbour (80x80). Returns nullptr on size mismatch or null data.
// `outHotspot` receives the overlay-space hotspot (the active point of the cursor surface).
// Caller owns and must call free() + delete on the returned surface.
Graphics::Surface *decodeSci0Cursor(const byte *data, int size, Common::Point &outHotspot);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_CURSOR_H
