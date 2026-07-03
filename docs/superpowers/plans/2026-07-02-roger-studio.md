# Roger Studio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A debug-only, raw-overlay tuning environment inside ScummVM (`ROGER_STUDIO=1`) for visually tuning omyac pic-enhancement parameters (including a reorderable pass-sequence editor) and comparing view-cel scaler variants, in isolation and composited on an enhanced plate.

**Architecture:** An `OmyacParams` struct threads today's hard-coded constants through `renderOmyac()` (defaults ⇒ bit-identical output, locked by tests). A new `RogerStudio` class owns a blocking event/draw loop at the same startup seam as the Roger launcher (engine + resources alive, no game scripts), renders through its own memory-mode `RogerAssetGen`, and draws directly to the OSystem overlay. Pure helpers (param registry, scaler variants, filename stamps) live in a separate SCI-free file so they are CxxTest-testable.

**Tech Stack:** C++11, ScummVM OSystem overlay API, CxxTest (`build_tests.ps1`), MSVC build (`build_and_run.ps1 -NoLaunch`).

**Spec:** `docs/superpowers/specs/2026-07-02-roger-studio-design.md`

## Global Constraints

- **Zero behavioral change to shipping code paths.** Default-constructed `OmyacParams` must produce bit-identical output; existing 2-arg `renderOmyac(ref, passes)` signature is kept as a forwarding overload so no shipping call site changes.
- **Never touch the disk cache from the studio.** The studio constructs its own `RogerAssetGen` in `kGenMemory` mode with an empty cache dir; `setOmyacParams()` with non-default params force-switches any instance to `kGenMemory` as belt-and-braces.
- **No `kTransformVersion` bump** in this work.
- Code style: C++11, tabs (width 4), K&R braces, pointer/reference right-aligned, no exceptions/RTTI, GPLv3+ header on every new file.
- `docs/` is gitignored — commit plan/spec docs with `git add -f`.
- Other agents may be working in `test/sci/roger/regression-*` / `run-regression.ps1` — never `git add -A`; always stage explicit paths.
- Env vars are per-launch (never written to scummvm.ini), pattern-matched to `ROGER_DIAG`.
- Build check: `.\build_and_run.ps1 -NoLaunch` (must succeed). Unit tests: `.\build_tests.ps1` (all suites must pass).

---

## File Structure

| File | Status | Responsibility |
|---|---|---|
| `engines/sci/roger/roger_omyac.h/.cpp` | modify | `OmyacParams` struct; thread params through `detectLineEndings`, `connectFillAnchors`, `enhance`, `renderOmyac` |
| `engines/sci/roger/roger_studio_render.h/.cpp` | create | SCI-free pure helpers: param registry (name/min/max/get/set), scaler-variant registry, pass/param stamps, export filename builder |
| `engines/sci/roger/roger_studio.h/.cpp` | create | `RogerStudio` class: event loop, three modes, HUD, judging tools, export |
| `engines/sci/roger/roger_asset_gen.h/.cpp` | modify | `setOmyacParams()`; extract `nativeCelIndexImage()` + `surfaceFromIndex()` from `generateViewCel()` (pure refactor) |
| `engines/sci/sci.cpp` | modify | env-gated studio hook after provider creation (~line 411) |
| `engines/sci/module.mk` | modify | add `roger/roger_studio.o`, `roger/roger_studio_render.o` |
| `build_and_run.ps1` | modify | `-Studio` switch → `ROGER_STUDIO=1` env var |
| `CLAUDE.md` | modify | one short paragraph documenting `-Studio` |
| `test/sci/roger/test_omyac_params.h` | create | default-equivalence golden test, param-differencing test, `isDefault()` test |
| `test/sci/roger/test_studio_render.h` | create | registry clamp/roundtrip, variant factor/dims, stamp/filename tests |
| `test/sci/roger/scripts/studio-smoke.rin` | create | autonomous loop smoke: wait + export + Esc |

---

### Task 1: `OmyacParams` struct threaded through `renderOmyac`

**Files:**
- Modify: `engines/sci/roger/roger_omyac.h`
- Modify: `engines/sci/roger/roger_omyac.cpp`
- Create: `test/sci/roger/test_omyac_params.h`

**Interfaces:**
- Consumes: existing `renderOmyac(const NativeRef &, const Common::Array<int> &)`, `nativePreRender`, `DrawCommand` (from `roger_pic_native.h` / `roger_pic_parser.h`).
- Produces (later tasks rely on these exact names):
  - `struct OmyacParams` in `Sci::Roger` with fields `int minVotesLine; int minVotesFillAll; int fillSuppressLineNeighbours; int endpointMaxSame; bool isolatedPixelPass; bool tieBreakBlend; bool diagFlankSuppress;` and `bool isDefault() const`.
  - `OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes, const OmyacParams &params);` (3-arg)
  - 2-arg overload preserved, forwarding `OmyacParams()`.

- [ ] **Step 1: Write the failing tests**

Create `test/sci/roger/test_omyac_params.h`:

