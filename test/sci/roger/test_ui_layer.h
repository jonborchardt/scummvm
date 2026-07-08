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
#include "sci/roger/overlay/roger_journal.h"

using namespace Sci::Roger;

static UiElement mkText(const Common::Rect &r, const char *t, uint32 token) {
	UiElement e;
	e.type = kUiText;
	e.nativeRect = r;
	e.text = t;
	e.token = token;
	return e;
}

class TestUiLayer : public CxxTest::TestSuite {
public:
	void test_push_appends() {
		RogerJournal j;
		TS_ASSERT(j.empty());
		j.append(mkText(Common::Rect(0, 0, 10, 10), "a", 1));
		j.append(mkText(Common::Rect(0, 20, 10, 30), "b", 1));
		TS_ASSERT_EQUALS(j.ops().size(), 2u);
	}

	void test_clear_token_removes_only_matching() {
		RogerJournal j;
		UiElement a = mkText(Common::Rect(0, 0, 10, 10), "a", 1); a.token = 1;
		UiElement b = mkText(Common::Rect(0, 20, 10, 30), "b", 2); b.token = 2;
		j.append(a);
		j.append(b);
		j.clearToken(1);
		TS_ASSERT_EQUALS(j.ops().size(), 1u);
		TS_ASSERT_EQUALS(j.ops()[0].token, 2u);
	}

	void test_clear_all() {
		RogerJournal j;
		j.append(mkText(Common::Rect(0, 0, 10, 10), "a", 1));
		j.clear();
		TS_ASSERT(j.empty());
	}

	void test_ui_element_native_metrics_default_to_zero() {
		Sci::Roger::UiElement e;
		TS_ASSERT_EQUALS(e.nativeFontH, 0);
		TS_ASSERT_EQUALS(e.nativeTextW, 0);
	}
};
