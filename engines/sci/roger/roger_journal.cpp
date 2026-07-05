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

#include "sci/roger/roger_journal.h"

namespace Sci {
namespace Roger {

bool opSupersedes(const UiElement &newer, const UiElement &older) {
	if (newer.type != older.type)
		return false;
	return newer.nativeRect.contains(older.nativeRect);
}

void RogerJournal::append(const UiElement &e) {
	UiElement tagged = e;
	if (tagged.windowId == 0) {
		for (uint i = _brackets.size(); i-- > 0;) {
			if (_brackets[i].rect.contains(tagged.nativeRect)) {
				tagged.windowId = _brackets[i].id;
				break;
			}
		}
	}
	for (uint i = 0; i < _ops.size();) {
		if (opSupersedes(tagged, _ops[i]))
			_ops.remove_at(i);
		else
			i++;
	}
	_ops.push_back(tagged);
}

bool RogerJournal::clearToken(uint32 token, Common::Array<Common::Rect> *removedNativeRects) {
	bool removed = false;
	for (uint i = 0; i < _ops.size();) {
		if (_ops[i].token == token) {
			if (removedNativeRects)
				removedNativeRects->push_back(_ops[i].nativeRect);
			_ops.remove_at(i);
			removed = true;
		} else {
			i++;
		}
	}
	return removed;
}

void RogerJournal::openBracket(uint32 windowId, const Common::Rect &winRect) {
	Bracket b; b.id = windowId; b.rect = winRect;
	_brackets.push_back(b);
}

bool RogerJournal::closeBracket(uint32 windowId, Common::Array<Common::Rect> *removedNativeRects) {
	for (uint i = _brackets.size(); i-- > 0;) {
		if (_brackets[i].id == windowId)
			_brackets.remove_at(i);
	}
	bool removed = false;
	for (uint i = 0; i < _ops.size();) {
		if (_ops[i].windowId == windowId && windowId != 0) {
			if (removedNativeRects)
				removedNativeRects->push_back(_ops[i].nativeRect);
			_ops.remove_at(i);
			removed = true;
		} else {
			i++;
		}
	}
	return removed;
}

} // namespace Roger
} // namespace Sci
