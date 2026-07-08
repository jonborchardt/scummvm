#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_passes.h"
#include "sci/roger/utils/eyetest/roger_eyetest_search.h"

// Eye Exam (kept dev utility): unit tests for the quarantined pure search
// module at engines/sci/roger/utils/eyetest/roger_eyetest_search.{h,cpp}.

using namespace Sci::Roger;

class EyeTestSearchSuite : public CxxTest::TestSuite {
public:
	void testBaseSeqIsSpec() {
		// The search base IS the shipping default (single swap point in
		// roger_passes) — this locks the two together whatever the default is.
		EyeSeq b = eyeBaseSeq();
		TS_ASSERT_EQUALS(b.size(), (uint)kEyeSeqLen);
		TS_ASSERT_EQUALS(eyeSeqCompact(b), Common::String(kDefaultPassString));
		// And the default must be one of the curated known-good patterns.
		bool inRegistry = false;
		for (int i = 0; i < goodPassPatternCount(); i++)
			if (Common::String(goodPassPattern(i).compact) == kDefaultPassString)
				inRegistry = true;
		TS_ASSERT(inRegistry);
	}

	void testPassChars() {
		TS_ASSERT_EQUALS(eyePassChar(2), 'f');
		TS_ASSERT_EQUALS(eyePassChar(1), 'l');
		TS_ASSERT_EQUALS(eyePassChar(0), 'a');
	}

	void testRandomSeqValid() {
		EyeRng rng(1234);
		for (int n = 0; n < 50; n++) {
			EyeSeq s = eyeRandomSeq(rng);
			TS_ASSERT_EQUALS(s.size(), (uint)kEyeSeqLen);
			for (uint i = 0; i < s.size(); i++)
				TS_ASSERT(s[i] >= 0 && s[i] <= 2);
		}
	}

	void testRngDeterministic() {
		EyeRng a(42), b(42);
		for (int i = 0; i < 10; i++)
			TS_ASSERT_EQUALS(a.next(), b.next());
	}

	// Structural assertions only — never assert exact RNG-dependent values.
	void testMutateChangesOneToFivePositions() {
		EyeRng rng(7);
		const EyeSeq base = eyeBaseSeq();
		for (int n = 0; n < 200; n++) {
			Common::String desc;
			EyeSeq m = eyeMutate(base, rng, desc);
			TS_ASSERT_EQUALS(m.size(), (uint)kEyeSeqLen);
			int changed = 0;
			for (int i = 0; i < kEyeSeqLen; i++) {
				if (m[i] != base[i]) {
					changed++;
					TS_ASSERT(m[i] >= 0 && m[i] <= 2);
					// desc must name this position with old and new chars
					Common::String frag = Common::String::format("pos%02d_%c_to_%c",
						i, eyePassChar(base[i]), eyePassChar(m[i]));
					TS_ASSERT(desc.contains(frag));
				}
			}
			TS_ASSERT(changed >= 1 && changed <= 5);
			TS_ASSERT(desc.hasPrefix("mut"));
		}
	}

	void testMutateNeverKeepsSelectedValue() {
		// Every position named in desc must actually differ from the source:
		// count "_to_" groups == changed positions.
		EyeRng rng(99);
		const EyeSeq base = eyeBaseSeq();
		for (int n = 0; n < 100; n++) {
			Common::String desc;
			EyeSeq m = eyeMutate(base, rng, desc);
			int changed = 0;
			for (int i = 0; i < kEyeSeqLen; i++)
				if (m[i] != base[i])
					changed++;
			int groups = 0;
			for (uint p = 0; p + 4 <= desc.size(); p++)
				if (desc[p] == '_' && desc[p+1] == 't' && desc[p+2] == 'o' && desc[p+3] == '_')
					groups++;
			TS_ASSERT_EQUALS(groups, changed);
		}
	}

	void testParseCompact() {
		EyeSeq s;
		TS_ASSERT(eyeParseCompact("ffflffaaaa", s));
		TS_ASSERT_EQUALS(eyeSeqCompact(s), Common::String("ffflffaaaa"));
		TS_ASSERT(eyeParseCompact("fffffflaaa", s));
		TS_ASSERT_EQUALS(eyeSeqCompact(s), Common::String("fffffflaaa"));
		TS_ASSERT(!eyeParseCompact("ffflffaaa", s));   // too short
		TS_ASSERT(!eyeParseCompact("ffflffaaaax", s)); // too long
		TS_ASSERT(!eyeParseCompact("ffflffaaza", s));  // bad char
		TS_ASSERT(!eyeParseCompact("", s));
		TS_ASSERT_EQUALS(s.size(), (uint)0); // cleared on failure
	}

