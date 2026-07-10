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

#ifndef SCI_ROGER_UI_ROGER_WIDGETS_H
#define SCI_ROGER_UI_ROGER_WIDGETS_H

// Minimal immediate-mode widget primitives shared by the Roger debug panels
// (the F12 tune panel in utils/tunepanel/ and the Studio in utils/studio/).
// Deliberately tiny and neutral: a rect+label+id widget record, the packed
// widget-id encoding, and hit-testing. Each panel defines its OWN widget-kind
// enum; this module never names one. SCI-free and engine-free.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

struct PanelWidget {
	Common::Rect rect;      // panel-local coords (each panel picks its space)
	uint32 id;
	Common::String label;
	bool on;                // toggled/active state (drawn highlighted)
	bool enabled;
};

// Packed widget id: (kind << 16) | (index & 0xffff). Kind 0 is reserved as
// "none" by every panel's kind enum (kWidNone / kTuneNone).
uint32 widId(int kind, int index = 0);
int widKind(uint32 id);
int widIndex(uint32 id);

// First widget whose rect contains (x, y) and is enabled; 0 (= the reserved
// "none" kind) if none.
uint32 hitTestWidgets(const Common::Array<PanelWidget> &widgets, int x, int y);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_UI_ROGER_WIDGETS_H
