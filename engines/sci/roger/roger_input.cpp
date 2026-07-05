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

#include "common/debug.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/path.h"
#include "common/system.h"
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
		int msVal = atoi(msStr.c_str());
		if (msVal < 0) {
			warning("ROGER-SCRIPT: negative wait clamped to 0: %s", line.c_str());
			msVal = 0;
		}
		cmd.ms = (uint32)msVal;
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
	if (verb == "snap" || verb == "fail") {
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
		cmd.type = (verb == "snap") ? kCmdSnap : kCmdFail;
		cmd.text = rest;
		return true;
	}
	if (verb == "state") {
		cmd.type = kCmdState;
		return true;
	}
	if (verb == "waituntil") {
		Common::String key = tok.nextToken(), vs = tok.nextToken(), ts = tok.nextToken();
		if (key.empty() || vs.empty() || ts.empty()) {
			warning("ROGER-SCRIPT: waituntil needs <key> <value> <timeoutMs>: %s", line.c_str());
			return false;
		}
		cmd.type = kCmdWaitUntil;
		cmd.text = key;
		cmd.value = atoi(vs.c_str());
		int t = atoi(ts.c_str());
		if (t < 0)
			t = 0;
		cmd.ms = (uint32)t;
		return true;
	}
	if (verb == "assert") {
		Common::String key = tok.nextToken(), vs = tok.nextToken();
		if (key.empty() || vs.empty()) {
			warning("ROGER-SCRIPT: assert needs <key> <value>: %s", line.c_str());
			return false;
		}
		cmd.type = kCmdAssert;
		cmd.text = key;
		cmd.value = atoi(vs.c_str());
		return true;
	}
	if (verb == "restore") {
		Common::String ss = tok.nextToken();
		if (ss.empty()) {
			warning("ROGER-SCRIPT: restore needs <slot>: %s", line.c_str());
			return false;
		}
		const int slot = atoi(ss.c_str());
		if (slot < 0) {
			warning("ROGER-SCRIPT: restore slot must be >= 0: %s", line.c_str());
			return false;
		}
		cmd.type = kCmdRestore;
		cmd.value = slot;
		return true;
	}

	warning("ROGER-SCRIPT: unknown command skipped: %s", line.c_str());
	return false;
}

InputScriptDriver::InputScriptDriver()
	: _next(0), _cursorRelMs(0), _baseMs(0), _haveBase(false), _done(false),
	  _capturePending(false), _liveOffset(0), _lastTailMs(0) {
}

void InputScriptDriver::pushMouse(Common::EventType type, int x, int y, uint32 relMs) {
	TimedAction a;
	a.relMs = relMs;
	a.isEvent = true;
	a.ev.type = type;
	a.ev.mouse = Common::Point((int16)x, (int16)y); // game space (320x200): see plan header
	_actions.push_back(a);
}

void InputScriptDriver::pushKey(Common::EventType type, Common::KeyCode kc, uint16 ascii, uint32 relMs) {
	TimedAction a;
	a.relMs = relMs;
	a.isEvent = true;
	a.ev.type = type;
	a.ev.kbd = Common::KeyState(kc, ascii);
	_actions.push_back(a);
}

void InputScriptDriver::pushCtrl(ScriptCmdType ctrl, const Common::String &label, uint32 relMs) {
	TimedAction a;
	a.relMs = relMs;
	a.isEvent = false;
	a.ctrl = ctrl;
	a.label = label;
	_actions.push_back(a);
}

void InputScriptDriver::expandCommand(const ScriptCommand &c) {
	switch (c.type) {
	case kCmdWait:
		_cursorRelMs += c.ms;
		break;
	case kCmdMove:
		pushMouse(Common::EVENT_MOUSEMOVE, c.x, c.y, _cursorRelMs);
		break;
	case kCmdClick:
	case kCmdRClick: {
		const bool left = (c.type == kCmdClick);
		pushMouse(Common::EVENT_MOUSEMOVE, c.x, c.y, _cursorRelMs);
		pushMouse(left ? Common::EVENT_LBUTTONDOWN : Common::EVENT_RBUTTONDOWN, c.x, c.y, _cursorRelMs);
		pushMouse(left ? Common::EVENT_LBUTTONUP : Common::EVENT_RBUTTONUP, c.x, c.y, _cursorRelMs + 60);
		_cursorRelMs += 120;
		break;
	}
	case kCmdKey:
		pushKey(Common::EVENT_KEYDOWN, c.keycode, c.ascii, _cursorRelMs);
		pushKey(Common::EVENT_KEYUP, c.keycode, c.ascii, _cursorRelMs + 30);
		_cursorRelMs += 60;
		break;
	case kCmdType:
		for (uint i = 0; i < c.text.size(); i++) {
			const char ch = c.text[i];
			Common::KeyCode kc = (ch >= 'A' && ch <= 'Z')
				? (Common::KeyCode)(ch - 'A' + Common::KEYCODE_a)
				: (Common::KeyCode)ch; // a-z, 0-9, space, punctuation: ASCII-valued keycodes
			pushKey(Common::EVENT_KEYDOWN, kc, (uint16)(byte)ch, _cursorRelMs);
			pushKey(Common::EVENT_KEYUP, kc, (uint16)(byte)ch, _cursorRelMs + 20);
			_cursorRelMs += 40;
		}
		break;
	case kCmdCapture:
	case kCmdLog:
	case kCmdQuit:
		pushCtrl(c.type, c.text, _cursorRelMs);
		break;
	default:
		break;
	}
}

