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
static const uint kPruneRetryStep = 32;

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
	// Window-scoped: ops belonging to DIFFERENT windows never supersede each other.
	// The covering window's pixels overprint, but the underlying window's content
	// is restored by its save-under on close (checkpoint rollback), and its ops die
	// with their own bracket — an in-place replace here would delete them forever
	// (Phase 2 final review, Important #1). Same window — including both on the
	// picture port, id 0 — keeps native redraw-in-place semantics.
	if (newer.windowId != older.windowId)
		return false;
	// Cross-hook duplicate guard: the SAME native text draw is seen by BOTH the semantic
	// control hook (uiPushText, 0x40000000 namespace: accurate native font height + a
	// single-line width cap) and the generic GfxText16::Box hook (onNativeText, 0x60000000
	// namespace: no width cap -> multi-line wrap-fit). They share the same rect, so a
	// geometry-only supersede would let whichever arrived LAST silently replace the other —
	// dropping the control copy and leaving the wrap-fit generic, which re-fits to a smaller
	// size on a later present (the dialog-text-size-flip bug). These two are reconciled by
	// dedupeGenericText (token-aware; keeps the control), NOT by append's supersede. Only
	// block the cross-namespace case; a same-namespace redraw (stat-value refresh) still
	// supersedes in place.
	const uint32 ns0 = newer.token & kTokenNamespaceMask;
	const uint32 ns1 = older.token & kTokenNamespaceMask;
	const bool controlVsGeneric =
	    (ns0 == kControlTokenNs && ns1 == kGenericTextTokenNs) || (ns0 == kGenericTextTokenNs && ns1 == kControlTokenNs);
	if (controlVsGeneric)
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
	// Amortized: when the journal is saturated with UNPRUNABLE ops, prune() finds
	// nothing and would otherwise run its O(n^2) scan on every append. Re-try only
	// every kPruneRetryStep appends past the threshold.
	if (_ops.size() > kJournalPruneThreshold && _ops.size() >= _nextPruneAt) {
		prune();
		_nextPruneAt = _ops.size() + kPruneRetryStep;
	}
}

bool RogerJournal::eraseContained(const Common::Rect &r, Common::Array<Common::Rect> *removedNativeRects,
                                  bool spareSaveUnderExempt) {
	bool removed = false;
	for (uint i = 0; i < _ops.size();) {
		if (r.contains(_ops[i].nativeRect) &&
		    !(spareSaveUnderExempt && isSaveUnderExemptSingleton(_ops[i].token))) {
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
