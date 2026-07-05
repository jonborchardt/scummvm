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
//   click X Y | rclick X Y | move X Y | mousedown X Y | mouseup X Y | key <token> | type "text"
//   (mousedown/mouseup bracket a press-and-hold drag: mousedown, intervening moves, mouseup —
//    e.g. the SCI0 mouse menu path that opens dropdowns while the button is held)
//   wait <ms> | waituntil <key> <value> <timeoutMs>
//   capture <label> | snap <label> | state
//   assert <key> <value> | restore <slot> | fail <msg> | log <text> | quit
// State keys (served by the registered ScriptHost): pic, windows, egox, egoy, mode.
// capture pends a dump consumed by the next present; snap grabs the presented
// overlay pixels immediately (works during blocking dialogs, no flush move needed).
// Note: '#' starts a comment anywhere on a line, so `type "..."` and `log`
// payloads must not contain '#' (it would truncate the line at that point).

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
	kCmdMouseDown, // press-and-hold left button at X,Y (drag-gesture start; menu mouse path)
	kCmdMouseUp,   // release left button at X,Y (drag-gesture end)
	kCmdKey,
	kCmdType,
	kCmdWait,
	kCmdCapture,
	kCmdLog,
	kCmdQuit,
	kCmdSnap,      // synchronous overlay grab at execution time (no present needed)
	kCmdState,     // emit ROGER-STATE line via the ScriptHost
	kCmdWaitUntil, // gate: block schedule until host state key == value, or timeout
	kCmdAssert,    // host state key must == value, else FAIL + quit
	kCmdRestore,   // delayed-restore a save slot via the ScriptHost
	kCmdFail       // unconditional FAIL marker + quit
};

struct ScriptCommand {
	ScriptCmdType type;
	int x, y;                 // click/rclick/move (game 320x200, clamped)
	Common::KeyCode keycode;  // key
	uint16 ascii;             // key
	Common::String text;      // type payload / capture+snap label / log+fail text / state key
	uint32 ms;                // wait / waituntil timeout
	int value;                // waituntil+assert wanted value / restore slot
	ScriptCommand() : type(kCmdNone), x(0), y(0),
		keycode(Common::KEYCODE_INVALID), ascii(0), ms(0), value(0) {}
};

// Parse one script line. Returns true and fills cmd for a real command; false
// for blank/comment lines (cmd.type = kCmdNone) and for malformed lines (a
// warning() is emitted; the caller skips the line — never fatal).
bool parseScriptLine(const Common::String &line, ScriptCommand &cmd);

// Map a key token to keycode + ascii. Case-sensitive single chars (a-z, 0-9),
// case-insensitive named tokens. Returns false for unknown tokens.
bool keyTokenToKey(const Common::String &tok, Common::KeyCode &keycode, uint16 &ascii);

// Game-side services for the new script commands. Implemented by the art
// provider; this interface uses ONLY Common types (MCP-readiness boundary).
class ScriptHost {
public:
	virtual ~ScriptHost() {}
	// One-line state summary, e.g. "pic=300 ego=160,120 windows=0 mode=enhanced".
	virtual Common::String describeState() = 0;
	// Numeric state for waituntil/assert. Known keys: pic, windows, egox, egoy,
	// mode (0=enhanced 1=original 2=sbs). Returns -1 for unknown keys.
	virtual int stateValue(const Common::String &key) = 0;
	// Immediate presented-frame dump (grabOverlay), autoshot naming with -<label>.
	virtual void onSnap(const Common::String &label) = 0;
	// Schedule a delayed save restore (processed by the normal game loop).
	virtual void onRestore(int slot) = 0;
};

// One scheduled action: either a synthetic input event or a control command.
struct TimedAction {
	uint32 relMs;          // due time relative to the driver's base time
	bool isEvent;
	Common::Event ev;      // valid when isEvent
	ScriptCmdType ctrl;    // control command when !isEvent
	Common::String label;  // capture/snap label / log+fail text / state key
	int wantValue;         // waituntil+assert wanted value / restore slot
	uint32 timeoutMs;      // waituntil timeout
	TimedAction() : relMs(0), isEvent(false), ctrl(kCmdNone), wantValue(0), timeoutMs(0) {}
};

// Timed synthetic-event source. Registered with the backend EventDispatcher
// (EventManager::getEventDispatcher()->registerSource) so due events are
// delivered through the normal pollEvent path — during blocking dialogs too
// (their own poll drives the dispatch). allowMapping() = false keeps the
// keymapper out of the loop. The per-poll cost is O(1): one due-time compare.
class InputScriptDriver : public Common::EventSource {
public:
	InputScriptDriver();

	// Parse + expand a whole script (scripted mode). Cumulative `wait`s
	// become relative due times from the first poll.
	void loadScriptFromString(const Common::String &text);
	// Read a script file (absolute or cwd-relative path). False if unreadable.
	bool loadScriptFile(const Common::String &path);
	// Enable live mode: tail an append-only command file (throttled, Task 3).
	void setLiveFile(const Common::String &path);

	// Common::EventSource
	bool pollEvent(Common::Event &ev) override;
	bool allowMapping() const override { return false; }

	// Testable core (no g_system): executes all control actions due at nowMs
	// and returns true when a due input event was yielded into ev.
	bool pollDue(uint32 nowMs, Common::Event &ev);

	// One-shot: true once per executed `capture`, handing over its label.
	bool takeCaptureRequest(Common::String &label);

	// Non-consuming peek: a `capture` command has executed and its dump is still
	// pending. The present barrier must not skip a present while this is true.
	bool capturePending() const { return _capturePending; }

	// Live-mode core (also the unit-test hook): parse complete lines out of
	// `text` (buffering a trailing partial line), scheduling new commands to
	// fire from `nowMs` onward.
	void appendLiveText(const Common::String &text, uint32 nowMs);

	// Register the game-side host for snap/state/waituntil/assert/restore.
	// Not owned. Null host: those commands log-and-skip (never crash).
	void setScriptHost(ScriptHost *host) { _host = host; }

private:
	void expandCommand(const ScriptCommand &cmd);
	void pushMouse(Common::EventType type, int x, int y, uint32 relMs);
	void pushKey(Common::EventType type, Common::KeyCode kc, uint16 ascii, uint32 relMs);
	void tailLive(uint32 nowMs); // Task 3

	Common::Array<TimedAction> _actions;
	uint _next;            // next action index
	uint32 _cursorRelMs;   // schedule cursor for expansion
	uint32 _baseMs;        // wall-clock of the first poll
	bool _haveBase;
	bool _done;            // quit delivered; stop yielding
	bool _capturePending;
	Common::String _captureLabel;
	Common::String _livePath;   // Task 3
	uint32 _liveOffset;         // Task 3: bytes consumed
	uint32 _lastTailMs;         // Task 3: tail throttle
	Common::String _livePartial; // Task 3: trailing incomplete line
	ScriptHost *_host = nullptr; // borrowed; registered by the art provider
	// Slow-command schedule re-anchor: a snap (grabOverlay + PNG, ~1s) runs synchronously
	// inside pollDue, so the next poll's clock has jumped ahead. On the next poll we slide
	// _baseMs by the drift past this command's due time so following actions stay spaced as
	// authored (prevents a mid-drag move burst that a frozen menu/dialog loop never sees).
	bool _reanchorPending = false;
	uint32 _reanchorDueMs = 0;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_INPUT_H
