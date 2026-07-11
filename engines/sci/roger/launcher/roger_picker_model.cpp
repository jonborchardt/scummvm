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

#include "sci/roger/launcher/roger_picker_model.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

Common::String cacheMarkerName(const Common::String &gameId, int version,
                               const Common::String &passStamp) {
	return Common::String::format("%s.done.v%d.%s.marker",
	                              gameId.c_str(), version, passStamp.c_str());
}

void splitGameDescription(const Common::String &desc,
                          Common::String &outTitle, Common::String &outSubtitle) {
	outTitle = desc;
	outSubtitle.clear();
	if (desc.size() < 3 || desc.lastChar() != ')')
		return;
	int open = -1;
	for (int i = (int)desc.size() - 2; i >= 0; --i) {
		if (desc[i] == '(') {
			open = i;
			break;
		}
	}
	if (open <= 0)
		return;
	outSubtitle = Common::String(desc.c_str() + open + 1, desc.size() - open - 2);
	outTitle = Common::String(desc.c_str(), open);
	outTitle.trim();
}

PickerLayout layoutPicker(int w, int h, int gameCount, int scrollOffset) {
	PickerLayout l;
	const int M   = h / 24;
	const int pad = MAX(2, h / 80);

	l.titleBox = Common::Rect(M, M, M + w / 2, M + h / 14);
	l.descBox  = Common::Rect(M, l.titleBox.bottom + pad, w - M,
	                           l.titleBox.bottom + pad + h / 18);

	const int listTop = l.descBox.bottom + M / 2;
	l.listPanel = Common::Rect(M, listTop, w - M, listTop + (h * 40) / 100);

	const int rowH = h / 11;
	const int rowGap = pad;
	const int rowsTop = listTop + 2 * pad;
	l.rowsVisible = MAX(1, (l.listPanel.bottom - pad - rowsTop + rowGap) / (rowH + rowGap));
	const int shown = CLIP(gameCount - scrollOffset, 0, l.rowsVisible);
	for (int i = 0; i < shown; ++i) {
		PickerRowLayout r;
		const int top = rowsTop + i * (rowH + rowGap);
		r.card = Common::Rect(M + 2 * pad, top, w - M - 2 * pad, top + rowH);
		const int btnH = (rowH * 44) / 100;
		const int btnY = top + (rowH - btnH) / 2;
		r.remove   = Common::Rect(r.card.right - 2 * pad - w / 16, btnY,
		                          r.card.right - 2 * pad, btnY + btnH);
		r.precache = Common::Rect(r.remove.left - 2 * pad - w / 13, btnY,
		                          r.remove.left - 2 * pad, btnY + btnH);
		const int badgeLeft = r.card.left + (r.card.width() * 42) / 100;
		r.badge = Common::Rect(badgeLeft, top + pad,
		                       MAX(badgeLeft, (int)r.precache.left - pad), top + rowH - pad);
		l.rows.push_back(r);
	}

	l.addGame = Common::Rect(w / 2 - w / 14, l.listPanel.bottom + 2 * pad,
	                         w / 2 + w / 14, l.listPanel.bottom + 2 * pad + h / 24);

	// 17% (not more): at h=1589 the panel bottom (~1418) must stay above the
	// bottom-anchored launch button top (~1424) — asserted by the layout test.
	const int setTop = l.addGame.bottom + 2 * pad;
	l.settingsPanel = Common::Rect(M, setTop, w - M, setTop + (h * 17) / 100);
	l.settingsTitle = Common::Rect(M + 2 * pad, setTop + pad, w - M - 2 * pad,
	                               setTop + pad + h / 32);
	const int rH = h / 24;
	const int labelW = w / 6;
	const int fieldW = w / 4;
	int ry = l.settingsTitle.bottom + pad;
	l.passesLabel = Common::Rect(M + 2 * pad, ry, M + 2 * pad + labelW, ry + rH);
	l.passesField = Common::Rect(l.passesLabel.right + 2 * pad, ry,
	                             l.passesLabel.right + 2 * pad + fieldW, ry + rH);
	l.passesHint  = Common::Rect(l.passesField.right + 2 * pad, ry, w - M - 2 * pad, ry + rH);
	ry += rH + pad;
	l.debugLabel  = Common::Rect(M + 2 * pad, ry, M + 2 * pad + labelW, ry + rH);
	l.debugToggle = Common::Rect(l.debugLabel.right + 2 * pad, ry,
	                             l.debugLabel.right + 2 * pad + rH * 2, ry + rH);
	l.debugHint   = Common::Rect(l.debugToggle.right + 2 * pad, ry, w - M - 2 * pad, ry + rH);

	l.launch = Common::Rect(w - M - w / 6, h - M - h / 16, w - M, h - M);
	return l;
}

Common::Rect passOptionRect(const PickerLayout &l, int index, int count, int canvasH) {
	const int oh = l.passesField.height();
	int top0 = l.passesField.bottom;
	const int overflow = top0 + count * oh - canvasH;
	if (overflow > 0)
		top0 -= overflow;
	return Common::Rect(l.passesField.left, top0 + index * oh,
	                    l.passesHint.right, top0 + (index + 1) * oh);
}

int clampScroll(int scroll, int gameCount, int rowsVisible) {
	return CLIP(scroll, 0, MAX(0, gameCount - rowsVisible));
}

bool standalonePickerWanted(bool hasNoLauncherKey, bool noLauncherValue,
                            bool envNoLauncher) {
	if (envNoLauncher)
		return false;
	if (hasNoLauncherKey && noLauncherValue)
		return false;
	return true;
}

} // namespace Roger
} // namespace Sci
