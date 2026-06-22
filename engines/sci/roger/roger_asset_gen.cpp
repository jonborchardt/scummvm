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

// --------------------------------------------------------------------------
// Always-compiled section (SCI-free): constructor, cacheKey, setEnhancePasses,
// prebuilt-mode early returns, disk-cache load/write helpers.
// --------------------------------------------------------------------------

#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/png_loader.h"

#include "common/fs.h"
#include "common/str.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "graphics/pixelformat.h"

// --------------------------------------------------------------------------
// ENABLE_SCI-guarded section: SCI engine includes and generation bodies.
// --------------------------------------------------------------------------
#ifdef ENABLE_SCI
#include "sci/sci.h"
#include "sci/resource/resource.h"
#include "sci/graphics/cache.h"
#include "sci/graphics/view.h"
#include "sci/graphics/palette16.h"

#include "sci/roger/roger_pic_parser.h"
#include "sci/roger/roger_pic_native.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_ega_blend.h"
#include "sci/roger/roger_scale.h"
#endif // ENABLE_SCI

namespace Sci {
namespace Roger {

// Bump this whenever the generation pipeline changes in a way that invalidates
// previously cached files. Cache files whose key contains a different version
// will simply not be found and will be regenerated.
static const int kTransformVersion = 1;

// -------------------------------------------------------------------------
// FNV-1a 32-bit hash over an arbitrary byte span.
// -------------------------------------------------------------------------
static uint32 fnv1a32(const byte *data, uint32 size) {
	uint32 h = 0x811c9dc5u;
	for (uint32 i = 0; i < size; ++i) {
		h ^= data[i];
		h *= 0x01000193u;
	}
	return h;
}

// FNV-1a convenience: hash a single uint32 (for view-cel identity).
static uint32 fnv1a32u(uint32 v) {
	return fnv1a32(reinterpret_cast<const byte *>(&v), sizeof(v));
}

#ifdef ENABLE_SCI
// Ensure the content cache directory exists before a cache write. Without this,
// dumpSurfacePng's DumpFile::open fails silently and kGenCache never persists
// (every load is a miss -> regenerate). The parent (<gameid>-roger/) already
// exists, so this only needs to create the "cache" leaf. Best-effort: if it
// cannot be created the write simply fails and we fall back to regeneration
// (Hard Constraint 6 — never crash). Only used by the generation paths below.
static void ensureCacheDir(const Common::String &dir) {
	if (dir.empty())
		return;
	Common::Path path(dir);
	Common::FSNode node(path);
	if (!node.exists())
		node.createDirectory();
}
#endif

// -------------------------------------------------------------------------
// RogerAssetGen — always-compiled methods
// -------------------------------------------------------------------------

RogerAssetGen::RogerAssetGen(const Common::String &gameId,
                             const Common::String &cacheDir,
                             GenMode mode)
	: _mode(mode), _gameId(gameId), _cacheDir(cacheDir) {
}

void RogerAssetGen::setEnhancePasses(const Common::Array<int> &passes) {
	_passes = passes;
}

Common::String RogerAssetGen::cacheKey(const char *transform, uint32 resourceHash) const {
	// Build passes string: "p0p2p1..." or "none" when empty (empty = wireframe, not default).
	// The provider always sets a concrete pass list before calling generatePlate:
	//   unset config => defaultPasses() (non-empty)
	//   empty config => empty array (wireframe)
	// So "none" is the correct semantic label — it will never collide with a default run.
	Common::String passesStr;
	if (_passes.empty()) {
		passesStr = "none";
	} else {
		for (uint i = 0; i < _passes.size(); ++i) {
			passesStr += Common::String::format("p%d", _passes[i]);
		}
	}

	// "<gameId>.<transform>.v<ver>.<hashHex>.<passesStr>"
	return Common::String::format("%s.%s.v%d.%08x.%s",
		_gameId.c_str(),
		transform,
		kTransformVersion,
		resourceHash,
		passesStr.c_str());
}

// -------------------------------------------------------------------------
// generatePlate
// -------------------------------------------------------------------------

Graphics::Surface *RogerAssetGen::generatePlate(int id, uint32 &outMs) {
	outMs = 0;

	// kGenPrebuilt: signal the provider to use the prebuilt PNG.
	if (_mode == kGenPrebuilt)
		return nullptr;

#ifdef ENABLE_SCI
	// Guard: engine must be running.
	if (!g_sci)
		return nullptr;
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan)
		return nullptr;

	// Fetch the raw pic resource bytes.
	Resource *res = resMan->findResource(ResourceId(kResourceTypePic, (uint16)id), false);
	if (!res || res->size() == 0)
		return nullptr;

	// Hash the raw bytes for the cache key.
	uint32 hash = fnv1a32(res->data(), (uint32)res->size());
	Common::String key = cacheKey("omyac", hash);
	Common::String cachePath = _cacheDir + "/" + key + ".png";

	// kGenCache: check disk first.
	if (_mode == kGenCache) {
		Graphics::Surface *cached = loadSurfaceRGBA(cachePath);
		if (cached) {
			// outMs stays 0 (cache hit).
			return cached;
		}
	}

	// Generate: parsePic -> nativePreRender -> renderOmyac -> blendToSurface.
	uint32 t0 = g_system->getMillis();

	Common::Array<DrawCommand> cmds = parsePic(res->data(), (uint32)res->size());
	NativeRef ref = nativePreRender(cmds);

	// _passes is always concrete: provider sets defaultPasses() when config is unset,
	// empty array when config is "" (wireframe). Never substitute defaultPasses() here.
	const Common::Array<int> &passes = _passes;
	OmyacResult omyac = renderOmyac(ref, passes);

