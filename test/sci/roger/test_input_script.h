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
