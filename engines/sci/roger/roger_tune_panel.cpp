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

#include "sci/roger/roger_tune_panel.h"
#include "sci/roger/roger_view_scaler.h"

namespace Sci {
namespace Roger {

bool tunePassesEqual(const Common::Array<int> &a, const Common::Array<int> &b) {
	if (a.size() != b.size())
		return false;
	for (uint i = 0; i < a.size(); i++)
		if (a[i] != b[i])
			return false;
	return true;
}

static void addTuneWidget(Common::Array<StudioWidget> &out, int kind, int index,
                          const Common::Rect &r, const Common::String &label,
                          bool on, bool enabled = true) {
	StudioWidget w;
	w.rect = r;
	w.id = widId(kind, index);
	w.label = label;
	w.on = on;
	w.enabled = enabled;
	out.push_back(w);
}

void buildTunePanel(const TunePanelState &st, Common::Array<StudioWidget> &out) {
	out.clear();
	const Common::Rect p = tunePanelRect();
	const int x0 = p.left + 2, x1 = p.right - 2;

	// Title row: close box top-right.
	int y = p.top + 2;
	addTuneWidget(out, kTuneClose, 0, Common::Rect(x1 - 10, y, x1, y + 10), "x", false);
	y += 12;

	// Variant rows (top-flowing).
	for (int i = 0; i < viewScalerPresetCount(); i++) {
		addTuneWidget(out, kTuneVariantRow, i, Common::Rect(x0, y, x1, y + 10),
		              viewScalerPreset(i).label, i == st.variant);
		y += 11;
	}

	// Bottom-anchored rows (fixed coordinates regardless of chip count):
	//   ops row    at bottom-36, clear/reset/apply at bottom-24,
	//   status text (drawn, not a widget) at bottom-12.
	const int yOps = p.bottom - 36;
	const int yCra = p.bottom - 24;

	// Chip strip flows between the variants and the ops row, 7 per row.
	int cx = x0, cy = y + 3;
	for (uint i = 0; i < st.stagedPasses.size(); i++) {
		if (cx + 11 > x1) { cx = x0; cy += 11; }
		if (cy + 10 > yOps - 2)
			break; // out of space; extra chips not clickable (debug tool)
		const char *lbl = st.stagedPasses[i] == 2 ? "f" : st.stagedPasses[i] == 1 ? "l" : "a";
		addTuneWidget(out, kTuneChip, (int)i, Common::Rect(cx, cy, cx + 10, cy + 10),
		              lbl, (int)i == st.selectedChip);
		cx += 11;
	}

	// Chip ops.
	const bool sel = st.selectedChip >= 0 && st.selectedChip < (int)st.stagedPasses.size();
	const struct { int kind; const char *lbl; bool en; } ops[] = {
		{ kTuneChipX,    "x",  sel  }, { kTuneChipLeft, "<",  sel  },
		{ kTuneChipRight, ">", sel  }, { kTuneChipAddF, "+f", true },
		{ kTuneChipAddL, "+l", true }, { kTuneChipAddA, "+a", true },
	};
	cx = x0;
	for (int i = 0; i < 6; i++) {
		addTuneWidget(out, ops[i].kind, 0, Common::Rect(cx, yOps, cx + 13, yOps + 10),
		              ops[i].lbl, false, ops[i].en);
		cx += 14;
	}

	// Clear / Reset / Apply. Apply highlights while edits are pending.
	addTuneWidget(out, kTuneClear, 0, Common::Rect(x0, yCra, x0 + 28, yCra + 10), "clear", false);
	addTuneWidget(out, kTuneReset, 0, Common::Rect(x0 + 30, yCra, x0 + 58, yCra + 10), "reset", false);
	addTuneWidget(out, kTuneApply, 0, Common::Rect(x0 + 60, yCra, x1, yCra + 10), "apply", tunePending(st));
}

Common::String tuneStatusLine(const TunePanelState &st) {
	Common::String s = omyacPassStamp(st.stagedPasses);
	if (tunePending(st))
		s += " *";
	s += Common::String::format(" %ums", st.lastGenMs);
	return s;
}

void drawTunePanel(Graphics::ManagedSurface &scene, const Common::Rect &gameRect,
                   const TunePanelState &st, const Common::Array<StudioWidget> &widgets) {
	// Implemented in the next commit (panel drawing).
	(void)scene; (void)gameRect; (void)st; (void)widgets;
}

} // namespace Roger
} // namespace Sci
