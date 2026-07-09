#include <cxxtest/TestSuite.h>
#include <cstdio>
#include "sci/roger/png_loader.h"
#include "graphics/surface.h"
#include "graphics/pixelformat.h"
#include "common/str.h"
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

	// dumpSurfacePng writes a PNG that loadSurfaceRGBA reads back identically.
	// This is the capture primitive behind the .rin capture path (Stage 0 / H1).
	void test_dump_png_roundtrip() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface src;
		src.create(2, 2, rgba);
		src.setPixel(0, 0, rgba.ARGBToColor(255, 255, 0, 0));   // opaque red
		src.setPixel(1, 0, rgba.ARGBToColor(255, 0, 255, 0));   // opaque green
		src.setPixel(0, 1, rgba.ARGBToColor(255, 0, 0, 255));   // opaque blue
		src.setPixel(1, 1, rgba.ARGBToColor(0, 0, 0, 0));       // transparent

		const Common::String path = Common::String(FIXTURE_DIR) + "/_tmp_dump_roundtrip.png";
		TS_ASSERT(Sci::Roger::dumpSurfacePng(src, path));
		src.free();

		Graphics::Surface *back = Sci::Roger::loadSurfaceRGBA(path);
		TS_ASSERT(back != nullptr);
		TS_ASSERT_EQUALS(back->w, 2);
		TS_ASSERT_EQUALS(back->h, 2);
		uint8 r, g, b, a;
		back->format.colorToARGB(back->getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(r, 255); TS_ASSERT_EQUALS(g, 0); TS_ASSERT_EQUALS(b, 0); TS_ASSERT_EQUALS(a, 255);
		back->format.colorToARGB(back->getPixel(1, 0), a, r, g, b);
		TS_ASSERT_EQUALS(r, 0); TS_ASSERT_EQUALS(g, 255); TS_ASSERT_EQUALS(b, 0);
		back->format.colorToARGB(back->getPixel(1, 1), a, r, g, b);
		TS_ASSERT_EQUALS(a, 0);
		back->free();
		delete back;

		::remove(path.c_str());
	}

	void test_dump_png_bad_path_returns_false() {
		const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
		Graphics::Surface s;
		s.create(1, 1, rgba);
		TS_ASSERT(!Sci::Roger::dumpSurfacePng(s, "/nope/dir/x.png"));
		s.free();
	}
};
