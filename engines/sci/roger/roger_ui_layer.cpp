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

#include "sci/roger/roger_ui_layer.h"

namespace Sci {
namespace Roger {

void RogerUiLayer::push(const UiElement &e) {
	// Replace an existing element with the same identity, but APPEND the replacement:
	// native SCI is immediate-mode, so the most recent draw is on top. Keeping the
	// original list position let an overlapping neighbor that was pushed later keep
	// painting over a re-drawn element (the QFG1 char-sheet selection frame lost its
	// bottom edge to the next row's blank cel, which overlaps it by one native px).
	for (uint i = 0; i < _elems.size(); i++) {
		if (_elems[i].type == e.type && _elems[i].token == e.token &&
		    _elems[i].nativeRect == e.nativeRect) {
			_elems.remove_at(i);
			break;
		}
	}
	_elems.push_back(e);
}

bool RogerUiLayer::clearToken(uint32 token, Common::Array<Common::Rect> *removedNativeRects) {
	bool removed = false;
	for (uint i = 0; i < _elems.size();) {
		if (_elems[i].token == token) {
			if (removedNativeRects)
				removedNativeRects->push_back(_elems[i].nativeRect);
			_elems.remove_at(i);
			removed = true;
		} else {
			i++;
		}
	}
	return removed;
}

} // namespace Roger
} // namespace Sci
