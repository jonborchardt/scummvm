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
#include "common/str.h"

/**
 * SDL Events manager for Emscripten
 */
class EmscriptenSdlEventSource : public SdlEventSource {
public:
	EmscriptenSdlEventSource() : _lastKeyDownScancode(SDL_SCANCODE_UNKNOWN), _lastKeyDownTimestamp(0), _lastTextInputTimestamp(0), _lastTextInputText() {}

	/**
	 * Gets and processes SDL events.
	 */
	bool pollEvent(Common::Event &event) override {
	
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
				_lastKeyDownTimestamp = event->key.timestamp;
			}
		} else if (event->type == SDL_EVENT_TEXT_INPUT) {
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
	Uint64 _lastKeyDownTimestamp;
	Uint64 _lastTextInputTimestamp;
	Common::String _lastTextInputText;
	static const Uint64 kTextInputDedupMs = 30; // real ms (see SDL_NS_TO_MS above); < any human repeat, > any phantom coincidence
};

#endif /* BACKEND_EVENTS_EMSCRIPTEN_H */
