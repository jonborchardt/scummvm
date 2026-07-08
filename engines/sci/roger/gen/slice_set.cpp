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

#include "sci/roger/gen/slice_set.h"
#include "common/scummsys.h"

namespace Sci {
namespace Roger {

// SCI0 EGA display colors per priority band (RGB), index = band (0..15).
// These are the standard CGA/EGA palette entries used by SCI0 for priority rendering.
static const uint8 kEgaBandRGB[16][3] = {
	{0x00, 0x00, 0x00}, // 0  black
	{0x00, 0x00, 0xaa}, // 1  dark blue
	{0x00, 0xaa, 0x00}, // 2  dark green
	{0x00, 0xaa, 0xaa}, // 3  dark cyan
	{0xaa, 0x00, 0x00}, // 4  dark red
	{0xaa, 0x00, 0xaa}, // 5  dark magenta
	{0xaa, 0x55, 0x00}, // 6  brown
	{0xaa, 0xaa, 0xaa}, // 7  light gray
	{0x55, 0x55, 0x55}, // 8  dark gray
	{0x55, 0x55, 0xff}, // 9  bright blue
	{0x55, 0xff, 0x55}, // 10 bright green
	{0x55, 0xff, 0xff}, // 11 bright cyan
	{0xff, 0x55, 0x55}, // 12 bright red
	{0xff, 0x55, 0xff}, // 13 bright magenta
	{0xff, 0xff, 0x55}, // 14 yellow
	{0xff, 0xff, 0xff}  // 15 white
};

int bandForRGB(int r, int g, int b) {
	int best = 0;
	long bestDist = 0x7fffffff;
	for (int i = 0; i < 16; i++) {
		int dr = r - kEgaBandRGB[i][0];
		int dg = g - kEgaBandRGB[i][1];
		int db = b - kEgaBandRGB[i][2];
		long d = (long)dr * dr + (long)dg * dg + (long)db * db;
		if (d < bestDist) {
			bestDist = d;
			best = i;
		}
	}
	return best;
}

} // namespace Roger
} // namespace Sci
