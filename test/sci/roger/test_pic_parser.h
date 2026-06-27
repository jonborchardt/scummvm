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

	void test_format_ega() {
		// SCI0 EGA: first byte is a valid opcode (>= 0xF0)
		const byte data[] = { 0xf0, 0x05, 0xff };
		TS_ASSERT_EQUALS((int)picResourceFormat(data, sizeof(data)), (int)kPicSci0Ega);
	}

	void test_format_sci11_vga() {
		// SCI1.1 VGA: first 2 bytes are LE word 0x0026 (38)
		const byte data[] = { 0x26, 0x00, 0x00, 0x00, 0x0e /*14 priority bands*/, 0x01 };
		TS_ASSERT_EQUALS((int)picResourceFormat(data, sizeof(data)), (int)kPicSci11VgaCel);
	}

	void test_format_vga_vector() {
		// SCI1 VGA vector: first byte is opcode 0xF0 but second byte > 0x0F (not EGA 4-bit)
		// Detected by the fact it starts with 0xF0 but is NOT SCI1.1 (no 0x26 header).
		// Format is ambiguous from bytes alone; this path returns kPicSci1VgaVector when
		// the caller passes viewType != kViewEga. For bytes-only detection, any first byte
		// >= 0xF0 that is not the 0x26 header => kPicSci0Ega (EGA or SCI1 vector, same parser gate).
		// For the purposes of this unit test, test the boundary: header word != 0x0026.
		const byte data[] = { 0xf0, 0x10, 0xff }; // first byte is opcode, not SCI1.1
		TS_ASSERT_EQUALS((int)picResourceFormat(data, sizeof(data)), (int)kPicSci0Ega);
	}

	void test_format_empty() {
		// Empty resource: treat as EGA (caller will fail to parse, returns empty)
		const byte *data = nullptr;
		TS_ASSERT_EQUALS((int)picResourceFormat(data, 0), (int)kPicSci0Ega);
	}
};
