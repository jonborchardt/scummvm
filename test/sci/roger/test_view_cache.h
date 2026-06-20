#include <cxxtest/TestSuite.h>
#include "sci/roger/view_cache.h"
#include "graphics/surface.h"
#include "../../system/null_osystem.h"

// FIXTURE_DIR must be defined on the compiler command line, e.g.:
//   -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"
// The Makefile (and build_tests.ps1 for MSVC) sets this automatically.

class TestViewCache : public CxxTest::TestSuite {
public:
	// ViewCache reads files via Common::FSNode, which dereferences the global
	// OSystem (g_system). Install the null backend for the duration of each test,
	// exactly as test_png_loader.h does.
	void setUp() {
		Common::install_null_g_system();
	}

	void tearDown() {
		Common::uninstall_null_g_system();
	}

	void test_cel_0_is_red_2x2() {
		Sci::Roger::ViewCache cache(Common::String(FIXTURE_DIR));
		// Fixture is named view.900.loop.0.* directly in FIXTURE_DIR, so the
		// cache must look for "<base>/900/view.900.loop.0.json"; for the test we
		// point base at a dir laid out that way (see Step 5 note).
		const Graphics::Surface *cel = cache.getCel(900, 0, 0);
		TS_ASSERT(cel != nullptr);
		TS_ASSERT_EQUALS(cel->w, 2);
		TS_ASSERT_EQUALS(cel->h, 2);
		uint8 a, r, g, b;
		cel->format.colorToARGB(cel->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(r, 255); TS_ASSERT_EQUALS(g, 0);
	}
	void test_cel_1_is_green() {
		Sci::Roger::ViewCache cache(Common::String(FIXTURE_DIR));
		const Graphics::Surface *cel = cache.getCel(900, 0, 1);
		TS_ASSERT(cel != nullptr);
		uint8 a, r, g, b;
		cel->format.colorToARGB(cel->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(g, 255); TS_ASSERT_EQUALS(r, 0);
	}
	void test_missing_view_returns_null() {
		Sci::Roger::ViewCache cache(Common::String(FIXTURE_DIR));
		TS_ASSERT(cache.getCel(12345, 0, 0) == nullptr);
	}
	void test_malformed_json_returns_null() {
		// View 901 has valid JSON but no "frames" key — must return nullptr, not crash.
		Sci::Roger::ViewCache cache(Common::String(FIXTURE_DIR));
		TS_ASSERT(cache.getCel(901, 0, 0) == nullptr);
	}
};
