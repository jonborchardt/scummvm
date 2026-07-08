/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCI_ROGER_UTILS_EYETEST_ROGER_EYETEST_SEARCH_H
#define SCI_ROGER_UTILS_EYETEST_ROGER_EYETEST_SEARCH_H

// EYE EXAM (kept dev utility, quarantined 2026-07-07): pure search logic for
// the interactive OMYAC pass-sequence tuner in roger_eyetest.{h,cpp}. SCI-free
// so it unit-tests without an engine (same isolation as roger_studio_render.h).
//
// QUARANTINE CONTRACT — nothing in the engine may depend on utils/eyetest/.
// The only permitted references are: the env-gated ROGER_EYETEST hook in
// sci.cpp, engines/sci/module.mk, build_tests.ps1's test registration, and the
// build_and_run.ps1 -EyeTest switch. Production code must never include these
// headers; this module may only consume stable roger seams (RogerAssetGen,
// png_loader) — never provider/compositor internals.

#include "common/array.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

enum {
	kEyeSeqLen        = 10,  // pass positions per sequence
	kEyePop           = 6,   // candidates alive per generation (incl. elites)
	kEyeElite         = 2,   // top pool entries carried forward (never re-rendered)
	kEyeMaxGens       = 20,
	kEyeMaxCandidates = 150,
	kEyeSameWindow    = 12   // convergence: >= 50% Same over the last 12 choices
};

// Pass values are renderOmyac mode ints, the same mapping parsePassString
// (roger_passes.h) uses: f(ill) = 2, l(ine) = 1, a(ll) = 0 — feed straight
// into setEnhancePasses().
typedef Common::Array<int> EyeSeq;

EyeSeq eyeBaseSeq();                            // f f f l f f a a a a
char eyePassChar(int v);                        // 2->'f', 1->'l', 0->'a'
Common::String eyeSeqCompact(const EyeSeq &s);  // "ffflffaaaa"

// Tiny deterministic xorshift32. Deliberately NOT Common::RandomSource: this is
// throwaway tool code, and a plain struct keeps the pure tests seedable without
// the event-recorder coupling.
struct EyeRng {
	uint32 s;
	explicit EyeRng(uint32 seed) : s(seed ? seed : 0x9e3779b9u) {}
	uint32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	uint32 below(uint32 n) { return n ? next() % n : 0; }
};

EyeSeq eyeRandomSeq(EyeRng &rng);

// Parse a compact sequence string ("ffflffaaaa") into pass ints. Accepts only
// exactly kEyeSeqLen chars of f/l/a; false (and out cleared) on anything else.
bool eyeParseCompact(const Common::String &line, EyeSeq &out);

// Mutate 1-5 positions (60/25/10/5% for 1/2/3/4-5). Every selected position
// changes to one of the OTHER two pass types. outDesc: "mut_pos04_f_to_a"
// (one _posNN_x_to_y group per changed position, positions ascending).
// posWeights (optional, length kEyeSeqLen, every entry >= 1) biases WHICH
// positions get mutated — e.g. tail-heavy weights concentrate the search on
// positions 7-9 while every position stays reachable. nullptr = uniform.
EyeSeq eyeMutate(const EyeSeq &src, EyeRng &rng, Common::String &outDesc,
                 const int *posWeights = nullptr);

// Uniform per-position crossover: out[i] is a[i] or b[i], 50/50.
EyeSeq eyeCrossover(const EyeSeq &a, const EyeSeq &b, EyeRng &rng);

struct EyeCandidate {
	int gen = 0, idx = 0;
	EyeSeq seq;
	Common::String source;                  // "base" / "mut_..." / "cross" / "rand"
	Common::Array<Common::String> parents;  // candidate ids
	Common::String file;                    // output PNG name (set at render time)
	int wins = 0, ties = 0, losses = 0;
	Common::String id() const { return Common::String::format("gen%03d_cand%03d", gen, idx); }
	int score() const { return wins - losses; }
};

enum EyeChoice { kEyeChoiceA = 0, kEyeChoiceB, kEyeChoiceSame, kEyeChoiceSkip };

struct EyeComparison {
	Common::String aId, bId;   // left (A) / right (B) as shown on screen
	int choice = kEyeChoiceSkip;
	uint32 millis = 0;         // g_system->getMillis() at choice time
	Common::String championAfter;
};

// True when >= window choices exist and Same is >= half of the last `window`.
bool eyeConverged(const Common::Array<int> &choices, int window);

// Indices into `all`, best score() first; ties broken newest-first (higher
// array index). At most maxPool entries.
Common::Array<int> eyeRankPool(const Common::Array<EyeCandidate> &all, int maxPool);

// Produce `count` NEW offspring for generation `gen` from the ranked pool
// (indices into `all`): 75% mutation of a random pool parent, 15% uniform
// crossover of two distinct pool parents, 10% random restart. Compact strings
// already in `seen` are re-rolled (20 tries, then random-until-unseen, then
// accepted as-is). Appends each offspring's compact string to `seen`.
// Offspring idx runs 0..count-1; elitism is implicit (the pool's top entries
// stay alive as comparison anchor + breeding parents, never re-rendered).
Common::Array<EyeCandidate> eyeBreed(const Common::Array<EyeCandidate> &all,
                                     const Common::Array<int> &pool,
                                     int gen, int count, EyeRng &rng,
                                     Common::Array<Common::String> &seen,
                                     const int *posWeights = nullptr);

// "n002_gen001_cand003_mut_pos04_f_to_a__ffflaaaaaa.png". Source strings are
// [a-z0-9_] by construction, so no sanitizing is needed.
Common::String eyeCandidateFileName(int picId, const EyeCandidate &c);

// One manifest object / one comparison record (single line, valid JSON).
Common::String eyeCandidateJson(int picId, const EyeCandidate &c);
Common::String eyeComparisonJson(const EyeComparison &c);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_UTILS_EYETEST_ROGER_EYETEST_SEARCH_H