```cpp
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_pic_native.h"
#include "sci/roger/roger_omyac.h"
using namespace Sci::Roger;

// Two crossing plines in different colours: enough LINE pixels + intersections
// that the enhance passes do real work, so param changes show up in the output.
static NativeRef crossingLinesRef() {
	Common::Array<DrawCommand> cmds;
	DrawCommand c1; c1.kind = kCmdPline; c1.drawMode = kDrawVisual; c1.drawCodes[0] = 2;
	Point a1 = {2, 2}, b1 = {60, 40}; c1.points.push_back(a1); c1.points.push_back(b1);
	cmds.push_back(c1);
	DrawCommand c2; c2.kind = kCmdPline; c2.drawMode = kDrawVisual; c2.drawCodes[0] = 4;
	Point a2 = {60, 2}, b2 = {2, 40}; c2.points.push_back(a2); c2.points.push_back(b2);
	cmds.push_back(c2);
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `.\build_tests.ps1`
Expected: compile FAILURE — `OmyacParams` not defined. (CxxTest discovers `test/sci/roger/*.h` automatically via `test/module.mk`; no registration needed.)

- [ ] **Step 3: Add `OmyacParams` to `roger_omyac.h`**

Insert after the `OmyacResult` struct (keep the existing 2-arg declaration, add the 3-arg one):

```cpp
// Tunable internals of the omyac pipeline. Every field defaults to the
// constant that was hard-coded before this struct existed, so a
// default-constructed OmyacParams produces BIT-IDENTICAL output (locked by
// test_omyac_params.h). Shipping callers never construct a non-default one;
// only the Roger Studio debug tool does.
struct OmyacParams {
	int minVotesLine = 1;              // enhance() vote floor, line mode
	int minVotesFillAll = 2;           // enhance() vote floor, fill/all modes
	int fillSuppressLineNeighbours = 3; // suppressFill when >= N line neighbours (9 = never)
	int endpointMaxSame = 2;           // isEndpoint = sameNeighbours < N
	bool isolatedPixelPass = true;     // enhance() isolated-pixel dilation pass
	bool tieBreakBlend = true;         // tie-break by BLEND_TABLE-nearest (false = first tied)
	bool diagFlankSuppress = true;     // fill-anchor diagonal-flanking suppression rule

	bool isDefault() const {
		const OmyacParams d;
		return minVotesLine == d.minVotesLine &&
		       minVotesFillAll == d.minVotesFillAll &&
		       fillSuppressLineNeighbours == d.fillSuppressLineNeighbours &&
		       endpointMaxSame == d.endpointMaxSame &&
		       isolatedPixelPass == d.isolatedPixelPass &&
		       tieBreakBlend == d.tieBreakBlend &&
		       diagFlankSuppress == d.diagFlankSuppress;
	}
};

// As below, with tunable internals. The 2-arg overload forwards OmyacParams()
// (all defaults) and is the signature every shipping call site uses.
OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes,
                        const OmyacParams &params);
```

- [ ] **Step 4: Thread params through `roger_omyac.cpp`**

Five surgical changes; everything else stays untouched:

1. `detectLineEndings` — add param, replace the constant `2`:

```cpp
static void detectLineEndings(const NativeRef &ref, Common::Array<Anchor> &anchors,
                              int endpointMaxSame) {
	...
			anchors[idx].isEndpoint = same < endpointMaxSame;
```

2. `connectFillAnchors` — add `bool diagFlankSuppress` param; gate the flanking-suppression `continue`:

```cpp
static void connectFillAnchors(const NativeRef &ref, Common::Array<Anchor> &anchors,
                               bool diagFlankSuppress) {
	...
				if (diagFlankSuppress && aIn && bIn) {
					if (isLineCmd(ref, ay * OMYAC_NATIVE_W + ax) &&
					    isLineCmd(ref, by * OMYAC_NATIVE_W + bx))
						continue;
				}
```

3. `enhance` — take `const OmyacParams &params`; four spots:

```cpp
static void enhance(Common::Array<byte> &buf, Common::Array<byte> &typeBuf, int mode,
                    const OmyacParams &params) {
	...
	int minVotes = mode == 1 ? params.minVotesLine : params.minVotesFillAll;
	...
			bool suppressFill = lineNeighbours >= params.fillSuppressLineNeighbours;
	...
			// tie-break block:
			if (ntied == 1) {
				resultColor = tied[0];
			} else if (ntied > 1) {
				if (!params.tieBreakBlend) {
					resultColor = tied[0];
				} else {
					// ... existing BLEND_TABLE averaging block, unchanged ...
				}
			}
	...
	// Isolated-pixel pass.
	if (!params.isolatedPixelPass)
		return;
	// ... existing second loop, unchanged ...
```

4. New 3-arg `renderOmyac`:

```cpp
OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes,
                        const OmyacParams &params) {
	Common::Array<Anchor> anchors;
	buildAnchors(ref, anchors);
	detectLineEndings(ref, anchors, params.endpointMaxSame);
	connectLineAnchors(ref, anchors);
	connectFillAnchors(ref, anchors, params.diagFlankSuppress);

	OmyacResult out;
	hybridRender(ref, anchors, out.pixels, out.cmdType);

	for (uint i = 0; i < passes.size(); i++)
		enhance(out.pixels, out.cmdType, passes[i], params);

	fillNullPixels(out.pixels, out.cmdType);

	return out;
}
```

5. Existing 2-arg entry becomes a forwarder (replace its old body):

```cpp
OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes) {
	return renderOmyac(ref, passes, OmyacParams());
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `.\build_tests.ps1`
Expected: ALL suites pass — the four new tests AND every pre-existing suite (especially `test_omyac.h`, `test_asset_cache.h`) untouched-green. If `test_params_change_output` or `test_endpoint_param_reaches_pipeline` fails with 0 diffs, strengthen `crossingLinesRef()` (add a third line, extend coordinates) until the pipeline reacts; never weaken the assertion.

- [ ] **Step 6: Full build check**

Run: `.\build_and_run.ps1 -NoLaunch`
Expected: build succeeds (no shipping call site changed — only the overload was added).

- [ ] **Step 7: Commit**

```powershell
git add engines/sci/roger/roger_omyac.h engines/sci/roger/roger_omyac.cpp test/sci/roger/test_omyac_params.h
git commit -m "Roger: OmyacParams threads tunable internals through renderOmyac (defaults bit-identical)"
```

---

### Task 2: Studio pure helpers — param registry, scaler variants, stamps

**Files:**
- Create: `engines/sci/roger/roger_studio_render.h`
- Create: `engines/sci/roger/roger_studio_render.cpp`
- Modify: `engines/sci/module.mk` (add `roger/roger_studio_render.o \` to the Roger block, alphabetical after `roger_selftest.o`)
- Create: `test/sci/roger/test_studio_render.h`

**Interfaces:**
- Consumes: `OmyacParams` (Task 1), `IndexImage`/`scale2x`/`scale3x`/`scale6x`/`scaleNearest` from `roger_scale.h`.
- Produces (Tasks 4–8 rely on these exact names):
  - `int omyacParamCount();`
  - `struct OmyacParamDesc { const char *name; int minV; int maxV; int step; bool isBool; };`
  - `OmyacParamDesc omyacParamDesc(int i);`
  - `int omyacParamGet(const OmyacParams &p, int i);`
  - `void omyacParamSet(OmyacParams &p, int i, int value);` (clamps to [minV,maxV])
  - `enum ScalerVariant { kScaler6x, kScaler2x3x, kScalerNearest6, kScaler2xN3, kScaler3xN2, kScaler4x, kScaler8x, kScalerCount };`
  - `const char *scalerVariantName(int v);`
  - `int scalerVariantFactor(int v);` (6,6,6,6,6,4,8)
  - `IndexImage applyScalerVariant(int v, const IndexImage &in);`
  - `Common::String omyacPassStamp(const Common::Array<int> &passes);` (letters `f`/`l`/`a`, `"none"` if empty)
  - `Common::String omyacParamStamp(const OmyacParams &p);` (`"default"` or short codes joined by `-`)
  - `Common::String studioExportName(const char *kind, int id, const Common::String &detail);` → `"studio-<kind><id, 3-digit zero-padded>-<detail>.png"`

- [ ] **Step 1: Write the failing tests**

Create `test/sci/roger/test_studio_render.h`:

```cpp
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_studio_render.h"
using namespace Sci::Roger;

class RogerStudioRenderTestSuite : public CxxTest::TestSuite {
public:
	void test_param_registry_roundtrip_and_clamp() {
		TS_ASSERT_EQUALS(omyacParamCount(), 7);
		OmyacParams p;
		// index 0 = minVotesLine (default 1)
		TS_ASSERT_EQUALS(omyacParamGet(p, 0), 1);
		omyacParamSet(p, 0, 5);
		TS_ASSERT_EQUALS(p.minVotesLine, 5);
		omyacParamSet(p, 0, 999); // clamps to maxV
		TS_ASSERT_EQUALS(omyacParamGet(p, 0), omyacParamDesc(0).maxV);
		omyacParamSet(p, 0, -5); // clamps to minV
		TS_ASSERT_EQUALS(omyacParamGet(p, 0), omyacParamDesc(0).minV);
		// bool param roundtrip (index 4 = isolatedPixelPass, default 1)
		TS_ASSERT(omyacParamDesc(4).isBool);
		TS_ASSERT_EQUALS(omyacParamGet(p, 4), 1);
		omyacParamSet(p, 4, 0);
		TS_ASSERT(!p.isolatedPixelPass);
	}

	void test_every_registry_index_maps_to_a_distinct_field() {
		// Setting each index to a non-default value must flip isDefault().
		for (int i = 0; i < omyacParamCount(); i++) {
			OmyacParams p;
			const OmyacParamDesc d = omyacParamDesc(i);
			const int def = omyacParamGet(p, i);
			const int other = (def == d.minV) ? d.maxV : d.minV;
			omyacParamSet(p, i, other);
			TS_ASSERT(!p.isDefault());
			TS_ASSERT_EQUALS(omyacParamGet(p, i), other);
		}
	}

	void test_scaler_variants_factor_and_dims() {
		IndexImage img;
		img.w = 4; img.h = 3;
		img.pixels.resize(12, 7);
		for (int v = 0; v < kScalerCount; v++) {
			const int f = scalerVariantFactor(v);
			IndexImage out = applyScalerVariant(v, img);
			TS_ASSERT_EQUALS(out.w, img.w * f);
			TS_ASSERT_EQUALS(out.h, img.h * f);
			TS_ASSERT_EQUALS(out.pixels.size(), (uint)(out.w * out.h));
			TS_ASSERT(scalerVariantName(v) != nullptr);
		}
		// A solid image stays solid through every variant.
		IndexImage out6 = applyScalerVariant(kScaler6x, img);
		for (uint i = 0; i < out6.pixels.size(); i++)
			TS_ASSERT_EQUALS(out6.pixels[i], (byte)7);
	}

	void test_pass_stamp() {
		Common::Array<int> passes;
		TS_ASSERT_EQUALS(omyacPassStamp(passes), Common::String("none"));
		passes.push_back(2); passes.push_back(2); passes.push_back(1); passes.push_back(0);
		TS_ASSERT_EQUALS(omyacPassStamp(passes), Common::String("ffla"));
	}

	void test_param_stamp() {
		OmyacParams p;
		TS_ASSERT_EQUALS(omyacParamStamp(p), Common::String("default"));
		p.minVotesLine = 3;
		p.isolatedPixelPass = false;
		Common::String s = omyacParamStamp(p);
		TS_ASSERT(s.contains("mvl3"));
		TS_ASSERT(s.contains("iso0"));
		TS_ASSERT(!s.contains("mvf")); // still-default fields omitted
	}

	void test_export_name() {
		TS_ASSERT_EQUALS(studioExportName("pic", 2, "default-ffflffaaaa"),
		                 Common::String("studio-pic002-default-ffflffaaaa.png"));
		TS_ASSERT_EQUALS(studioExportName("view", 300, "l2c0-scale6x"),
		                 Common::String("studio-view300-l2c0-scale6x.png"));
	}
};
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `.\build_tests.ps1`
Expected: compile FAILURE — `roger_studio_render.h` not found.

- [ ] **Step 3: Implement `roger_studio_render.{h,cpp}`**

`engines/sci/roger/roger_studio_render.h` (with the standard GPL header block copied from `roger_scale.h`):

```cpp
#ifndef SCI_ROGER_ROGER_STUDIO_RENDER_H
#define SCI_ROGER_ROGER_STUDIO_RENDER_H

// SCI-free pure helpers for the Roger Studio debug tool (see
// docs/superpowers/specs/2026-07-02-roger-studio-design.md). Everything here
// is unit-testable without a running engine.

#include "common/array.h"
#include "common/str.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_scale.h"

namespace Sci {
namespace Roger {

// ── OmyacParams registry: index-addressable fields for the studio HUD ────────
struct OmyacParamDesc {
	const char *name;
	int minV;
	int maxV;
	int step;
	bool isBool;
};

int omyacParamCount();
OmyacParamDesc omyacParamDesc(int i);
int omyacParamGet(const OmyacParams &p, int i);
void omyacParamSet(OmyacParams &p, int i, int value); // clamps to [minV, maxV]

// ── Scaler variants for view-cel comparison ──────────────────────────────────
// Factor-6 variants are eligible for Combined mode (must match the 6x plate);
// kScaler4x / kScaler8x are View-mode-only exploration.
enum ScalerVariant {
	kScaler6x = 0,   // scale3x(scale2x(in)) — the shipping path
	kScaler2x3x,     // scale2x(scale3x(in)) — order swapped
	kScalerNearest6, // blocky reference
	kScaler2xN3,     // scale2x then nearest x3
	kScaler3xN2,     // scale3x then nearest x2
	kScaler4x,       // scale2x(scale2x(in))
	kScaler8x,       // scale2x(scale2x(scale2x(in)))
	kScalerCount
};

const char *scalerVariantName(int v);
int scalerVariantFactor(int v);
IndexImage applyScalerVariant(int v, const IndexImage &in);

// ── Export filename stamps ───────────────────────────────────────────────────
Common::String omyacPassStamp(const Common::Array<int> &passes); // "ffla" / "none"
Common::String omyacParamStamp(const OmyacParams &p);            // "default" / "mvl3-iso0"
Common::String studioExportName(const char *kind, int id, const Common::String &detail);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_STUDIO_RENDER_H
```

`engines/sci/roger/roger_studio_render.cpp` (GPL header, then):

```cpp
#include "sci/roger/roger_studio_render.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

static const OmyacParamDesc PARAM_DESCS[] = {
	{ "minVotesLine",        1, 8, 1, false },
	{ "minVotesFillAll",     1, 8, 1, false },
	{ "fillSuppressLineNb",  1, 9, 1, false }, // 9 = never suppress (8 neighbours max)
	{ "endpointMaxSame",     0, 9, 1, false },
	{ "isolatedPixelPass",   0, 1, 1, true },
	{ "tieBreakBlend",       0, 1, 1, true },
	{ "diagFlankSuppress",   0, 1, 1, true },
};

int omyacParamCount() {
	return ARRAYSIZE(PARAM_DESCS);
}

OmyacParamDesc omyacParamDesc(int i) {
	return PARAM_DESCS[i];
}

int omyacParamGet(const OmyacParams &p, int i) {
	switch (i) {
	case 0: return p.minVotesLine;
	case 1: return p.minVotesFillAll;
	case 2: return p.fillSuppressLineNeighbours;
	case 3: return p.endpointMaxSame;
	case 4: return p.isolatedPixelPass ? 1 : 0;
	case 5: return p.tieBreakBlend ? 1 : 0;
	case 6: return p.diagFlankSuppress ? 1 : 0;
	default: return 0;
	}
}

void omyacParamSet(OmyacParams &p, int i, int value) {
	const OmyacParamDesc &d = PARAM_DESCS[i];
	value = CLIP(value, d.minV, d.maxV);
	switch (i) {
	case 0: p.minVotesLine = value; break;
	case 1: p.minVotesFillAll = value; break;
	case 2: p.fillSuppressLineNeighbours = value; break;
	case 3: p.endpointMaxSame = value; break;
	case 4: p.isolatedPixelPass = value != 0; break;
	case 5: p.tieBreakBlend = value != 0; break;
	case 6: p.diagFlankSuppress = value != 0; break;
	default: break;
	}
}

static const char *VARIANT_NAMES[kScalerCount] = {
	"scale6x (3x*2x)", "scale2x*3x", "nearest6", "scale2x+near3", "scale3x+near2",
	"scale4x", "scale8x"
};

const char *scalerVariantName(int v) {
	return (v >= 0 && v < kScalerCount) ? VARIANT_NAMES[v] : "?";
}

int scalerVariantFactor(int v) {
	switch (v) {
	case kScaler4x: return 4;
	case kScaler8x: return 8;
	default: return 6;
	}
}

IndexImage applyScalerVariant(int v, const IndexImage &in) {
	switch (v) {
	case kScaler6x:      return scale6x(in);
	case kScaler2x3x:    return scale2x(scale3x(in));
	case kScalerNearest6: return scaleNearest(in, 6);
	case kScaler2xN3:    return scaleNearest(scale2x(in), 3);
	case kScaler3xN2:    return scaleNearest(scale3x(in), 2);
	case kScaler4x:      return scale2x(scale2x(in));
	case kScaler8x:      return scale2x(scale2x(scale2x(in)));
	default:             return scale6x(in);
	}
}

Common::String omyacPassStamp(const Common::Array<int> &passes) {
	if (passes.empty())
		return "none";
	Common::String s;
	for (uint i = 0; i < passes.size(); i++)
		s += (passes[i] == 2) ? 'f' : (passes[i] == 1) ? 'l' : 'a';
	return s;
}

Common::String omyacParamStamp(const OmyacParams &p) {
	if (p.isDefault())
		return "default";
	const OmyacParams d;
	Common::String s;
	// Short codes, only for fields that differ from the default.
	if (p.minVotesLine != d.minVotesLine)
		s += Common::String::format("mvl%d-", p.minVotesLine);
	if (p.minVotesFillAll != d.minVotesFillAll)
		s += Common::String::format("mvf%d-", p.minVotesFillAll);
	if (p.fillSuppressLineNeighbours != d.fillSuppressLineNeighbours)
		s += Common::String::format("fsn%d-", p.fillSuppressLineNeighbours);
	if (p.endpointMaxSame != d.endpointMaxSame)
		s += Common::String::format("ems%d-", p.endpointMaxSame);
	if (p.isolatedPixelPass != d.isolatedPixelPass)
		s += Common::String::format("iso%d-", p.isolatedPixelPass ? 1 : 0);
	if (p.tieBreakBlend != d.tieBreakBlend)
		s += Common::String::format("tie%d-", p.tieBreakBlend ? 1 : 0);
	if (p.diagFlankSuppress != d.diagFlankSuppress)
		s += Common::String::format("dfs%d-", p.diagFlankSuppress ? 1 : 0);
	s.deleteLastChar(); // trailing '-'
	return s;
}

Common::String studioExportName(const char *kind, int id, const Common::String &detail) {
	return Common::String::format("studio-%s%03d-%s.png", kind, id, detail.c_str());
}

} // namespace Roger
} // namespace Sci
```

- [ ] **Step 4: Add to `engines/sci/module.mk`**

In the `# Roger art replacement` object list, after `roger/roger_selftest.o \` add:

```
	roger/roger_studio_render.o \
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `.\build_tests.ps1`
Expected: all suites pass, including the 6 new `RogerStudioRenderTestSuite` tests.

- [ ] **Step 6: Commit**

```powershell
git add engines/sci/roger/roger_studio_render.h engines/sci/roger/roger_studio_render.cpp engines/sci/module.mk test/sci/roger/test_studio_render.h
git commit -m "Roger studio: pure helpers - param registry, scaler variants, export stamps"
```

---

### Task 3: `RogerAssetGen` params support + cel-extraction refactor

**Files:**
- Modify: `engines/sci/roger/roger_asset_gen.h`
- Modify: `engines/sci/roger/roger_asset_gen.cpp`

**Interfaces:**
- Consumes: `OmyacParams` (Task 1).
- Produces (Tasks 5–8 rely on these exact names on `RogerAssetGen`):
  - `void setOmyacParams(const OmyacParams &p);` — stores params; if `!p.isDefault()` and mode isn't `kGenMemory`, force-switches to `kGenMemory` (cache-poisoning guard: params are not in the cache key).
  - `const OmyacParams &omyacParams() const;`
  - `bool nativeCelIndexImage(int viewId, int loopNo, int celNo, IndexImage &out, byte &outClearKey);` — the de-undithered native cel as an `IndexImage` (extracted from `generateViewCel`; returns false on any failure).
  - `Graphics::Surface *surfaceFromIndex(const IndexImage &img, byte clearKey);` — palette-mapped RGBA surface, clearKey pixels fully transparent (extracted `generateViewCel` tail; caller owns).

- [ ] **Step 1: Add declarations to `roger_asset_gen.h`**

Add `#include "sci/roger/roger_omyac.h"` (for `OmyacParams` — note `roger_omyac.h` includes `roger_pic_native.h`, which is SCI-free, so the header stays SCI-free). In the public section of `RogerAssetGen`, after `setMode`:

```cpp
	// Studio tuning support. Non-default params force kGenMemory: params are
	// NOT part of the cache key, so caching a tuned render would poison the
	// content-hash cache for normal launches.
	void setOmyacParams(const OmyacParams &p) {
		_omyacParams = p;
		if (!p.isDefault() && _mode != kGenMemory)
			_mode = kGenMemory;
	}
	const OmyacParams &omyacParams() const { return _omyacParams; }

	// De-undithered native cel pixels as an IndexImage (pre-upscale). False on
	// any failure. Extracted from generateViewCel so the studio can feed the
	// SAME source pixels through alternative scaler variants.
	bool nativeCelIndexImage(int viewId, int loopNo, int celNo,
	                         IndexImage &out, byte &outClearKey);

	// Palette-map an IndexImage to a new RGBA32 surface (caller owns; ->free()
	// then delete). clearKey pixels get alpha 0. Extracted generateViewCel tail.
	Graphics::Surface *surfaceFromIndex(const IndexImage &img, byte clearKey);
```

And a private member next to `_passes`:

```cpp
	OmyacParams _omyacParams; // default-constructed == today's constants
```

- [ ] **Step 2: Refactor `roger_asset_gen.cpp`**

1. In `generatePlateWithIndex` (~line 215) and `generatePriorityMap` (~line 471), change both `renderOmyac(ref, ...)` calls to pass params:

```cpp
	OmyacResult omyac = renderOmyac(ref, passes, _omyacParams);
```
```cpp
	OmyacResult omyac = renderOmyac(ref, _passes, _omyacParams);
```

2. Extract `nativeCelIndexImage` from `generateViewCel`: move the block from the `viewId < 0` guard through the de-undither loop (GfxCache/GfxView lookup, `getCelInfo`, `getBitmap`, `egaDeUndither` loop) into the new method. It must NOT contain the `kGenPrebuilt` early-return or any cache logic — it is a raw extraction usable in any mode. Keep the whole body inside `#ifdef ENABLE_SCI` (returning `false` otherwise), exactly like the code it came from:

```cpp
bool RogerAssetGen::nativeCelIndexImage(int viewId, int loopNo, int celNo,
                                        IndexImage &out, byte &outClearKey) {
	if (viewId < 0)
		return false;
#ifdef ENABLE_SCI
	if (!g_sci)
		return false;
	GfxCache *gfxCache = g_sci->_gfxCache;
	if (!gfxCache)
		return false;
	GfxView *view = gfxCache->getView((GuiResourceId)viewId);
	if (!view)
		return false;
	const CelInfo *celInfo = view->getCelInfo((int16)loopNo, (int16)celNo);
	if (!celInfo)
		return false;
	int w = celInfo->width;
	int h = celInfo->height;
	if (w <= 0 || h <= 0)
		return false;
	const SciSpan<const byte> &bmp = view->getBitmap((int16)loopNo, (int16)celNo);
	if (bmp.size() < (uint)(w * h))
		return false;
	out.w = w;
	out.h = h;
	out.pixels.resize((uint32)(w * h), celInfo->clearKey);
	outClearKey = celInfo->clearKey;
	for (int row = 0; row < h; ++row)
		for (int col = 0; col < w; ++col)
			out.pixels[(uint32)(row * w + col)] =
				egaDeUndither(bmp[row * w + col], col, row, celInfo->clearKey);
	return true;
#else
	return false;
#endif
}
```

3. Extract `surfaceFromIndex` from the `generateViewCel` tail (the `PixelFormat(4,8,8,8,8,24,16,8,0)` create + `_sysPalette`/`ARGBToColor` pack loop, clearKey → alpha 0) — copy that code verbatim into the new method, parameterized on `img`/`clearKey`.

4. Rewrite `generateViewCel` to call the two new methods, preserving its existing behavior EXACTLY: same `kGenPrebuilt` early-return, same cache-key computation, same `kGenCache` disk-read attempt, same `scale6x` call between extraction and packing, same timing capture around `scale6x`, same cache-write on `kGenCache`/`kGenAlways`.

- [ ] **Step 3: Run tests (regression gate)**

Run: `.\build_tests.ps1`
Expected: ALL suites pass — this is a pure refactor; `test_view_cache.h`, `test_asset_cache.h`, `test_omyac.h`, `test_omyac_params.h` all green with no test changes.

- [ ] **Step 4: Full build check**

Run: `.\build_and_run.ps1 -NoLaunch`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```powershell
git add engines/sci/roger/roger_asset_gen.h engines/sci/roger/roger_asset_gen.cpp
git commit -m "Roger: asset gen accepts OmyacParams (memory-forced when non-default); extract nativeCelIndexImage/surfaceFromIndex"
```

---

### Task 4: Studio scaffold — loop, HUD, hook, `-Studio` flag

**Files:**
- Create: `engines/sci/roger/roger_studio.h`
- Create: `engines/sci/roger/roger_studio.cpp`
- Modify: `engines/sci/module.mk` (add `roger/roger_studio.o \` after `roger_studio_render.o`)
- Modify: `engines/sci/sci.cpp:411-421` (env-gated hook)
- Modify: `build_and_run.ps1` (`-Studio` switch)
- Modify: `CLAUDE.md` (one paragraph)
- Create: `test/sci/roger/scripts/studio-smoke.rin`

**Interfaces:**
- Consumes: `RogerAssetGen` (Task 3), `roger_studio_render.h` helpers (Task 2), OSystem overlay API, `FontMan`.
- Produces: `class RogerStudio { RogerStudio(const Common::String &gameId); void run(); }` in `Sci::Roger`; later tasks add private mode methods to this class.

No unit tests — this task is engine-loop plumbing; verification is the autonomous `.rin` smoke run plus build/tests staying green.

- [ ] **Step 1: Create `roger_studio.h`**

GPL header, then:

```cpp
#ifndef SCI_ROGER_ROGER_STUDIO_H
#define SCI_ROGER_ROGER_STUDIO_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_studio_render.h"

namespace Graphics { class ManagedSurface; struct Surface; }
namespace Common { struct Event; }

namespace Sci {
namespace Roger {

// Debug-only interactive tuning environment (ROGER_STUDIO=1 /
// build_and_run.ps1 -Studio). Owns the overlay; never touches the disk cache
// (own RogerAssetGen in kGenMemory with an empty cache dir). See
// docs/superpowers/specs/2026-07-02-roger-studio-design.md.
class RogerStudio {
public:
	explicit RogerStudio(const Common::String &gameId);
	~RogerStudio();

	// Blocking loop; returns when the user quits (Esc / window close).
	void run();

private:
	enum Mode { kModePic, kModeView, kModeCombined };

	// Frame / input
	void handleEvent(const Common::Event &ev);
	void drawFrame();               // compose _display + push to overlay
	void drawHud();
	void markDirty() { _dirty = true; }

	// Rendering (filled in by Tasks 5-8)
	void rerender();                // dispatch by _mode; updates _current/_previous
	void setCurrent(Graphics::Surface *s, const Common::String &label);

	RogerAssetGen        _gen;      // kGenMemory, empty cache dir
	Graphics::ManagedSurface *_display = nullptr; // overlay-format compose target

	Mode  _mode = kModePic;
	bool  _dirty = true;
	bool  _quit = false;
	bool  _showKeymap = false;

	// Current / previous / baseline renders (RGBA fmt(4,8,8,8,8,24,16,8,0)).
	Graphics::Surface *_current = nullptr;
	Graphics::Surface *_previous = nullptr;
	Graphics::Surface *_baseline = nullptr;
	Common::String _currentLabel, _previousLabel, _baselineLabel;
	bool _showPrevious = false;     // A/B flip
	bool _split = false;            // split view vs pinned baseline

	// View transform
	int _zoomIdx = 2;               // index into ZOOM_STEPS; 2 == 1.0
	int _panX = 0, _panY = 0;
	bool _dragging = false;
	int _dragX = 0, _dragY = 0;

	// Tuning state
	OmyacParams        _params;
	Common::Array<int> _passes;     // starts = defaultPasses()
	int _paramCursor = 0;
	int _passCursor = 0;

	// Resource browsing
	Common::Array<int> _picIds;     int _picIdx = 0;
	Common::Array<int> _viewIds;    int _viewIdx = 0;
	int _loopNo = 0, _celNo = 0;
	int _variant = kScaler6x;       // view-mode highlight / combined-mode active
	int _spriteX = 160, _spriteY = 120; // combined-mode cel position (native coords)

	uint32 _lastRenderMs = 0;
	Common::String _status;         // one-line HUD status / error line
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_STUDIO_H
```

- [ ] **Step 2: Create `roger_studio.cpp` (scaffold scope)**

GPL header, then the scaffold: constructor, `run()`, event handling, frame/HUD draw. `rerender()` is a stub for now (fills `_status`), replaced in Task 5. Key implementation notes the code below already encodes — read them before typing:

- Resource enumeration needs the SCI engine → guard engine-touching includes/usage under `#ifdef ENABLE_SCI` exactly as `roger_asset_gen.cpp` does.
- The HUD is drawn small and blitted 2x via `ManagedSurface::blitFrom(src, srcRect, dstRect)` (which scales AND format-converts) so it's readable at 2862-wide overlays.
- Redraw only when `_dirty` (state changed) — the loop otherwise just polls + `delayMillis(10)`; `updateScreen()` only after a real push.

```cpp
#include "sci/roger/roger_studio.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "graphics/managed_surface.h"

#ifdef ENABLE_SCI
#include "sci/sci.h"
#include "sci/resource/resource.h"
#endif

namespace Sci {
namespace Roger {

static const float ZOOM_STEPS[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
static const int ZOOM_COUNT = ARRAYSIZE(ZOOM_STEPS);

RogerStudio::RogerStudio(const Common::String &gameId)
	: _gen(gameId, "", kGenMemory) {
	_passes = defaultPasses();
#ifdef ENABLE_SCI
	if (g_sci && g_sci->getResMan()) {
		ResourceManager *resMan = g_sci->getResMan();
		Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
		for (Common::List<ResourceId>::iterator it = pics.begin(); it != pics.end(); ++it)
			_picIds.push_back(it->getNumber());
		Common::sort(_picIds.begin(), _picIds.end());
		Common::List<ResourceId> views = resMan->listResources(kResourceTypeView);
		for (Common::List<ResourceId>::iterator it = views.begin(); it != views.end(); ++it)
			_viewIds.push_back(it->getNumber());
		Common::sort(_viewIds.begin(), _viewIds.end());
	}
#endif
}

RogerStudio::~RogerStudio() {
	if (_display) { _display->free(); delete _display; }
	if (_current) { _current->free(); delete _current; }
	if (_previous) { _previous->free(); delete _previous; }
	if (_baseline) { _baseline->free(); delete _baseline; }
}

void RogerStudio::setCurrent(Graphics::Surface *s, const Common::String &label) {
	if (_previous) { _previous->free(); delete _previous; }
	_previous = _current;
	_previousLabel = _currentLabel;
	_current = s;
	_currentLabel = label;
	_showPrevious = false;
	markDirty();
}

void RogerStudio::rerender() {
	// Task 5+ replaces this dispatch. Scaffold: status only.
	_status = Common::String::format("studio scaffold - %u pics, %u views discovered",
	                                 (unsigned)_picIds.size(), (unsigned)_viewIds.size());
	markDirty();
}

void RogerStudio::run() {
	g_system->showOverlay(false);
	_display = new Graphics::ManagedSurface(
		g_system->getOverlayWidth(), g_system->getOverlayHeight(),
		g_system->getOverlayFormat());
	rerender();
	Common::EventManager *em = g_system->getEventManager();
	while (!_quit && !em->shouldQuit()) {
		Common::Event ev;
		while (em->pollEvent(ev))
			handleEvent(ev);
		if (_dirty) {
			drawFrame();
			_dirty = false;
		}
		g_system->delayMillis(10);
	}
}

void RogerStudio::handleEvent(const Common::Event &ev) {
	switch (ev.type) {
	case Common::EVENT_QUIT:
	case Common::EVENT_RETURN_TO_LAUNCHER:
		_quit = true;
		return;
	case Common::EVENT_KEYDOWN:
		break; // handled below
	case Common::EVENT_WHEELUP:
		_zoomIdx = MIN(_zoomIdx + 1, ZOOM_COUNT - 1); markDirty(); return;
	case Common::EVENT_WHEELDOWN:
		_zoomIdx = MAX(_zoomIdx - 1, 0); markDirty(); return;
	case Common::EVENT_LBUTTONDOWN:
		_dragging = true; _dragX = ev.mouse.x; _dragY = ev.mouse.y; return;
	case Common::EVENT_LBUTTONUP:
		_dragging = false; return;
	case Common::EVENT_MOUSEMOVE:
		if (_dragging) {
			_panX += ev.mouse.x - _dragX;
			_panY += ev.mouse.y - _dragY;
			_dragX = ev.mouse.x; _dragY = ev.mouse.y;
			markDirty();
		}
		return;
	default:
		return;
	}

	// Global keys (mode-specific keys are added by Tasks 5-8).
	switch (ev.kbd.keycode) {
	case Common::KEYCODE_ESCAPE:
		_quit = true; break;
	case Common::KEYCODE_TAB:
		_mode = (Mode)((_mode + 1) % 3);
		rerender();
		break;
	case Common::KEYCODE_F1:
		_showKeymap = !_showKeymap; markDirty(); break;
	case Common::KEYCODE_0:
		_zoomIdx = 2; _panX = _panY = 0; markDirty(); break;
	case Common::KEYCODE_PLUS:
	case Common::KEYCODE_EQUALS:
		_zoomIdx = MIN(_zoomIdx + 1, ZOOM_COUNT - 1); markDirty(); break;
	case Common::KEYCODE_MINUS:
		_zoomIdx = MAX(_zoomIdx - 1, 0); markDirty(); break;
	default:
		break;
	}
}

void RogerStudio::drawFrame() {
	const Graphics::PixelFormat fmt = _display->format;
	_display->fillRect(Common::Rect(_display->w, _display->h),
	                   fmt.RGBToColor(24, 24, 24));

	const Graphics::Surface *shown = _showPrevious ? _previous : _current;
	const int hudH = 220; // bottom HUD strip (2x-scaled text lives here)
	const Common::Rect imageArea(0, 0, _display->w, _display->h - hudH);

	if (shown) {
		const float zoom = ZOOM_STEPS[_zoomIdx];
		// Destination rect of the (zoomed, panned) image; blitFrom scales + converts.
		if (!_split || !_baseline) {
			Common::Rect dst(_panX, _panY,
			                 _panX + (int)(shown->w * zoom), _panY + (int)(shown->h * zoom));
			dst.clip(imageArea);
			if (!dst.isEmpty()) {
				// Map the visible dst back to the src region.
				Common::Rect src((int)((dst.left - _panX) / zoom), (int)((dst.top - _panY) / zoom),
				                 (int)((dst.right - _panX) / zoom), (int)((dst.bottom - _panY) / zoom));
				src.clip(Common::Rect(shown->w, shown->h));
				if (!src.isEmpty())
					_display->blitFrom(*shown, src, dst);
			}
		} else {
			// Split: current left, baseline right, same zoom/pan each side.
			const int halfW = imageArea.width() / 2;
			const Common::Rect leftArea(0, 0, halfW, imageArea.bottom);
			const Common::Rect rightArea(halfW, 0, imageArea.right, imageArea.bottom);
			const Graphics::Surface *sides[2] = { _current, _baseline };
			const Common::Rect areas[2] = { leftArea, rightArea };
			for (int i = 0; i < 2; i++) {
				if (!sides[i])
					continue;
				Common::Rect dst(areas[i].left + _panX, areas[i].top + _panY,
				                 areas[i].left + _panX + (int)(sides[i]->w * zoom),
				                 areas[i].top + _panY + (int)(sides[i]->h * zoom));
				dst.clip(areas[i]);
				if (dst.isEmpty())
					continue;
				Common::Rect src((int)((dst.left - areas[i].left - _panX) / zoom),
				                 (int)((dst.top - areas[i].top - _panY) / zoom),
				                 (int)((dst.right - areas[i].left - _panX) / zoom),
				                 (int)((dst.bottom - areas[i].top - _panY) / zoom));
				src.clip(Common::Rect(sides[i]->w, sides[i]->h));
				if (!src.isEmpty())
					_display->blitFrom(*sides[i], src, dst);
			}
			_display->vLine(halfW, 0, imageArea.bottom, fmt.RGBToColor(255, 255, 255));
		}
	}

	drawHud();

	g_system->copyRectToOverlay(_display->getPixels(), _display->pitch,
	                            0, 0, _display->w, _display->h);
	g_system->updateScreen();
}

void RogerStudio::drawHud() {
	// Render HUD text small, then blit 2x so it is readable at hires overlays.
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (!font)
		return;
	const int hudH = 220;
	const int smallW = _display->w / 2, smallH = hudH / 2;
	Graphics::ManagedSurface small(smallW, smallH, _display->format);
	const uint32 bg = _display->format.RGBToColor(0, 0, 0);
	const uint32 fg = _display->format.RGBToColor(220, 220, 220);
	const uint32 hi = _display->format.RGBToColor(255, 255, 0);
	small.fillRect(Common::Rect(smallW, smallH), bg);

	static const char *MODE_NAMES[] = { "PIC", "VIEW", "COMBINED" };
	int y = 2;
	const int lh = font->getFontHeight() + 2;
	small.frameRect(Common::Rect(smallW, smallH), fg);
	font->drawString(&small, Common::String::format(
		"[%s]  %s  render %ums   Tab=mode A=flip P=pin S=split E=export F1=keys Esc=quit",
		MODE_NAMES[_mode], _currentLabel.c_str(), _lastRenderMs), 4, y, smallW - 8, fg);
	y += lh;
	if (!_status.empty()) {
		font->drawString(&small, _status, 4, y, smallW - 8, hi);
		y += lh;
	}
	// Tasks 5-8 append mode-specific lines here (param panel, pass strip, ids).

	if (_showKeymap) {
		// Full keymap block (kept current as later tasks add keys).
		static const char *KEYS[] = {
			"Global: Tab mode | A flip prev | P pin baseline | S split | E export PNG",
			"        +/-/wheel zoom | drag pan | 0 reset view | F1 this help | Esc quit",
			"Pic:    PgUp/PgDn pic | Up/Dn param | Lt/Rt adjust | [ ] pass cursor",
			"        f/l insert pass, Shift+A insert all | Del remove | Shift+9/0 reorder | R reset",
			"View:   PgUp/PgDn view | Home/End loop | ,/. cel",
			"Combined: V variant (6x only) | arrows move cel (Shift x10)",
		};
		for (uint i = 0; i < ARRAYSIZE(KEYS); i++) {
			font->drawString(&small, KEYS[i], 4, y, smallW - 8, fg);
			y += lh;
		}
	}

	const Common::Rect srcR(0, 0, smallW, smallH);
	const Common::Rect dstR(0, _display->h - hudH, _display->w, _display->h);
	_display->blitFrom(small.rawSurface(), srcR, dstR);
}

} // namespace Roger
} // namespace Sci
```

Note for the implementer: exact API spellings to verify while wiring this up (they exist in this codebase — copy the incantations from the named files rather than guessing): `ManagedSurface::rawSurface()` vs `surfacePtr()` (see usages under `engines/sci/roger/`), `Common::sort` (`common/algorithm.h`), `ResourceId::getNumber()` (see `file_roger_art_provider.cpp` precacheAll), `showOverlay(false)` (see `roger_compositor.cpp:678`). If `blitFrom(surface, srcRect, dstRect)` needs a `ManagedSurface` source wrapper for scaling, wrap with `Graphics::ManagedSurface(surface)` shallow constructor — check `graphics/managed_surface.h`.

- [ ] **Step 3: Hook into `sci.cpp`**

Add `#include "sci/roger/roger_studio.h"` next to the existing `roger_launcher.h` include (line 76). Immediately after `g_sciRogerProvider = new FileRogerArtProvider(...)` (line 411), before the `skipLauncher` block:

```cpp
	// Roger Studio: debug-only tuning environment (build_and_run.ps1 -Studio /
	// ROGER_STUDIO=1). Runs its own blocking loop at this seam — resources and
	// graphics are alive, no game scripts have run — then exits the process.
	if (getenv("ROGER_STUDIO") != nullptr) {
		Roger::RogerStudio studio(getGameIdStr());
		studio.run();
		return Common::kNoError;
	}
```

- [ ] **Step 4: Add `roger/roger_studio.o \` to `engines/sci/module.mk`** (right after `roger_studio_render.o \`).

- [ ] **Step 5: Add `-Studio` to `build_and_run.ps1`**

In `param(...)` after the `[switch]$Diag` entry:

```powershell
    [switch]$Studio,          # launch the Roger Studio tuning environment (ROGER_STUDIO=1,
                              # this launch only; see docs/superpowers/specs/2026-07-02-roger-studio-design.md)
```

Add `"ROGER_STUDIO"` to the stale-env `foreach` clear list. Add `$Studio` to the logfile condition (`if ($Script -or $Live -or $CycleLog -or $Diag -or $Studio)`). After the `if ($Diag)` block:

```powershell
if ($Studio) {
    $env:ROGER_STUDIO = "1"
    Write-Host "Roger Studio: tuning environment (this launch only)" -ForegroundColor Cyan
}
```

- [ ] **Step 6: Create the smoke script**

`test/sci/roger/scripts/studio-smoke.rin`:

```
# Roger Studio loop smoke: the studio polls backend events, so the .rin driver
# reaches it. Waits for the loop to draw, toggles the keymap, quits via Esc.
wait 3000
key f1
wait 500
key f1
wait 500
key esc
wait 500
quit
```

(Verify `f1` and `esc` are valid key tokens in `engines/sci/roger/roger_input.h`; if not, use the tokens that file defines for those keys.)

- [ ] **Step 7: Build + autonomous smoke run**

Run: `.\build_and_run.ps1 -NoLaunch`
Expected: build succeeds.

Run: `.\build_and_run.ps1 -Game qfg1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin`
Expected: process launches, blocks, and **exits on its own** (the Esc path works; if the process hangs, the loop or quit handling is broken — kill it and fix). `screenshots\roger-run.log` exists.

Run: `.\build_tests.ps1`
Expected: all suites still pass.

- [ ] **Step 8: Document in `CLAUDE.md`**

In the Windows quick-start section of `CLAUDE.md`, after the `-Mode` paragraph, add:

```
`-Studio` launches the **Roger Studio** tuning environment instead of a game
(`ROGER_STUDIO=1`, per-launch): an interactive omyac-parameter tuner + view-scaler
comparator on the raw overlay. Esc quits. Debug-only; it never touches the
generation disk cache. Spec: `docs/superpowers/specs/2026-07-02-roger-studio-design.md`.
```

- [ ] **Step 9: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp engines/sci/module.mk engines/sci/sci.cpp build_and_run.ps1 CLAUDE.md test/sci/roger/scripts/studio-smoke.rin
git commit -m "Roger studio: scaffold - overlay loop, HUD, ROGER_STUDIO hook, -Studio flag"
```

---

### Task 5: Pic mode — browse, render, param panel, pass editor

**Files:**
- Modify: `engines/sci/roger/roger_studio.h` (private method decls)
- Modify: `engines/sci/roger/roger_studio.cpp`

**Interfaces:**
- Consumes: `RogerAssetGen::setEnhancePasses/setOmyacParams/generatePlate` (Tasks 1/3), registry helpers (Task 2).
- Produces: `void renderPicMode();` and pic-mode key handling; `_current` holds the plate; `_currentLabel` = `"pic <id>  <paramStamp>  <passStamp>"`.

- [ ] **Step 1: Add `renderPicMode` and wire the dispatch**

Add to the class: `void renderPicMode();` and replace the `rerender()` stub dispatch:

```cpp
void RogerStudio::rerender() {
	switch (_mode) {
	case kModePic:      renderPicMode(); break;
	case kModeView:     /* Task 7 */ _status = "view mode: Task 7"; markDirty(); break;
	case kModeCombined: /* Task 8 */ _status = "combined mode: Task 8"; markDirty(); break;
	}
}

