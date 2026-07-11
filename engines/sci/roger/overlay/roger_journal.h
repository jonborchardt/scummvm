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

#ifndef SCI_ROGER_ROGER_JOURNAL_H
#define SCI_ROGER_ROGER_JOURNAL_H

#include "common/array.h"
#include "common/rect.h"
#include "sci/roger/overlay/roger_ui_layer.h"
#include "sci/roger/overlay/roger_tokens.h"

namespace Sci {
namespace Roger {

// Forward-declare the pure compositor helper so RogerJournal can expose a thin
// forwarder without a circular include (roger_compositor.h already includes this
// file via roger_ui_layer.h, so we cannot include roger_compositor.h here).
void dedupeGenericTextElements(Common::Array<UiElement>&, uint32);

// True when a newly appended op replaces `older` outright: same type, and the new
// rect contains the old one, within the SAME window bracket (windowId). Models
// native immediate-mode redraw-in-place (SCI erases/overprints the box before or
// while redrawing it). Pure: unit-testable.
bool opSupersedes(const UiElement &newer, const UiElement &older);

// An op whose fill hides everything beneath its rect (prune cover test). Pure.
bool opIsOpaque(const UiElement &e);

// Append-only draw journal: ops render in append order (native "last draw wins").
// Lifetime is structural -- erase-rect containment and window brackets own it;
// clearToken survives ONLY for the explicit singletons (status bar 0x10000000,
// menu dropdown 0x20000000, frame box 0x70000000).
class RogerJournal {
public:
	void append(const UiElement &e);
	const Common::Array<UiElement> &ops() const { return _ops; }
	bool empty() const { return _ops.empty(); }
	void clear() {
		_ops.clear();
		_brackets.clear();
		_checkpoints.clear();
		_nextPruneAt = 0;
	}
	bool clearToken(uint32 token, Common::Array<Common::Rect> *removedNativeRects = nullptr);
	bool eraseContained(const Common::Rect &r, Common::Array<Common::Rect> *removedNativeRects = nullptr,
	                    bool spareSaveUnderExempt = false);
	// Drop ops fully covered by a LATER opaque op. Called automatically by append()
	// past kJournalPruneThreshold; safe to call any time (render output unchanged).
	void prune();

	// Window brackets: openWindow/removeWindow are THE UI lifetime model.
	// append() tags each op with the innermost open bracket containing its rect;
	// closeBracket drops the bracket and every op tagged with it.
	void openBracket(uint32 windowId, const Common::Rect &winRect);
	bool closeBracket(uint32 windowId, Common::Array<Common::Rect> *removedNativeRects = nullptr);

	// Cross-hook dedup: the same native text draw is seen by both the semantic control
	// hook and the generic GfxText16::Box hook. Drop the generic re-capture when the
	// control already claimed it -- not an ordering or lifetime concern.
	void dedupeGenericText(uint32 genericNamespace) { dedupeGenericTextElements(_ops, genericNamespace); }

	uint32 seqNow() const { return _seq; }
	void checkpoint(uint32 handleToken, const Common::Rect &savedRect);
	void dropCheckpoint(uint32 handleToken);
	// Remove every op appended after the handle's checkpoint whose rect lies inside
	// restoredRect (SCI just overwrote those pixels with the saved background).
	// Returns true iff the handle had a checkpoint; the checkpoint is consumed.
	bool rollback(uint32 handleToken, const Common::Rect &restoredRect,
	              Common::Array<Common::Rect> *removedNativeRects = nullptr);

private:
	struct Bracket { uint32 id; Common::Rect rect; };
	struct Checkpoint { uint32 handle; uint32 seq; Common::Rect rect; };
	Common::Array<UiElement> _ops;
	Common::Array<Bracket> _brackets; // stack order: last = innermost
	Common::Array<Checkpoint> _checkpoints;
	uint32 _seq = 0;
	uint _nextPruneAt = 0; // amortized re-prune floor (see append)
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_JOURNAL_H
