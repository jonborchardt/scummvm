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

#ifndef SCI_ROGER_SLICE_SET_H
#define SCI_ROGER_SLICE_SET_H

namespace Sci {
namespace Roger {

// SCI0 EGA priority-band helpers.
//
// (This file formerly also held a SliceSet class that loaded foreground "slice"
// PNGs + a manifest for occlusion. That approach was replaced by per-pixel
// priority occlusion against the real EGA-color priority map and has been removed;
// only the band-decode helpers remain. The filename is kept to avoid regenerating
// the MSVC solution / module lists.)

// Nearest SCI0 EGA priority band (0..15) for an RGB triple. Used to decode an
// EGA-color-encoded priority map (each pixel's color encodes its priority band).
int bandForRGB(int r, int g, int b);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_SLICE_SET_H
