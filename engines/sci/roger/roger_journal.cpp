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

static const uint kJournalPruneThreshold = 256;

// Persistent overlay singletons whose lifetime is owned by their token (explicit
// clearToken / reapply), NOT by any SCI save-under. A bitsRestore over their strip
// must not roll them back: the status banner (0x10000000) is re-drawn into the top
// strip whenever SCI redraws the menu bar, so it postdates a bar save-under's
// checkpoint and would otherwise be dropped on menu close (the enhanced banner
// reverting to the native bitmap font); the frame box (0x70000000) is overlay-only,
// re-pushed each move, and no save-under owns it. The menu dropdown (0x20000000) is
// deliberately NOT spared — its OWN save-under restore is exactly what must remove it.
static bool isSaveUnderExemptSingleton(uint32 token) {
	return token == 0x10000000u || token == 0x70000000u;
}

bool opIsOpaque(const UiElement &e) {
	// Filled windows/buttons/edits hide what they cover. Text, frame boxes
	// (backColor -1), and icons (alpha cels) do not.
	if (e.type == kUiWindow || e.type == kUiButton || e.type == kUiTextEdit)
		return e.backColor >= 0;
	return false;
}

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
	tagged.seq = ++_seq;
	_ops.push_back(tagged);
	if (_ops.size() > kJournalPruneThreshold)
		prune();
}

bool RogerJournal::eraseContained(const Common::Rect &r, Common::Array<Common::Rect> *removedNativeRects) {
	bool removed = false;
	for (uint i = 0; i < _ops.size();) {
		if (r.contains(_ops[i].nativeRect)) {
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

void RogerJournal::prune() {
	for (uint i = 0; i < _ops.size();) {
		bool covered = false;
		for (uint k = i + 1; k < _ops.size(); k++) {
			if (opIsOpaque(_ops[k]) && _ops[k].nativeRect.contains(_ops[i].nativeRect)) {
				covered = true;
				break;
			}
		}
		if (covered)
			_ops.remove_at(i);
		else
			i++;
	}
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

void RogerJournal::checkpoint(uint32 handleToken, const Common::Rect &savedRect) {
	dropCheckpoint(handleToken); // handles are reused; latest save wins
	Checkpoint c; c.handle = handleToken; c.seq = _seq; c.rect = savedRect;
	_checkpoints.push_back(c);
}

void RogerJournal::dropCheckpoint(uint32 handleToken) {
	for (uint i = _checkpoints.size(); i-- > 0;) {
		if (_checkpoints[i].handle == handleToken)
			_checkpoints.remove_at(i);
	}
}

bool RogerJournal::rollback(uint32 handleToken, const Common::Rect &restoredRect,
                            Common::Array<Common::Rect> *removedNativeRects) {
	int found = -1;
	for (uint i = 0; i < _checkpoints.size(); i++) {
		if (_checkpoints[i].handle == handleToken) { found = (int)i; break; }
	}
	if (found < 0)
		return false;
	const uint32 mark = _checkpoints[found].seq;
	_checkpoints.remove_at(found);
	for (uint i = 0; i < _ops.size();) {
		if (_ops[i].seq > mark && restoredRect.contains(_ops[i].nativeRect) &&
		    !isSaveUnderExemptSingleton(_ops[i].token)) {
			if (removedNativeRects)
				removedNativeRects->push_back(_ops[i].nativeRect);
			_ops.remove_at(i);
		} else {
			i++;
		}
	}
	return true;
}

} // namespace Roger
} // namespace Sci
