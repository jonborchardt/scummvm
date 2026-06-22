// test/sci/roger/test_scale.h
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_scale.h"
using namespace Sci::Roger;

class RogerScaleTestSuite : public CxxTest::TestSuite {
public:
	void test_scale2x_solid_block() {
		IndexImage in; in.w = 2; in.h = 2; in.pixels.resize(4, 7); // all color 7
		IndexImage out = scale2x(in);
		TS_ASSERT_EQUALS(out.w, 4); TS_ASSERT_EQUALS(out.h, 4);
		for (uint i = 0; i < out.pixels.size(); i++) TS_ASSERT_EQUALS(out.pixels[i], 7);
	}
	void test_scale6x_dims() {
		IndexImage in; in.w = 3; in.h = 5; in.pixels.resize(15, 1);
		IndexImage out = scale6x(in);
		TS_ASSERT_EQUALS(out.w, 18); TS_ASSERT_EQUALS(out.h, 30);
	}
	void test_scale3x_center_preserved() {
		// 3x3 with distinct center; EPX keeps center class in the middle 3x3 block.
		IndexImage in; in.w = 3; in.h = 3; in.pixels.resize(9, 0); in.pixels[4] = 9;
		IndexImage out = scale3x(in); // 9x9
		TS_ASSERT_EQUALS(out.pixels[4*9 + 4], 9); // dead-center stays center color
	}
};
