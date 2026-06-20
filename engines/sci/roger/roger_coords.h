/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCI_ROGER_ROGER_COORDS_H
#define SCI_ROGER_ROGER_COORDS_H

#include "common/rect.h"

namespace Sci {
namespace Roger {

/**
 * Scale a 320x200 SCI cel rect into hires overlay pixel coordinates.
 *
 * scaleX = overlayW / 320.0, scaleY = overlayH / 200.0
 */
inline Common::Rect sciCelRectToOverlay(const Common::Rect &r, int overlayW, int overlayH) {
	const float sx = (float)overlayW / 320.0f;
	const float sy = (float)overlayH / 200.0f;
	return Common::Rect((int16)(r.left * sx), (int16)(r.top * sy),
	                    (int16)(r.right * sx), (int16)(r.bottom * sy));
}

/**
 * Return the SCI0 priority band (0..14) for a given y coordinate.
 *
 * Band 0 for y < 42 (above the horizon); bands 1..14 are distributed
 * evenly across the range [42, gameHeight-1].
 */
inline int sciPriorityBand(int y, int gameHeight = 190) {
	const int topBand = 42;
	if (y < topBand)
		return 0;
	const int range = gameHeight - topBand;
	if (range <= 0)
		return 14;
	int band = ((y - topBand) * 14) / range + 1;
	if (band > 14) band = 14;
	return band;
}

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_COORDS_H
