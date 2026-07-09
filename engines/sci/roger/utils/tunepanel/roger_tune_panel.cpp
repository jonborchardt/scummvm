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

#include "sci/roger/utils/tunepanel/roger_tune_panel.h"
#include "sci/roger/gen/roger_passes.h" // omyacPassStamp, goodPassPattern, parsePassString
#include "sci/roger/gen/roger_view_scaler.h"
#include "common/util.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "graphics/managed_surface.h"
#include "sci/roger/overlay/roger_coords.h"

namespace Sci {
namespace Roger {

bool tunePassesEqual(const Common::Array<int> &a, const Common::Array<int> &b) {
	return passesEqual(a, b); // neutral helper in roger_passes
}

static void addTuneWidget(Common::Array<PanelWidget> &out, int kind, int index,
                          const Common::Rect &r, const Common::String &label,
                          bool on, bool enabled = true) {
	PanelWidget w;
	w.rect = r;
	w.id = widId(kind, index);
	w.label = label;
	w.on = on;
	w.enabled = enabled;
	out.push_back(w);
}

int tuneViewModeCount() {
	return viewEnhanceModeCount(); // registry scalers + the trailing "nearest"
}

bool tuneViewModeIsNearest(int viewMode) {
	return viewEnhanceModeIsNearest(viewMode);
}

static Common::String tuneViewModeLabel(int viewMode) {
	return viewEnhanceModeLabel(viewMode);
}

static Common::String tunePicModeLabel(const TunePanelState &st) {
	if (tunePicModeIsNearest(st))
		return "nearest"; // the trailing zero-enhancement plate slot
	if (st.picModeSel < 0)
		return "-";
	return omyacPassStamp(st.picModes[st.picModeSel]);
}

void tuneSeedPicModes(TunePanelState &st) {
	if (!st.picModes.empty())
		return; // already populated (session-added modes survive reopen)
	for (int i = 0; i < goodPassPatternCount(); i++)
		st.picModes.push_back(parsePassString(goodPassPattern(i).compact));
	st.picModeSel = 0;
}

void tuneSelectOrAddMode(TunePanelState &st, const Common::Array<int> &passes) {
	for (uint i = 0; i < st.picModes.size(); i++) {
		if (tunePassesEqual(st.picModes[i], passes)) {
			st.picModeSel = (int)i;
			return;
		}
	}
	st.picModes.push_back(passes);
	st.picModeSel = (int)st.picModes.size() - 1;
}

void buildTunePanel(const TunePanelState &st, Common::Array<PanelWidget> &out) {
	out.clear();
	const Common::Rect p = tunePanelRect(st.leftSide);
	const int x0 = p.left + 2, x1 = p.right - 2;

	// Title row: side-toggle then close box, top-right. The side button's label
	// points at the side the panel will move TO.
	int y = p.top + 2;
	addTuneWidget(out, kTuneSide, 0, Common::Rect(x1 - 22, y, x1 - 12, y + 10),
	              st.leftSide ? ">" : "<", false);
	addTuneWidget(out, kTuneClose, 0, Common::Rect(x1 - 10, y, x1, y + 10), "x", false);
	y += 12;

	// F11 mirror: per-frame diagnostic-log toggle (lights while on).
	addTuneWidget(out, kTuneDebugLog, 0, Common::Rect(x0, y, x1, y + 10),
	              st.debugLog ? "log: on" : "log: off", st.debugLog);
	y += 11;

	// View-enhance toggle: one row that cycles the view-scaler modes (registry
	// scalers + nearest) and applies on click; label shows the current mode.
	addTuneWidget(out, kTuneViewEnhance, 0, Common::Rect(x0, y, x1, y + 10),
	              Common::String("view enhance: ") + tuneViewModeLabel(st.viewMode), false);
	y += 11;

	// Pic-enhance toggle: one row that cycles the available pass modes and
	// applies on click; label shows the selected mode's compact string.
	addTuneWidget(out, kTunePicEnhance, 0, Common::Rect(x0, y, x1, y + 10),
	              Common::String("pic enhance: ") + tunePicModeLabel(st), false);
	y += 11;

	// Bottom-anchored build row (one line): +f +l +a clear add. Fixed
	// coordinates regardless of chip count (script stability).
	const int yBuild = p.bottom - 24;

	// Chip strip: DISPLAY-ONLY (the built sequence), non-interactive. Flows
	// between the toggles and the build row, 7 per row.
	int cx = x0, cy = y + 3;
	for (uint i = 0; i < st.stagedPasses.size(); i++) {
		if (cx + 11 > x1) { cx = x0; cy += 11; }
		if (cy + 10 > yBuild - 2)
			break; // out of space; extra chips not shown (debug tool)
		const char *lbl = st.stagedPasses[i] == 2 ? "f" : st.stagedPasses[i] == 1 ? "l" : "a";
		addTuneWidget(out, kTuneChip, (int)i, Common::Rect(cx, cy, cx + 10, cy + 10),
		              lbl, false, false); // enabled=false -> no hover, drawn dim
		cx += 11;
	}

	// Build row: +f +l +a append, clear empties, add registers+applies.
	int bx = x0;
	const struct { int kind; const char *lbl; int w; } builds[] = {
		{ kTuneChipAddF, "+f", 12 }, { kTuneChipAddL, "+l", 12 },
		{ kTuneChipAddA, "+a", 12 }, { kTuneClear, "clear", 22 },
	};
	for (int i = 0; i < ARRAYSIZE(builds); i++) {
		addTuneWidget(out, builds[i].kind, 0,
		              Common::Rect(bx, yBuild, bx + builds[i].w, yBuild + 10), builds[i].lbl, false);
		bx += builds[i].w + 1;
	}
	// Add fills the rest of the row and highlights while edits are pending.
	addTuneWidget(out, kTuneAdd, 0, Common::Rect(bx, yBuild, x1, yBuild + 10), "add", tunePending(st));
}

Common::String tuneStatusLine(const TunePanelState &st) {
	Common::String s = omyacPassStamp(st.stagedPasses);
	if (tunePending(st))
		s += " *";
	s += Common::String::format(" %ums", st.lastGenMs);
	return s;
}

void drawTunePanel(Graphics::ManagedSurface &scene, const Common::Rect &gameRect,
                   const TunePanelState &st, const Common::Array<PanelWidget> &widgets) {
	if (gameRect.isEmpty())
		return;
	const Common::Rect panelGame = tunePanelRect(st.leftSide);
	const Common::Rect panel = sciRectToDest(panelGame, gameRect);
	const uint32 bg   = scene.format.ARGBToColor(255, 22, 22, 30);
	const uint32 fg   = scene.format.ARGBToColor(255, 190, 190, 200);
	const uint32 hi   = scene.format.ARGBToColor(255, 255, 220, 120);
	const uint32 hov  = scene.format.ARGBToColor(255, 60, 60, 84);
	const uint32 dim  = scene.format.ARGBToColor(255, 96, 96, 104);
	scene.fillRect(panel, bg);
	scene.frameRect(panel, fg);

	const Graphics::Font *f = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);

