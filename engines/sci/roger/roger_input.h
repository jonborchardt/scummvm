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

#ifndef SCI_ROGER_ROGER_INPUT_H
#define SCI_ROGER_ROGER_INPUT_H

// Input automation for the Roger verification loop (design spec:
// docs/superpowers/specs/2026-07-01-roger-input-automation-design.md).
//
// MCP-READINESS BOUNDARY: this unit speaks pure Common::Event and may include
// ONLY common/* headers — no SCI/Roger engine types. It is the engine-agnostic
// core a future MCP server (or any other front-end) would reuse unchanged.
//
// .rin grammar (game-space 320x200 coords; '#' comments; blank lines skipped):
//   click X Y | rclick X Y | move X Y | key <token> | type "text"
//   wait <ms> | capture <label> | log <text> | quit
// Key tokens: ENTER ESC SPACE TAB BACKSPACE UP DOWN LEFT RIGHT F1..F12 a-z 0-9

#include "common/array.h"
#include "common/events.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

enum ScriptCmdType {
	kCmdNone = 0,
	kCmdClick,
	kCmdRClick,
	kCmdMove,
	kCmdKey,
	kCmdType,
	kCmdWait,
	kCmdCapture,
	kCmdLog,
	kCmdQuit
};

struct ScriptCommand {
	ScriptCmdType type;
	int x, y;                 // click/rclick/move (game 320x200, clamped)
	Common::KeyCode keycode;  // key
	uint16 ascii;             // key
	Common::String text;      // type payload / capture label / log text
	uint32 ms;                // wait
	ScriptCommand() : type(kCmdNone), x(0), y(0),
		keycode(Common::KEYCODE_INVALID), ascii(0), ms(0) {}
};

// Parse one script line. Returns true and fills cmd for a real command; false
// for blank/comment lines (cmd.type = kCmdNone) and for malformed lines (a
// warning() is emitted; the caller skips the line — never fatal).
bool parseScriptLine(const Common::String &line, ScriptCommand &cmd);

// Map a key token to keycode + ascii. Case-sensitive single chars (a-z, 0-9),
// case-insensitive named tokens. Returns false for unknown tokens.
bool keyTokenToKey(const Common::String &tok, Common::KeyCode &keycode, uint16 &ascii);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_INPUT_H
