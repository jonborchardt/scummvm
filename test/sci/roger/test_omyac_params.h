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

// Two flood-filled halves split by a vertical line: exercises fill-vs-fill and
// fill-vs-line boundaries, where enhance/backfill can push one fill's colour
// into the other's native cells (the erodeForeignFill leak class).
static NativeRef dividedFillsRef() {
	Common::Array<DrawCommand> cmds;
	// Border box so the fills are bounded.
	DrawCommand box; box.kind = kCmdPline; box.drawMode = kDrawVisual; box.drawCodes[0] = 0;
	Point p0 = {10, 10}, p1 = {150, 10}, p2 = {150, 100}, p3 = {10, 100}, p4 = {10, 10};
	box.points.push_back(p0); box.points.push_back(p1); box.points.push_back(p2);
	box.points.push_back(p3); box.points.push_back(p4);
	cmds.push_back(box);
	// Diagonal-ish divider (staircase boundary -> smoothing has work to do).
	DrawCommand div; div.kind = kCmdPline; div.drawMode = kDrawVisual; div.drawCodes[0] = 0;
	Point d0 = {75, 10}, d1 = {90, 100}; div.points.push_back(d0); div.points.push_back(d1);
	cmds.push_back(div);
	// Fill left half colour 11 (light cyan), right half colour 4 (dark red).
	DrawCommand fl; fl.kind = kCmdFill; fl.drawMode = kDrawVisual; fl.drawCodes[0] = 11;
	Point sl = {40, 50}; fl.points.push_back(sl);
	cmds.push_back(fl);
	DrawCommand fr; fr.kind = kCmdFill; fr.drawMode = kDrawVisual; fr.drawCodes[0] = 4;
	Point sr = {120, 50}; fr.points.push_back(sr);
	cmds.push_back(fr);
	return nativePreRender(cmds);
}