void RogerStudio::renderPicMode() {
	_status.clear();
	if (_picIds.empty()) {
		_status = "no pic resources found";
		markDirty();
		return;
	}
	const int picId = _picIds[_picIdx];
	_gen.setEnhancePasses(_passes);
	_gen.setOmyacParams(_params);
	uint32 ms = 0;
	Graphics::Surface *plate = _gen.generatePlate(picId, ms);
	_lastRenderMs = ms;
	if (!plate) {
		_status = Common::String::format("pic %d: generation FAILED (parse/render error)", picId);
		markDirty();
		return;
	}
	setCurrent(plate, Common::String::format("pic %d  %s  passes:%s", picId,
		omyacParamStamp(_params).c_str(), omyacPassStamp(_passes).c_str()));
}
```

- [ ] **Step 2: Pic-mode key handling**

In `handleEvent`'s KEYDOWN switch, add a mode-gated block (after the global keys; structure: `if (_mode == kModePic) { switch (ev.kbd.keycode) { ... } }`):

```cpp
	if (_mode == kModePic) {
		switch (ev.kbd.keycode) {
		case Common::KEYCODE_PAGEUP:
			_picIdx = (_picIdx + (int)_picIds.size() - 1) % (int)_picIds.size();
			rerender(); break;
		case Common::KEYCODE_PAGEDOWN:
			_picIdx = (_picIdx + 1) % (int)_picIds.size();
			rerender(); break;
		case Common::KEYCODE_UP:
			_paramCursor = (_paramCursor + omyacParamCount() - 1) % omyacParamCount();
			markDirty(); break;
		case Common::KEYCODE_DOWN:
			_paramCursor = (_paramCursor + 1) % omyacParamCount();
			markDirty(); break;
		case Common::KEYCODE_LEFT:
			omyacParamSet(_params, _paramCursor,
				omyacParamGet(_params, _paramCursor) - omyacParamDesc(_paramCursor).step);
			rerender(); break;
		case Common::KEYCODE_RIGHT:
			omyacParamSet(_params, _paramCursor,
				omyacParamGet(_params, _paramCursor) + omyacParamDesc(_paramCursor).step);
			rerender(); break;
		case Common::KEYCODE_LEFTBRACKET:
			if (_passCursor > 0) _passCursor--;
			markDirty(); break;
		case Common::KEYCODE_RIGHTBRACKET:
			if (_passCursor + 1 < (int)_passes.size()) _passCursor++;
			markDirty(); break;
		case Common::KEYCODE_f:
			_passes.insert_at(MIN(_passCursor, (int)_passes.size()), 2); rerender(); break;
		case Common::KEYCODE_l:
			_passes.insert_at(MIN(_passCursor, (int)_passes.size()), 1); rerender(); break;
		case Common::KEYCODE_a: // Shift+A inserts an 'all' pass; plain 'a' is the global A/B flip
			if (ev.kbd.flags & Common::KBD_SHIFT) {
				_passes.insert_at(MIN(_passCursor, (int)_passes.size()), 0);
				rerender();
			}
			break;
		case Common::KEYCODE_DELETE:
		case Common::KEYCODE_BACKSPACE:
			if (!_passes.empty() && _passCursor < (int)_passes.size()) {
				_passes.remove_at(_passCursor);
				if (_passCursor >= (int)_passes.size() && _passCursor > 0) _passCursor--;
				rerender();
			}
			break;
		case Common::KEYCODE_9: // Shift+9 = '(' moves pass left; plain 9 unused
			if ((ev.kbd.flags & Common::KBD_SHIFT) &&
			    _passCursor > 0 && _passCursor < (int)_passes.size()) {
				SWAP(_passes[_passCursor], _passes[_passCursor - 1]);
				_passCursor--; rerender();
			}
			break;
		case Common::KEYCODE_0: // Shift+0 = ')' moves pass right; plain 0 is global reset-view
			if ((ev.kbd.flags & Common::KBD_SHIFT) &&
			    _passCursor + 1 < (int)_passes.size()) {
				SWAP(_passes[_passCursor], _passes[_passCursor + 1]);
				_passCursor++; rerender();
			}
			break;
		case Common::KEYCODE_r: // reset params + passes to defaults
			_params = OmyacParams();
			_passes = defaultPasses();
			_passCursor = 0;
			rerender();
			break;
		default:
			break;
		}
	}
