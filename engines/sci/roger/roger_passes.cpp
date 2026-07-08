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

#include "sci/roger/roger_passes.h"

#include "common/textconsole.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

const char *const kDefaultPassString = "ffflffaaaa";

// True when every char is in the compact vocabulary. Separators are not in
// the set, so a separated legacy string can never read as compact.
static bool isCompactForm(const Common::String &s) {
	if (s.empty())
		return false;
	for (uint i = 0; i < s.size(); i++) {
		const char c = s[i];
		if (c != 'f' && c != 'l' && c != 'a' && c != '2' && c != '1' && c != '0')
			return false;
	}
	return true;
}

Common::Array<int> parsePassString(const Common::String &s) {
	Common::Array<int> passes;
	// Trim leading/trailing whitespace so e.g. "ffflffaaaa " from the picker
	// edit field still parses as compact rather than falling through to the
	// legacy tokenizer as a single unknown token.
	Common::String t = s;
	t.trim();
	// Exact whole-word keywords: match these BEFORE the compact-form check
	// because "all" consists entirely of compact-vocabulary chars and would
	// otherwise parse as [0,1,1] instead of the single all-pass [0].
	if (t == "fill") {
		passes.push_back(2);
		return passes;
	}
	if (t == "line") {
		passes.push_back(1);
		return passes;
	}
	if (t == "all") {
		passes.push_back(0);
		return passes;
	}
	if (isCompactForm(t)) {
		for (uint i = 0; i < t.size(); i++) {
			const char c = t[i];
			passes.push_back((c == 'f' || c == '2') ? 2 : (c == 'l' || c == '1') ? 1 : 0);
		}
		return passes;
	}
	// Legacy separated-token form (the pre-2026-07 parser, verbatim).
	Common::String tok;
	for (uint i = 0; i <= t.size(); ++i) {
		const char c = (i < t.size()) ? t[i] : '\0';
		if (c == ',' || c == ' ' || c == '\t' || c == '\0') {
			if (!tok.empty()) {
				// Numeric tokens are the raw renderOmyac mode values (2=fill,
				// 1=line, 0=all) — the same ints the cache key stamps as pNpN.
				if (tok == "fill" || tok == "f" || tok == "2")
					passes.push_back(2);
				else if (tok == "line" || tok == "l" || tok == "1")
					passes.push_back(1);
				else if (tok == "all" || tok == "a" || tok == "0")
					passes.push_back(0);
				else
					warning("ROGER: roger_omyac_passes token '%s' not recognized "
					        "(use fill/f/2, line/l/1, all/a/0) — skipped", tok.c_str());
				tok.clear();
			}
		} else {
			tok += c;
		}
	}
	return passes;
}

Common::String passString(const Common::Array<int> &passes) {
	Common::String s;
	for (uint i = 0; i < passes.size(); i++)
		s += (passes[i] == 2) ? 'f' : (passes[i] == 1) ? 'l' : 'a';
	return s;
}

Common::String omyacPassStamp(const Common::Array<int> &passes) {
	if (passes.empty())
		return "none";
	return passString(passes);
}

Common::Array<int> effectivePasses(bool hasKey, const Common::String &s) {
	if (!hasKey)
		return parsePassString(kDefaultPassString);
	// hasKey + empty string = wireframe (zero passes).
	return parsePassString(s);
}

void passInsertAfter(Common::Array<int> &passes, int &selected, int passVal) {
	int at = (selected < 0 || selected >= (int)passes.size())
	         ? (int)passes.size() : selected + 1;
	passes.insert_at(at, passVal);
	selected = at;
}

void passRemoveAt(Common::Array<int> &passes, int &selected) {
	if (selected < 0 || selected >= (int)passes.size())
		return;
	passes.remove_at(selected);
	if (passes.empty())
		selected = -1;
	else if (selected >= (int)passes.size())
		selected = (int)passes.size() - 1;
}

bool passMove(Common::Array<int> &passes, int &selected, int dir) {
	if (dir != -1 && dir != 1)
		return false;
	if (selected < 0 || selected >= (int)passes.size())
		return false;
	const int to = selected + dir;
	if (to < 0 || to >= (int)passes.size())
		return false;
	SWAP(passes[selected], passes[to]);
	selected = to;
	return true;
}

} // namespace Roger
} // namespace Sci
