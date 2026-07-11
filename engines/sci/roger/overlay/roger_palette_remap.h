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

#ifndef SCI_ROGER_ROGER_PALETTE_REMAP_H
#define SCI_ROGER_ROGER_PALETTE_REMAP_H

#include "common/scummsys.h"
#include "common/rect.h"

namespace Graphics { struct Surface; }

namespace Sci {
namespace Roger {

// Build a 256-entry gain-applied blend table from the room-load palette snapshot
// (snapEga) to the live palette (liveEga), each 16 EGA colors as RGB triples (48 bytes).
// Identity (== BLEND_TABLE) when snapEga == liveEga.
void buildLivePaletteTable(const byte snapEga[48], const byte liveEga[48], uint32 outTable[256]);

// Per-EGA-index change flags; returns the count of changed indices (0 == no change).
int paletteDiffMask(const byte snapEga[48], const byte liveEga[48], bool changed[16]);

// Re-color (in place) every plate pixel whose doubled-nibble index references a changed
// EGA color, using `table`. Accumulates the affected bounding box into outDirty (plate-space).
void reblendChangedPixels(const byte *indexMap, int w, int h, const uint32 table[256],
                          const bool changed[16], Graphics::Surface &plate, Common::Rect &outDirty);

} // namespace Roger
} // namespace Sci
#endif // SCI_ROGER_ROGER_PALETTE_REMAP_H
