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

#include "sci/roger/roger_eyetest_search.h"

namespace Sci {
namespace Roger {

EyeSeq eyeBaseSeq() {
	static const int base[kEyeSeqLen] = {2, 2, 2, 1, 2, 2, 0, 0, 0, 0}; // f f f l f f a a a a
	EyeSeq s;
	for (int i = 0; i < kEyeSeqLen; i++)
		s.push_back(base[i]);
	return s;
}

char eyePassChar(int v) {
	switch (v) {
	case 2:  return 'f';
	case 1:  return 'l';
	default: return 'a';
	}
}

Common::String eyeSeqCompact(const EyeSeq &s) {
	Common::String out;
	for (uint i = 0; i < s.size(); i++)
		out += eyePassChar(s[i]);
	return out;
}

EyeSeq eyeRandomSeq(EyeRng &rng) {
	EyeSeq s;
	for (int i = 0; i < kEyeSeqLen; i++)
		s.push_back((int)rng.below(3)); // 0/1/2 are exactly the three pass values
	return s;
}

EyeSeq eyeMutate(const EyeSeq &src, EyeRng &rng, Common::String &outDesc) {
	// Position-count distribution: 60/25/10/5% for 1/2/3/4-5.
	const uint32 roll = rng.below(100);
	int count;
	if (roll < 60)
		count = 1;
	else if (roll < 85)
		count = 2;
	else if (roll < 95)
		count = 3;
	else
		count = 4 + (int)rng.below(2);

	bool picked[kEyeSeqLen] = {};
	for (int n = 0; n < count; n++) {
		int p;
		do {
			p = (int)rng.below(kEyeSeqLen);
		} while (picked[p]);
		picked[p] = true;
	}

	EyeSeq out = src;
	outDesc = "mut";
	for (int p = 0; p < kEyeSeqLen; p++) {
		if (!picked[p])
			continue;
		// Always switch to one of the OTHER two pass values.
		int alt[2];
		int na = 0;
		for (int v = 0; v < 3; v++)
			if (v != src[p])
				alt[na++] = v;
		out[p] = alt[rng.below(2)];
		outDesc += Common::String::format("_pos%02d_%c_to_%c",
			p, eyePassChar(src[p]), eyePassChar(out[p]));
	}
	return out;
}

EyeSeq eyeCrossover(const EyeSeq &a, const EyeSeq &b, EyeRng &rng) {
	EyeSeq out;
	for (int i = 0; i < kEyeSeqLen; i++)
		out.push_back(rng.below(2) ? b[i] : a[i]);
	return out;
}

} // namespace Roger
} // namespace Sci
