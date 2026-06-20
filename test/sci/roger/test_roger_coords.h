#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_coords.h"
#include "common/rect.h"

class TestRogerCoords : public CxxTest::TestSuite {
public:
	void test_cel_rect_scales_6x() {
		Common::Rect in(10, 20, 30, 40);          // 320x200 space
		Common::Rect out = Sci::Roger::sciCelRectToOverlay(in, 1920, 1200);
		TS_ASSERT_EQUALS(out.left, 60);           // 10 * (1920/320)
		TS_ASSERT_EQUALS(out.top, 120);           // 20 * (1200/200)
		TS_ASSERT_EQUALS(out.right, 180);         // 30 * 6
		TS_ASSERT_EQUALS(out.bottom, 240);        // 40 * 6
	}
	void test_priority_band_above_horizon_is_zero() {
		TS_ASSERT_EQUALS(Sci::Roger::sciPriorityBand(10), 0);
	}
	void test_priority_band_increases_with_y() {
		int top = Sci::Roger::sciPriorityBand(45);
		int bottom = Sci::Roger::sciPriorityBand(185);
		TS_ASSERT(bottom > top);
		TS_ASSERT(top >= 1);
		TS_ASSERT(bottom <= 14);
	}
};
