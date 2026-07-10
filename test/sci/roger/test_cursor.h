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

#include <cxxtest/TestSuite.h>
#include "sci/roger/overlay/roger_cursor.h"
#include "graphics/surface.h"

// SCI0 cursor resource format (68 bytes):
//   bytes 0-3:  header. byte[3] nonzero = centered hotspot.
//   bytes 4-35: maskA  (16 rows x 2 bytes LE uint16 = 16-bit mask per row)
//   bytes 36-67: maskB (same)
// color = 2*maskA_bit + maskB_bit:
//   0 = black (maskA=0, maskB=0)
//   1 = white (maskA=0, maskB=1)
//   2 = transparent (maskA=1, maskB=0)
//   3 = white/SCI0 (maskA=1, maskB=1)

class TestCursor : public CxxTest::TestSuite {
public:
	static void buildAllBlack(byte (&data)[68]) {
		memset(data, 0, 68); // all maskA=0, maskB=0 -> color=0 = black
	}

	static void buildAllTransparent(byte (&data)[68]) {
		memset(data, 0, 68);
		// maskA=0xFFFF per row, maskB=0 -> color=2 = transparent
		for (int y = 0; y < 16; y++) {
			data[4 + y * 2]     = 0xFF;
			data[4 + y * 2 + 1] = 0xFF;
		}
	}

	void test_decode_all_black_produces_96x96_black_surface() {
		byte data[68];
		buildAllBlack(data);
		Common::Point hotspot;
		Graphics::Surface *surf = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT(surf != nullptr);
		TS_ASSERT_EQUALS(surf->w, 96); // 16 * 6 — same enhanced scale as view cels
		TS_ASSERT_EQUALS(surf->h, 96);
		uint8 a, r, g, b;
		surf->format.colorToARGB(surf->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(a, 255);
		TS_ASSERT_EQUALS(r, 0);
		TS_ASSERT_EQUALS(g, 0);
		TS_ASSERT_EQUALS(b, 0);
		surf->free(); delete surf;
	}

	void test_decode_all_transparent_produces_alpha_zero() {
		byte data[68];
		buildAllTransparent(data);
		Common::Point hotspot;
		Graphics::Surface *surf = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT(surf != nullptr);
		uint8 a, r, g, b;
		surf->format.colorToARGB(surf->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(a, 0);
		surf->free(); delete surf;
	}

	void test_hotspot_topleft_when_byte3_zero() {
		byte data[68];
		buildAllBlack(data);
		data[3] = 0; // not centered -> top-left hotspot
		Common::Point hotspot;
		Graphics::Surface *s = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT_EQUALS(hotspot.x, 0);
		TS_ASSERT_EQUALS(hotspot.y, 0);
		s->free(); delete s;
	}

	void test_hotspot_centered_when_byte3_nonzero() {
		byte data[68];
		buildAllBlack(data);
		data[3] = 1; // centered -> hotspot = (8 * 6, 8 * 6) = (48, 48)
		Common::Point hotspot;
		Graphics::Surface *s = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT_EQUALS(hotspot.x, 48);
		TS_ASSERT_EQUALS(hotspot.y, 48);
		s->free(); delete s;
	}

	void test_returns_null_for_wrong_size() {
		byte data[10];
		memset(data, 0, sizeof(data));
		Common::Point hotspot;
		Graphics::Surface *s = Sci::Roger::decodeSci0Cursor(data, 10, hotspot);
		TS_ASSERT(s == nullptr);
	}

	void test_single_white_pixel_at_col0_row0() {
		// maskA row0=0, maskB row0=0x8000 -> bit 15 of maskB at x=0 is 1 -> color=1 (white)
		// READ_LE_UINT16(data+36) = data[36] | (data[37]<<8). To get 0x8000: data[37]=0x80.
		byte data[68];
		memset(data, 0, 68);
		data[37] = 0x80; // maskB row0 high byte -> maskB=0x8000 -> pixel (0,0) = white
		Common::Point hotspot;
		Graphics::Surface *s = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT(s != nullptr);
		uint8 a, r, g, b;
		// The top-left block (scaled from original pixel 0,0) should be white.
		s->format.colorToARGB(s->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(a, 255);
		TS_ASSERT_EQUALS(r, 255);
		TS_ASSERT_EQUALS(g, 255);
		TS_ASSERT_EQUALS(b, 255);
		s->free(); delete s;
	}

	void test_diagonal_is_edge_enhanced_not_plain_nearest() {
		// A white diagonal on black. The enhanced (EPX scale6x) upscale smooths
		// the stair-steps, so at least one output pixel must differ from what
		// plain nearest-neighbour replication would produce. Plain replication
		// (the old 5x path) would match everywhere.
		byte data[68];
		memset(data, 0, 68); // all black
		// White (color 1: maskA=0, maskB bit set) at (x, y) where x == y.
		for (int y = 0; y < 16; y++) {
			const uint16 maskB = (uint16)(0x8000 >> y);
			data[36 + y * 2]     = (byte)(maskB & 0xFF);
			data[36 + y * 2 + 1] = (byte)(maskB >> 8);
		}
		Common::Point hotspot;
		Graphics::Surface *s = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT(s != nullptr);
		const int scale = s->w / 16;
		bool anyDiffers = false;
		for (int y = 0; y < s->h && !anyDiffers; y++) {
			for (int x = 0; x < s->w; x++) {
				const bool srcWhite = (x / scale) == (y / scale); // nearest expectation
				uint8 a, r, g, b;
				s->format.colorToARGB(s->getPixel(x, y), a, r, g, b);
				const bool outWhite = (r == 255);
				if (outWhite != srcWhite) {
					anyDiffers = true;
					break;
				}
			}
		}
		TS_ASSERT(anyDiffers);
		s->free(); delete s;
	}

	void test_both_masks_set_white_matches_plain_white() {
		// Color 3 (both masks set) is white in SCI0 — it must scale identically
		// to color 1 white (no seam artifacts from the enhancer treating the two
		// white encodings as different colors). All-3 image => uniformly white.
		byte data[68];
		memset(data, 0xFF, 68); // maskA and maskB all set -> color 3 everywhere
		data[3] = 0;            // top-left hotspot (byte 3 is header, keep it 0)
		Common::Point hotspot;
		Graphics::Surface *s = Sci::Roger::decodeSci0Cursor(data, 68, hotspot);
		TS_ASSERT(s != nullptr);
		for (int y = 0; y < s->h; y++) {
			for (int x = 0; x < s->w; x++) {
				uint8 a, r, g, b;
				s->format.colorToARGB(s->getPixel(x, y), a, r, g, b);
				TS_ASSERT_EQUALS(a, 255);
				TS_ASSERT_EQUALS(r, 255);
			}
		}
		s->free(); delete s;
	}
};
