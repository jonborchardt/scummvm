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

#include "sci/roger/roger_effects.h"

namespace Sci {
namespace Roger {

TransitionFamily transitionFamilyFor(int sciType) {
	switch (sciType) {
	case 10:                      return kFxFade;     // FADEPALETTE
	case 8: case 9:               return kFxDissolve; // BLOCKS, PIXELATION
	case 2: case 3: case 4: case 5:                   // STRAIGHT_FROM_*
	case 0: case 1: case 6: case 7:                   // ROLL_*_FROMCENTER / DIAGONAL
	case 300: case 301:           return kFxWipe;     // ROLL_*_TOCENTER
	case 11: case 12: case 13: case 14: return kFxScroll; // SCROLL_*
	case 15: case 100:            return kFxNone;     // NONE_LONGBOW, NONE
	default:                      return kFxFade;     // unknown -> safe default
	}
}

TransitionFamily effectiveFamily(TransitionFamily f) {
	if (f == kFxWipe || f == kFxScroll)
		return kFxDissolve; // Phase 1 collapse
	return f;
}

int defaultDurationMs(TransitionFamily f) {
	switch (f) {
	case kFxFade:     return 250;
	case kFxDissolve: return 350;
	case kFxWipe:     return 300;
	case kFxScroll:   return 300;
	default:          return 0;
	}
}

} // namespace Roger
} // namespace Sci
