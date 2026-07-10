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
#include "sci/sci_gfx_observer.h"

using namespace Sci;

class TestGfxObserverTokens : public CxxTest::TestSuite {
public:
	// Raw hex pins, same discipline as roger_tokens.h: tests pin the values on
	// purpose; everything else names them.
	void test_namespace_constants_raw_hex() {
		TS_ASSERT_EQUALS((uint32)kGfxTokenNamespaceMask, 0xF0000000u);
		TS_ASSERT_EQUALS((uint32)kGfxTokenStatus,        0x10000000u);
		TS_ASSERT_EQUALS((uint32)kGfxTokenMenuDropdown,  0x20000000u);
		TS_ASSERT_EQUALS((uint32)kGfxTokenNsWindow,      0x40000000u);
		TS_ASSERT_EQUALS((uint32)kGfxTokenNsIcon,        0x50000000u);
		TS_ASSERT_EQUALS((uint32)kGfxTokenNsText,        0x60000000u);
		TS_ASSERT_EQUALS((uint32)kGfxTokenFrameBox,      0x70000000u);
	}
	void test_window_token_matches_legacy_packing() {
		TS_ASSERT_EQUALS(gfxWindowToken(3u), 0x40000003u); // 0x40000000 | windowId
	}
	void test_text_port_token_matches_legacy_packing() {
		TS_ASSERT_EQUALS(gfxTextPortToken(3u), 0x60000003u); // 0x60000000 | portId
	}
	void test_icon_token_matches_legacy_packing() {
		TS_ASSERT_EQUALS(gfxIconToken(7u), 0x50000007u); // 0x50000000 | portId
	}
	void test_handle_token_packs_segment_and_offset_unmasked() {
		// Save-handle identity: exact match for the (seg << 16) | offset packing
		// at bitsSave/bitsRestore/bitsFree today (no offset mask).
		TS_ASSERT_EQUALS(gfxHandleToken(0x0012u, 0x0034u), 0x00120034u);
	}
	void test_owner_token_truncates_offset_to_16_bits() {
		// Animate-owner identity: exact match for animate.cpp's rogerOwnerToken
		// (SCI0 offsets fit 16 bits; the mask makes the packing well-defined).
		TS_ASSERT_EQUALS(gfxOwnerToken(0x0002u, 0x1ABCDu), 0x0002ABCDu);
	}
	void test_token_namespace_extraction() {
		TS_ASSERT_EQUALS(gfxTokenNamespace(gfxWindowToken(9u)), (uint32)kGfxTokenNsWindow);
		TS_ASSERT_EQUALS(gfxTokenNamespace(kGfxTokenStatus), (uint32)kGfxTokenStatus);
	}
	// Contract: the base observer is a pure no-op and is directly constructible
	// (no pure virtuals) — call sites may rely on default behavior being inert
	// and every claim returning false (native path runs).
	void test_default_observer_is_noop_and_claims_nothing() {
		SciGfxObserver obs;
		obs.onFrameStart();
		obs.onFrameEnd();
		obs.beginSelfDraw();
		obs.endSelfDraw();
		obs.onErase(Common::Rect(0, 0, 10, 10));
		TS_ASSERT(!obs.claimTransition(0, Common::Rect(0, 0, 320, 190), -1));
		TS_ASSERT(!obs.claimShake(1, 1));
		TS_ASSERT(!obs.claimCursor());
		TS_ASSERT(!obs.wantsUnclampedTextEdit());
	}
};
