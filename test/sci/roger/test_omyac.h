#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_pic_native.h"
#include "sci/roger/roger_omyac.h"
using namespace Sci::Roger;

class RogerOmyacTestSuite : public CxxTest::TestSuite {
public:
	void test_native_horizontal_line_tracked() {
		// One PLINE in visual color 2 from (0,0)-(3,0); rest stays white background.
		Common::Array<DrawCommand> cmds;
		DrawCommand c; c.kind = kCmdPline; c.drawMode = kDrawVisual; c.drawCodes[0] = 2;
		Point a = {0,0}, b = {3,0}; c.points.push_back(a); c.points.push_back(b);
		cmds.push_back(c);
		NativeRef ref = nativePreRender(cmds);
		// DEFAULT_PALETTE[2] == 0x22 (solid green doubled-nibble).
		TS_ASSERT_EQUALS(ref.refPixel[0], 0x22);
		TS_ASSERT_EQUALS(ref.cmdType[0], (byte)CMD_LINE);
		TS_ASSERT_EQUALS(ref.refCmd[0], (int16)0);
		// A background cell far from the line is promoted to white fill.
		int far = 100 * OMYAC_NATIVE_W + 100;
		TS_ASSERT_EQUALS(ref.cmdType[far], (byte)CMD_FILL);
		TS_ASSERT_EQUALS(ref.refPixel[far], 0xff);
	}

	void test_omyac_fills_no_cmd_none_remains() {
		// A short diagonal line + default white background; after full pipeline,
		// every output pixel must be non-CMD_NONE (null-fill guarantees it).
		Common::Array<DrawCommand> cmds;
		DrawCommand c; c.kind = kCmdPline; c.drawMode = kDrawVisual; c.drawCodes[0] = 4; // red-ish
		Point a = {2,2}, b = {8,6}; c.points.push_back(a); c.points.push_back(b);
		cmds.push_back(c);
		NativeRef ref = nativePreRender(cmds);
		Common::Array<int> passes; // empty -> wireframe; then we also test default
		OmyacResult wire = renderOmyac(ref, passes);
		TS_ASSERT_EQUALS(wire.pixels.size(), (uint)(OMYAC_HYBRID_W * OMYAC_HYBRID_H));

		Common::Array<int> def = defaultPasses();
		OmyacResult out = renderOmyac(ref, def);
		bool anyNone = false;
		for (uint i = 0; i < out.cmdType.size(); i++) if (out.cmdType[i] == CMD_NONE) { anyNone = true; break; }
		TS_ASSERT(!anyNone); // fillNullPixels leaves nothing unfilled
	}
};
