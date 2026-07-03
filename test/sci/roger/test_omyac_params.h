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

// FNV-1a 32-bit hash over both output arrays. Dependency-free, C++11.
// Used for the golden-checksum test — any pipeline change that shifts the
// default output must bump kTransformVersion AND update the golden constant.
static uint32 omyacResultHash(const OmyacResult &r) {
	const uint32 FNV_PRIME = 0x01000193u;
	const uint32 FNV_OFFSET = 0x811c9dc5u;
	uint32 h = FNV_OFFSET;
	for (uint i = 0; i < r.pixels.size(); i++) {
		h ^= (uint32)r.pixels[i];
		h *= FNV_PRIME;
	}
	for (uint i = 0; i < r.cmdType.size(); i++) {
		h ^= (uint32)r.cmdType[i];
		h *= FNV_PRIME;
	}
	return h;
}

class RogerOmyacParamsTestSuite : public CxxTest::TestSuite {
public:
	// Pin every default field value against silent drift. These values are the
	// hard-coded constants that existed before OmyacParams: changing ANY of them
	// changes every shipping omyac plate silently, because params are NOT part of
	// the cache key (only kTransformVersion is). If a default must change, also
	// bump kTransformVersion and update the golden checksum below.
	void test_default_params_is_default() {
		OmyacParams p;
		TS_ASSERT(p.isDefault());

		// --- Explicit field assertions (the real drift guard) ---
		TS_ASSERT_EQUALS(p.minVotesLine, 1);
		TS_ASSERT_EQUALS(p.minVotesFillAll, 2);
		TS_ASSERT_EQUALS(p.fillSuppressLineNeighbours, 3);
		TS_ASSERT_EQUALS(p.endpointMaxSame, 2);
		TS_ASSERT_EQUALS(p.isolatedPixelPass, true);
		TS_ASSERT_EQUALS(p.tieBreakBlend, true);
		TS_ASSERT_EQUALS(p.diagFlankSuppress, true);

		// isDefault() must reject any single-field mutation.
		p.minVotesLine = 2;        TS_ASSERT(!p.isDefault()); p.minVotesLine = 1;
		p.minVotesFillAll = 1;     TS_ASSERT(!p.isDefault()); p.minVotesFillAll = 2;
		p.fillSuppressLineNeighbours = 2; TS_ASSERT(!p.isDefault()); p.fillSuppressLineNeighbours = 3;
		p.endpointMaxSame = 0;     TS_ASSERT(!p.isDefault()); p.endpointMaxSame = 2;
		p.isolatedPixelPass = false; TS_ASSERT(!p.isDefault()); p.isolatedPixelPass = true;
		p.tieBreakBlend = false;   TS_ASSERT(!p.isDefault()); p.tieBreakBlend = true;
		p.diagFlankSuppress = false; TS_ASSERT(!p.isDefault()); p.diagFlankSuppress = true;

		TS_ASSERT(p.isDefault()); // restored to all-defaults
	}

	// Overload-contract check: the 2-arg convenience overload forwards OmyacParams()
	// (all defaults), so its output must be byte-identical to the explicit 3-arg form.
	// This tests the forwarding contract, NOT the default constants (use the golden
	// checksum below for that).
	void test_twoarg_overload_forwards_default_params() {
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacResult oldPath = renderOmyac(ref, passes);
		OmyacResult newPath = renderOmyac(ref, passes, OmyacParams());
		TS_ASSERT_EQUALS(countDiffs(oldPath, newPath), 0);
	}

	// Golden-checksum test: pins the ACTUAL numeric output of the default pipeline
	// over the crossing-lines fixture. If a default OmyacParams field drifts (or
	// any pipeline logic changes), this hash changes and the test fails — even if
	// every other param test stays green. When this fails intentionally (deliberate
	// pipeline change), also bump kTransformVersion so stale cache files are
	// invalidated, then re-run the test once to read the new hash from the failure
	// output and update kDefaultPipelineGolden here.
	void test_default_pipeline_golden_checksum() {
		// Golden FNV-1a hash (pixels then cmdType) of renderOmyac(crossingLinesRef(),
		// defaultPasses(), OmyacParams()). Derived by running the test with placeholder
		// 0 and reading the CxxTest failure output, then baked here permanently.
		// Pipeline: minVotesLine=1 minVotesFillAll=2 fillSuppressLineNeighbours=3
		//           endpointMaxSame=2 isolatedPixelPass=true tieBreakBlend=true
		//           diagFlankSuppress=true ; passes=3f1l2f4a (defaultPasses()).
		static const uint32 kDefaultPipelineGolden = 0x40B38BFFu; // FNV-1a over pixels+cmdType
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacResult out = renderOmyac(ref, passes, OmyacParams());
		uint32 got = omyacResultHash(out);
		TS_ASSERT_EQUALS(got, kDefaultPipelineGolden);
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
