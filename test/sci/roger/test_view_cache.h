#include <cxxtest/TestSuite.h>
#include "sci/roger/view_cache.h"
#include "sci/roger/gen/roger_asset_gen.h"
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

	// Tune panel: clear() frees every cached cel (incl. cached-nullptr
	// misses) so the next getCel regenerates with the new variant.
	void test_clear_empties_generated_cels() {
		Sci::Roger::ViewCache vc("unused");
		Graphics::Surface *s = new Graphics::Surface();
		s->create(4, 4, Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0));
		vc.insertForTest(7, 0, 1, s);           // owned by the cache now
		vc.insertForTest(7, 0, 2, nullptr);     // cached miss
		TS_ASSERT_EQUALS(vc.genCelCount(), 2u);
		vc.clear();
		TS_ASSERT_EQUALS(vc.genCelCount(), 0u);
		// No generator set: getCel after clear is a clean nullptr, no stale entry.
		TS_ASSERT(vc.getCel(7, 0, 1) == nullptr);
	}
};
