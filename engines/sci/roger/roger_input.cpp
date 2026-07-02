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

#include "sci/roger/roger_input.h"

#include "common/textconsole.h"
#include "common/tokenizer.h"

namespace Sci {
namespace Roger {

bool keyTokenToKey(const Common::String &tok, Common::KeyCode &keycode, uint16 &ascii) {
	// Single printable character: a-z, 0-9 (KEYCODE_a..z / 0..9 are ASCII-valued).
	if (tok.size() == 1) {
		const char c = tok[0];
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			keycode = (Common::KeyCode)c;
			ascii = (uint16)c;
			return true;
		}
		return false;
	}

	Common::String t = tok;
	t.toUppercase();
	struct NamedKey { const char *name; Common::KeyCode kc; uint16 ascii; };
	static const NamedKey namedKeys[] = {
		{ "ENTER",     Common::KEYCODE_RETURN,    13 },
		{ "ESC",       Common::KEYCODE_ESCAPE,    27 },
		{ "SPACE",     Common::KEYCODE_SPACE,     32 },
		{ "TAB",       Common::KEYCODE_TAB,        9 },
		{ "BACKSPACE", Common::KEYCODE_BACKSPACE,  8 },
		{ "UP",        Common::KEYCODE_UP,         0 },
		{ "DOWN",      Common::KEYCODE_DOWN,       0 },
		{ "LEFT",      Common::KEYCODE_LEFT,       0 },
		{ "RIGHT",     Common::KEYCODE_RIGHT,      0 },
	};
	for (uint i = 0; i < ARRAYSIZE(namedKeys); i++) {
		if (t == namedKeys[i].name) {
			keycode = namedKeys[i].kc;
			ascii = namedKeys[i].ascii;
			return true;
		}
	}
	// F1..F12 (KEYCODE_F1..F12 are contiguous).
	if (t.size() >= 2 && t[0] == 'F') {
		int n = atoi(t.c_str() + 1);
		if (n >= 1 && n <= 12) {
			keycode = (Common::KeyCode)(Common::KEYCODE_F1 + (n - 1));
			ascii = 0;
			return true;
		}
	}
	return false;
}

static int clampCoord(int v, int maxExclusive) {
	if (v < 0)
		return 0;
	if (v >= maxExclusive)
		return maxExclusive - 1;
	return v;
}

bool parseScriptLine(const Common::String &line, ScriptCommand &cmd) {
	cmd = ScriptCommand();

	// Strip comments and whitespace-only lines.
	Common::String s = line;
	uint32 hash = s.findFirstOf('#');
	if (hash != Common::String::npos)
		s = Common::String(s.c_str(), hash);
	s.trim();
	if (s.empty())
		return false;

	Common::StringTokenizer tok(s, " \t");
	Common::String verb = tok.nextToken();

	if (verb == "click" || verb == "rclick" || verb == "move") {
		Common::String xs = tok.nextToken(), ys = tok.nextToken();
		if (xs.empty() || ys.empty()) {
			warning("ROGER-SCRIPT: malformed '%s' (need X Y): %s", verb.c_str(), line.c_str());
			return false;
		}
		cmd.type = (verb == "click") ? kCmdClick : (verb == "rclick") ? kCmdRClick : kCmdMove;
		cmd.x = clampCoord(atoi(xs.c_str()), 320);
		cmd.y = clampCoord(atoi(ys.c_str()), 200);
		return true;
	}
	if (verb == "key") {
		Common::String t = tok.nextToken();
		if (t.empty() || !keyTokenToKey(t, cmd.keycode, cmd.ascii)) {
			warning("ROGER-SCRIPT: unknown key token: %s", line.c_str());
			return false;
		}
		cmd.type = kCmdKey;
		return true;
	}
	if (verb == "type") {
		// Payload is everything between the first and last double quote.
		uint32 q1 = s.findFirstOf('"');
		uint32 q2 = s.findLastOf('"');
		if (q1 == Common::String::npos || q2 <= q1) {
			warning("ROGER-SCRIPT: type needs a \"quoted\" string: %s", line.c_str());
			return false;
		}
		cmd.type = kCmdType;
		cmd.text = Common::String(s.c_str() + q1 + 1, q2 - q1 - 1);
		return true;
	}
	if (verb == "wait") {
		Common::String msStr = tok.nextToken();
		if (msStr.empty()) {
			warning("ROGER-SCRIPT: wait needs <ms>: %s", line.c_str());
			return false;
		}
		cmd.type = kCmdWait;
		cmd.ms = (uint32)atoi(msStr.c_str());
		return true;
	}
	if (verb == "capture" || verb == "log") {
		// Rest of the line (single token label for capture; free text for log).
		Common::String rest;
		while (!tok.empty()) {
			if (!rest.empty())
				rest += " ";
			rest += tok.nextToken();
		}
		if (rest.empty()) {
			warning("ROGER-SCRIPT: %s needs an argument: %s", verb.c_str(), line.c_str());
			return false;
		}
		cmd.type = (verb == "capture") ? kCmdCapture : kCmdLog;
		cmd.text = rest;
		return true;
	}
	if (verb == "quit") {
		cmd.type = kCmdQuit;
		return true;
	}

	warning("ROGER-SCRIPT: unknown command skipped: %s", line.c_str());
	return false;
}

} // namespace Roger
} // namespace Sci
