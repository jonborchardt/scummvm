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
#include "sci/roger/roger_effects.h"
using namespace Sci::Roger;

class RogerEffectsTestSuite : public CxxTest::TestSuite {
public:
	void test_family_mapping() {
		TS_ASSERT_EQUALS(transitionFamilyFor(10), kFxFade);   // FADEPALETTE
		TS_ASSERT_EQUALS(transitionFamilyFor(8),  kFxDissolve); // BLOCKS
		TS_ASSERT_EQUALS(transitionFamilyFor(9),  kFxDissolve); // PIXELATION
		TS_ASSERT_EQUALS(transitionFamilyFor(2),  kFxWipe);   // STRAIGHT_FROM_RIGHT
		TS_ASSERT_EQUALS(transitionFamilyFor(0),  kFxWipe);   // VERTICALROLL_FROMCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(300), kFxWipe);  // VERTICALROLL_TOCENTER
		TS_ASSERT_EQUALS(transitionFamilyFor(11), kFxScroll); // SCROLL_RIGHT
		TS_ASSERT_EQUALS(transitionFamilyFor(15), kFxNone);   // NONE_LONGBOW
		TS_ASSERT_EQUALS(transitionFamilyFor(100), kFxNone);  // NONE
		TS_ASSERT_EQUALS(transitionFamilyFor(9999), kFxFade); // unknown -> safe default
	}
	void test_phase1_collapse() {
		// Phase 1: Wipe/Scroll temporarily render as Dissolve; Fade/Dissolve/None unchanged.
		TS_ASSERT_EQUALS(effectiveFamily(kFxWipe),     kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxScroll),   kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxFade),     kFxFade);
		TS_ASSERT_EQUALS(effectiveFamily(kFxDissolve), kFxDissolve);
		TS_ASSERT_EQUALS(effectiveFamily(kFxNone),     kFxNone);
	}
};
