#include <cxxtest/TestSuite.h>
#include "sci/roger/gen/roger_byte_reader.h"
using namespace Sci::Roger;

class RogerByteReaderTestSuite : public CxxTest::TestSuite {
public:
	void test_read8_advances() {
		const byte data[] = { 0xf0, 0x12, 0x34 };
		ByteReader r(data, 3);
		TS_ASSERT_EQUALS(r.read8(), 0xf0u);
		TS_ASSERT_EQUALS(r.read8(), 0x12u);
		TS_ASSERT_EQUALS(r.peek8(), 0x34);   // peek does not advance
		TS_ASSERT_EQUALS(r.read8(), 0x34u);
		TS_ASSERT_EQUALS(r.peek8(), -1);     // EOF sentinel
		TS_ASSERT(r.eof());
	}
	void test_read24_big_endian() {
		// getPoint24 reads 24 bits MSB-first: byte0<<16 | byte1<<8 | byte2.
		const byte data[] = { 0xAB, 0xCD, 0xEF };
		ByteReader r(data, 3);
		TS_ASSERT_EQUALS(r.read24(), 0xABCDEFu);
	}
	void test_skip() {
		const byte data[] = { 0x01, 0x02, 0x03, 0x04 };
		ByteReader r(data, 4);
		r.skip(3);
		TS_ASSERT_EQUALS(r.read8(), 0x04u);
	}
};
