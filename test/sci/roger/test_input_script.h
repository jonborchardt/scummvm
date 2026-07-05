#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_input.h"
using namespace Sci::Roger;

class RogerInputParseTestSuite : public CxxTest::TestSuite {
public:
	void test_parse_click() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("click 120 80", c));
		TS_ASSERT_EQUALS(c.type, kCmdClick);
		TS_ASSERT_EQUALS(c.x, 120);
		TS_ASSERT_EQUALS(c.y, 80);
	}

	void test_parse_rclick_move_wait() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("rclick 10 20", c));
		TS_ASSERT_EQUALS(c.type, kCmdRClick);
		TS_ASSERT(parseScriptLine("move 300 150", c));
		TS_ASSERT_EQUALS(c.type, kCmdMove);
		TS_ASSERT(parseScriptLine("wait 500", c));
		TS_ASSERT_EQUALS(c.type, kCmdWait);
		TS_ASSERT_EQUALS(c.ms, 500u);
	}

	void test_parse_key_tokens() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("key ENTER", c));
		TS_ASSERT_EQUALS(c.type, kCmdKey);
		TS_ASSERT_EQUALS(c.keycode, Common::KEYCODE_RETURN);
		TS_ASSERT_EQUALS(c.ascii, 13);
		TS_ASSERT(parseScriptLine("key ESC", c));
		TS_ASSERT_EQUALS(c.keycode, Common::KEYCODE_ESCAPE);
		TS_ASSERT(parseScriptLine("key UP", c));
		TS_ASSERT_EQUALS(c.keycode, Common::KEYCODE_UP);
		TS_ASSERT(parseScriptLine("key F3", c));
		TS_ASSERT_EQUALS(c.keycode, Common::KEYCODE_F3);
		TS_ASSERT(parseScriptLine("key a", c));
		TS_ASSERT_EQUALS(c.keycode, Common::KEYCODE_a);
		TS_ASSERT_EQUALS(c.ascii, (uint16)'a');
		TS_ASSERT(parseScriptLine("key 7", c));
		TS_ASSERT_EQUALS(c.ascii, (uint16)'7');
	}

	void test_parse_type_quoted() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("type \"look door\"", c));
		TS_ASSERT_EQUALS(c.type, kCmdType);
		TS_ASSERT_EQUALS(c.text, Common::String("look door"));
	}

	void test_parse_capture_log_quit() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("capture after-dialog", c));
		TS_ASSERT_EQUALS(c.type, kCmdCapture);
		TS_ASSERT_EQUALS(c.text, Common::String("after-dialog"));
		TS_ASSERT(parseScriptLine("log reached town", c));
		TS_ASSERT_EQUALS(c.type, kCmdLog);
		TS_ASSERT_EQUALS(c.text, Common::String("reached town"));
		TS_ASSERT(parseScriptLine("quit", c));
		TS_ASSERT_EQUALS(c.type, kCmdQuit);
	}

	void test_parse_comment_blank_malformed() {
		ScriptCommand c;
		TS_ASSERT(!parseScriptLine("# a comment", c));
		TS_ASSERT(!parseScriptLine("", c));
		TS_ASSERT(!parseScriptLine("   ", c));
		TS_ASSERT(!parseScriptLine("frobnicate 1 2", c)); // unknown verb
		TS_ASSERT(!parseScriptLine("click 10", c));       // missing Y
		TS_ASSERT(!parseScriptLine("key NOSUCH", c));     // unknown token
	}

	void test_coordinate_clamp() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("click 999 -5", c));
		TS_ASSERT_EQUALS(c.x, 319);
		TS_ASSERT_EQUALS(c.y, 0);
	}
};

class RogerInputDriverTestSuite : public CxxTest::TestSuite {
	// Drain all input events due at nowMs into out; returns count.
	static int drain(InputScriptDriver &d, uint32 nowMs, Common::Array<Common::Event> &out) {
		Common::Event ev;
		int n = 0;
		while (d.pollDue(nowMs, ev)) {
			out.push_back(ev);
			n++;
		}
		return n;
	}

public:
	void test_click_expansion_and_timing() {
		InputScriptDriver d;
		d.loadScriptFromString("click 120 80\n");
		Common::Array<Common::Event> evs;
		// First poll establishes base time (5000). Due at once: move + down (up is +60).
		drain(d, 5000, evs);
		TS_ASSERT_EQUALS((int)evs.size(), 2);
		TS_ASSERT_EQUALS(evs[0].type, Common::EVENT_MOUSEMOVE);
		TS_ASSERT_EQUALS(evs[0].mouse.x, 120);
		TS_ASSERT_EQUALS(evs[0].mouse.y, 80);
		TS_ASSERT_EQUALS(evs[1].type, Common::EVENT_LBUTTONDOWN);
		// Not yet due.
		Common::Event ev;
		TS_ASSERT(!d.pollDue(5059, ev));
		// Due at base+60.
		TS_ASSERT(d.pollDue(5060, ev));
		TS_ASSERT_EQUALS(ev.type, Common::EVENT_LBUTTONUP);
	}

