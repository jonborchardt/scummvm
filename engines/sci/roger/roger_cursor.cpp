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

#include "sci/roger/roger_cursor.h"
#include "sci/roger/gen/roger_scale.h"
#include "common/endian.h"
#include "graphics/pixelformat.h"

namespace Sci {
namespace Roger {

Graphics::Surface *decodeSci0Cursor(const byte *data, int size, Common::Point &outHotspot) {
	// SCI0 cursor: 4 header bytes + 32 maskA + 32 maskB = 68 bytes total.
	if (!data || size != 68)
		return nullptr;

	const int kHW = 16;    // SCI_CURSOR_SCI0_HEIGHTWIDTH
	const int kScale = 5;

	// Byte 3 nonzero -> hotspot centered (half-width); zero -> top-left.
	const bool centered = (data[3] != 0);

	// Decode 16x16 bitmask into an IndexImage.
	// color = 2*maskA_bit + maskB_bit:
	//   0 = black, 1 = white, 2 = transparent, 3 = white (SCI0 both-masks-set)
	IndexImage img;
	img.w = kHW;
	img.h = kHW;
	img.pixels.resize(kHW * kHW);
	for (int y = 0; y < kHW; y++) {
		const uint16 maskA = READ_LE_UINT16(data + 4 + y * 2);
		const uint16 maskB = READ_LE_UINT16(data + 4 + 32 + y * 2);
		for (int x = 0; x < kHW; x++) {
			const byte colorIdx = (byte)((((maskA << x) & 0x8000) | (((maskB << x) >> 1) & 0x4000)) >> 14);
			img.pixels[y * kHW + x] = colorIdx;
		}
	}

	// Scale 5x nearest-neighbour -> 80x80.
	const IndexImage scaled = scaleNearest(img, kScale);

	// Convert IndexImage to RGBA32.
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *surf = new Graphics::Surface();
	surf->create(scaled.w, scaled.h, rgba);
	const int N = scaled.w * scaled.h;
	uint32 *dst = (uint32 *)surf->getPixels();
	for (int i = 0; i < N; i++) {
		switch (scaled.pixels[i]) {
		case 0:   dst[i] = rgba.ARGBToColor(255, 0,   0,   0);   break; // black
		case 2:   dst[i] = rgba.ARGBToColor(0,   0,   0,   0);   break; // transparent
		default:  dst[i] = rgba.ARGBToColor(255, 255, 255, 255);  break; // white (1 or 3)
		}
	}

	// Hotspot in overlay-pixel space.
	const int hs = centered ? (kHW / 2 * kScale) : 0;
	outHotspot = Common::Point(hs, hs);

	return surf;
}

} // namespace Roger
} // namespace Sci