```

**Key-convention note (already encoded above):** plain `a` (A/B flip) and plain `0` (reset view) stay GLOBAL; pic mode claims only the Shift-modified variants (`Shift+A` insert `all` pass, `Shift+9`/`Shift+0` reorder). Order the dispatch so the global switch runs first and the pic-mode switch only consumes what the global layer ignored (Shift-modified keys, f/l, brackets, Del, arrows, R, PgUp/PgDn). Update the F1 keymap text in `drawHud()` to match exactly: `f/l insert, Shift+A insert all, Shift+9/0 reorder` — the keymap is the only UI documentation, keep it truthful.

- [ ] **Step 3: HUD panel for pic mode**

In `drawHud()` where the Task-4 comment says mode-specific lines go, add for `kModePic`:

```cpp
	if (_mode == kModePic) {
		for (int i = 0; i < omyacParamCount(); i++) {
			const OmyacParamDesc d = omyacParamDesc(i);
			Common::String line = Common::String::format("%c %-20s %d",
				i == _paramCursor ? '>' : ' ', d.name, omyacParamGet(_params, i));
			font->drawString(&small, line, 4, y, smallW - 8,
			                 i == _paramCursor ? hi : fg);
			y += lh;
		}
		// Pass strip with cursor: "passes: f f f [l] f f a a a a"
		Common::String strip = "passes: ";
		for (uint i = 0; i < _passes.size(); i++) {
			const char c = _passes[i] == 2 ? 'f' : _passes[i] == 1 ? 'l' : 'a';
			if ((int)i == _passCursor)
				strip += Common::String::format("[%c] ", c);
			else
				strip += Common::String::format("%c ", c);
		}
		if (_passes.empty())
			strip += "(none - wireframe)";
		font->drawString(&small, strip, 4, y, smallW - 8, fg);
		y += lh;
	}
