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

#include "sci/roger/roger_studio_render.h"
#include "sci/roger/roger_view_scaler.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

static const OmyacParamDesc PARAM_DESCS[] = {
	{ "minVotesLine",       1, 8, 1, false,
	  "Min neighbour votes needed to fill a gap on a line edge" },
	{ "minVotesFillAll",    1, 8, 1, false,
	  "Min neighbour votes needed to fill a gap on a fill/all edge" },
	{ "fillSuppressLineNb", 1, 9, 1, false, // 9 = never suppress (8 neighbours max)
	  "Suppress fill-voting when at least this many line neighbours surround a gap" },
	{ "endpointMaxSame",    0, 9, 1, false,
	  "Endpoint when a pixel has fewer than this many same-command neighbours" },
	{ "isolatedPixelPass",  0, 1, 1, true,
	  "Spread isolated pixels into adjacent background gaps" },
	{ "tieBreakBlend",      0, 1, 1, true,
	  "On voting ties blend okLab colours instead of picking the first candidate" },
	{ "diagFlankSuppress",  0, 1, 1, true,
	  "Skip diagonal fill connections when both cardinal flanks are line pixels" },
	// backfillOwnCell / backfillFloodRounds (v6 bounded backfill) are deliberately
	// NOT registered: the settings panel layout (and its tests) are sized for 7
	// rows. Expose them together with a panel-layout change if studio tuning is
	// ever wanted; the shipping defaults are what matter.
};

int omyacParamCount() {
	return ARRAYSIZE(PARAM_DESCS);
}

OmyacParamDesc omyacParamDesc(int i) {
	if (i < 0 || i >= omyacParamCount())
		return PARAM_DESCS[0];
	return PARAM_DESCS[i];
}

int omyacParamGet(const OmyacParams &p, int i) {
	switch (i) {
	case 0: return p.minVotesLine;
	case 1: return p.minVotesFillAll;
	case 2: return p.fillSuppressLineNeighbours;
	case 3: return p.endpointMaxSame;
	case 4: return p.isolatedPixelPass ? 1 : 0;
	case 5: return p.tieBreakBlend ? 1 : 0;
	case 6: return p.diagFlankSuppress ? 1 : 0;
	default: return 0;
	}
}

void omyacParamSet(OmyacParams &p, int i, int value) {
	if (i < 0 || i >= omyacParamCount())
		return;
	const OmyacParamDesc &d = PARAM_DESCS[i];
	value = CLIP(value, d.minV, d.maxV);
	switch (i) {
	case 0: p.minVotesLine = value; break;
	case 1: p.minVotesFillAll = value; break;
	case 2: p.fillSuppressLineNeighbours = value; break;
	case 3: p.endpointMaxSame = value; break;
	case 4: p.isolatedPixelPass = value != 0; break;
	case 5: p.tieBreakBlend = value != 0; break;
	case 6: p.diagFlankSuppress = value != 0; break;
	default: break;
	}
}

Common::String omyacPassStamp(const Common::Array<int> &passes) {
	if (passes.empty())
		return "none";
	Common::String s;
	for (uint i = 0; i < passes.size(); i++)
		s += (passes[i] == 2) ? 'f' : (passes[i] == 1) ? 'l' : 'a';
	return s;
}

Common::String omyacParamStamp(const OmyacParams &p) {
	if (p.isDefault())
		return "default";
	const OmyacParams d;
	Common::String s;
	// Short codes, only for fields that differ from the default.
	if (p.minVotesLine != d.minVotesLine)
		s += Common::String::format("mvl%d-", p.minVotesLine);
	if (p.minVotesFillAll != d.minVotesFillAll)
		s += Common::String::format("mvf%d-", p.minVotesFillAll);
	if (p.fillSuppressLineNeighbours != d.fillSuppressLineNeighbours)
		s += Common::String::format("fsn%d-", p.fillSuppressLineNeighbours);
	if (p.endpointMaxSame != d.endpointMaxSame)
		s += Common::String::format("ems%d-", p.endpointMaxSame);
	if (p.isolatedPixelPass != d.isolatedPixelPass)
		s += Common::String::format("iso%d-", p.isolatedPixelPass ? 1 : 0);
	if (p.tieBreakBlend != d.tieBreakBlend)
		s += Common::String::format("tie%d-", p.tieBreakBlend ? 1 : 0);
	if (p.diagFlankSuppress != d.diagFlankSuppress)
		s += Common::String::format("dfs%d-", p.diagFlankSuppress ? 1 : 0);
	s.deleteLastChar(); // trailing '-'
	return s;
}

