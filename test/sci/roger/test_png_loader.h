#include <cxxtest/TestSuite.h>
#include "sci/roger/png_loader.h"
#include "common/array.h"
#include "../../system/null_osystem.h"

// FIXTURE_DIR must be defined on the compiler command line, e.g.:
//   -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"
// The Makefile (and build_tests.ps1 for MSVC) sets this automatically.

class TestPngLoader : public CxxTest::TestSuite {
public:
	// loadGrayscale8 reads files via Common::FSNode, which dereferences the global
	// OSystem (g_system). Install the null backend for the duration of each test,
	// exactly as test/common/encoding.h does for its file-reading tests.
	void setUp() {
		Common::install_null_g_system();
	}

	void tearDown() {
		Common::uninstall_null_g_system();
	}

	void test_load_uniform_png_returns_correct_size() {
		Common::Array<byte> result = Sci::Roger::loadGrayscale8(
			Common::String(FIXTURE_DIR) + "/4x4_p5.png");
		TS_ASSERT_EQUALS(result.size(), (uint)16);
	}

	void test_load_uniform_png_all_pixels_equal_5() {
		Common::Array<byte> result = Sci::Roger::loadGrayscale8(
			Common::String(FIXTURE_DIR) + "/4x4_p5.png");
		TS_ASSERT_EQUALS(result.size(), (uint)16);
		for (uint i = 0; i < result.size(); i++) {
			TS_ASSERT_EQUALS(result[i], (byte)5);
		}
	}

	void test_load_gradient_png_known_pixel_values() {
		Common::Array<byte> result = Sci::Roger::loadGrayscale8(
			Common::String(FIXTURE_DIR) + "/4x4_gradient.png");
		TS_ASSERT_EQUALS(result.size(), (uint)16);
		// Row 0
		TS_ASSERT_EQUALS(result[0],  (byte)3);
		TS_ASSERT_EQUALS(result[1],  (byte)7);
		TS_ASSERT_EQUALS(result[3],  (byte)15);
		// Row 1, col 1
		TS_ASSERT_EQUALS(result[5],  (byte)14);
		// Row 3, col 3
		TS_ASSERT_EQUALS(result[15], (byte)12);
	}

	void test_missing_file_returns_empty() {
		Common::Array<byte> result = Sci::Roger::loadGrayscale8(
			"/nonexistent/path/file.png");
		TS_ASSERT(result.empty());
	}
};
