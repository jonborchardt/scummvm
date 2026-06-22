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

#include "common/fs.h"
#include "common/stream.h"
#include "common/formats/json.h"
#include "common/textconsole.h"
#include "graphics/surface.h"
#include "sci/roger/view_cache.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/png_loader.h"

namespace Sci {
namespace Roger {

ViewCache::ViewCache(const Common::String &viewsBasePath) : _base(viewsBasePath) {}

ViewCache::~ViewCache() {
	for (Common::HashMap<Common::String, Loop>::iterator it = _loops.begin(); it != _loops.end(); ++it) {
		for (uint i = 0; i < it->_value.cels.size(); i++) {
			it->_value.cels[i]->free();
			delete it->_value.cels[i];
		}
		if (it->_value.sheet) {
			it->_value.sheet->free();
			delete it->_value.sheet;
		}
	}
	for (Common::HashMap<Common::String, Graphics::Surface *>::iterator it = _genCels.begin(); it != _genCels.end(); ++it) {
		if (it->_value) {
			it->_value->free();
			delete it->_value;
		}
	}
	_genCels.clear();
}

static Common::String readFile(const Common::String &path) {
	Common::Path fsPath(path);
	Common::FSNode node(fsPath);
	if (!node.exists()) return Common::String();
	Common::SeekableReadStream *s = node.createReadStream();
	if (!s) return Common::String();
	Common::String out;
	while (!s->eos()) {
		char c = s->readByte();
		if (s->eos()) break;
		out += c;
	}
	delete s;
	return out;
}

ViewCache::Loop *ViewCache::loadLoop(int viewId, int loopNo) {
	Common::String key = Common::String::format("%d/%d", viewId, loopNo);
	if (_loops.contains(key))
		return &_loops[key];

	Common::String dir = _base + "/" + Common::String::format("%d", viewId) + "/";
	Common::String stem = Common::String::format("view.%d.loop.%d", viewId, loopNo);
	Common::String jsonStr = readFile(dir + stem + ".json");
	if (jsonStr.empty())
		return nullptr;

	Common::JSONValue *root = Common::JSON::parse(jsonStr.c_str());
	if (!root || !root->isObject()) { delete root; return nullptr; }
	Common::JSONObject obj = root->asObject();

	Graphics::Surface *sheet = loadSurfaceRGBA(dir + stem + ".png");
	if (!sheet) { delete root; return nullptr; }

	// Guard: "frames" must exist and be an object.
	if (!obj.contains("frames") || !obj["frames"]->isObject()) {
		sheet->free(); delete sheet; delete root; return nullptr;
	}

	Loop loop;
	loop.sheet = sheet;

	// Build cels in animations.loop order if present, else frames key order.
	const Common::JSONObject frames = obj["frames"]->asObject();
	Common::Array<Common::String> order;
	if (obj.contains("animations") && obj["animations"]->isObject()) {
		Common::JSONObject anim = obj["animations"]->asObject();
		if (anim.contains("loop") && anim["loop"]->isArray()) {
			Common::JSONArray arr = anim["loop"]->asArray();
			for (uint i = 0; i < arr.size(); i++) {
				if (arr[i] && arr[i]->isString())
					order.push_back(arr[i]->asString());
			}
		}
	}
	if (order.empty())
		for (Common::JSONObject::const_iterator it = frames.begin(); it != frames.end(); ++it)
			order.push_back(it->_key);

	for (uint i = 0; i < order.size(); i++) {
		// Guard: frame entry must exist and have a valid "frame" sub-object.
		if (!frames.contains(order[i]) || !frames[order[i]]->isObject())
			continue;
		Common::JSONObject frameEntry = frames[order[i]]->asObject();
		if (!frameEntry.contains("frame") || !frameEntry["frame"]->isObject())
			continue;
		Common::JSONObject f = frameEntry["frame"]->asObject();
		if (!f.contains("x") || !f["x"]->isIntegerNumber() ||
		    !f.contains("y") || !f["y"]->isIntegerNumber() ||
		    !f.contains("w") || !f["w"]->isIntegerNumber() ||
		    !f.contains("h") || !f["h"]->isIntegerNumber())
			continue;
		int x = (int)f["x"]->asIntegerNumber();
		int y = (int)f["y"]->asIntegerNumber();
		int w = (int)f["w"]->asIntegerNumber();
		int h = (int)f["h"]->asIntegerNumber();
		Graphics::Surface *cel = new Graphics::Surface();
		cel->create(w, h, sheet->format);
		cel->copyRectToSurface(*sheet, 0, 0, Common::Rect(x, y, x + w, y + h));
		loop.cels.push_back(cel);
	}
	delete root;
	_loops[key] = loop;
	return &_loops[key];
}

const Graphics::Surface *ViewCache::getCel(int viewId, int loopNo, int celNo) {
	Loop *loop = loadLoop(viewId, loopNo);
	if (!loop || celNo < 0 || (uint)celNo >= loop->cels.size()) {
		// Prebuilt spritesheet absent: try generator fallback.
		if (_gen && _gen->mode() != Roger::kGenPrebuilt) {
			Common::String key = Common::String::format("%d/%d/%d", viewId, loopNo, celNo);
			if (_genCels.contains(key))
				return _genCels[key]; // may be nullptr (known-missing)
			uint32 ms = 0;
			Graphics::Surface *gen = _gen->generateViewCel(viewId, loopNo, celNo, ms);
			_genCels[key] = gen;
			return gen;
		}
		return nullptr;
	}
	return loop->cels[celNo];
}

} // namespace Roger
} // namespace Sci
