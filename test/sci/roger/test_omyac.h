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

	void test_native_exposes_priority_band() {
		// A FILL in priority mode (drawMode=Priority) with priority code 5 over a
		// region; the exposed band buffer must carry 5 where the fill landed.
		Common::Array<DrawCommand> cmds;
		DrawCommand c; c.kind = kCmdFill; c.drawMode = kDrawPriority; c.drawCodes[1] = 5;
		Point pos = {10, 10}; c.points.push_back(pos);
		cmds.push_back(c);
		NativeRef ref = nativePreRender(cmds);
		TS_ASSERT_EQUALS(ref.priority.size(), (uint)(OMYAC_NATIVE_W * OMYAC_NATIVE_H));
		TS_ASSERT_EQUALS(ref.priority[10 * OMYAC_NATIVE_W + 10], (byte)5);
	}

	void test_omyac_srcindex_roundtrips_priority() {
		// A priority FILL (band 5) at (10,10) over white background + a visual line.
		// In wireframe (no enhance passes) the anchor dot for native cell (10,10)
		// lands at hires (63,63) and must carry that cell's source index, so looking
		// it up in ref.priority reproduces band 5.
		Common::Array<DrawCommand> cmds;
		DrawCommand pf; pf.kind = kCmdFill; pf.drawMode = kDrawPriority; pf.drawCodes[1] = 5;
		Point p = {10, 10}; pf.points.push_back(p); cmds.push_back(pf);
		DrawCommand ln; ln.kind = kCmdPline; ln.drawMode = kDrawVisual; ln.drawCodes[0] = 2;
		Point a = {2, 2}, b = {8, 6}; ln.points.push_back(a); ln.points.push_back(b); cmds.push_back(ln);

		NativeRef ref = nativePreRender(cmds);
		OmyacResult out = renderOmyac(ref, Common::Array<int>()); // wireframe: no enhance
		TS_ASSERT_EQUALS(out.srcNativeIdx.size(), (uint)(OMYAC_HYBRID_W * OMYAC_HYBRID_H));

		const int dot = 63 * OMYAC_HYBRID_W + 63; // anchor dot of native (10,10)
		const int32 s = out.srcNativeIdx[dot];
		TS_ASSERT(s >= 0 && s < OMYAC_NATIVE_W * OMYAC_NATIVE_H);
		TS_ASSERT_EQUALS(ref.priority[s], (byte)5);
	}
};
