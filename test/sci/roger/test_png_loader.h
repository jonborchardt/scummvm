#include <cxxtest/TestSuite.h>
#include "sci/roger/png_loader.h"
#include "common/array.h"

// FIXTURE_DIR must be defined on the compiler command line, e.g.:
//   -DFIXTURE_DIR=\"/abs/path/to/test/sci/roger/fixtures\"
// The Makefile rule below sets this automatically.

class TestPngLoader : public CxxTest::TestSuite {
public:
	void test_load_uniform_png_returns_correct_size() {
		Common::Array<byte> result = Sci::Roger::loadGrayscale8(
			Common::String(FIXTURE_DIR) + "/4x4_p5.png");
		TS_ASSERT_EQUALS(result.size(), (uint)16);
	}

	void test_load_uniform_png_all_pixels_equal_5() {
		Common::Array<byte> result = Sci::Roger::loadGrayscale8(
			Common::String(FIXTURE_DIR) + "/4x4_p5.png");
		TS_ASSERT_EQUALS(result.size(), (uint)16);
		for (uint i = 0; i < result.size(); i++)
			TS_ASSERT_EQUALS(result[i], (byte)5);
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
