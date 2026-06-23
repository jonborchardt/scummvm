// test/sci/roger/test_asset_cache.h
#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_asset_gen.h"
using namespace Sci::Roger;

class RogerAssetCacheTestSuite : public CxxTest::TestSuite {
public:
	void test_pass_change_changes_key() {
		RogerAssetGen g("sq3", "cache", kGenCache);
		Common::Array<int> p1; p1.push_back(2); p1.push_back(0);
		Common::Array<int> p2; p2.push_back(2); p2.push_back(2);
		g.setEnhancePasses(p1);
		Common::String k1 = g.testKey("omyac", 0xdeadbeef);
		g.setEnhancePasses(p2);
		Common::String k2 = g.testKey("omyac", 0xdeadbeef);
		TS_ASSERT_DIFFERS(k1, k2);
	}
	void test_same_inputs_same_key() {
		RogerAssetGen g("sq3", "cache", kGenCache);
		Common::Array<int> p; p.push_back(0);
		g.setEnhancePasses(p);
		TS_ASSERT_EQUALS(g.testKey("omyac", 7u), g.testKey("omyac", 7u));
	}
	void test_prebuilt_mode_returns_null_plate() {
		RogerAssetGen g("sq3", "cache", kGenPrebuilt);
		uint32 ms = 999;
		// In prebuilt mode generatePlate is a no-op signal even without engine state.
		TS_ASSERT(g.generatePlate(2, ms) == nullptr);
	}
	void test_omyacprio_key_distinct_from_omyac() {
		RogerAssetGen g("sq3", "cache", kGenCache);
		Common::Array<int> p; p.push_back(2);
		g.setEnhancePasses(p);
		TS_ASSERT_DIFFERS(g.testKey("omyac", 5u), g.testKey("omyacprio", 5u));
	}
	void test_omyacprio_prebuilt_returns_false() {
		RogerAssetGen g("sq3", "cache", kGenPrebuilt);
		Common::Array<byte> bands; int w = 1, h = 1; uint32 ms = 9;
		TS_ASSERT(!g.generatePriorityMap(2, bands, w, h, ms));
		TS_ASSERT_EQUALS(bands.size(), (uint)0);
	}
	void test_priority_band_gray_roundtrips_and_spreads() {
		// Every SCI band (0..15) must survive store->load, and map across the full
		// 0..255 grayscale range so the cached omyacprio PNG is inspectable.
		for (int b = 0; b <= 15; ++b) {
			byte gray = priorityBandToGray((byte)b);
			TS_ASSERT_EQUALS((int)grayToPriorityBand(gray), b); // lossless round-trip
		}
		TS_ASSERT_EQUALS((int)priorityBandToGray(0), 0);    // black
		TS_ASSERT_EQUALS((int)priorityBandToGray(15), 255); // white (full range)
	}
};
