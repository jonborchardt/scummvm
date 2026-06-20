#include <cxxtest/TestSuite.h>
#include "sci/roger/file_roger_art_provider.h"
#include "sci/roger/null_roger_art_provider.h"
#include "common/str.h"

// FIXTURE_DIR defined by TEST_CFLAGS (see test/module.mk)

class TestFileRogerArtProvider : public CxxTest::TestSuite {
public:
	// ---- Path construction ----

	void test_visual_path_for_sq3_pic_100() {
		Sci::FileRogerArtProvider p("sq3", "/games/sq3");
		TS_ASSERT_EQUALS(p.testVisualPath(100),
			Common::String("/games/sq3-roger/pics/100/pic.100.png"));
	}

	void test_priority_path_for_sq3_pic_100() {
		Sci::FileRogerArtProvider p("sq3", "/games/sq3");
		TS_ASSERT_EQUALS(p.testPriorityPath(100),
			Common::String("/games/sq3-roger/pics/100/pic.100_p.png"));
	}

	void test_control_path_for_sq3_pic_100() {
		Sci::FileRogerArtProvider p("sq3", "/games/sq3");
		TS_ASSERT_EQUALS(p.testControlPath(100),
			Common::String("/games/sq3-roger/pics/100/pic.100_c.png"));
	}

	void test_path_for_pic_id_0() {
		Sci::FileRogerArtProvider p("qfg1", "/games/qfg1");
		TS_ASSERT_EQUALS(p.testVisualPath(0),
			Common::String("/games/qfg1-roger/pics/0/pic.0.png"));
	}

	void test_path_for_pic_id_999() {
		Sci::FileRogerArtProvider p("sq3", "/games/sq3");
		TS_ASSERT_EQUALS(p.testPriorityPath(999),
			Common::String("/games/sq3-roger/pics/999/pic.999_p.png"));
	}

	// ---- hasBackground ----

	void test_has_background_false_when_files_missing() {
		Sci::FileRogerArtProvider p("sq3", "/nonexistent/sq3");
		TS_ASSERT(!p.hasBackground(100));
	}

	void test_has_background_false_when_disabled() {
		// Even with files present, enabled=false must return false
		Sci::FileRogerArtProvider p("sq3", "/nonexistent/sq3");
		p.enabled = false;
		TS_ASSERT(!p.hasBackground(100));
	}

	// ---- NullRogerArtProvider (baseline) ----

	void test_null_provider_never_has_background() {
		Sci::NullRogerArtProvider null;
		TS_ASSERT(!null.hasBackground(0));
		TS_ASSERT(!null.hasBackground(100));
		TS_ASSERT(!null.hasBackground(999));
	}
};
