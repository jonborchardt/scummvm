#include <cxxtest/TestSuite.h>
#include "sci/roger/view_cache.h"
#include "sci/roger/roger_asset_gen.h"
#include "graphics/surface.h"

// ViewCache now serves cels solely from the generator (no on-disk spritesheets),
// so these tests exercise the generated-cel map mechanics without touching the FS.
// In this test exe ENABLE_SCI is undefined, so RogerAssetGen::generateViewCel is the
// stub that returns nullptr; the cache must store and return that known-missing entry.

class TestViewCache : public CxxTest::TestSuite {
public:
	void test_no_generator_returns_null() {
		// No generator set -> no cel source -> nullptr.
		Sci::Roger::ViewCache cache(Common::String("unused"));
		TS_ASSERT(cache.getCel(900, 0, 0) == nullptr);
	}

	void test_prebuilt_mode_returns_null() {
		// A generator in prebuilt mode is not a generating source -> nullptr.
		Sci::Roger::RogerAssetGen gen(Common::String("sq3"), Common::String("cache"), Sci::Roger::kGenPrebuilt);
		Sci::Roger::ViewCache cache(Common::String("unused"));
		cache.setGenerator(&gen);
		TS_ASSERT(cache.getCel(900, 0, 0) == nullptr);
	}

	void test_generating_mode_caches_known_missing() {
		// Generating mode: generateViewCel is the no-ENABLE_SCI stub (nullptr). The
		// cache stores the nullptr and a second call returns it without crashing.
		Sci::Roger::RogerAssetGen gen(Common::String("sq3"), Common::String("cache"), Sci::Roger::kGenCache);
		Sci::Roger::ViewCache cache(Common::String("unused"));
		cache.setGenerator(&gen);
		TS_ASSERT(cache.getCel(900, 0, 0) == nullptr);
		TS_ASSERT(cache.getCel(900, 0, 0) == nullptr); // cached known-missing
	}
};
