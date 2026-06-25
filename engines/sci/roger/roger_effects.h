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

#ifndef SCI_ROGER_ROGER_EFFECTS_H
#define SCI_ROGER_ROGER_EFFECTS_H

namespace Sci {
namespace Roger {

enum TransitionFamily { kFxNone, kFxFade, kFxDissolve, kFxWipe, kFxScroll };

// Map an SCI transition type (transitions.h enum values) to an overlay family.
// Unknown values default to kFxFade (a safe, always-correct effect).
TransitionFamily transitionFamilyFor(int sciType);

// Phase-1 collapse: Wipe and Scroll are temporarily rendered as Dissolve until
// the directional reveals land (see plan "Out of scope"). Identity otherwise.
TransitionFamily effectiveFamily(TransitionFamily f);

// Default per-family effect duration in milliseconds (tuned from SCI's timing).
int defaultDurationMs(TransitionFamily f);

} // namespace Roger
} // namespace Sci
#endif