	void test_wait_delays_key() {
		InputScriptDriver d;
		d.loadScriptFromString("wait 500\nkey ENTER\n");
		Common::Event ev;
		TS_ASSERT(!d.pollDue(1000, ev));  // base = 1000; key due at 1500
		TS_ASSERT(!d.pollDue(1499, ev));
		TS_ASSERT(d.pollDue(1500, ev));
		TS_ASSERT_EQUALS(ev.type, Common::EVENT_KEYDOWN);
		TS_ASSERT_EQUALS(ev.kbd.keycode, Common::KEYCODE_RETURN);
		TS_ASSERT_EQUALS(ev.kbd.ascii, 13);
		TS_ASSERT(d.pollDue(1530, ev));
		TS_ASSERT_EQUALS(ev.type, Common::EVENT_KEYUP);
	}

	void test_type_emits_per_char() {
		InputScriptDriver d;
		d.loadScriptFromString("type \"ab\"\n");
		Common::Array<Common::Event> evs;
		drain(d, 100, evs);           // 'a' down due at base
		drain(d, 100 + 200, evs);     // everything else well past due
		TS_ASSERT_EQUALS((int)evs.size(), 4); // a down/up, b down/up
		TS_ASSERT_EQUALS(evs[0].kbd.ascii, (uint16)'a');
		TS_ASSERT_EQUALS(evs[2].kbd.ascii, (uint16)'b');
	}

	void test_capture_sets_request_in_order() {
		InputScriptDriver d;
		d.loadScriptFromString("capture boot\nkey ENTER\n");
		Common::String label;
		TS_ASSERT(!d.takeCaptureRequest(label)); // not due until first poll
		Common::Event ev;
		TS_ASSERT(d.pollDue(2000, ev));          // capture consumed, then keydown yields
		TS_ASSERT_EQUALS(ev.type, Common::EVENT_KEYDOWN);
		TS_ASSERT(d.takeCaptureRequest(label));
		TS_ASSERT_EQUALS(label, Common::String("boot"));
		TS_ASSERT(!d.takeCaptureRequest(label)); // one-shot
	}

	void test_quit_yields_quit_event_once() {
		InputScriptDriver d;
		d.loadScriptFromString("quit\n");
		Common::Event ev;
		TS_ASSERT(d.pollDue(3000, ev));
		TS_ASSERT_EQUALS(ev.type, Common::EVENT_QUIT);
		TS_ASSERT(!d.pollDue(9999, ev)); // script exhausted
	}

	void test_live_append_fires_now_and_buffers_partial_lines() {
		InputScriptDriver d;
		d.loadScriptFromString("");   // empty script; live-style appends only
		Common::Event ev;
		TS_ASSERT(!d.pollDue(1000, ev)); // base = 1000, nothing scheduled
		// Append a complete line + an incomplete one at t=5000.
		d.appendLiveText("key ENTER\nkey ES", 5000);
		TS_ASSERT(d.pollDue(5000, ev));  // fires immediately (cursor moved to now)
		TS_ASSERT_EQUALS(ev.kbd.keycode, Common::KEYCODE_RETURN);
		TS_ASSERT(d.pollDue(5030, ev));  // its keyup
		TS_ASSERT(!d.pollDue(5100, ev)); // "key ES" is buffered, not parsed
		// Complete the partial line.
		d.appendLiveText("C\n", 6000);
		TS_ASSERT(d.pollDue(6000, ev));
		TS_ASSERT_EQUALS(ev.kbd.keycode, Common::KEYCODE_ESCAPE);
	}
};

class RogerInputParseNewCmdsTestSuite : public CxxTest::TestSuite {
public:
	void test_parse_snap_state() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("snap boot", c));
		TS_ASSERT_EQUALS(c.type, kCmdSnap);
		TS_ASSERT_EQUALS(c.text, Common::String("boot"));
		TS_ASSERT(parseScriptLine("state", c));
		TS_ASSERT_EQUALS(c.type, kCmdState);
		TS_ASSERT(!parseScriptLine("snap", c)); // label required
	}

	void test_parse_waituntil() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("waituntil pic 300 8000", c));
		TS_ASSERT_EQUALS(c.type, kCmdWaitUntil);
		TS_ASSERT_EQUALS(c.text, Common::String("pic"));
		TS_ASSERT_EQUALS(c.value, 300);
		TS_ASSERT_EQUALS(c.ms, 8000u);
		TS_ASSERT(!parseScriptLine("waituntil pic 300", c));   // timeout required
		TS_ASSERT(!parseScriptLine("waituntil pic", c));       // value required
	}

	void test_parse_assert_restore_fail() {
		ScriptCommand c;
		TS_ASSERT(parseScriptLine("assert pic 300", c));
		TS_ASSERT_EQUALS(c.type, kCmdAssert);
		TS_ASSERT_EQUALS(c.text, Common::String("pic"));
		TS_ASSERT_EQUALS(c.value, 300);
		TS_ASSERT(!parseScriptLine("assert pic", c));          // value required
		TS_ASSERT(parseScriptLine("restore 1", c));
		TS_ASSERT_EQUALS(c.type, kCmdRestore);
		TS_ASSERT_EQUALS(c.value, 1);
		TS_ASSERT(!parseScriptLine("restore -2", c));          // negative slot rejected
		TS_ASSERT(!parseScriptLine("restore", c));             // slot required
		TS_ASSERT(parseScriptLine("fail dialog never appeared", c));
		TS_ASSERT_EQUALS(c.type, kCmdFail);
		TS_ASSERT_EQUALS(c.text, Common::String("dialog never appeared"));
		TS_ASSERT(!parseScriptLine("fail", c));                // message required
	}
};