```

Increase `hudH` (both in `drawFrame` and `drawHud`) from 220 to a value that fits `2 + omyacParamCount() + 2` lines at 2x — make `hudH` a single class constant `kHudH = 380` used in both places.

- [ ] **Step 4: Build + interactive verify (user checkpoint)**

Run: `.\build_and_run.ps1 -Game qfg1 -Studio`
Manual checklist (this is a USER-facing verification; report and ask the user to confirm):
- PgUp/PgDn cycles pics; each shows a distinct enhanced plate; render ms in HUD.
- Up/Down moves the `>` cursor; Left/Right changes the value and visibly re-renders.
- `[`/`]` moves the pass cursor; `f`/`l`/`Shift+A` insert; Del removes; `Shift+9`/`Shift+0` reorder; deleting ALL passes shows the raw wireframe.
- `R` restores defaults; the plate matches the first render again.
- Wheel zooms around, drag pans, `0` resets.

Run: `.\build_tests.ps1` — all green.

- [ ] **Step 5: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp
git commit -m "Roger studio: pic mode - browse, param panel, reorderable pass editor"
```

---

### Task 6: Judging tools — A/B flip, pin baseline, split, PNG export

**Files:**
- Modify: `engines/sci/roger/roger_studio.h`
- Modify: `engines/sci/roger/roger_studio.cpp`

