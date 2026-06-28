// test/sci/roger/test_roger_capabilities.h
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_capabilities.h"
using namespace Sci::Roger;

class RogerCapabilitiesTestSuite : public CxxTest::TestSuite {
public:
	void test_ega_sci0_geometry() {
		// SCI0: script (play-area) height 200; status bar is the top 10 rows.
		RogerCapabilities c = RogerCapabilities::fromProbes(true, true, 200);
		TS_ASSERT(c.isEga);
		TS_ASSERT(c.hasParser);
		TS_ASSERT_EQUALS(c.screenRows, 200);
		TS_ASSERT_EQUALS(c.statusBarRows, 10);
	}
	void test_no_parser_flag_carried() {
		RogerCapabilities c = RogerCapabilities::fromProbes(true, false, 200);
		TS_ASSERT(!c.hasParser);
	}
	void test_non_ega_carried() {
		// Non-EGA is still representable in the struct; the provider rejects it elsewhere.
		RogerCapabilities c = RogerCapabilities::fromProbes(false, true, 200);
		TS_ASSERT(!c.isEga);
	}
	void test_status_bar_derived_from_geometry_not_gameid() {
		// statusBarRows must come from screen geometry, never a game id.
		RogerCapabilities c = RogerCapabilities::fromProbes(true, true, 200);
		TS_ASSERT_EQUALS(c.statusBarRows, 10);
	}
};
