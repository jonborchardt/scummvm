// test/sci/roger/test_ega_blend.h
#include <cxxtest/TestSuite.h>
#include "sci/roger/gen/roger_ega_blend.h"
#include "graphics/surface.h"
using namespace Sci::Roger;

class RogerEgaBlendTestSuite : public CxxTest::TestSuite {
public:
	void test_solid_colors() {
		// Solid black 0x00 -> 0xff000000; solid white 0xff -> 0xffffffff.
		TS_ASSERT_EQUALS(BLEND_TABLE[0x00], 0xff000000u);
		TS_ASSERT_EQUALS(BLEND_TABLE[0xff], 0xffffffffu);
		// Solid green 0x22 -> green channel only: 0xff00aa00 (G=0xaa).
		TS_ASSERT_EQUALS(BLEND_TABLE[0x22], 0xff00aa00u);
	}
	void test_dither_pair_is_blend() {
		// 0x0f (black+white) blends to mid-gray; not equal to either endpoint.
		TS_ASSERT_DIFFERS(BLEND_TABLE[0x0f], 0xff000000u);
		TS_ASSERT_DIFFERS(BLEND_TABLE[0x0f], 0xffffffffu);
	}
	void test_blend_to_surface_dimensions() {
		Common::Array<byte> px; px.resize(4, 0x22); // 2x2 solid green
		Graphics::Surface *s = blendToSurface(px, 2, 2);
		TS_ASSERT(s != nullptr);
		TS_ASSERT_EQUALS(s->w, 2); TS_ASSERT_EQUALS(s->h, 2);
		uint32 c = s->getPixel(0, 0);
		uint8 r,g,b,a; s->format.colorToARGB(c, a, r, g, b);
		TS_ASSERT_EQUALS((int)g, 0xaa); TS_ASSERT_EQUALS((int)r, 0x00); TS_ASSERT_EQUALS((int)b, 0x00);
		s->free(); delete s;
	}
};