Common::String studioExportName(const char *kind, int id, const Common::String &detail) {
	return Common::String::format("studio-%s%03d-%s.png", kind, id, detail.c_str());
}

StudioDefaults studioDefaultsForGame(const Common::String &gameId) {
	StudioDefaults d = { -1, -1, 0, 0, 160, 150 };
	if (gameId == "sq3") {
		d.picId = 2; d.viewId = 12; d.loopNo = 1; d.celNo = 0;
	}
	return d;
}

void passInsertAfter(Common::Array<int> &passes, int &selected, int passVal) {
	int at = (selected < 0 || selected >= (int)passes.size())
	         ? (int)passes.size() : selected + 1;
	passes.insert_at(at, passVal);
	selected = at;
}

void passRemoveAt(Common::Array<int> &passes, int &selected) {
	if (selected < 0 || selected >= (int)passes.size())
		return;
	passes.remove_at(selected);
	if (passes.empty())
		selected = -1;
	else if (selected >= (int)passes.size())
		selected = (int)passes.size() - 1;
}

bool passMove(Common::Array<int> &passes, int &selected, int dir) {
	if (dir != -1 && dir != 1)
		return false;
	if (selected < 0 || selected >= (int)passes.size())
		return false;
	const int to = selected + dir;
	if (to < 0 || to >= (int)passes.size())
		return false;
	SWAP(passes[selected], passes[to]);
	selected = to;
	return true;
}

Common::String studioSceneExportName(int picId, char slot, const Common::String &detail) {
	return Common::String::format("studio-scene%03d-%c-%s.png", picId, slot, detail.c_str());
}

Common::String studioCompareExportName(int picId, bool diff,
                                       const Common::String &stampA,
                                       const Common::String &stampB) {
	return Common::String::format("studio-scene%03d-%s-%s-vs-%s.png",
		picId, diff ? "diff" : "AB", stampA.c_str(), stampB.c_str());
}

uint32 widId(int kind, int index) {
	return ((uint32)kind << 16) | ((uint32)index & 0xffff);
}
int widKind(uint32 id) { return (int)(id >> 16); }
int widIndex(uint32 id) { return (int)(id & 0xffff); }

namespace {

struct PanelCursor {
	Common::Array<StudioWidget> *out;
	int x, y;
	int rowH;
	int right;
	int leftEdge;

	void newRow() { x = leftEdge; y += rowH; }

	// Emits one widget; returns its rect. Non-clickable text -> id kWidNone.
	Common::Rect emit(const Common::String &label, uint32 id, bool on, bool enabled) {
		const int wpx = (int)label.size() * kStudioCharW + 10;
		if (x + wpx > right) newRow();          // wrap long rows defensively
		StudioWidget wgt;
		wgt.rect = Common::Rect((int16)x, (int16)(y + 2), (int16)(x + wpx), (int16)(y + rowH - 2));
		wgt.id = id; wgt.label = label; wgt.on = on; wgt.enabled = enabled;
		out->push_back(wgt);
		x += wpx + 6;
		return wgt.rect;
	}
	void text(const Common::String &label) { emit(label, kWidNone, false, false); }
	void btn(const Common::String &label, uint32 id, bool on = false) { emit(label, id, on, true); }
};

} // anonymous namespace

