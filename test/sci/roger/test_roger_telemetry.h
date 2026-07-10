/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cxxtest/TestSuite.h>
#include "sci/roger/roger_telemetry.h"

using namespace Sci::Roger;

class TestRogerTelemetry : public CxxTest::TestSuite {
public:
	void test_first_cycle_reports_period_zero() {
		CycleTelemetry t;
		t.frameStart(1000);
		uint32 period = 99, busy = 99;
		TS_ASSERT(t.frameRendered(1012, period, busy));
		TS_ASSERT_EQUALS(period, 0u);   // no previous cycle yet (old s_prevCycleT0==0 behavior)
		TS_ASSERT_EQUALS(busy, 12u);
	}
	void test_steady_state_period_is_entry_to_entry() {
		CycleTelemetry t;
		uint32 period = 0, busy = 0;
		t.frameStart(1000); t.frameRendered(1012, period, busy);
		t.frameStart(1083); TS_ASSERT(t.frameRendered(1095, period, busy));
		TS_ASSERT_EQUALS(period, 83u);  // walking-speed number: entry-to-entry
		TS_ASSERT_EQUALS(busy, 12u);
	}
	void test_render_without_start_emits_nothing() {
		// reAnimate fires onAnimateFrame outside a cycle: must not emit.
		CycleTelemetry t;
		uint32 period = 0, busy = 0;
		TS_ASSERT(!t.frameRendered(500, period, busy));
	}
	void test_double_render_emits_once_per_cycle() {
		CycleTelemetry t;
		uint32 period = 0, busy = 0;
		t.frameStart(1000);
		TS_ASSERT(t.frameRendered(1012, period, busy));
		TS_ASSERT(!t.frameRendered(1015, period, busy)); // reAnimate after the cycle tail
	}
	void test_skipped_cycle_folds_into_next_period() {
		// A kernelAnimate early return (frameStart with no frameRendered) must
		// fold into the NEXT emitted period, measured from the last EMITTED
		// cycle's entry — exactly the old s_prevCycleT0 discipline.
		CycleTelemetry t;
		uint32 period = 0, busy = 0;
		t.frameStart(1000); t.frameRendered(1012, period, busy);
		t.frameStart(1083);                    // early-return cycle: no render
		t.frameStart(1166); TS_ASSERT(t.frameRendered(1178, period, busy));
		TS_ASSERT_EQUALS(period, 166u);        // 1166 - 1000
	}
};
