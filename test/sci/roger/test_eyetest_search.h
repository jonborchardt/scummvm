#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_eyetest_search.h"

// TEMPORARY EXPERIMENT (eye-test genetic pass search, 2026-07-06) — delete with
// roger_eyetest_search.{h,cpp}.

using namespace Sci::Roger;

class EyeTestSearchSuite : public CxxTest::TestSuite {
public:
	void testBaseSeqIsSpec() {
		EyeSeq b = eyeBaseSeq();
		TS_ASSERT_EQUALS(b.size(), (uint)kEyeSeqLen);
		TS_ASSERT_EQUALS(eyeSeqCompact(b), Common::String("ffflffaaaa"));
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
};
