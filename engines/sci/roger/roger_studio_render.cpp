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
#include "common/util.h"

namespace Sci {
namespace Roger {

static const OmyacParamDesc PARAM_DESCS[] = {
	{ "minVotesLine",        1, 8, 1, false },
	{ "minVotesFillAll",     1, 8, 1, false },
	{ "fillSuppressLineNb",  1, 9, 1, false }, // 9 = never suppress (8 neighbours max)
	{ "endpointMaxSame",     0, 9, 1, false },
	{ "isolatedPixelPass",   0, 1, 1, true },
	{ "tieBreakBlend",       0, 1, 1, true },
	{ "diagFlankSuppress",   0, 1, 1, true },
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

static const char *VARIANT_NAMES[kScalerCount] = {
	"scale6x (3x*2x)", "scale2x*3x", "nearest6", "scale2x+near3", "scale3x+near2",
	"scale4x", "scale8x"
};

const char *scalerVariantName(int v) {
	return (v >= 0 && v < kScalerCount) ? VARIANT_NAMES[v] : "?";
}

int scalerVariantFactor(int v) {
	switch (v) {
	case kScaler4x: return 4;
	case kScaler8x: return 8;
	default: return 6;
	}
}

IndexImage applyScalerVariant(int v, const IndexImage &in) {
	switch (v) {
	case kScaler6x:      return scale6x(in);
	case kScaler2x3x:    return scale2x(scale3x(in));
	case kScalerNearest6: return scaleNearest(in, 6);
	case kScaler2xN3:    return scaleNearest(scale2x(in), 3);
	case kScaler3xN2:    return scaleNearest(scale3x(in), 2);
	case kScaler4x:      return scale2x(scale2x(in));
	case kScaler8x:      return scale2x(scale2x(scale2x(in)));
	default:             return scale6x(in);
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

} // namespace Roger
} // namespace Sci