**Interfaces:**
- Consumes: `studioExportName` (Task 2), `Image::writePNG` (`image/png.h`), `Common::DumpFile`, ConfMan `screenshotpath`.
- Produces: `void exportCurrent();` plus global keys A/P/S/E working in every mode. `_split`/`_showPrevious`/`_baseline` semantics as in the scaffold header.

- [ ] **Step 1: Implement the four global keys**

In `handleEvent`'s global KEYDOWN switch add:

```cpp
	case Common::KEYCODE_a:
		if (ev.kbd.flags & Common::KBD_SHIFT)
			break; // Shift+A = pic-mode insert-all-pass, handled there
		if (_previous) { _showPrevious = !_showPrevious; markDirty(); }
		else _status = "no previous render to flip to";
		break;
	case Common::KEYCODE_p:
		if (_current) {
			if (_baseline) { _baseline->free(); delete _baseline; }
			_baseline = new Graphics::Surface();
			_baseline->copyFrom(*_current);
			_baselineLabel = _currentLabel;
			_status = "baseline pinned: " + _baselineLabel;
			markDirty();
		}
		break;
	case Common::KEYCODE_s:
		if (_baseline) { _split = !_split; markDirty(); }
		else _status = "pin a baseline first (P)";
		break;
	case Common::KEYCODE_e:
		exportCurrent();
		break;
```

