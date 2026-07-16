/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cxxtest/TestSuite.h>

#include "backends/events/emscriptensdl/emscripten-keynorm.h"

class EmscriptenKeynormTestSuite : public CxxTest::TestSuite {
public:
	// The mobile OSK bridge pushes backspace as (8, ascii 0); SCI0 drops
	// ascii-0 keys, so normalization must give control keys their ASCII.
	void test_backspace_gains_ascii() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey(8, 0);
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.keycode, Common::KEYCODE_BACKSPACE);
		TS_ASSERT_EQUALS(k.ascii, (uint16)8);
		TS_ASSERT_EQUALS(k.flags, (byte)0);
	}

	void test_return_passthrough() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey(13, 13);
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.keycode, Common::KEYCODE_RETURN);
		TS_ASSERT_EQUALS(k.ascii, (uint16)13);
	}

	// Common::KeyCode has no codes 65-90: uppercase must map to the
	// lowercase keycode with KBD_SHIFT, keeping the uppercase ascii.
	void test_uppercase_maps_to_lowercase_keycode_with_shift() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey('R', 'R');
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.keycode, Common::KEYCODE_r);
		TS_ASSERT_EQUALS(k.ascii, (uint16)'R');
		TS_ASSERT_EQUALS(k.flags, (byte)Common::KBD_SHIFT);
	}

	void test_lowercase_passthrough() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey('r', 'r');
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.keycode, Common::KEYCODE_r);
		TS_ASSERT_EQUALS(k.ascii, (uint16)'r');
		TS_ASSERT_EQUALS(k.flags, (byte)0);
	}

	// Mobile autocorrect substitutes smart punctuation; map to ASCII.
	void test_curly_apostrophe_maps_to_ascii() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey(0x2019, 0x2019);
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.ascii, (uint16)'\'');
	}

	void test_curly_quote_maps_to_ascii() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey(0x201C, 0x201C);
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.ascii, (uint16)'"');
	}

	// Unmappable non-Latin-1 (emoji surrogate halves) must be dropped,
	// not injected as garbage.
	void test_emoji_surrogate_dropped() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey(0xD83D, 0xD83D);
		TS_ASSERT(!k.valid);
	}

	// Action-bar keys (arrows, F10) legitimately carry ascii 0 and must
	// pass through untouched.
	void test_arrow_key_keeps_zero_ascii() {
		EmscriptenInjectedKey k = normalizeEmscriptenInjectedKey(273, 0);
		TS_ASSERT(k.valid);
		TS_ASSERT_EQUALS(k.keycode, Common::KEYCODE_UP);
		TS_ASSERT_EQUALS(k.ascii, (uint16)0);
	}
};
