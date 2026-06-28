// test/sci/roger/test_roger_selftest.h
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_selftest.h"
#include "sci/roger/roger_capabilities.h"
using namespace Sci::Roger;

class RogerSelfTestSuite : public CxxTest::TestSuite {
	RogerCapabilities ega() { return RogerCapabilities::fromProbes(true, true, 200); }
	SelfTestInputs ok() {
		SelfTestInputs in;
		in.plateGenerated = true; in.plateW = 1920; in.plateH = 1140;
		in.expectW = 1920; in.expectH = 1140;
		in.priorityMapPresent = true; in.overlayEnabled = true;
		return in;
	}
public:
	void test_all_invariants_pass() {
		TS_ASSERT(evaluateInvariants(ok(), ega()).pass);
	}
	void test_missing_plate_fails() {
		SelfTestInputs in = ok(); in.plateGenerated = false;
		SelfTestResult r = evaluateInvariants(in, ega());
		TS_ASSERT(!r.pass);
		TS_ASSERT(r.firstFailure != nullptr);
	}
	void test_wrong_dimensions_fail() {
		SelfTestInputs in = ok(); in.plateW = 100;
		TS_ASSERT(!evaluateInvariants(in, ega()).pass);
	}
	void test_native_fallthrough_fails() {
		SelfTestInputs in = ok(); in.overlayEnabled = false;
		TS_ASSERT(!evaluateInvariants(in, ega()).pass);
	}
	void test_missing_priority_map_fails() {
		SelfTestInputs in = ok(); in.priorityMapPresent = false;
		TS_ASSERT(!evaluateInvariants(in, ega()).pass);
	}
};