Make sure the ordering with pic-mode handling can't swallow these: process the GLOBAL switch first, and have pic-mode handling skip keycodes the global layer consumed (`a` without Shift, `p`, `s`, `e`, `0` without Shift).

- [ ] **Step 2: Implement `exportCurrent()`**

```cpp
void RogerStudio::exportCurrent() {
	if (!_current) {
		_status = "nothing to export";
		markDirty();
		return;
	}
	Common::String dir = ConfMan.hasKey("screenshotpath") ? ConfMan.get("screenshotpath") : ".";
	Common::String detail, kind; int id = 0;
	if (_mode == kModePic) {
		kind = "pic"; id = _picIds.empty() ? 0 : _picIds[_picIdx];
		detail = omyacParamStamp(_params) + "-" + omyacPassStamp(_passes);
	} else if (_mode == kModeView) {
		kind = "view"; id = _viewIds.empty() ? 0 : _viewIds[_viewIdx];
		detail = Common::String::format("l%dc%d-grid", _loopNo, _celNo);
	} else {
		kind = "combo"; id = _picIds.empty() ? 0 : _picIds[_picIdx];
		detail = Common::String::format("v%d-l%dc%d-%s",
			_viewIds.empty() ? 0 : _viewIds[_viewIdx], _loopNo, _celNo,
			scalerVariantName(_variant));
	}
	// Filenames must stay filesystem-safe: variant names contain '*'/'+'/'('.
	// Sanitize: keep [a-z0-9-], map everything else to '_'.
	Common::String safe;
	for (uint i = 0; i < detail.size(); i++) {
		const char c = detail[i];
		safe += (Common::isAlnum(c) || c == '-') ? c : '_';
	}
	Common::String name = studioExportName(kind.c_str(), id, safe);
	Common::DumpFile out;
	if (out.open(Common::Path(dir).appendComponent(name))) {
		Image::writePNG(out, *_current);
		out.close();
		_status = "exported " + name;
	} else {
		_status = "export FAILED: cannot open " + name;
	}
	markDirty();
}
```

Add includes: `image/png.h`, `common/file.h` (DumpFile), `common/path.h`.

- [ ] **Step 3: Extend the smoke script for export evidence**

Append to `test/sci/roger/scripts/studio-smoke.rin` before the `key esc` line:

```
key e
wait 800
```

- [ ] **Step 4: Build + autonomous verify**

Run: `.\build_and_run.ps1 -NoLaunch` then `.\build_and_run.ps1 -Game qfg1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin`
Expected: process exits on its own AND a `studio-pic*-default-ffflffaaaa.png` (default stamps) exists in the game's `screenshotpath` — check with `Get-ChildItem` and confirm the PNG is non-empty and opens (read it with the Read tool to eyeball the plate).

Run: `.\build_tests.ps1` — all green.

- [ ] **Step 5: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp test/sci/roger/scripts/studio-smoke.rin
git commit -m "Roger studio: judging tools - A/B flip, pin baseline, split view, PNG export"
```

---

### Task 7: View mode — cel browser + scaler-variant grid

**Files:**
- Modify: `engines/sci/roger/roger_studio.h`
- Modify: `engines/sci/roger/roger_studio.cpp`

**Interfaces:**
- Consumes: `RogerAssetGen::nativeCelIndexImage/surfaceFromIndex` (Task 3), `applyScalerVariant`/`scalerVariantName`/`scalerVariantFactor` (Task 2), `GfxView::getLoopCount()/getCelCount(loop)` via `g_sci->_gfxCache->getView(id)` (under `#ifdef ENABLE_SCI`).
- Produces: `void renderViewMode();` — `_current` becomes ONE composed grid surface (all variants side by side with a label strip under each), so A/B flip, split, zoom, and export work on it unchanged.

- [ ] **Step 1: Implement `renderViewMode()`**

```cpp
void RogerStudio::renderViewMode() {
	_status.clear();
	if (_viewIds.empty()) {
		_status = "no view resources found";
		markDirty();
		return;
	}
	const int viewId = _viewIds[_viewIdx];
	IndexImage cel;
	byte clearKey = 0;
	uint32 t0 = g_system->getMillis();
	if (!_gen.nativeCelIndexImage(viewId, _loopNo, _celNo, cel, clearKey)) {
		_status = Common::String::format("view %d l%d c%d: cel extraction FAILED", viewId, _loopNo, _celNo);
		markDirty();
		return;
	}

	// Render every variant, measure the grid.
	Common::Array<Graphics::Surface *> tiles;
	int tileW = 0, tileH = 0;
	for (int v = 0; v < kScalerCount; v++) {
		IndexImage scaled = applyScalerVariant(v, cel);
		Graphics::Surface *s = _gen.surfaceFromIndex(scaled, clearKey);
		tiles.push_back(s); // may be null; grid slot shows label only
		if (s) { tileW = MAX(tileW, (int)s->w); tileH = MAX(tileH, (int)s->h); }
	}
	_lastRenderMs = g_system->getMillis() - t0;
	if (tileW == 0) {
		_status = "all variants failed to render";
		markDirty();
		return;
	}

	// Compose: N columns of (tile + label strip), checkerboard behind alpha.
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	const int labelH = font ? font->getFontHeight() + 4 : 16;
	const int pad = 8;
	const Graphics::PixelFormat fmt(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::ManagedSurface grid((tileW + pad) * kScalerCount + pad, tileH + labelH + 2 * pad, fmt);
	grid.fillRect(Common::Rect(grid.w, grid.h), fmt.RGBToColor(40, 40, 40));
	for (int v = 0; v < kScalerCount; v++) {
		const int x0 = pad + v * (tileW + pad);
		// Checkerboard so transparency is visible.
		for (int cy = 0; cy < tileH; cy += 8)
			for (int cx = 0; cx < tileW; cx += 8)
				if (((cx / 8) ^ (cy / 8)) & 1)
					grid.fillRect(Common::Rect(x0 + cx, pad + cy,
						MIN(x0 + cx + 8, x0 + tileW), MIN(pad + cy + 8, pad + tileH)),
						fmt.RGBToColor(56, 56, 56));
		if (tiles[v])
			grid.blitFrom(*tiles[v], Common::Point(x0, pad));
		if (font)
			font->drawString(&grid, scalerVariantName(v), x0, pad + tileH + 2, tileW + pad,
			                 fmt.RGBToColor(255, 255, 255));
	}
	for (uint i = 0; i < tiles.size(); i++)
		if (tiles[i]) { tiles[i]->free(); delete tiles[i]; }

	// Hand the composed grid to setCurrent as a bare Surface copy.
	Graphics::Surface *composed = new Graphics::Surface();
	composed->copyFrom(*grid.surfacePtr());
	setCurrent(composed, Common::String::format("view %d loop %d cel %d - variant grid",
	                                            viewId, _loopNo, _celNo));
}
```

