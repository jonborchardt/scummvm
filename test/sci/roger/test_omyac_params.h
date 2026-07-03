#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_pic_native.h"
#include "sci/roger/roger_omyac.h"
using namespace Sci::Roger;

// Two crossing plines in different colours plus two same-colour segments that
// share an endpoint pixel (adjacent, different cmdId, same colour). With
// endpointMaxSame=2 (default) the shared-end pixels are endpoints and the
// bridging pass connects them across the boundary; with endpointMaxSame=0
// nothing is ever an endpoint so the bridging never fires, leaving a different
// connection pattern in the anchor graph and therefore different rendered output.
static NativeRef crossingLinesRef() {
	Common::Array<DrawCommand> cmds;
	// Diagonal crossing pair (long enough for enhance passes to react).
	DrawCommand c1; c1.kind = kCmdPline; c1.drawMode = kDrawVisual; c1.drawCodes[0] = 2;
	Point a1 = {2, 2}, b1 = {60, 40}; c1.points.push_back(a1); c1.points.push_back(b1);
	cmds.push_back(c1);
	DrawCommand c2; c2.kind = kCmdPline; c2.drawMode = kDrawVisual; c2.drawCodes[0] = 4;
	Point a2 = {60, 2}, b2 = {2, 40}; c2.points.push_back(a2); c2.points.push_back(b2);
	cmds.push_back(c2);
	// Two short horizontal segments of the same colour that TOUCH at one end
	// (c3 ends at x=80, c4 starts at x=81 — adjacent so the tip of c3 sees the
	// start of c4 as a same-colour different-cmd neighbor). With isEndpoint=true
	// (endpointMaxSame=2), phase-1 endpoint bridging fires and adds a connect bit
	// toward c4; with endpointMaxSame=0 no endpoint is set and no bridging runs.
	DrawCommand c3; c3.kind = kCmdPline; c3.drawMode = kDrawVisual; c3.drawCodes[0] = 2;
	Point a3 = {70, 60}, b3 = {80, 60}; c3.points.push_back(a3); c3.points.push_back(b3);
	cmds.push_back(c3);
	DrawCommand c4; c4.kind = kCmdPline; c4.drawMode = kDrawVisual; c4.drawCodes[0] = 2;
	Point a4 = {81, 60}, b4 = {91, 60}; c4.points.push_back(a4); c4.points.push_back(b4);
	cmds.push_back(c4);
	return nativePreRender(cmds);
}

static int countDiffs(const OmyacResult &x, const OmyacResult &y) {
	if (x.pixels.size() != y.pixels.size())
		return -1;
	int diffs = 0;
	for (uint i = 0; i < x.pixels.size(); i++)
		if (x.pixels[i] != y.pixels[i] || x.cmdType[i] != y.cmdType[i])
			diffs++;
	return diffs;
}

class RogerOmyacParamsTestSuite : public CxxTest::TestSuite {
public:
	void test_default_params_is_default() {
		OmyacParams p;
		TS_ASSERT(p.isDefault());
		p.minVotesLine = 2;
		TS_ASSERT(!p.isDefault());
	}

	// THE isolation guarantee: 3-arg with default params == existing 2-arg,
	// byte for byte, over the full pipeline with the default pass sequence.
	void test_default_params_bit_identical() {
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacResult oldPath = renderOmyac(ref, passes);
		OmyacResult newPath = renderOmyac(ref, passes, OmyacParams());
		TS_ASSERT_EQUALS(countDiffs(oldPath, newPath), 0);
	}

	// Plumbing check: a non-default param actually reaches the pipeline.
	// If this fixture happens to produce identical output, strengthen the
	// fixture (longer/more crossing lines), do NOT weaken the assertion —
	// the point is that params change behavior.
	void test_params_change_output() {
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacResult defOut = renderOmyac(ref, passes, OmyacParams());
		OmyacParams p;
		p.minVotesLine = 3; // line passes need 3 votes instead of 1 → less line growth
		OmyacResult tuned = renderOmyac(ref, passes, p);
		TS_ASSERT(countDiffs(defOut, tuned) > 0);
	}

	// endpointMaxSame reaches detectLineEndings even with zero enhance passes
	// only via connections; safest observable: full default pipeline differs.
	void test_endpoint_param_reaches_pipeline() {
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacResult defOut = renderOmyac(ref, passes, OmyacParams());
		OmyacParams p;
		p.endpointMaxSame = 0; // nothing is an endpoint → endpoint bridging disabled
		OmyacResult tuned = renderOmyac(ref, passes, p);
		TS_ASSERT(countDiffs(defOut, tuned) > 0);
	}
};