void buildStudioPanel(const Common::Rect &panel, const StudioPanelState &st,
                      Common::Array<StudioWidget> &out) {
	out.clear();
	PanelCursor c;
	c.out = &out; c.leftEdge = panel.left + 4; c.x = c.leftEdge; c.y = panel.top; c.rowH = kStudioRowH;
	c.right = panel.right - 4;

	// Row 1: scene
	c.text("pic");
	c.btn("<", widId(kWidPicPrev));
	c.text(Common::String::format("%03d", st.picId));
	c.btn(">", widId(kWidPicNext));
	c.text("view");
	c.btn("<", widId(kWidViewPrev));
	c.text(Common::String::format("%03d", st.viewId));
	c.btn(">", widId(kWidViewNext));
	c.text("loop");
	c.btn("<", widId(kWidLoopPrev));
	c.text(Common::String::format("%d", st.loopNo));
	c.btn(">", widId(kWidLoopNext));
	c.text("cel");
	c.btn("<", widId(kWidCelPrev));
	c.text(Common::String::format("%d", st.celNo));
	c.btn(">", widId(kWidCelNext));
	c.text(Common::String::format("@(%d,%d)", st.celX, st.celY));
	c.btn(Common::String::format("variant: %s", st.variantName), widId(kWidVariantCycle));
	c.btn(st.plateNearest ? "plate: nearest" : "plate: omyac", widId(kWidPlateMode), st.plateNearest);
	c.btn(st.showView ? "view: on" : "view: off", widId(kWidShowView), st.showView);
	c.btn("Fit", widId(kWidFit));
	c.newRow();

	// Row 2: edit tab + judge
	c.text("edit:");
	c.btn("A", widId(kWidTabA), st.activeSlot == 0);
	c.btn("B", widId(kWidTabB), st.activeSlot == 1);
	c.text(" ");
	c.btn("Show A", widId(kWidShowA), st.displayMode == 0);
	c.btn("Show B", widId(kWidShowB), st.displayMode == 1);
	c.btn("Split", widId(kWidSplit), st.displayMode == 2);
	c.btn("Diff", widId(kWidDiff), st.displayMode == 3);
	c.btn("Grid", widId(kWidGrid6), st.displayMode == 4);
	c.text(" ");
	c.btn("Copy A>B", widId(kWidCopyAB));
	c.btn("Export PNG", widId(kWidExport));
	c.text(" ");
	c.btn(st.showBackfill ? "pink: on" : "pink: off", widId(kWidShowBackfill), st.showBackfill);
	c.btn(st.showGrid ? "grid: on" : "grid: off", widId(kWidShowGrid), st.showGrid);
	c.text(" ");
	c.btn(st.animPlaying ? "pause" : "play", widId(kWidAnimPlay), st.animPlaying);
	c.btn("spd-", widId(kWidAnimSlower));
	c.text(Common::String::format("%dms", st.animMs));
	c.btn("spd+", widId(kWidAnimFaster));
	c.newRow();

	// Param rows (active slot values)
	for (int i = 0; i < omyacParamCount(); i++) {
		const OmyacParamDesc d = omyacParamDesc(i);
		const int v = (i < (int)st.paramValues.size()) ? st.paramValues[i] : 0;
		c.text(Common::String::format("%-20s", d.name));
		if (d.isBool) {
			c.btn(v ? "on" : "off", widId(kWidParamToggle, i), v != 0);
		} else {
			c.btn("-", widId(kWidParamMinus, i));
			c.text(Common::String::format("%d", v));
			c.btn("+", widId(kWidParamPlus, i));
		}
		c.newRow();
	}

	// Chip row
	// The caret "^" (kWidNone, on=true so it draws highlighted) marks the
	// insertion point: after the selected chip's x-button when selection is
	// valid, else after the last chip's x-button (or right after the label
	// when the list is empty).
	const int caretAfter = (st.selectedChip >= 0 && st.selectedChip < (int)st.passes.size())
	                       ? st.selectedChip
	                       : (int)st.passes.size() - 1; // -1 = no chips
	c.text("passes:");
	if (st.passes.empty()) {
		c.text("(none - wireframe)");
		// Caret at end (after the empty-list label, before the nav buttons).
		c.emit("^", kWidNone, true, false);
	} else {
		for (uint i = 0; i < st.passes.size(); i++) {
			const char ch = st.passes[i] == 2 ? 'f' : st.passes[i] == 1 ? 'l' : 'a';
			c.btn(Common::String::format("%c", ch), widId(kWidChip, (int)i), (int)i == st.selectedChip);
			c.btn("x", widId(kWidChipX, (int)i));
			if ((int)i == caretAfter)
				c.emit("^", kWidNone, true, false);
		}
	}
	c.btn("<", widId(kWidChipLeft));
	c.btn(">", widId(kWidChipRight));
	c.btn("+f", widId(kWidChipAddF));
	c.btn("+l", widId(kWidChipAddL));
	c.btn("+a", widId(kWidChipAddA));
	c.btn("Clear", widId(kWidChipClear));
	c.btn("Reset", widId(kWidChipReset));
}

