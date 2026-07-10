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
                   const TunePanelState &st, const Common::Array<PanelWidget> &widgets,
                   TunePanelBake &bake) {
	if (gameRect.isEmpty())
		return;
	const Common::Rect panelGame = tunePanelRect(st.leftSide);
	const Common::Rect panel = sciRectToDest(panelGame, gameRect);
	if (panel.isEmpty())
		return;

	// Everything the rendered pixels depend on. Rebuild only on change so the
	// per-present cost is one panel-sized alpha blit (per-cycle discipline).
	Common::String sig = Common::String::format("%d,%d,%dx%d|h%u|%s|",
		panel.left, panel.top, panel.width(), panel.height(),
		st.hoverId, tuneStatusLine(st).c_str());
	for (uint i = 0; i < widgets.size(); i++)
		sig += Common::String::format("%u:%s:%d%d:%d,%d,%d,%d;", widgets[i].id,
			widgets[i].label.c_str(), widgets[i].on ? 1 : 0,
			widgets[i].enabled ? 1 : 0,
			widgets[i].rect.left, widgets[i].rect.top,
			widgets[i].rect.right, widgets[i].rect.bottom);

	if (sig != bake.sig) {
		bake.sig = sig;
		if (bake.surface.w != panel.width() || bake.surface.h != panel.height())
			bake.surface.create(panel.width(), panel.height(), scene.format);

		// One game-space row (10 px) in dest px sizes the fonts.
		const int rowDestH = panel.height() * 10 / MAX(1, (int)panelGame.height());
		const int sizes[kFontRoleCount] = {
			rowDestH,             // title: "TUNE (F12)"
			0,                    // sub: unused
			rowDestH * 3 / 5,     // body: widget labels
			rowDestH / 2,         // small: status line
			rowDestH * 3 / 5      // mono: unused today, sized anyway
		};
		bake.fonts.load(sizes);
		PanelPainter paint(bake.surface, bake.fonts);

		// Translucent navy panel: baked WITH alpha so the game scene shows
		// through when the bake is alpha-blended over it. Widgets drawn on top
		// composite against the panel color and end up ~opaque — intended.
		bake.surface.fillRect(Common::Rect(0, 0, bake.surface.w, bake.surface.h),
			bake.surface.format.ARGBToColor(216, PanelStyle::kPanelFill.r,
				PanelStyle::kPanelFill.g, PanelStyle::kPanelFill.b));
		paint.strokeRect(Common::Rect(0, 0, bake.surface.w, bake.surface.h),
			PanelStyle::kPanelLine);

		// All rects below: map game space -> scene dest -> panel-local.
		const int ox = panel.left, oy = panel.top;
		Common::Rect t = sciRectToDest(
			Common::Rect(panelGame.left + 2, panelGame.top + 2,
			             panelGame.right - 26, panelGame.top + 12), gameRect);
		t.translate(-ox, -oy);
		paint.drawTextIn(kFontTitle, "TUNE (F12)", t, PanelStyle::kBlue,
		                 Graphics::kTextAlignLeft);

		for (uint i = 0; i < widgets.size(); i++) {
			const PanelWidget &w = widgets[i];
			Common::Rect r = sciRectToDest(w.rect, gameRect);
			r.translate(-ox, -oy);
			paint.drawButton(r, w.label,
			                 w.on ? PanelStyle::kBlue : PanelStyle::kPanelLine,
			                 false, w.enabled,
			                 w.enabled && w.id == st.hoverId, kFontBody);
		}

		Common::Rect s = sciRectToDest(
			Common::Rect(panelGame.left + 2, panelGame.bottom - 12,
			             panelGame.right - 2, panelGame.bottom - 2), gameRect);
		s.translate(-ox, -oy);
		paint.drawTextIn(kFontSmall, tuneStatusLine(st), s, PanelStyle::kTextDim,
		                 Graphics::kTextAlignLeft);
	}

	// blendBlitFrom is safe here: dst == src size and tunePanelRect is strictly
	// inside 320x200, so the panel dest rect is always fully contained in the scene.
	scene.blendBlitFrom(bake.surface,
		Common::Rect(0, 0, bake.surface.w, bake.surface.h), panel);
}

} // namespace Roger
} // namespace Sci
