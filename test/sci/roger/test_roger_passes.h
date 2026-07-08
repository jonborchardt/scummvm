#include <cxxtest/TestSuite.h>
#include "common/util.h"
#include "engines/sci/roger/gen/roger_passes.h"
#include "engines/sci/roger/gen/roger_omyac.h"

using namespace Sci::Roger;

class RogerPassesTestSuite : public CxxTest::TestSuite {
	// Compare a parsed array against expected values given as a C string of
	// digits ('2','1','0'), keeping the assertions readable.
	static void assertPasses(const Common::Array<int> &got, const char *expect) {
		TS_ASSERT_EQUALS(got.size(), (uint)strlen(expect));
		if (got.size() != strlen(expect))
			return;
		for (uint i = 0; i < got.size(); i++)
			TS_ASSERT_EQUALS(got[i], expect[i] - '0');
	}

public:
	void test_compact_default_string() {
		assertPasses(parsePassString("ffflffaaaa"), "2221220000");
	}

	void test_compact_digits_and_mixed_chars() {
		assertPasses(parsePassString("21"), "21");
		assertPasses(parsePassString("fla"), "210");
		assertPasses(parsePassString("f1a"), "210"); // digits and letters mix per-char
	}

	void test_legacy_token_forms() {
		assertPasses(parsePassString("2 1"), "21");
		assertPasses(parsePassString("fill line"), "21");
		assertPasses(parsePassString("f,l"), "21");
		assertPasses(parsePassString("f\tl all"), "210");
	}

	void test_unknown_tokens_skipped() {
		// Separated form: unknown token warns and is skipped.
		assertPasses(parsePassString("fill bogus line"), "21");
		// No-separator string with a non-vocabulary char is NOT compact; it
		// falls to the tokenizer as one unknown token -> empty (documented
		// failure mode, matches the old parser's behavior for junk).
		assertPasses(parsePassString("ffx"), "");
	}

	void test_empty_string_is_zero_passes() {
		assertPasses(parsePassString(""), "");
	}

	void test_effective_three_state() {
		// unset -> the default sequence (derived from kDefaultPassString so a
		// default promotion doesn't need this test re-baselined).
		TS_ASSERT_EQUALS(passString(effectivePasses(false, "")),
		                 Common::String(kDefaultPassString));
		TS_ASSERT_EQUALS(passString(effectivePasses(false, "ignored")),
		                 Common::String(kDefaultPassString));
		// set + empty -> wireframe (zero passes)
		assertPasses(effectivePasses(true, ""), "");
		// set + tokens -> parsed
		assertPasses(effectivePasses(true, "fl"), "21");
	}

	void test_format_roundtrip_and_default_lock() {
		TS_ASSERT_EQUALS(passString(parsePassString(kDefaultPassString)),
		                 Common::String(kDefaultPassString));
		TS_ASSERT_EQUALS(passString(Common::Array<int>()), Common::String(""));
		// defaultPasses() (roger_omyac) and the codified string can never diverge.
		const Common::Array<int> viaOmyac = defaultPasses();
		const Common::Array<int> viaString = parsePassString(kDefaultPassString);
		TS_ASSERT_EQUALS(viaOmyac.size(), viaString.size());
		for (uint i = 0; i < viaOmyac.size() && i < viaString.size(); i++)
			TS_ASSERT_EQUALS(viaOmyac[i], viaString[i]);
	}

	void test_whole_word_keywords_and_trim() {
		// Whole-word keywords must match as a single pass, not per-char.
		// "all" = [0], NOT the compact mis-parse [0,1,1].
		assertPasses(parsePassString("all"),  "0");
		assertPasses(parsePassString("fill"), "2");
		assertPasses(parsePassString("line"), "1");

		// Trailing/leading whitespace is trimmed: compact form is reached.
		assertPasses(parsePassString("ffflffaaaa "), "2221220000");
		assertPasses(parsePassString(" ffflffaaaa"), "2221220000");

		// Legacy -> compact round-trip: "2 1" parses to [2,1], passString -> "fl".
		TS_ASSERT_EQUALS(passString(parsePassString("2 1")), Common::String("fl"));
	}
};
