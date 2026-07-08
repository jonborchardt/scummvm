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

#include "sci/roger/utils/eyetest/roger_eyetest_search.h"

#include "sci/roger/roger_passes.h"

namespace Sci {
namespace Roger {

// The search starts from the SHIPPING default (kDefaultPassString, the single
// swap point in roger_passes) so the tool always measures against what the
// game actually renders. The GA machinery assumes kEyeSeqLen positions, so a
// default of any other length falls back to the classic 10-char sequence.
EyeSeq eyeBaseSeq() {
	EyeSeq s = parsePassString(kDefaultPassString);
	if ((int)s.size() != kEyeSeqLen) {
		static const int classic[kEyeSeqLen] = {2, 2, 2, 1, 2, 2, 0, 0, 0, 0}; // f f f l f f a a a a
		s.clear();
		for (int i = 0; i < kEyeSeqLen; i++)
			s.push_back(classic[i]);
	}
	return s;
}

char eyePassChar(int v) {
	switch (v) {
	case 2:  return 'f';
	case 1:  return 'l';
	default: return 'a';
	}
}

Common::String eyeSeqCompact(const EyeSeq &s) {
	Common::String out;
	for (uint i = 0; i < s.size(); i++)
		out += eyePassChar(s[i]);
	return out;
}

EyeSeq eyeRandomSeq(EyeRng &rng) {
	EyeSeq s;
	for (int i = 0; i < kEyeSeqLen; i++)
		s.push_back((int)rng.below(3)); // 0/1/2 are exactly the three pass values
	return s;
}

bool eyeParseCompact(const Common::String &line, EyeSeq &out) {
	out.clear();
	if (line.size() != (uint)kEyeSeqLen)
		return false;
	for (uint i = 0; i < line.size(); i++) {
		switch (line[i]) {
		case 'f': out.push_back(2); break;
		case 'l': out.push_back(1); break;
		case 'a': out.push_back(0); break;
		default:
			out.clear();
			return false;
		}
	}
	return true;
}

EyeSeq eyeMutate(const EyeSeq &src, EyeRng &rng, Common::String &outDesc,
                 const int *posWeights) {
	// Position-count distribution: 60/25/10/5% for 1/2/3/4-5.
	const uint32 roll = rng.below(100);
	int count;
	if (roll < 60)
		count = 1;
	else if (roll < 85)
		count = 2;
	else if (roll < 95)
		count = 3;
	else
		count = 4 + (int)rng.below(2);

	bool picked[kEyeSeqLen] = {};
	for (int n = 0; n < count; n++) {
		int p = -1;
		if (posWeights) {
			// Weighted pick over the not-yet-picked positions. Weights must be
			// >= 1 (documented); a zero total falls through to the uniform pick.
			uint32 total = 0;
			for (int i = 0; i < kEyeSeqLen; i++)
				if (!picked[i])
					total += (uint32)posWeights[i];
			if (total > 0) {
				uint32 w = rng.below(total);
				for (int i = 0; i < kEyeSeqLen; i++) {
					if (picked[i])
						continue;
					if (w < (uint32)posWeights[i]) { p = i; break; }
					w -= (uint32)posWeights[i];
				}
			}
		}
		if (p < 0) {
			do {
				p = (int)rng.below(kEyeSeqLen);
			} while (picked[p]);
		}
		picked[p] = true;
	}

	EyeSeq out = src;
	outDesc = "mut";
	for (int p = 0; p < kEyeSeqLen; p++) {
		if (!picked[p])
			continue;
		// Always switch to one of the OTHER two pass values.
		int alt[2];
		int na = 0;
		for (int v = 0; v < 3; v++)
			if (v != src[p])
				alt[na++] = v;
		out[p] = alt[rng.below(2)];
		outDesc += Common::String::format("_pos%02d_%c_to_%c",
			p, eyePassChar(src[p]), eyePassChar(out[p]));
	}
	return out;
}

EyeSeq eyeCrossover(const EyeSeq &a, const EyeSeq &b, EyeRng &rng) {
	EyeSeq out;
	for (int i = 0; i < kEyeSeqLen; i++)
		out.push_back(rng.below(2) ? b[i] : a[i]);
	return out;
}

bool eyeConverged(const Common::Array<int> &choices, int window) {
	if ((int)choices.size() < window)
		return false;
	int same = 0;
	for (int i = (int)choices.size() - window; i < (int)choices.size(); i++)
		if (choices[i] == kEyeChoiceSame)
			same++;
	return same * 2 >= window;
}

Common::Array<int> eyeRankPool(const Common::Array<EyeCandidate> &all, int maxPool) {
	Common::Array<int> idx;
	for (uint i = 0; i < all.size(); i++)
		idx.push_back((int)i);
	// Insertion sort (n <= kEyeMaxCandidates): score desc, ties newest-first.
	for (uint i = 1; i < idx.size(); i++) {
		const int v = idx[i];
		int j = (int)i - 1;
		while (j >= 0 && (all[idx[j]].score() < all[v].score() ||
		       (all[idx[j]].score() == all[v].score() && idx[j] < v))) {
			idx[j + 1] = idx[j];
			j--;
		}
		idx[j + 1] = v;
	}
	while ((int)idx.size() > maxPool)
		idx.pop_back();
	return idx;
}

static bool eyeSeen(const Common::Array<Common::String> &seen, const Common::String &compact) {
	for (uint i = 0; i < seen.size(); i++)
		if (seen[i] == compact)
			return true;
	return false;
}

Common::Array<EyeCandidate> eyeBreed(const Common::Array<EyeCandidate> &all,
                                     const Common::Array<int> &pool,
                                     int gen, int count, EyeRng &rng,
                                     Common::Array<Common::String> &seen,
                                     const int *posWeights) {
	Common::Array<EyeCandidate> out;
	for (int k = 0; k < count; k++) {
		EyeCandidate c;
		c.gen = gen;
		c.idx = k;
		bool fresh = false;
		for (int attempt = 0; attempt < 20 && !fresh; attempt++) {
			c.parents.clear();
			const uint32 roll = rng.below(100);
			if (pool.empty() || roll >= 90) {
				// Random restart: rare, keeps diversity / escapes local maxima.
				c.seq = eyeRandomSeq(rng);
				c.source = "rand";
			} else if (roll < 75 || pool.size() < 2) {
				const EyeCandidate &p = all[pool[rng.below(pool.size())]];
				c.seq = eyeMutate(p.seq, rng, c.source, posWeights);
				c.parents.push_back(p.id());
			} else {
				uint32 i0 = rng.below(pool.size()), i1;
				do {
					i1 = rng.below(pool.size());
				} while (i1 == i0);
				const EyeCandidate &pa = all[pool[i0]];
				const EyeCandidate &pb = all[pool[i1]];
				c.seq = eyeCrossover(pa.seq, pb.seq, rng);
				c.source = "cross";
				c.parents.push_back(pa.id());
				c.parents.push_back(pb.id());
			}
			fresh = !eyeSeen(seen, eyeSeqCompact(c.seq));
		}
		// Last resort: random until unseen (bounded), then accept a duplicate.
		for (int attempt = 0; attempt < 100 && !fresh; attempt++) {
			c.seq = eyeRandomSeq(rng);
			c.source = "rand";
			c.parents.clear();
			fresh = !eyeSeen(seen, eyeSeqCompact(c.seq));
		}
		seen.push_back(eyeSeqCompact(c.seq));
		out.push_back(c);
	}
	return out;
}

Common::String eyeCandidateFileName(int picId, const EyeCandidate &c) {
	return Common::String::format("n%03d_gen%03d_cand%03d_%s__%s.png",
	                              picId, c.gen, c.idx, c.source.c_str(),
	                              eyeSeqCompact(c.seq).c_str());
}

Common::String eyeCandidateJson(int picId, const EyeCandidate &c) {
	Common::String seqArr;
	for (uint i = 0; i < c.seq.size(); i++)
		seqArr += Common::String::format("%s\"%c\"", i ? ", " : "", eyePassChar(c.seq[i]));
	Common::String par;
	for (uint i = 0; i < c.parents.size(); i++)
		par += Common::String::format("%s\"%s\"", i ? ", " : "", c.parents[i].c_str());
	return Common::String::format(
		"{\"candidate_id\": \"%s\", \"generation\": %d, \"picture\": \"n%03d\", "
		"\"sequence\": [%s], \"sequence_compact\": \"%s\", \"source\": \"%s\", "
		"\"parents\": [%s], \"wins\": %d, \"ties\": %d, \"losses\": %d, "
		"\"output_file\": \"%s\"}",
		c.id().c_str(), c.gen, picId, seqArr.c_str(),
		eyeSeqCompact(c.seq).c_str(), c.source.c_str(), par.c_str(),
		c.wins, c.ties, c.losses, c.file.c_str());
}

Common::String eyeComparisonJson(const EyeComparison &c) {
	const char *choice = (c.choice == kEyeChoiceA)    ? "A"
	                   : (c.choice == kEyeChoiceB)    ? "B"
	                   : (c.choice == kEyeChoiceSame) ? "same" : "skip";
	return Common::String::format(
		"{\"a\": \"%s\", \"b\": \"%s\", \"choice\": \"%s\", \"millis\": %u, "
		"\"champion_after\": \"%s\"}",
		c.aId.c_str(), c.bId.c_str(), choice, c.millis, c.championAfter.c_str());
}

} // namespace Roger
} // namespace Sci
