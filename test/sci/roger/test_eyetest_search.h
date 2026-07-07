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
};