	void testMutateWeightsBiasPositions() {
		// Overwhelming weight on position 9: nearly every mutation must touch
		// it (first pick lands there with ~99% probability per mutation).
		static const int w[kEyeSeqLen] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1000};
		EyeRng rng(123);
		const EyeSeq base = eyeBaseSeq();
		int touched9 = 0;
		for (int n = 0; n < 200; n++) {
			Common::String desc;
			EyeSeq m = eyeMutate(base, rng, desc, w);
			TS_ASSERT_EQUALS(m.size(), (uint)kEyeSeqLen);
			int changed = 0;
			for (int i = 0; i < kEyeSeqLen; i++)
				if (m[i] != base[i])
					changed++;
			TS_ASSERT(changed >= 1 && changed <= 5); // distribution unchanged
			if (m[9] != base[9])
				touched9++;
		}
		TS_ASSERT(touched9 > 180); // decisive skew, seed-stable
	}

	void testCrossoverTakesEveryPositionFromAParent() {
		EyeRng rng(5);
		EyeSeq a = eyeBaseSeq();
		EyeSeq b;
		for (int i = 0; i < kEyeSeqLen; i++)
			b.push_back(0); // "aaaaaaaaaa"
		for (int n = 0; n < 100; n++) {
			EyeSeq c = eyeCrossover(a, b, rng);
			TS_ASSERT_EQUALS(c.size(), (uint)kEyeSeqLen);
			for (int i = 0; i < kEyeSeqLen; i++)
				TS_ASSERT(c[i] == a[i] || c[i] == b[i]);
		}
	}

	void testConvergedNeedsFullWindow() {
		Common::Array<int> ch;
		for (int i = 0; i < kEyeSameWindow - 1; i++)
			ch.push_back(kEyeChoiceSame);
		TS_ASSERT(!eyeConverged(ch, kEyeSameWindow)); // 11 Sames, window unfilled
		ch.push_back(kEyeChoiceSame);
		TS_ASSERT(eyeConverged(ch, kEyeSameWindow));  // 12/12
	}

	void testConvergedHalfSameInLastWindow() {
		Common::Array<int> ch;
		// 12 old A-picks, then 6 Same + 6 A in the last 12 -> exactly 50% = converged.
		for (int i = 0; i < 12; i++)
			ch.push_back(kEyeChoiceA);
		for (int i = 0; i < 6; i++)
			ch.push_back(kEyeChoiceSame);
		for (int i = 0; i < 6; i++)
			ch.push_back(kEyeChoiceA);
		TS_ASSERT(eyeConverged(ch, kEyeSameWindow));
		ch.push_back(kEyeChoiceA); // last 12 now has 5 Sames -> not converged
		TS_ASSERT(!eyeConverged(ch, kEyeSameWindow));
	}

	void testRankPoolOrdersByScoreThenRecency() {
		Common::Array<EyeCandidate> all;
		for (int i = 0; i < 4; i++) {
			EyeCandidate c;
			c.gen = 0; c.idx = i; c.seq = eyeBaseSeq();
			all.push_back(c);
		}
		all[0].wins = 1;                    // score 1
		all[1].wins = 3; all[1].losses = 1; // score 2
		all[2].wins = 2;                    // score 2 (newer than [1])
		all[3].losses = 2;                  // score -2
		Common::Array<int> pool = eyeRankPool(all, 3);
		TS_ASSERT_EQUALS(pool.size(), (uint)3);
		TS_ASSERT_EQUALS(pool[0], 2); // score 2, newest wins the tie
		TS_ASSERT_EQUALS(pool[1], 1); // score 2, older
		TS_ASSERT_EQUALS(pool[2], 0); // score 1
	}

	void testBreedCountSourcesAndDedup() {
		Common::Array<EyeCandidate> all;
		EyeCandidate a; a.gen = 0; a.idx = 0; a.seq = eyeBaseSeq(); a.wins = 2; a.source = "base";
		EyeCandidate b; b.gen = 0; b.idx = 1; b.seq = eyeBaseSeq(); b.seq[0] = 0; b.wins = 1;
		all.push_back(a); all.push_back(b);
		Common::Array<int> pool = eyeRankPool(all, kEyePop);
		Common::Array<Common::String> seen;
		seen.push_back(eyeSeqCompact(a.seq));
		seen.push_back(eyeSeqCompact(b.seq));
		EyeRng rng(31);
		Common::Array<EyeCandidate> kids = eyeBreed(all, pool, 1, 4, rng, seen);
		TS_ASSERT_EQUALS(kids.size(), (uint)4);
		for (uint k = 0; k < kids.size(); k++) {
			TS_ASSERT_EQUALS(kids[k].gen, 1);
			TS_ASSERT_EQUALS(kids[k].idx, (int)k);
			TS_ASSERT(!kids[k].source.empty());
			TS_ASSERT_EQUALS(kids[k].seq.size(), (uint)kEyeSeqLen);
			// dedup: no offspring repeats base or b or an earlier sibling
			const Common::String cs = eyeSeqCompact(kids[k].seq);
			int hits = 0;
			for (uint s = 0; s < seen.size(); s++)
				if (seen[s] == cs)
					hits++;
			TS_ASSERT_EQUALS(hits, 1); // exactly its own entry, appended by eyeBreed
			// mutation/crossover offspring must record parent ids
			if (kids[k].source.hasPrefix("mut") || kids[k].source == "cross")
				TS_ASSERT(kids[k].parents.size() >= 1);
		}
	}

	void testCandidateFileName() {
		EyeCandidate c;
		c.gen = 0; c.idx = 0; c.seq = eyeBaseSeq(); c.source = "base";
		// Compact suffix is derived from the base seq (== the shipping default),
		// so a default promotion doesn't need this expectation re-baselined.
		TS_ASSERT_EQUALS(eyeCandidateFileName(2, c),
			Common::String("n002_gen000_cand000_base__") + eyeSeqCompact(eyeBaseSeq()) + ".png");
	}

	void testCandidateJsonFields() {
		EyeCandidate c;
		c.gen = 1; c.idx = 3; c.seq = eyeBaseSeq(); c.source = "mut_pos04_f_to_a";
		c.parents.push_back("gen000_cand000");
		c.file = "n002_gen001_cand003_mut_pos04_f_to_a__ffflffaaaa.png";
		c.wins = 1;
		const Common::String j = eyeCandidateJson(2, c);
		TS_ASSERT(j.contains("\"candidate_id\": \"gen001_cand003\""));
		TS_ASSERT(j.contains("\"generation\": 1"));
		TS_ASSERT(j.contains("\"picture\": \"n002\""));
		TS_ASSERT(j.contains("\"source\": \"mut_pos04_f_to_a\""));
		TS_ASSERT(j.contains("\"parents\": [\"gen000_cand000\"]"));
		// sequence fields are derived from the base seq (== the shipping default),
		// so a default promotion doesn't need these expectations re-baselined.
		const Common::String compact = eyeSeqCompact(eyeBaseSeq());
		TS_ASSERT(j.contains(Common::String("\"sequence_compact\": \"") + compact + "\""));
		Common::String seqArr = "\"sequence\": [";
		for (uint i = 0; i < compact.size(); i++)
			seqArr += Common::String::format("%s\"%c\"", i ? ", " : "", compact[i]);
		seqArr += "]";
		TS_ASSERT(j.contains(seqArr));
		TS_ASSERT(j.contains("\"output_file\": "));
		TS_ASSERT(j.contains("\"wins\": 1"));
	}

	void testComparisonJson() {
		EyeComparison r;
		r.aId = "gen000_cand000"; r.bId = "gen001_cand002";
		r.choice = kEyeChoiceSame; r.millis = 123; r.championAfter = "gen000_cand000";
		const Common::String j = eyeComparisonJson(r);
		TS_ASSERT(j.contains("\"a\": \"gen000_cand000\""));
		TS_ASSERT(j.contains("\"b\": \"gen001_cand002\""));
		TS_ASSERT(j.contains("\"choice\": \"same\""));
		TS_ASSERT(j.contains("\"champion_after\": \"gen000_cand000\""));
	}
};