	Graphics::Surface *plate = blendToSurface(omyac.pixels, OMYAC_HYBRID_W, OMYAC_HYBRID_H);

	uint32 t1 = g_system->getMillis();
	outMs = t1 - t0;

	if (!plate)
		return nullptr;

	// Write to cache for kGenCache and kGenAlways.
	if (_mode == kGenCache || _mode == kGenAlways) {
		ensureCacheDir(_cacheDir);
		dumpSurfacePng(*plate, cachePath);
	}

	return plate;
#else
	// Test build without ENABLE_SCI: can't generate.
	return nullptr;
#endif // ENABLE_SCI
}

// -------------------------------------------------------------------------
// priorityBands — native priority bands for overlay occlusion
// -------------------------------------------------------------------------

bool RogerAssetGen::priorityBands(int picId, Common::Array<byte> &outBands, int &outW, int &outH) {
	outBands.clear(); outW = 0; outH = 0;
#ifdef ENABLE_SCI
	if (!g_sci) return false;
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan) return false;
	Resource *res = resMan->findResource(ResourceId(kResourceTypePic, (uint16)picId), false);
	if (!res || res->size() == 0) return false;
	Common::Array<DrawCommand> cmds = parsePic(res->data(), (uint32)res->size());
	NativeRef ref = nativePreRender(cmds);
	if (ref.priority.empty()) return false;
	outBands = ref.priority;
	outW = OMYAC_NATIVE_W; outH = OMYAC_NATIVE_H;
	return true;
#else
	(void)picId; return false;
#endif
}

// -------------------------------------------------------------------------
// generateViewCel
// -------------------------------------------------------------------------

Graphics::Surface *RogerAssetGen::generateViewCel(int viewId, int loopNo, int celNo, uint32 &outMs) {
	outMs = 0;

	if (_mode == kGenPrebuilt)
		return nullptr;

#ifdef ENABLE_SCI
	if (!g_sci)
		return nullptr;
	GfxCache *gfxCache = g_sci->_gfxCache;
	if (!gfxCache)
		return nullptr;

	GfxView *view = gfxCache->getView((GuiResourceId)viewId);
	if (!view)
		return nullptr;

	const CelInfo *celInfo = view->getCelInfo((int16)loopNo, (int16)celNo);
	if (!celInfo)
		return nullptr;

	int w = celInfo->width;
	int h = celInfo->height;
	if (w <= 0 || h <= 0)
		return nullptr;

	// Hash the view identity for the cache key (not the raw bytes — we use
	// the stable (viewId, loopNo, celNo) triple).
	uint32 hash = fnv1a32u((uint32)viewId);
	hash = fnv1a32u(hash ^ ((uint32)loopNo << 16));
	hash = fnv1a32u(hash ^ (uint32)celNo);
	Common::String key = cacheKey("scale6x", hash);
	Common::String cachePath = _cacheDir + "/" + key + ".png";

	if (_mode == kGenCache) {
		Graphics::Surface *cached = loadSurfaceRGBA(cachePath);
		if (cached)
			return cached;
	}

	// Build IndexImage from the native cel bitmap.
	const SciSpan<const byte> &bmp = view->getBitmap((int16)loopNo, (int16)celNo);
	if (bmp.size() < (uint)(w * h)) // need a full w*h row-major cel; else bail (Hard Constraint 6)
		return nullptr;

	IndexImage idx;
	idx.w = w;
	idx.h = h;
	idx.pixels.resize((uint32)(w * h), celInfo->clearKey);
	for (int row = 0; row < h; ++row) {
		for (int col = 0; col < w; ++col) {
			idx.pixels[(uint32)(row * w + col)] = bmp[row * w + col];
		}
	}

	uint32 t0 = g_system->getMillis();
	IndexImage scaled = scale6x(idx);
	uint32 t1 = g_system->getMillis();
	outMs = t1 - t0;

	// Map indices through live palette to RGBA32.
	int sw = scaled.w;
	int sh = scaled.h;
	if (sw <= 0 || sh <= 0)
		return nullptr;

	// Match the exact format returned by loadSurfaceRGBA (png_loader.cpp line 92):
	// PixelFormat(4, 8,8,8,8, aShift=24, rShift=16, gShift=8, bShift=0).
	// In a uint32: bits [31:24]=A, [23:16]=R, [15:8]=G, [7:0]=B (ARGB32 / 0xAARRGGBB).
	const Graphics::PixelFormat fmt(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *surf = new Graphics::Surface();
	surf->create((uint16)sw, (uint16)sh, fmt);
	if (!surf->getPixels()) {
		delete surf;
		return nullptr;
	}

	const Palette &pal = g_sci->_gfxPalette16->_sysPalette;
	byte clearKey = celInfo->clearKey;

	for (int row = 0; row < sh; ++row) {
		uint32 *dst = (uint32 *)surf->getBasePtr(0, row);
		for (int col = 0; col < sw; ++col) {
			byte idx_val = scaled.pixels[(uint32)(row * sw + col)];
			if (idx_val == clearKey) {
				dst[col] = 0x00000000u; // fully transparent (A=0)
			} else {
				const Color &c = pal.colors[idx_val];
				// 0xAARRGGBB
				dst[col] = (0xffu << 24)
				         | ((uint32)c.r << 16)
				         | ((uint32)c.g << 8)
				         | ((uint32)c.b);
			}
		}
	}

	if (_mode == kGenCache || _mode == kGenAlways) {
		ensureCacheDir(_cacheDir);
		dumpSurfacePng(*surf, cachePath);
	}

	return surf;
#else
	return nullptr;
#endif // ENABLE_SCI
}

} // namespace Roger
} // namespace Sci
