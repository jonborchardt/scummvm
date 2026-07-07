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

} // namespace Roger
} // namespace Sci