// Count final CMD_FILL pixels erodeForeignFill must eliminate: in a LINE cell,
// ANY colour foreign to the cell (zero rim, v8); in a FILL cell, a foreign
// colour not anchored to home territory (an 8-neighbour of the same colour
// whose cell is undrawn or natively that colour — the 1 px rim allowance).
static int countUnanchoredForeignFill(const NativeRef &ref, const OmyacResult &out) {
	int bad = 0;
	for (int y = 0; y < OMYAC_HYBRID_H; y++) {
		for (int x = 0; x < OMYAC_HYBRID_W; x++) {
			uint idx = (uint)y * OMYAC_HYBRID_W + x;
			if (out.cmdType[idx] != CMD_FILL)
				continue;
			int cell = (y / OMYAC_SCALE) * OMYAC_NATIVE_W + (x / OMYAC_SCALE);
			if (ref.cmdType[cell] == CMD_NONE)
				continue;
			byte c = out.pixels[idx];
			if (c == ref.refPixel[cell])
				continue;
			if (ref.cmdType[cell] == CMD_LINE) {
				bad++; // zero rim in line cells
				continue;
			}
			bool anchored = false;
			for (int dy = -1; dy <= 1 && !anchored; dy++) {
				for (int dx = -1; dx <= 1 && !anchored; dx++) {
					if (!dx && !dy)
						continue;
					int nx = x + dx, ny = y + dy;
					if (nx < 0 || nx >= OMYAC_HYBRID_W || ny < 0 || ny >= OMYAC_HYBRID_H)
						continue;
					uint nidx = (uint)ny * OMYAC_HYBRID_W + nx;
					if (out.cmdType[nidx] == CMD_NONE || out.pixels[nidx] != c)
						continue;
					int ncell = (ny / OMYAC_SCALE) * OMYAC_NATIVE_W + (nx / OMYAC_SCALE);
					if (ref.cmdType[ncell] == CMD_NONE || ref.refPixel[ncell] == c)
						anchored = true;
				}
			}
			if (!anchored)
				bad++;
		}
	}
	return bad;
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
		p.backfillOwnCell = false; TS_ASSERT(!p.isDefault()); p.backfillOwnCell = true;
		p.backfillFloodRounds = 0; TS_ASSERT(!p.isDefault()); p.backfillFloodRounds = 3;
		p.erodeForeignFill = false; TS_ASSERT(!p.isDefault()); p.erodeForeignFill = true;

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
		//           diagFlankSuppress=true backfillOwnCell=true (kTransformVersion 5);
		//           passes=3f1l2f4a (defaultPasses()).
		static const uint32 kDefaultPipelineGolden = 0xBA1D1C23u; // FNV-1a over pixels+cmdType (v8)
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacResult out = renderOmyac(ref, passes, OmyacParams());
		uint32 got = omyacResultHash(out);
		TS_ASSERT_EQUALS(got, kDefaultPipelineGolden);
	}

	// TS-port fidelity lock: with backfillOwnCell=false the pipeline reproduces the
	// original agi-up/sci.js port bit-exactly — this hash is the pre-v5 default
	// pipeline golden and must never change.
	void test_legacy_backfill_pipeline_golden_checksum() {
		static const uint32 kLegacyPipelineGolden = 0x40B38BFFu; // FNV-1a over pixels+cmdType
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes = defaultPasses();
		OmyacParams p;
		p.backfillOwnCell = false;
		p.erodeForeignFill = false;
		OmyacResult out = renderOmyac(ref, passes, p);
		uint32 got = omyacResultHash(out);
		TS_ASSERT_EQUALS(got, kLegacyPipelineGolden);
	}

	// backfillOwnCell invariant (bounded-flood disabled, rounds=0): every pixel
	// fillNullPixels painted equals its own native cell's refPixel — an unclaimed
	// pixel never takes a neighbouring cell's colour (the down-right cascade of
	// the legacy majority flood). The shipping default adds backfillFloodRounds
	// bounded flood rounds before this terminal fill; rounds=0 isolates the
	// own-cell phase for the invariant.
	void test_backfill_own_cell_is_native_faithful() {
		NativeRef ref = crossingLinesRef();
		Common::Array<int> passes; // wireframe: large null regions -> heavy backfill
		OmyacParams pOwnOnly;
		pOwnOnly.backfillFloodRounds = 0;
		OmyacResult out = renderOmyac(ref, passes, pOwnOnly);
		int checked = 0;
		for (int y = 0; y < OMYAC_HYBRID_H; y++) {
			for (int x = 0; x < OMYAC_HYBRID_W; x++) {
				uint idx = (uint)y * OMYAC_HYBRID_W + x;
				if (!out.backfilled[idx])
					continue;
				int cell = (y / OMYAC_SCALE) * OMYAC_NATIVE_W + (x / OMYAC_SCALE);
				TS_ASSERT_EQUALS(out.pixels[idx], ref.refPixel[cell]);
				if (out.pixels[idx] != ref.refPixel[cell])
					return; // one failure is enough; don't spam 2M asserts
				checked++;
			}
		}
		TS_ASSERT(checked > 0);
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

	// Backfill mask (studio "unfilled pixels" diagnostic): sized to the hybrid
	// buffer, records exactly the pixels fillNullPixels painted. For a sparse
	// fixture it must contain both 1s (backfilled background) and 0s (drawn/
	// enhanced pixels). Recording it must NOT alter pixels/cmdType — the golden
	// checksum test above already pins that invariant.
	void test_backfill_mask_populated() {
		NativeRef ref = crossingLinesRef();
		// Zero enhance passes: raw wireframe leaves large CMD_NONE regions that
		// fillNullPixels backfills, so the mask has both 1s (backfilled empty
		// space) and 0s (the drawn line pixels) — the sparse-fixture assertion the
		// studio "pink" toggle depends on. (The default-pass pipeline can flood the
		// whole buffer, leaving little/no backfill; wireframe is the honest probe.)
		Common::Array<int> passes; // empty == zero passes
		OmyacResult out = renderOmyac(ref, passes, OmyacParams());
		const uint expected = (uint)OMYAC_HYBRID_W * (uint)OMYAC_HYBRID_H;
		TS_ASSERT_EQUALS(out.backfilled.size(), expected);
		TS_ASSERT_EQUALS(out.pixels.size(), expected);
		int ones = 0, zeros = 0;
		for (uint i = 0; i < out.backfilled.size(); i++) {
			if (out.backfilled[i])
				ones++;
			else
				zeros++;
		}
		TS_ASSERT(ones > 0);   // sparse fixture -> lots of backfilled background
		TS_ASSERT(zeros > 0);  // ...but the drawn line pixels are not backfilled
		// A backfilled pixel must be CMD_FILL in the final cmdType (fillNullPixels
		// stamps CMD_FILL), i.e. the mask never marks an untouched pixel.
		for (uint i = 0; i < out.backfilled.size(); i++)
			if (out.backfilled[i]) { TS_ASSERT(out.cmdType[i] != 0 /*CMD_NONE*/); break; }
	}

	// erodeForeignFill invariant (v7): in the final default-pipeline output, a
	// fill colour never sits deeper than 1 px inside a drawn native cell of a
	// different colour — every foreign CMD_FILL pixel is anchored to home
	// territory. The sanity half proves the fixture actually exercises the leak
	// (with erosion off, unanchored foreign pixels exist), so the invariant half
	// cannot pass vacuously.
	void test_erode_foreign_fill_bounds_fringes() {
		NativeRef ref = dividedFillsRef();
		Common::Array<int> passes = defaultPasses();

		OmyacParams noErode;
		noErode.erodeForeignFill = false;
		OmyacResult leaky = renderOmyac(ref, passes, noErode);
		TS_ASSERT(countUnanchoredForeignFill(ref, leaky) > 0);

		OmyacResult out = renderOmyac(ref, passes, OmyacParams());
		TS_ASSERT_EQUALS(countUnanchoredForeignFill(ref, out), 0);
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
