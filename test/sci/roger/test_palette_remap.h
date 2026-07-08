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

#include <cxxtest/TestSuite.h>
#include "sci/roger/overlay/roger_palette_remap.h"
#include "sci/roger/gen/roger_ega_blend.h"
#include "graphics/surface.h"
#include "common/rect.h"
using namespace Sci::Roger;

class RogerPaletteRemapTestSuite : public CxxTest::TestSuite {
	// Decode the 16 EGA base colors baked into BLEND_TABLE solids (idx i*0x11).
	static void canonicalEga(byte ega[48]) {
		for (int i = 0; i < 16; i++) {
			uint32 v = BLEND_TABLE[i * 0x11];
			ega[i * 3 + 0] = v & 0xff;
			ega[i * 3 + 1] = (v >> 8) & 0xff;
			ega[i * 3 + 2] = (v >> 16) & 0xff;
		}
	}
public:
	void test_identity_when_unchanged() {
		byte snap[48]; canonicalEga(snap);
		byte live[48]; canonicalEga(live);
		uint32 table[256];
		buildLivePaletteTable(snap, live, table);
		for (int i = 0; i < 256; i++)
			TS_ASSERT_EQUALS(table[i], BLEND_TABLE[i]); // identity: snap==live
	}
	void test_uniform_half_intensity_halves_rgb() {
		byte snap[48]; canonicalEga(snap);
		byte live[48]; canonicalEga(live);
		for (int i = 0; i < 48; i++) live[i] = snap[i] / 2; // uniform 50% fade
		uint32 table[256];
		buildLivePaletteTable(snap, live, table);
		// White solid (0xff) baked is 0xffffffff -> ~0x808080 after half scale.
		uint32 w = table[0xff];
		TS_ASSERT_DELTA((int)(w & 0xff), 127, 2);
		TS_ASSERT_DELTA((int)((w >> 8) & 0xff), 127, 2);
		TS_ASSERT_DELTA((int)((w >> 16) & 0xff), 127, 2);
	}
	void test_diff_mask_counts() {
		byte snap[48]; canonicalEga(snap);
		byte live[48]; canonicalEga(live);
		bool changed[16];
		TS_ASSERT_EQUALS(paletteDiffMask(snap, live, changed), 0); // none
		live[3 * 3 + 1] = (byte)(live[3 * 3 + 1] ^ 0xff);          // change index 3
		TS_ASSERT_EQUALS(paletteDiffMask(snap, live, changed), 1);
		TS_ASSERT(changed[3]);
		TS_ASSERT(!changed[2]);
	}
	void test_reblend_only_changed_index() {
		// 2x1 plate: pixel0 index 0x22 (solid green), pixel1 index 0x44 (solid blue-ish).
		Common::Array<byte> idx; idx.push_back(0x22); idx.push_back(0x44);
		Graphics::Surface *plate = blendToSurface(idx, 2, 1);
		byte snap[48]; canonicalEga(snap);
		byte live[48]; canonicalEga(live);
		for (int i = 0; i < 3; i++) live[2 * 3 + i] = snap[2 * 3 + i] / 2; // halve EGA color 2 only
		uint32 table[256]; buildLivePaletteTable(snap, live, table);
		bool changed[16]; paletteDiffMask(snap, live, changed);
		Common::Rect dirty;
		reblendChangedPixels(idx.begin(), 2, 1, table, changed, *plate, dirty);
		// pixel0 (index 0x22 references color 2 -> changed) updated; pixel1 (0x44) untouched.
		uint8 a, r, g, b;
		plate->format.colorToARGB(plate->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_DELTA((int)g, 0xaa / 2, 3);
		plate->format.colorToARGB(plate->getPixel(1, 0), a, r, g, b);
		TS_ASSERT_EQUALS((int)b, (int)((BLEND_TABLE[0x44] >> 16) & 0xff)); // unchanged
		TS_ASSERT(!dirty.isEmpty());
		plate->free(); delete plate;
	}
};