	// Title (top-left, inside the panel, left of the side/close boxes).
	if (f) {
		const Common::Rect t = sciRectToDest(
			Common::Rect(panelGame.left + 2, panelGame.top + 2,
			             panelGame.right - 26, panelGame.top + 12), gameRect);
		f->drawString(&scene, "TUNE (F12)", t.left, t.top, t.width(), hi);
	}

	for (uint i = 0; i < widgets.size(); i++) {
		const PanelWidget &w = widgets[i];
		const Common::Rect r = sciRectToDest(w.rect, gameRect);
		if (w.id == st.hoverId && w.enabled)
			scene.fillRect(r, hov);
		scene.frameRect(r, w.on ? hi : (w.enabled ? fg : dim));
		if (f) {
			const int ty = r.top + (r.height() - f->getFontHeight()) / 2;
			f->drawString(&scene, w.label, r.left + 2, ty, r.width() - 4,
			              w.enabled ? (w.on ? hi : fg) : dim,
			              Graphics::kTextAlignCenter);
		}
	}

	// Status line at the bottom-anchored text row.
	if (f) {
		const Common::Rect s = sciRectToDest(
			Common::Rect(panelGame.left + 2, panelGame.bottom - 12,
			             panelGame.right - 2, panelGame.bottom - 2), gameRect);
		f->drawString(&scene, tuneStatusLine(st), s.left, s.top, s.width(), fg);
	}
}

} // namespace Roger
} // namespace Sci
