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

#ifndef SCI_ROGER_NULL_ROGER_ART_PROVIDER_H
#define SCI_ROGER_NULL_ROGER_ART_PROVIDER_H

#include "sci/sci_gfx_observer.h" // GuiResourceId typedef

namespace Sci {

// Baseline no-op provider for unit tests: hasBackground always false.
// Standalone (no SciGfxObserver base needed  --  the test only exercises
// hasBackground, which is a fork-only FileRogerArtProvider method now,
// not an observer virtual).
class NullRogerArtProvider {
public:
	bool hasBackground(GuiResourceId /*pictureId*/) const { return false; }
};

} // namespace Sci

#endif // SCI_ROGER_NULL_ROGER_ART_PROVIDER_H
