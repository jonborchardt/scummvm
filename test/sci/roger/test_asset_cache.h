// test/sci/roger/test_asset_cache.h
#include <cxxtest/TestSuite.h>
#include <cstdio>              // remove() for tmp cache files (test-only; cf. test_load_surface.h)
#include "sci/roger/gen/roger_asset_gen.h"
#include "common/file.h"       // Common::DumpFile
#include "common/path.h"
#include "../../system/null_osystem.h"
using namespace Sci::Roger;

class RogerAssetCacheTestSuite : public CxxTest::TestSuite {
public:
	void setUp() {
		Common::install_null_g_system();
	}
	void tearDown() {
		Common::uninstall_null_g_system();
	}
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
	void test_gameid_changes_key() {
		RogerAssetGen a("sq3", "cache", kGenCache);
		RogerAssetGen b("qfg1", "cache", kGenCache);
		Common::Array<int> p; p.push_back(2);
		a.setEnhancePasses(p); b.setEnhancePasses(p);
		TS_ASSERT_DIFFERS(a.testKey("omyac", 5u), b.testKey("omyac", 5u));
	}
	void test_transform_kind_changes_key() {
		RogerAssetGen g("sq3", "cache", kGenCache);
		Common::Array<int> p; p.push_back(2); g.setEnhancePasses(p);
		// Each asset kind (visual/priority/view) must be distinct for one (game,hash).
		TS_ASSERT_DIFFERS(g.testKey("omyac", 5u),    g.testKey("scale6x", 5u));
		TS_ASSERT_DIFFERS(g.testKey("omyacprio", 5u), g.testKey("scale6x", 5u));
	}
	void test_resource_hash_changes_key() {
		RogerAssetGen g("sq3", "cache", kGenCache);
		Common::Array<int> p; p.push_back(2); g.setEnhancePasses(p);
		TS_ASSERT_DIFFERS(g.testKey("omyac", 1u), g.testKey("omyac", 2u));
	}
	void test_version_embedded_in_key() {
		RogerAssetGen g("sq3", "cache", kGenCache);
		Common::Array<int> p; p.push_back(2); g.setEnhancePasses(p);
		Common::String k = g.testKey("omyac", 5u);
		Common::String tag = Common::String::format("v%d", kTransformVersion);
		TS_ASSERT(k.contains(tag));   // a pipeline-version bump invalidates stale files
	}

	// viewCelHash is the stable identity key: deterministic, and distinct
	// across each element of the triple.
	void test_view_cel_hash_stable_and_distinct() {
		TS_ASSERT_EQUALS(RogerAssetGen::viewCelHash(12, 1, 0),
		                 RogerAssetGen::viewCelHash(12, 1, 0));
		TS_ASSERT_DIFFERS(RogerAssetGen::viewCelHash(12, 1, 0),
		                  RogerAssetGen::viewCelHash(13, 1, 0));
		TS_ASSERT_DIFFERS(RogerAssetGen::viewCelHash(12, 1, 0),
		                  RogerAssetGen::viewCelHash(12, 2, 0));
		TS_ASSERT_DIFFERS(RogerAssetGen::viewCelHash(12, 1, 0),
		                  RogerAssetGen::viewCelHash(12, 1, 1));
	}

	// The probes are kGenCache-only: every other mode must fall through to
	// the generate path, so the probe reports "not cached".
	void test_cache_probes_gated_to_cache_mode() {
		RogerAssetGen m("sq3", "cache", kGenMemory);
		TS_ASSERT(!m.isViewCelCached(1, 0, 0));
		RogerAssetGen a("sq3", "cache", kGenAlways);
		TS_ASSERT(!a.isViewCelCached(1, 0, 0));
		RogerAssetGen p("sq3", "cache", kGenPrebuilt);
		TS_ASSERT(!p.isPicCached(2));
		// kGenCache but no engine running (unit tests have no g_sci):
		// must return false without crashing, never true.
		RogerAssetGen c("sq3", "cache", kGenCache);
		TS_ASSERT(!c.isPicCached(2));
	}

	// End-to-end existence check: plant a file under the exact keyed name in
	// FIXTURE_DIR and probe it. No decode happens -- an empty file suffices,
	// which is itself the point of the feature.
	void test_view_cel_exists_check_hits_planted_file() {
		const Common::String dir(FIXTURE_DIR);
		RogerAssetGen g("sq3", dir, kGenCache);
		Common::Array<int> p; p.push_back(2); g.setEnhancePasses(p);

		TS_ASSERT(!g.isViewCelCached(12, 1, 0)); // nothing planted yet

		const Common::String path = dir + "/" +
			g.testKey("scale6x", RogerAssetGen::viewCelHash(12, 1, 0)) + ".png";
		{
			Common::DumpFile f;
			TS_ASSERT(f.open(Common::Path(path)));
			f.finalize();
			f.close();
		}
		TS_ASSERT(g.isViewCelCached(12, 1, 0));
		TS_ASSERT(!g.isViewCelCached(12, 1, 1)); // different cel -> different key -> miss
		remove(path.c_str());
	}
};
