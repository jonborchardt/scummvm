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

#if !defined(BACKEND_EVENTS_EMSCRIPTEN_H) && !defined(DISABLE_DEFAULT_EVENTMANAGER)
#define BACKEND_EVENTS_EMSCRIPTEN_H

#include "backends/events/sdl/sdl-events.h"
#include "backends/platform/sdl/emscripten/emscripten.h"
#include "common/events.h"
#include "common/queue.h"
#include "common/str.h"
#include "backends/events/emscriptensdl/emscripten-keynorm.h"

class EmscriptenSdlEventSource;

// FIXME: non-const global var -- set by EmscriptenSdlEventSource's own
// constructor and cleared by its own destructor (below), so it always
// points at the live event source or is null. The mobile-keyboard JS
// bridge's EmscriptenKbd_pushKey() C shim (emscripten.cpp) needs a stable
// entry point to reach the live event source from outside the class.
// Defined in backends/platform/sdl/emscripten/emscripten.cpp.
extern EmscriptenSdlEventSource *g_emscriptenKbdSource;

/**
 * SDL Events manager for Emscripten
 */
class EmscriptenSdlEventSource : public SdlEventSource {
public:
	EmscriptenSdlEventSource() : _lastKeyDownScancode(SDL_SCANCODE_UNKNOWN), _lastKeyDownKeycode(0), _lastKeyDownTimestamp(0), _lastTextInputTimestamp(0), _lastTextInputText() {
		g_emscriptenKbdSource = this;
	}

	~EmscriptenSdlEventSource() override {
		if (g_emscriptenKbdSource == this)
			g_emscriptenKbdSource = nullptr;
	}

	/**
	 * Injected by the mobile-keyboard JS bridge via EmscriptenKbd_pushKey():
	 * enqueues a keydown+keyup pair to be returned by pollEvent() ahead of
	 * (but additively with) the normal SDL-polled events.
	 */
	void injectKey(Common::KeyCode keycode, uint16 ascii) {
		EmscriptenInjectedKey norm = normalizeEmscriptenInjectedKey((int)keycode, (int)ascii);
		if (!norm.valid)
			return;
		Common::Event down;
		down.type = Common::EVENT_KEYDOWN;
		down.kbd.keycode = norm.keycode;
		down.kbd.ascii = norm.ascii;
		down.kbd.flags = norm.flags;
		_injected.push(down);
		Common::Event up = down;
		up.type = Common::EVENT_KEYUP;
		_injected.push(up);
	}

