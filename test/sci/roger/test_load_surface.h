#include <cxxtest/TestSuite.h>
#include "sci/roger/png_loader.h"
#include "graphics/surface.h"
#include "../../system/null_osystem.h"

// FIXTURE_DIR defined by test/module.mk's TEST_CFLAGS (make) or
// build_tests.ps1 (MSVC), e.g.: -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"

class TestLoadSurface : public CxxTest::TestSuite {
public:
	// loadSurfaceRGBA reads files via Common::FSNode, which dereferences the
	// global OSystem (g_system). Install the null backend for each test,
	// exactly as test_png_loader.h does.
	void setUp() {
		Common::install_null_g_system();
	}

	void tearDown() {
		Common::uninstall_null_g_system();
	}

	void test_rgba_dimensions_and_pixels() {
		Graphics::Surface *s = Sci::Roger::loadSurfaceRGBA(
			Common::String(FIXTURE_DIR) + "/rgba_2x2.png");
		TS_ASSERT(s != nullptr);
		TS_ASSERT_EQUALS(s->w, 2);
		TS_ASSERT_EQUALS(s->h, 2);
		TS_ASSERT_EQUALS(s->format.bytesPerPixel, 4);
		// Top-left red, fully opaque
		uint8 r, g, b, a;
		s->format.colorToARGB(s->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(r, 255); TS_ASSERT_EQUALS(g, 0);
		TS_ASSERT_EQUALS(b, 0);   TS_ASSERT_EQUALS(a, 255);
		// Bottom-right transparent
		s->format.colorToARGB(s->getPixel(1, 1), a, r, g, b);
		TS_ASSERT_EQUALS(a, 0);
		s->free();
		delete s;
	}

	void test_missing_file_returns_null() {
		TS_ASSERT(Sci::Roger::loadSurfaceRGBA("/nope/x.png") == nullptr);
	}
};