void InputScriptDriver::loadScriptFromString(const Common::String &text) {
	Common::String line;
	for (uint i = 0; i <= text.size(); i++) {
		if (i == text.size() || text[i] == '\n') {
			ScriptCommand cmd;
			if (parseScriptLine(line, cmd))
				expandCommand(cmd);
			line.clear();
		} else if (text[i] != '\r') {
			line += text[i];
		}
	}
}

bool InputScriptDriver::pollEvent(Common::Event &ev) {
	return pollDue(g_system->getMillis(), ev);
}

bool InputScriptDriver::pollDue(uint32 nowMs, Common::Event &ev) {
	if (_done)
		return false;
	if (!_haveBase) {
		_baseMs = nowMs;
		_haveBase = true;
	}
	if (_next >= _actions.size()) {
		if (!_livePath.empty())
			tailLive(nowMs); // Task 3 (no-op stub until then)
		if (_next >= _actions.size())
			return false;
	}
	while (_next < _actions.size()) {
		const TimedAction &a = _actions[_next];
		if (nowMs < _baseMs + a.relMs)
			return false; // front not due; O(1) exit — the steady-state path
		_next++;
		if (a.isEvent) {
			ev = a.ev;
			return true;
		}
		switch (a.ctrl) {
		case kCmdCapture:
			if (_capturePending)
				warning("ROGER-SCRIPT: capture '%s' overwrites pending capture '%s'", a.label.c_str(), _captureLabel.c_str());
			_capturePending = true;
			_captureLabel = a.label;
			break;
		case kCmdLog:
			warning("ROGER-SCRIPT: %s", a.label.c_str());
			break;
		case kCmdQuit:
			_done = true;
			ev.type = Common::EVENT_QUIT;
			return true;
		default:
			break;
		}
	}
	return false;
}

bool InputScriptDriver::takeCaptureRequest(Common::String &label) {
	if (!_capturePending)
		return false;
	_capturePending = false;
	label = _captureLabel;
	return true;
}

bool InputScriptDriver::loadScriptFile(const Common::String &path) {
	Common::Path p(path);
	Common::FSNode node(p);
	Common::SeekableReadStream *stream = node.createReadStream();
	if (!stream) {
		warning("ROGER-SCRIPT: cannot open script: %s", path.c_str());
		return false;
	}
	Common::String text;
	while (!stream->eos()) {
		byte buf[4096];
		uint32 n = stream->read(buf, sizeof(buf));
		if (!n)
			break;
		text += Common::String((const char *)buf, n);
	}
	delete stream;
	loadScriptFromString(text);
	debug(1, "ROGER-SCRIPT: loaded %s (%u actions)", path.c_str(), (unsigned)_actions.size());
	return true;
}

void InputScriptDriver::setLiveFile(const Common::String &path) {
	_livePath = path;
}

void InputScriptDriver::appendLiveText(const Common::String &text, uint32 nowMs) {
	if (!_haveBase) {
		_baseMs = nowMs;
		_haveBase = true;
	}
	// New commands fire from now (never in the past); `wait` still spaces
	// commands within one append.
	const uint32 nowRel = nowMs - _baseMs;
	if (_cursorRelMs < nowRel)
		_cursorRelMs = nowRel;

	Common::String pending = _livePartial + text;
	_livePartial.clear();
	Common::String line;
	for (uint i = 0; i < pending.size(); i++) {
		if (pending[i] == '\n') {
			ScriptCommand cmd;
			if (parseScriptLine(line, cmd))
				expandCommand(cmd);
			line.clear();
		} else if (pending[i] != '\r') {
			line += pending[i];
		}
	}
	_livePartial = line; // incomplete trailing line: wait for its newline
}

void InputScriptDriver::tailLive(uint32 nowMs) {
	// Perf discipline: never on the steady-state path (only reached when the
	// action queue is exhausted) and throttled to >= 100 ms between reads.
	if (nowMs - _lastTailMs < 100)
		return;
	_lastTailMs = nowMs;

	Common::Path p(_livePath);
	Common::FSNode node(p);
	Common::SeekableReadStream *stream = node.createReadStream();
	if (!stream)
		return; // file may not exist yet; keep polling
	const int64 size = stream->size();
	if (size > (int64)_liveOffset) {
		stream->seek(_liveOffset, SEEK_SET);
		Common::String text;
		while (!stream->eos()) {
			byte buf[4096];
			uint32 n = stream->read(buf, sizeof(buf));
			if (!n)
				break;
			text += Common::String((const char *)buf, n);
		}
		_liveOffset = (uint32)size;
		appendLiveText(text, nowMs);
	}
	delete stream;
}

} // namespace Roger
} // namespace Sci