	/**
	 * Gets and processes SDL events.
	 */
	bool pollEvent(Common::Event &event) override {
		if (!_injected.empty()) {
			event = _injected.pop();
			return true;
		}

		bool ret_value = SdlEventSource::pollEvent(event);
		if (event.type != Common::EVENT_QUIT && event.type != Common::EVENT_RETURN_TO_LAUNCHER) {
			// yield to the browser and process timers
			// (after polling the events to ensure synchronous event processing)
			g_system->delayMillis(0);
		}
		return ret_value;
	};

protected:
	/**
	 * Emscripten's SDL3 port manufactures a burst of spurious repeat
	 * SDL_EVENT_KEY_DOWN / SDL_EVENT_TEXT_INPUT events for every real key
	 * press: several events sharing one identical (corrupted) timestamp
	 * arrive within a few milliseconds of the real press. Drop only those
	 * same-timestamp duplicates here, before they reach dispatchSDLEvent;
	 * genuine held-key auto-repeat (whose timestamps keep advancing) is
	 * left untouched.
	 *
	 * The SDL_EVENT_TEXT_INPUT phantom does not always share the exact
	 * timestamp of the real event (unlike the key-down burst above), so an
	 * exact-match guard misses it under fast (e.g. synthetic/machine-speed)
	 * typing. Guard on content *and* a small tolerance window instead: a
	 * TEXT_INPUT repeating the same text within kTextInputDedupMs is a
	 * phantom, since real human keystrokes are never that close together
	 * (double-tap intervals run >= ~60 ms). This does not claim to catch
	 * every machine-speed phantom -- its own latency is not bounded, so
	 * an occasional duplicate can still slip past any window comfortably
	 * below the human threshold -- but it never drops a genuine repeat
	 * (verified: real double letters, e.g. "book", always keep both
	 * letters) and it eliminates most phantom repeats seen under
	 * synthetic/automated typing. Real human typing was already clean
	 * before this change.
	 */
	void preprocessEvents(SDL_Event *event) override {
		if (event->type == SDL_EVENT_KEY_DOWN) {
			if (event->key.repeat && event->key.scancode == _lastKeyDownScancode &&
					event->key.timestamp == _lastKeyDownTimestamp) {
				event->type = SDL_EVENT_FIRST;
			} else {
				_lastKeyDownScancode = event->key.scancode;
				_lastKeyDownKeycode = event->key.key;
				_lastKeyDownTimestamp = event->key.timestamp;
			}
		} else if (event->type == SDL_EVENT_TEXT_INPUT) {
			// A TEXT_INPUT reaching this poll loop is one the KEY_DOWN
			// pairing peek (SdlEventSource::obtainUnicode) did NOT consume:
			// on this single-threaded runtime the poll regularly lands
			// between a keydown and its keypress-derived TEXT_INPUT, the
			// KEY_DOWN delivers the character via mapKey's keycode fallback,
			// and the orphaned TEXT_INPUT would fake a second key-down with
			// the same character (intermittent doubled letters while
			// typing). Drop it when it merely echoes the last KEY_DOWN.
			if (emscriptenTextInputEchoesKeyDown(_lastKeyDownKeycode, _lastKeyDownTimestamp,
					event->text.text, event->text.timestamp, kTextInputEchoWindowMs)) {
				event->type = SDL_EVENT_FIRST;
				return;
			}
			// event->text.timestamp is in nanoseconds (SDL_GetTicksNS(), see
			// SDL_CommonEvent::timestamp); convert the delta to milliseconds
			// before comparing to kTextInputDedupMs, as sdl3-events.cpp
			// already does for SDL_NS_TO_MS(event->tfinger.timestamp).
			// Guard against a timestamp going backwards (should not happen,
			// but if it does, fail safe and keep the event instead of
			// underflowing the unsigned subtraction into a huge delta that
			// would coincidentally pass the <= check).
			if (_lastTextInputText == event->text.text &&
					event->text.timestamp >= _lastTextInputTimestamp &&
					SDL_NS_TO_MS(event->text.timestamp - _lastTextInputTimestamp) <= kTextInputDedupMs) {
				event->type = SDL_EVENT_FIRST;
			} else {
				_lastTextInputTimestamp = event->text.timestamp;
				_lastTextInputText = event->text.text;
			}
		}
	}

private:
	SDL_Scancode _lastKeyDownScancode;
	Uint32 _lastKeyDownKeycode;
	Uint64 _lastKeyDownTimestamp;
	Uint64 _lastTextInputTimestamp;
	Common::String _lastTextInputText;
	static const Uint64 kTextInputDedupMs = 30; // real ms (see SDL_NS_TO_MS above); < any human repeat, > any phantom coincidence
	// Echo window for orphaned TEXT_INPUTs vs their own KEY_DOWN: the pair is
	// generated back-to-back in one browser input sequence (delta typically
	// <1 ms; the window only absorbs timestamp jitter). A genuine repeated
	// letter is safe at ANY gap: its own KEY_DOWN both delivers the character
	// and becomes the new comparison point before its TEXT_INPUT arrives.
	static const Uint64 kTextInputEchoWindowMs = 100;
	Common::Queue<Common::Event> _injected;
};

#endif /* BACKEND_EVENTS_EMSCRIPTEN_H */
