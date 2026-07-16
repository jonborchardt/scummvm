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

#ifndef BACKENDS_EVENTS_EMSCRIPTEN_KEYNORM_H
#define BACKENDS_EVENTS_EMSCRIPTEN_KEYNORM_H

#include "common/keyboard.h"

/**
 * Normalized form of a key pushed by the mobile-OSK JS bridge
 * (custom_shell.html -> EmscriptenKbd_pushKey). The bridge sends raw
 * JS charCodeAt values as both keycode and ascii; this maps them onto
 * the Common::KeyCode contract. valid == false means drop the event.
 */
struct EmscriptenInjectedKey {
	Common::KeyCode keycode;
	uint16 ascii;
	byte flags;
	bool valid;
};

inline EmscriptenInjectedKey normalizeEmscriptenInjectedKey(int keycode, int ascii) {
	EmscriptenInjectedKey k;
	k.flags = 0;
	k.valid = true;

	// Smart punctuation from mobile autocorrect -> ASCII equivalents.
	switch (ascii) {
	case 0x2018: // left single quote
	case 0x2019: // right single quote / apostrophe
		ascii = '\'';
		keycode = ascii;
		break;
	case 0x201C: // left double quote
	case 0x201D: // right double quote
		ascii = '"';
		keycode = ascii;
		break;
	case 0x2013: // en dash
	case 0x2014: // em dash
		ascii = '-';
		keycode = ascii;
		break;
	case 0x2026: // ellipsis
		ascii = '.';
		keycode = ascii;
		break;
	default:
		break;
	}

	// Unmappable beyond Latin-1 (emoji surrogates etc.): drop rather than
	// inject garbage the engine cannot interpret.
	if (ascii > 0xFF || ascii < 0) {
		k.keycode = Common::KEYCODE_INVALID;
		k.ascii = 0;
		k.valid = false;
		return k;
	}

	if (keycode >= 'A' && keycode <= 'Z') {
		// Common::KeyCode defines no uppercase codes; keycode is the
		// lowercase letter with KBD_SHIFT, ascii keeps the uppercase.
		k.flags = Common::KBD_SHIFT;
		keycode += 'a' - 'A';
	} else if (keycode > 0 && keycode < 32 && ascii == 0) {
		// Control keys (backspace/tab/return/escape) carry their ASCII
		// value; SCI0 drops ascii-0 keys outright (event.cpp).
		ascii = keycode;
	} else if (keycode > 0x7F && keycode <= 0xFF) {
		// Latin-1 text character: no meaningful keycode exists.
		keycode = Common::KEYCODE_INVALID;
	}

	k.keycode = (Common::KeyCode)keycode;
	k.ascii = (uint16)ascii;
	return k;
}

/**
 * True when a standalone SDL_EVENT_TEXT_INPUT merely echoes the character
 * that the immediately-preceding SDL_EVENT_KEY_DOWN already delivered.
 *
 * SdlEventSource pairs a KEY_DOWN with its TEXT_INPUT by peeking the SDL
 * queue (obtainUnicode) and consuming the TEXT_INPUT. On Emscripten the
 * runtime is single-threaded: the browser can only queue the keypress-
 * derived TEXT_INPUT when control returns to its event loop, so the game's
 * poll regularly lands BETWEEN the pair. The KEY_DOWN then delivers the
 * character anyway (mapKey derives printable ASCII from the keycode) and
 * the orphaned TEXT_INPUT later fakes a second key-down with the same
 * character -- the intermittent doubled letters seen while typing. An
 * orphaned TEXT_INPUT always follows its own KEY_DOWN in the SDL queue
 * (browser event order is preserved, and a TEXT_INPUT still queued when a
 * later KEY_DOWN is polled gets consumed by the pairing peek), so echo
 * detection only ever needs the latest KEY_DOWN.
 *
 * keydownKeycode is the SDL keycode of the last accepted KEY_DOWN (layout
 * keycode: lowercase ASCII for letter keys). Letters match case-
 * insensitively -- shift/caps deliver the uppercase from the same physical
 * key. Other printable keys match exactly (mapKey's fallback returns the
 * keycode itself). Multi-byte/multi-char text (IME, paste) never matches.
 */
inline bool emscriptenTextInputEchoesKeyDown(uint32 keydownKeycode, uint64 keydownTimestampNs, const char *text, uint64 textTimestampNs, uint64 windowMs) {
	if (!text || !text[0] || text[1] != '\0')
		return false;
	if (textTimestampNs < keydownTimestampNs)
		return false;
	if ((textTimestampNs - keydownTimestampNs) / 1000000ULL > windowMs)
		return false;
	byte c = (byte)text[0];
	if (c < 0x20 || c > 0x7E)
		return false;
	if (keydownKeycode >= 'a' && keydownKeycode <= 'z')
		return c == keydownKeycode || c == (keydownKeycode & ~0x20u);
	return keydownKeycode >= 0x20 && keydownKeycode <= 0x7E && c == keydownKeycode;
}

#endif
