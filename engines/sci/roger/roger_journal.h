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
#include "sci/roger/roger_ui_layer.h"

namespace Sci {
namespace Roger {

// True when a newly appended op replaces `older` outright: same type, and the new
// rect contains the old one. Models native immediate-mode redraw-in-place (SCI
// erases/overprints the box before or while redrawing it). Pure: unit-testable.
bool opSupersedes(const UiElement &newer, const UiElement &older);

// Append-only draw journal: ops render in append order (native "last draw wins").
// Lifetime is structural — erase-rect containment and window brackets (Tasks 2-3);
// clearToken survives ONLY for the explicit singletons (status bar 0x10000000,
// menu dropdown 0x20000000, frame box 0x70000000).
class RogerJournal {
public:
	void append(const UiElement &e);
	const Common::Array<UiElement> &ops() const { return _ops; }
	bool empty() const { return _ops.empty(); }
	void clear() { _ops.clear(); }
	bool clearToken(uint32 token, Common::Array<Common::Rect> *removedNativeRects = nullptr);

private:
	Common::Array<UiElement> _ops;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_JOURNAL_H
