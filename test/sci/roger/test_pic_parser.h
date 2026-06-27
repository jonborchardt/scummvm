#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_pic_parser.h"
using namespace Sci::Roger;

class RogerPicParserTestSuite : public CxxTest::TestSuite {
public:
	void test_set_color_line_fill_terminate() {
		// 0xf0 0x05            SET_COLOR visual=5
		// 0xf6 <p24> <p24> ... LONG_LINES: (0,0) -> (1,2)
		//   p24 for (0,0):   0x00 0x00 0x00
		//   p24 for (1,2):   x=1,y=2 -> high nibbles 0,0; low x=1,low y=2 => 0x00 0x01 0x02
		// 0xf8 <p24>          FILL at (1,2): 0x00 0x01 0x02
		// 0xff                TERMINATE
		const byte data[] = {
			0xf0, 0x05,
			0xf6, 0x00,0x00,0x00, 0x00,0x01,0x02,
			0xf8, 0x00,0x01,0x02,
			0xff
		};
		Common::Array<DrawCommand> cmds = parsePic(data, sizeof(data));
		TS_ASSERT_EQUALS(cmds.size(), 2u);              // SET_COLOR yields no command
		TS_ASSERT_EQUALS((int)cmds[0].kind, (int)kCmdPline);
		TS_ASSERT_EQUALS(cmds[0].drawCodes[0], 5);       // visual code carried
		TS_ASSERT_EQUALS(cmds[0].points.size(), 2u);
		TS_ASSERT_EQUALS(cmds[0].points[0].x, 0);
		TS_ASSERT_EQUALS(cmds[0].points[1].x, 1);
		TS_ASSERT_EQUALS(cmds[0].points[1].y, 2);
		TS_ASSERT_EQUALS((int)cmds[1].kind, (int)kCmdFill);
		TS_ASSERT_EQUALS(cmds[1].points[0].x, 1);
		TS_ASSERT_EQUALS(cmds[1].points[0].y, 2);
	}
};
