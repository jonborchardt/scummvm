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

#include "sci/roger/gen/roger_passes.h"

#include "common/textconsole.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

// Known-good pass sequences, judged with the Eye Exam (utils/eyetest) â€”
// multi-scene GA runs + showdowns, 2026-07-07. Order = user's ranking,
// best-first. The picker offers these as one-click suggestions.
static const GoodPassPattern kGoodPassPatterns[] = {
	{"fffflffaaa", "eye-exam finalist (2026-07-07)"},
	{"affffflaaa", "shootout1, qfg11 (2026-07-07)"},
	{"ffffffflff", "sq3 multi-scene winner (2026-07-07)"},
	{"fffffflaaa", "run-1 winner, pic 2 (2026-07-06)"},
	{"ffflffaaaa", "original hand-tuned default"},
};

int goodPassPatternCount() {
	return ARRAYSIZE(kGoodPassPatterns);
}

const GoodPassPattern &goodPassPattern(int i) {
	return kGoodPassPatterns[CLIP(i, 0, ARRAYSIZE(kGoodPassPatterns) - 1)];
}

// THE default best â€” the single swap point. Everything that renders with an
// unset roger_omyac_passes key follows this: the game (defaultPasses()), the
// picker's shown value, Studio slot seeds, and the Eye Exam's base sequence.
// To promote a new winner, change this to another kGoodPassPatterns compact.
const char *const kDefaultPassString = kGoodPassPatterns[1].compact;

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
				// 1=line, 0=all) â€” the same ints the cache key stamps as pNpN.
				if (tok == "fill" || tok == "f" || tok == "2")
					passes.push_back(2);
				else if (tok == "line" || tok == "l" || tok == "1")
					passes.push_back(1);
				else if (tok == "all" || tok == "a" || tok == "0")
					passes.push_back(0);
				else
					warning("ROGER: roger_omyac_passes token '%s' not recognized "
					        "(use fill/f/2, line/l/1, all/a/0) â€” skipped", tok.c_str());
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

bool passesEqual(const Common::Array<int> &a, const Common::Array<int> &b) {
	if (a.size() != b.size())
		return false;
	for (uint i = 0; i < a.size(); i++)
		if (a[i] != b[i])
			return false;
	return true;
}

} // namespace Roger
} // namespace Sci
