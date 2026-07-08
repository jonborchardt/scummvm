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

#include "sci/roger/roger_widgets.h"

namespace Sci {
namespace Roger {

uint32 widId(int kind, int index) {
	return ((uint32)kind << 16) | ((uint32)index & 0xffff);
}
int widKind(uint32 id) { return (int)(id >> 16); }
int widIndex(uint32 id) { return (int)(id & 0xffff); }

uint32 hitTestWidgets(const Common::Array<PanelWidget> &widgets, int x, int y) {
	for (uint i = 0; i < widgets.size(); i++) {
		if (!widgets[i].enabled)
			continue;
		const Common::Rect &r = widgets[i].rect;
		if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
			return widgets[i].id;
	}
	return 0; // the reserved "none" kind
}

} // namespace Roger
} // namespace Sci