uint32 hitTestWidgets(const Common::Array<StudioWidget> &widgets, int x, int y) {
	for (uint i = 0; i < widgets.size(); i++) {
		if (!widgets[i].enabled)
			continue;
		const Common::Rect &r = widgets[i].rect;
		if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
			return widgets[i].id;
	}
	return (uint32)kWidNone;
}

void diffMapRGBA(const byte *a, const byte *b, int w, int h, byte *out) {
	const int n = w * h;
	for (int i = 0; i < n; i++) {
		const byte *pa = a + i * 4, *pb = b + i * 4;
		int m = 0;
		for (int c = 0; c < 3; c++) {
			const int d = (int)pa[c] - (int)pb[c];
			m = MAX(m, d < 0 ? -d : d);
		}
		byte *po = out + i * 4;
		po[0] = po[1] = po[2] = (byte)m;
		po[3] = 255;
	}
}

bool estimateOffsetSAD(const byte *a, const byte *b, int w, int h, int radius,
                       int &outDx, int &outDy) {
	if (w <= 2 * radius || h <= 2 * radius)
		return false;
	double best = -1.0;
	int bestDx = 0, bestDy = 0, bestDist = 0;
	int bestAbsDy = 0, bestAbsDx = 0;
	for (int dy = -radius; dy <= radius; dy++) {
		for (int dx = -radius; dx <= radius; dx++) {
			// a sampled at (x, y), b at (x + dx, y + dy), over the overlap.
			const int x0 = MAX(0, -dx), x1 = MIN(w, w - dx);
			const int y0 = MAX(0, -dy), y1 = MIN(h, h - dy);
			uint64 sad = 0;
			for (int y = y0; y < y1; y++) {
				const byte *ra = a + (y * w + x0) * 4;
				const byte *rb = b + ((y + dy) * w + (x0 + dx)) * 4;
				for (int x = x0; x < x1; x++, ra += 4, rb += 4)
					for (int c = 0; c < 3; c++) {
						const int d = (int)ra[c] - (int)rb[c];
						sad += (uint64)(d < 0 ? -d : d);
					}
			}
			const long overlap = (long)(x1 - x0) * (y1 - y0);
			if (overlap <= 0)
				continue;
			const double norm = (double)sad / (double)overlap;
			const int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
			const int absDy = (dy < 0 ? -dy : dy);
			const int absDx = (dx < 0 ? -dx : dx);
			// Accept if: better SAD, or equal SAD and smaller dist, or equal SAD and dist but smaller |dy|, or all equal and smaller |dx|
			if (best < 0.0 || norm < best ||
			    (norm == best && (dist < bestDist ||
			                     (dist == bestDist && (absDy < bestAbsDy ||
			                                          (absDy == bestAbsDy && absDx < bestAbsDx)))))) {
				best = norm; bestDx = dx; bestDy = dy; bestDist = dist;
				bestAbsDy = absDy; bestAbsDx = absDx;
			}
		}
	}
	outDx = bestDx; outDy = bestDy;
	return true;
}

// ── Grid mode + animation helpers ────────────────────────────────────────────

int gridTileCount() {
	return MIN(viewScalerCount(), 6);
}

int gridPresetSlot(int tile) {
	return (tile >= 0 && tile < gridTileCount()) ? tile : -1;
}

Common::Rect gridTileRect(const Common::Rect &area, int tile) {
	const int kGutter = 2;
	const int col = tile % 3, row = tile / 3;
	const int tw = (area.width() - 2 * kGutter) / 3;
	const int th = (area.height() - kGutter) / 2;
	const int left = area.left + col * (tw + kGutter);
	const int top = area.top + row * (th + kGutter);
	return Common::Rect(left, top, left + tw, top + th);
}

static const int ANIM_SPEEDS_MS[5] = { 300, 200, 150, 100, 66 };

int animSpeedMs(int idx) {
	return ANIM_SPEEDS_MS[CLIP<int>(idx, 0, 4)];
}

int animSpeedStep(int idx, int dir) {
	return CLIP<int>(idx + dir, 0, 4);
}

Common::Rect celAnchorRect(int w, int h, int displaceX, int displaceY,
                           int anchorX, int anchorY) {
	const int left = anchorX + displaceX - (w >> 1);
	const int bottom = anchorY + displaceY + 1;
	return Common::Rect(left, bottom - h, left + w, bottom);
}

} // namespace Roger
} // namespace Sci
