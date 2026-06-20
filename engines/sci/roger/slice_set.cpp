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

#include "sci/roger/slice_set.h"
#include "sci/roger/png_loader.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/formats/json.h"
#include "graphics/surface.h"
#include <cstdlib>

namespace Sci {
namespace Roger {

// SCI0 EGA display colors per priority band (RGB), index = band (0..15).
// These are the standard CGA/EGA palette entries used by SCI0 for priority rendering.
static const uint8 kEgaBandRGB[16][3] = {
	{0x00, 0x00, 0x00}, // 0  black
	{0x00, 0x00, 0xaa}, // 1  dark blue
	{0x00, 0xaa, 0x00}, // 2  dark green
	{0x00, 0xaa, 0xaa}, // 3  dark cyan
	{0xaa, 0x00, 0x00}, // 4  dark red
	{0xaa, 0x00, 0xaa}, // 5  dark magenta
	{0xaa, 0x55, 0x00}, // 6  brown
	{0xaa, 0xaa, 0xaa}, // 7  light gray
	{0x55, 0x55, 0x55}, // 8  dark gray
	{0x55, 0x55, 0xff}, // 9  bright blue
	{0x55, 0xff, 0x55}, // 10 bright green
	{0x55, 0xff, 0xff}, // 11 bright cyan
	{0xff, 0x55, 0x55}, // 12 bright red
	{0xff, 0x55, 0xff}, // 13 bright magenta
	{0xff, 0xff, 0x55}, // 14 yellow
	{0xff, 0xff, 0xff}  // 15 white
};

int bandForColor(const Common::String &hex) {
	Common::String h = hex;
	if (h.hasPrefix("#"))
		h.deleteChar(0);
	if (h.size() < 6)
		return 0;
	long v = strtol(h.c_str(), nullptr, 16);
	int r = (v >> 16) & 0xff;
	int g = (v >> 8) & 0xff;
	int b = v & 0xff;
	int best = 0;
	long bestDist = 0x7fffffff;
	for (int i = 0; i < 16; i++) {
		int dr = r - kEgaBandRGB[i][0];
		int dg = g - kEgaBandRGB[i][1];
		int db = b - kEgaBandRGB[i][2];
		long d = (long)dr * dr + (long)dg * dg + (long)db * db;
		if (d < bestDist) {
			bestDist = d;
			best = i;
		}
	}
	return best;
}

SliceSet::SliceSet(const Common::String &dir, const Common::String &manifestName)
	: _dir(dir), _manifest(manifestName) {}

SliceSet::~SliceSet() {
	for (uint i = 0; i < _pieces.size(); i++) {
		if (_pieces[i].surface) {
			_pieces[i].surface->free();
			delete _pieces[i].surface;
		}
	}
}

bool SliceSet::load() {
	Common::FSNode node(Common::Path(_dir + "/" + _manifest));
	if (!node.exists())
		return false;
	Common::SeekableReadStream *s = node.createReadStream();
	if (!s)
		return false;
	Common::String txt;
	while (!s->eos()) {
		char c = s->readByte();
		if (s->eos())
			break;
		txt += c;
	}
	delete s;

	Common::JSONValue *root = Common::JSON::parse(txt.c_str());
	if (!root || !root->isObject()) {
		delete root;
		return false;
	}
	Common::JSONObject obj = root->asObject();
	if (!obj.contains("pieces") || !obj["pieces"]->isArray()) {
		delete root;
		return false;
	}
	Common::JSONArray arr = obj["pieces"]->asArray();
	for (uint i = 0; i < arr.size(); i++) {
		if (!arr[i] || !arr[i]->isObject())
			continue;
		Common::JSONObject p = arr[i]->asObject();

		// Guard each required field — skip malformed pieces rather than crashing.
		if (!p.contains("x") || !p["x"]->isIntegerNumber())
			continue;
		if (!p.contains("y") || !p["y"]->isIntegerNumber())
			continue;
		if (!p.contains("color") || !p["color"]->isString())
			continue;
		if (!p.contains("filename") || !p["filename"]->isString())
			continue;

		SlicePiece piece;
		piece.x = (int)p["x"]->asIntegerNumber();
		piece.y = (int)p["y"]->asIntegerNumber();
		piece.band = bandForColor(p["color"]->asString());
		piece.surface = loadSurfaceRGBA(_dir + "/" + p["filename"]->asString());
		if (piece.surface)
			_pieces.push_back(piece);
	}
	delete root;
	return true;
}

} // namespace Roger
} // namespace Sci