(`surfacePtr()` vs `rawSurface()`: use whichever `graphics/managed_surface.h` exposes for a const `Surface&`/`Surface*` — match Task 4's resolution.)

- [ ] **Step 2: View-mode keys + loop/cel clamping**

In `handleEvent`, mode-gated for `kModeView` (and reused by Task 8 for cel selection):

```cpp
	if (_mode == kModeView || _mode == kModeCombined) {
		switch (ev.kbd.keycode) {
		case Common::KEYCODE_PAGEUP:
			if (_mode == kModeView) {
				_viewIdx = (_viewIdx + (int)_viewIds.size() - 1) % (int)_viewIds.size();
				_loopNo = _celNo = 0;
				rerender();
			}
			break;
		case Common::KEYCODE_PAGEDOWN:
			if (_mode == kModeView) {
				_viewIdx = (_viewIdx + 1) % (int)_viewIds.size();
				_loopNo = _celNo = 0;
				rerender();
			}
			break;
		case Common::KEYCODE_HOME: _loopNo = MAX(0, _loopNo - 1); _celNo = 0; rerender(); break;
		case Common::KEYCODE_END:  _loopNo = _loopNo + 1; _celNo = 0; rerender(); break; // clamped in render
		case Common::KEYCODE_COMMA:  _celNo = MAX(0, _celNo - 1); rerender(); break;
		case Common::KEYCODE_PERIOD: _celNo = _celNo + 1; rerender(); break; // clamped in render
		default: break;
		}
	}
```

Clamp `_loopNo`/`_celNo` at the top of `renderViewMode` (and Task 8's render) using the real counts, under `#ifdef ENABLE_SCI`:

```cpp
#ifdef ENABLE_SCI
	if (g_sci && g_sci->_gfxCache) {
		GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
		if (view) {
			_loopNo = CLIP<int>(_loopNo, 0, MAX(0, (int)view->getLoopCount() - 1));
			_celNo = CLIP<int>(_celNo, 0, MAX(0, (int)view->getCelCount((int16)_loopNo) - 1));
		}
	}
#endif
```

Also add the HUD line for view mode in `drawHud()`:

```cpp
	if (_mode == kModeView)
		font->drawString(&small, Common::String::format(
			"view %d (%d/%u)  loop %d  cel %d   PgUp/PgDn view  Home/End loop  ,/. cel",
			_viewIds.empty() ? -1 : _viewIds[_viewIdx], _viewIdx + 1,
			(unsigned)_viewIds.size(), _loopNo, _celNo), 4, y, smallW - 8, fg);
```

- [ ] **Step 3: Build + interactive verify (user checkpoint)**

Run: `.\build_and_run.ps1 -Game qfg1 -Studio`, Tab to VIEW.
Checklist: grid shows 7 labeled tiles for the same cel; the scale6x tile matches in-game sprite quality; nearest6 is visibly blocky; browsing views/loops/cels updates the grid; zoom + export (`studio-view*-l*c*-grid.png`) work.

Run: `.\build_tests.ps1` — all green.

- [ ] **Step 4: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp
git commit -m "Roger studio: view mode - cel browser with scaler-variant comparison grid"
```

---

### Task 8: Combined mode — cel on plate

**Files:**
- Modify: `engines/sci/roger/roger_studio.h`
- Modify: `engines/sci/roger/roger_studio.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces: `void renderCombinedMode();` — `_current` = plate (current pic + current params/passes) with the selected cel composited at `(_spriteX,_spriteY)*6` using the selected **factor-6** variant.

- [ ] **Step 1: Implement `renderCombinedMode()`**

```cpp
void RogerStudio::renderCombinedMode() {
	_status.clear();
	if (_picIds.empty() || _viewIds.empty()) {
		_status = "need at least one pic and one view";
		markDirty();
		return;
	}
	// Combined only accepts factor-6 variants (the plate is 6x).
	while (scalerVariantFactor(_variant) != 6)
		_variant = (_variant + 1) % kScalerCount;

	const int picId = _picIds[_picIdx];
	_gen.setEnhancePasses(_passes);
	_gen.setOmyacParams(_params);
	uint32 ms = 0;
	Graphics::Surface *plate = _gen.generatePlate(picId, ms);
	if (!plate) {
		_status = Common::String::format("pic %d: generation FAILED", picId);
		markDirty();
		return;
	}

	const int viewId = _viewIds[_viewIdx];
	IndexImage cel;
	byte clearKey = 0;
	Graphics::Surface *celSurf = nullptr;
	if (_gen.nativeCelIndexImage(viewId, _loopNo, _celNo, cel, clearKey)) {
		IndexImage scaled = applyScalerVariant(_variant, cel);
		celSurf = _gen.surfaceFromIndex(scaled, clearKey);
	}
	_lastRenderMs = ms;

	Graphics::ManagedSurface composed(plate->w, plate->h, plate->format);
	composed.blitFrom(*plate);
	plate->free(); delete plate;
	if (celSurf) {
		// Cel origin: SCI cels anchor at their bottom-centre on the y position;
		// for judging purposes top-left placement at (x,y)*6 is sufficient and
		// simpler — the point is edge quality against the plate, not game-exact
		// anchoring.
		composed.blitFrom(*celSurf, Common::Point(_spriteX * 6, _spriteY * 6));
		celSurf->free(); delete celSurf;
	} else {
		_status = "cel render failed; showing plate only";
	}

	Graphics::Surface *out = new Graphics::Surface();
	out->copyFrom(*composed.surfacePtr());
	setCurrent(out, Common::String::format("pic %d + view %d l%d c%d @(%d,%d) %s",
		picId, viewId, _loopNo, _celNo, _spriteX, _spriteY, scalerVariantName(_variant)));
}
```

`blitFrom` with an alpha source must alpha-blend (clearKey pixels are a=0 from `surfaceFromIndex`). If plain `blitFrom` ignores alpha in this ScummVM version, use `transBlitFrom`/`blendBlitFrom` per `graphics/managed_surface.h` — the compositor's sprite path in `roger_compositor.cpp` shows the working incantation; copy it.

- [ ] **Step 2: Combined-mode keys**

Add to `handleEvent` (kModeCombined only): arrows move the cel (`Shift` = ×10 — note arrows are param keys ONLY in pic mode, so no conflict), `V` cycles to the next factor-6 variant, PgUp/PgDn browse pics (reuse pic browsing but `rerender()` into combined):

```cpp
	if (_mode == kModeCombined) {
		const int d = (ev.kbd.flags & Common::KBD_SHIFT) ? 10 : 1;
		switch (ev.kbd.keycode) {
		case Common::KEYCODE_LEFT:  _spriteX = MAX(0, _spriteX - d); rerender(); break;
		case Common::KEYCODE_RIGHT: _spriteX = MIN(319, _spriteX + d); rerender(); break;
		case Common::KEYCODE_UP:    _spriteY = MAX(0, _spriteY - d); rerender(); break;
		case Common::KEYCODE_DOWN:  _spriteY = MIN(189, _spriteY + d); rerender(); break;
		case Common::KEYCODE_v:
			do { _variant = (_variant + 1) % kScalerCount; }
			while (scalerVariantFactor(_variant) != 6);
			rerender(); break;
		case Common::KEYCODE_PAGEUP:
			_picIdx = (_picIdx + (int)_picIds.size() - 1) % (int)_picIds.size(); rerender(); break;
		case Common::KEYCODE_PAGEDOWN:
			_picIdx = (_picIdx + 1) % (int)_picIds.size(); rerender(); break;
		default: break;
		}
	}
```

(Remove the PgUp/PgDn cases from the Task 7 shared block for combined, since combined now handles them itself; Home/End/,/. remain shared for loop/cel selection.)

HUD line for combined mode:

```cpp
	if (_mode == kModeCombined)
		font->drawString(&small, Common::String::format(
			"cel @(%d,%d) variant %s   arrows move (Shift x10)  V variant  PgUp/PgDn pic",
			_spriteX, _spriteY, scalerVariantName(_variant)), 4, y, smallW - 8, fg);
```

- [ ] **Step 3: Wire dispatch + build + interactive verify (user checkpoint)**

Replace the Task-5 stubs in `rerender()` with the real calls (`renderViewMode()`, `renderCombinedMode()`).

Run: `.\build_and_run.ps1 -Game qfg1 -Studio`, Tab to COMBINED.
Checklist: cel sits on the enhanced plate; arrows move it (Shift jumps); V cycles the five 6x variants with visible quality differences at sprite edges; A/B flip compares two variants at the same position; export produces `studio-combo*-...png`.

Run: `.\build_tests.ps1` — all green. Final regression: `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1` (NO -Studio) must boot the game normally — the studio hook must be inert without the env var.

- [ ] **Step 4: Commit**

```powershell
git add engines/sci/roger/roger_studio.h engines/sci/roger/roger_studio.cpp
git commit -m "Roger studio: combined mode - view cel composited on enhanced plate"
```

---

## Verification (whole feature)

1. `.\build_tests.ps1` — every suite green (incl. `test_omyac_params.h`, `test_studio_render.h`).
2. `.\build_and_run.ps1 -Game qfg1 -Studio -Script test\sci\roger\scripts\studio-smoke.rin` — exits on its own; export PNG exists.
3. `.\build_and_run.ps1 -Game qfg1 -SaveSlot 1` — normal launch unaffected (hook inert, plates come from the same cache files: no cache key changed, no `kTransformVersion` bump).
4. User drive-through of the Task 5/7/8 checklists.
