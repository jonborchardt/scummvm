#include <cxxtest/TestSuite.h>
#include "common/util.h"
#include "engines/sci/roger/launcher/roger_picker_model.h"
#include "engines/sci/roger/ui/roger_widgets.h"

using namespace Sci::Roger;

class PickerModelTestSuite : public CxxTest::TestSuite {
public:
	void test_cache_marker_name() {
		TS_ASSERT_EQUALS(cacheMarkerName("sq3", 6, "affffflaaa"),
		                 Common::String("sq3.done.v6.affffflaaa.marker"));
		TS_ASSERT_EQUALS(cacheMarkerName("qfg1", 7, "none"),
		                 Common::String("qfg1.done.v7.none.marker"));
	}

	void test_split_description() {
		Common::String t, s;
		splitGameDescription("Space Quest III: The Pirates of Pestulon (DOS/English)", t, s);
		TS_ASSERT_EQUALS(t, Common::String("Space Quest III: The Pirates of Pestulon"));
		TS_ASSERT_EQUALS(s, Common::String("DOS/English"));
		splitGameDescription("qfg1", t, s);
		TS_ASSERT_EQUALS(t, Common::String("qfg1"));
		TS_ASSERT_EQUALS(s, Common::String(""));
	}

	void test_layout_rects_within_bounds() {
		const int W = 2290, H = 1589; // 80% of the 2862x1986 overlay
		PickerLayout l = layoutPicker(W, H, 3, 0);
		Common::Rect all(0, 0, W, H);
		TS_ASSERT(all.contains(l.titleBox));
		TS_ASSERT(all.contains(l.listPanel));
		TS_ASSERT(all.contains(l.addGame));
		TS_ASSERT(all.contains(l.settingsPanel));
		TS_ASSERT(all.contains(l.launch));
		// The mockup shows 3 rows; this size must fit all 3.
		TS_ASSERT_EQUALS((int)l.rows.size(), 3);
		// Launch button must not overlap the settings panel.
		TS_ASSERT(l.launch.top >= l.settingsPanel.bottom);
	}

	void test_layout_row_ordering() {
		PickerLayout l = layoutPicker(2290, 1589, 2, 0);
		const PickerRowLayout &r = l.rows[0];
		TS_ASSERT(r.card.contains(r.badge));
		TS_ASSERT(r.card.contains(r.precache));
		TS_ASSERT(r.card.contains(r.remove));
		TS_ASSERT(r.badge.right <= r.precache.left);
		TS_ASSERT(r.precache.right <= r.remove.left);
	}

	void test_layout_scroll_window() {
		PickerLayout l = layoutPicker(2290, 1589, 10, 2);
		TS_ASSERT_EQUALS((int)l.rows.size(), MIN(10 - 2, l.rowsVisible));
	}

	void test_layout_tiny_size_safe() {
		PickerLayout l = layoutPicker(320, 200, 5, 0);
		TS_ASSERT(l.rowsVisible >= 1);
		TS_ASSERT(!l.launch.isEmpty());
	}

	void test_clamp_scroll() {
		TS_ASSERT_EQUALS(clampScroll(-3, 10, 3), 0);
		TS_ASSERT_EQUALS(clampScroll(99, 10, 3), 7);
		TS_ASSERT_EQUALS(clampScroll(2, 3, 5), 0); // all rows fit -> no scroll
	}

	void test_pass_option_rects_stack_and_clamp() {
		PickerLayout l = layoutPicker(2290, 1589, 1, 0);
		Common::Rect o0 = passOptionRect(l, 0, 3, 1589);
		Common::Rect o1 = passOptionRect(l, 1, 3, 1589);
		TS_ASSERT_EQUALS(o0.bottom, o1.top);
		TS_ASSERT_EQUALS(o0.left, l.passesField.left);
		// A long list is shifted up so the last option stays on-canvas.
		Common::Rect last = passOptionRect(l, 19, 20, 1589);
		TS_ASSERT(last.bottom <= 1589);
	}

	void test_widget_id_roundtrip() {
		uint32 id = widId(kPickRowRemove, 7);
		TS_ASSERT_EQUALS(widKind(id), (int)kPickRowRemove);
		TS_ASSERT_EQUALS(widIndex(id), 7);
	}
};
