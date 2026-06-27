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
#include "sci/graphics/screen.h"
#include "sci/graphics/view.h"
#include "sci/graphics/palette16.h"
#include "sci/graphics/scifont.h"

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
//   v1: initial omyac plate + scale6x view-cel pipeline.
//   v2: in-engine art path (prebuilt visual/occlusion consumption removed;
//       overlay occlusion derived from the native priority bands). Pipeline
//       outputs are unchanged, but bumping forces a clean cache to avoid mixing
//       files written by the superseded prebuilt-era pipeline.
//   v3: view cels de-undither EGA bytes (egaDeUndither) so dithered cels keep
//       saturated EGA colours instead of ScummVM's washed-out blend palette.
//   v4: view cels pack pixels via PixelFormat::ARGBToColor (was a hand-rolled
//       0xAARRGGBB pack that mismatched the 0xRRGGBBAA format -> alpha in the
//       wrong byte -> semi-transparent / washed-out sprites).
static const int kTransformVersion = 4;

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
// (every load is a miss -> regenerate). FSNode::createDirectory only creates a
// SINGLE level, so we create the parent (<gameid>-roger/) first when it is
// missing — under in-engine generation there is no prebuilt art, so that parent
// dir may not exist at all (creating only the "cache" leaf would then fail and
// nothing would ever persist). Best-effort: if it cannot be created the write
// simply fails and we fall back to regeneration (Hard Constraint 6 — never
// crash). Only used by the generation paths below.
static void ensureCacheDir(const Common::String &dir) {
	if (dir.empty())
		return;
	Common::Path path(dir);
	Common::FSNode node(path);
	if (node.exists())
		return;
	Common::FSNode parent(path.getParent());
	if (!parent.exists())
		parent.createDirectory();
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
// engineIsEga — true when the engine is rendering in EGA (4-bit palette) mode.
// -------------------------------------------------------------------------

#ifdef ENABLE_SCI
static bool engineIsEga() {
	return g_sci && g_sci->getResMan() &&
		   g_sci->getResMan()->getViewType() == kViewEga;
}
#endif // ENABLE_SCI

#ifdef ENABLE_SCI
// Generate a 1920x1140 RGBA32 plate for a SCI1.1 VGA pic by reading the native
// visual screen (populated by GfxPicture::drawSci11Vga(), which already ran before
// pushHiresBackground() called us). Nearest-neighbour 6x upscale.
// Returns nullptr on any failure. `id` used only for cache key.
static Graphics::Surface *generatePlateSci11(
		const Common::String &gameId, const Common::String &cacheDir, GenMode mode,
		int id, uint32 resourceHash, uint32 &outMs) {
	outMs = 0;
	if (!g_sci || !g_sci->_gfxScreen || !g_sci->_gfxPalette16)
		return nullptr;

	const Common::String key = Common::String::format(
		"%s.sci11scale6x.v%d.%08x", gameId.c_str(), kTransformVersion, resourceHash);
	const Common::String cachePath = cacheDir + "/" + key + ".png";

	if (mode == kGenCache) {
		Graphics::Surface *cached = loadSurfaceRGBA(cachePath);
		if (cached)
			return cached;
	}

	const uint32 t0 = g_system->getMillis();

	// Read 320x190 native visual screen (game area below the status bar).
	// getVisual(x, y) returns a palette index 0-255.
	const int NW = OMYAC_NATIVE_W, NH = OMYAC_NATIVE_H; // 320, 190
	const int SW = OMYAC_HYBRID_W, SH = OMYAC_HYBRID_H; // 1920, 1140
	const int SCALE = OMYAC_SCALE;                        // 6

	// Build a 320x190 IndexImage with 8-bit palette indices.
	IndexImage idx;
	idx.w = NW; idx.h = NH;
	idx.pixels.resize((uint32)(NW * NH));
	for (int y = 0; y < NH; y++)
		for (int x = 0; x < NW; x++)
			idx.pixels[(uint32)(y * NW + x)] = g_sci->_gfxScreen->getVisual((int16)x, (int16)y);

	// Scale 6x with nearest-neighbour (reuses existing roger_scale machinery).
	IndexImage scaled = scaleNearest(idx, SCALE);
	if (scaled.w != SW || scaled.h != SH)
		return nullptr;

	// Map 256-color indices through the system palette to RGBA32.
	const Graphics::PixelFormat fmt(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *plate = new Graphics::Surface();
	plate->create((uint16)SW, (uint16)SH, fmt);
	if (!plate->getPixels()) { delete plate; return nullptr; }

	const Palette &pal = g_sci->_gfxPalette16->_sysPalette;
	for (int y = 0; y < SH; y++) {
		uint32 *dst = (uint32 *)plate->getBasePtr(0, y);
		for (int x = 0; x < SW; x++) {
			const byte palIdx = scaled.pixels[(uint32)(y * SW + x)];
			const Color &c = pal.colors[palIdx];
			dst[x] = fmt.ARGBToColor(255, c.r, c.g, c.b);
		}
	}

	outMs = g_system->getMillis() - t0;

	if (mode == kGenCache || mode == kGenAlways) {
		ensureCacheDir(cacheDir);
		dumpSurfacePng(*plate, cachePath);
	}

	return plate;
}

// Build a 1920x1140 priority band map for a SCI1.1 VGA pic by reading the native
// priority screen (populated by GfxPicture::drawSci11Vga()). Nearest-neighbour 6x.
static bool generatePriorityMapSci11(Common::Array<byte> &outBands, int &outW, int &outH) {
	outBands.clear(); outW = 0; outH = 0;
	if (!g_sci || !g_sci->_gfxScreen)
		return false;

	const int SW = OMYAC_HYBRID_W, SH = OMYAC_HYBRID_H;
	const int SCALE = OMYAC_SCALE;

	outBands.resize((uint32)(SW * SH));
	for (int y = 0; y < SH; y++) {
		for (int x = 0; x < SW; x++) {
			// nearest-neighbour: map hires pixel back to native pixel
			const int nx = x / SCALE, ny = y / SCALE;
			outBands[(uint32)(y * SW + x)] = g_sci->_gfxScreen->getPriority((int16)nx, (int16)ny);
		}
	}
	outW = SW; outH = SH;
	return true;
}
#endif // ENABLE_SCI

// -------------------------------------------------------------------------
// generatePlateFromScreen — SCI1.1 VGA live-screen path (public)
// -------------------------------------------------------------------------

Graphics::Surface *RogerAssetGen::generatePlateFromScreen(int id, uint32 &outMs) {
	outMs = 0;
	if (_mode == kGenPrebuilt)
		return nullptr;
#ifdef ENABLE_SCI
	if (!g_sci || !g_sci->getResMan())
		return nullptr;
	ResourceManager *resMan = g_sci->getResMan();
	Resource *res = resMan->findResource(ResourceId(kResourceTypePic, (uint16)id), false);
	if (!res || res->size() < 2)
		return nullptr;
	if (picResourceFormat(res->data(), (uint32)res->size(), engineIsEga()) != kPicSci11VgaCel)
		return nullptr;
	uint32 hash = fnv1a32(res->data(), (uint32)res->size());
	return generatePlateSci11(_gameId, _cacheDir, _mode, id, hash, outMs);
#else
	return nullptr;
#endif
}

// -------------------------------------------------------------------------
// generatePlate — thin wrapper; delegates to generatePlateWithIndex
// -------------------------------------------------------------------------

Graphics::Surface *RogerAssetGen::generatePlate(int id, uint32 &outMs) {
	Common::Array<byte> throwaway;
	return generatePlateWithIndex(id, throwaway, outMs);
}

// -------------------------------------------------------------------------
// generatePlateWithIndex — full implementation; also returns the pre-blend
// doubled-nibble index buffer (OMYAC_HYBRID_W*OMYAC_HYBRID_H) in outIndex.
// outIndex is cleared on any failure or cache-only path where the index is
// unavailable; caller must check !outIndex.empty() before using it.
// -------------------------------------------------------------------------

Graphics::Surface *RogerAssetGen::generatePlateWithIndex(int id, Common::Array<byte> &outIndex, uint32 &outMs) {
	outMs = 0;
	outIndex.clear();

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

	// Detect pic format. SCI1.0 VGA vector is currently unsupported (omyac is
	// EGA-specific); return nullptr so the native SCI render shows. SCI1.1 VGA
	// cel pics use a separate generation path added in Task 3.
	const PicFormat fmt = picResourceFormat(res->data(), (uint32)res->size(), engineIsEga());
	if (fmt == kPicSci1VgaVector)
		return nullptr; // SCI1.0 VGA vector: native render shows (future work)
	if (fmt == kPicSci11VgaCel)
		return nullptr; // on-demand only via generatePlateFromScreen; never from precache

	// Hash the raw bytes for the cache key.
	uint32 hash = fnv1a32(res->data(), (uint32)res->size());
	Common::String key = cacheKey("omyac", hash);
	Common::String cachePath = _cacheDir + "/" + key + ".png";

	// kGenCache: check disk first. Index is NOT available from a PNG cache hit;
	// outIndex stays empty so callers fall back to regeneration (Task 9 policy).
	if (_mode == kGenCache) {
		Graphics::Surface *cached = loadSurfaceRGBA(cachePath);
		if (cached) {
			// outMs stays 0 (cache hit); outIndex stays empty (unavailable from PNG).
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

	// Preserve the pre-blend doubled-nibble index map BEFORE blendToSurface
	// consumes omyac.pixels. This is the color source for live palette re-apply.
	outIndex = omyac.pixels;

	Graphics::Surface *plate = blendToSurface(omyac.pixels, OMYAC_HYBRID_W, OMYAC_HYBRID_H);

	uint32 t1 = g_system->getMillis();
	outMs = t1 - t0;

	if (!plate) {
		outIndex.clear();
		return nullptr;
	}

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
	const PicFormat fmt = picResourceFormat(res->data(), (uint32)res->size(), engineIsEga());
	if (fmt != kPicSci0Ega)
		return false; // only the EGA path produces native priority bands
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
	// VGA views use 256-color cel data — the EGA omyac pipeline produces wrong output.
	// Return nullptr to fall back to native SCI cel rendering for VGA sprites.
	if (g_sci->getResMan() && g_sci->getResMan()->getViewType() != kViewEga)
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
	const byte clearKeyIdx = celInfo->clearKey;
	for (int row = 0; row < h; ++row) {
		for (int col = 0; col < w; ++col) {
			idx.pixels[(uint32)(row * w + col)] = egaDeUndither(bmp[row * w + col], col, row, clearKeyIdx);
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

	// Same format as loadSurfaceRGBA / the compositor. NOTE: the PixelFormat ctor is
	// (bpp, Rbits,Gbits,Bbits,Abits, Rshift,Gshift,Bshift,Ashift), so this is
	// rShift=24,gShift=16,bShift=8,aShift=0 (in-memory 0xRRGGBBAA, alpha in the LOW
	// byte). Pack via ARGBToColor so the channels land correctly regardless of layout
	// — a hand-rolled (0xff<<24|r<<16|g<<8|b) pack put alpha in the wrong byte, which
	// made every generated cel semi-transparent (washed-out pink/orange).
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
				dst[col] = fmt.ARGBToColor(0, 0, 0, 0); // fully transparent
			} else {
				const Color &c = pal.colors[idx_val];
				dst[col] = fmt.ARGBToColor(255, c.r, c.g, c.b);
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

// -------------------------------------------------------------------------
// generatePriorityMap — hires omyac-aligned priority bands (cached)
// -------------------------------------------------------------------------

#ifdef ENABLE_SCI
// Recover the per-pixel SCI priority band (0..15) from an omyac-rendered priority
// surface. The priority screen is upscaled through omyac as EGA colours (code N ->
// solid colour 0xNN), so each output pixel's colour maps back to a band by nearest
// EGA colour. Solid regions recover exactly; omyac-blended edge pixels resolve to the
// nearer of the two adjacent bands (a sub-pixel occlusion boundary, which is the win).
static void deriveBandsFromSurface(const Graphics::Surface &surf, Common::Array<byte> &outBands) {
	int egaR[16], egaG[16], egaB[16];
	for (int c = 0; c < 16; ++c) {
		uint32 v = BLEND_TABLE[(c << 4) | c]; // packed 0xAABBGGRR
		egaR[c] = (int)(v & 0xff);
		egaG[c] = (int)((v >> 8) & 0xff);
		egaB[c] = (int)((v >> 16) & 0xff);
	}
	const Graphics::PixelFormat &fmt = surf.format;
	const int w = surf.w, h = surf.h;
	outBands.resize((uint32)(w * h));
	for (int y = 0; y < h; ++y) {
		const uint32 *src = (const uint32 *)surf.getBasePtr(0, y);
		for (int x = 0; x < w; ++x) {
			byte a, r, g, bb;
			fmt.colorToARGB(src[x], a, r, g, bb);
			int best = 0, bestD = 0x7fffffff;
			for (int c = 0; c < 16; ++c) {
				int dr = (int)r - egaR[c], dg = (int)g - egaG[c], db = (int)bb - egaB[c];
				int d = dr * dr + dg * dg + db * db;
				if (d < bestD) { bestD = d; best = c; }
			}
			outBands[(uint32)(y * w + x)] = (byte)best;
		}
	}
}
#endif // ENABLE_SCI

bool RogerAssetGen::generatePriorityMap(int picId, Common::Array<byte> &outBands,
                                        int &outW, int &outH, uint32 &outMs) {
	outBands.clear(); outW = 0; outH = 0; outMs = 0;

	if (_mode == kGenPrebuilt)
		return false;

#ifdef ENABLE_SCI
	if (!g_sci)
		return false;
	ResourceManager *resMan = g_sci->getResMan();
	if (!resMan)
		return false;
	Resource *res = resMan->findResource(ResourceId(kResourceTypePic, (uint16)picId), false);
	if (!res || res->size() == 0)
		return false;

	// Non-EGA pics: no omyac priority map. SCI1.1 handled by Task 3.
	const PicFormat fmt = picResourceFormat(res->data(), (uint32)res->size(), engineIsEga());
	if (fmt == kPicSci1VgaVector)
		return false;
	if (fmt == kPicSci11VgaCel)
		return generatePriorityMapSci11(outBands, outW, outH);

	uint32 hash = fnv1a32(res->data(), (uint32)res->size());
	Common::String key = cacheKey("omyacprio", hash);
	Common::String cachePath = _cacheDir + "/" + key + ".png";

	const uint32 hiresCount = (uint32)(OMYAC_HYBRID_W * OMYAC_HYBRID_H);

	// kGenCache: the PNG is the colour priority picture (EGA colours). Recover the
	// occlusion bands by mapping each pixel's colour back to its priority code.
	if (_mode == kGenCache) {
		Graphics::Surface *cached = loadSurfaceRGBA(cachePath);
		if (cached) {
			deriveBandsFromSurface(*cached, outBands);
			cached->free();
			delete cached;
			if (outBands.size() == hiresCount) {
				outW = OMYAC_HYBRID_W; outH = OMYAC_HYBRID_H;
				return true; // cache hit, outMs stays 0
			}
			outBands.clear(); // wrong dimensions -> fall through and regenerate
		}
	}

	uint32 t0 = g_system->getMillis();

	// Render the PRIORITY screen through the SAME omyac pipeline as the visual: the
	// priority codes are encoded as EGA colours (nativePreRender kDrawPriority), so the
	// output is a colour hires priority picture, upscaled and edge-enhanced identically.
	Common::Array<DrawCommand> cmds = parsePic(res->data(), (uint32)res->size());
	NativeRef ref = nativePreRender(cmds, kDrawPriority);

	// Same passes as the plate (the provider sets _passes once), so edges agree.
	OmyacResult omyac = renderOmyac(ref, _passes);
	Graphics::Surface *plate = blendToSurface(omyac.pixels, OMYAC_HYBRID_W, OMYAC_HYBRID_H);
	if (!plate)
		return false;

	// Occlusion bands fall out of the rendered colour picture.
	deriveBandsFromSurface(*plate, outBands);
	outW = OMYAC_HYBRID_W; outH = OMYAC_HYBRID_H;

	uint32 t1 = g_system->getMillis();
	outMs = t1 - t0;

	// Persist the COLOUR priority picture (the same way the plate is cached), so the
	// cached omyacprio PNG is a readable EGA priority view. (kGenMemory: skip the write.)
	if (_mode == kGenCache || _mode == kGenAlways) {
		ensureCacheDir(_cacheDir);
		dumpSurfacePng(*plate, cachePath);
	}

	plate->free();
	delete plate;
	return true;
#else
	(void)picId;
	return false;
#endif // ENABLE_SCI
}

// ---------------------------------------------------------------------------
// finishGlyphSurface — nearest-upscale 6x + palette->RGBA.
// ---------------------------------------------------------------------------
Graphics::Surface *RogerAssetGen::finishGlyphSurface(const IndexImage &idx, int penColor, byte ck) {
#ifdef ENABLE_SCI
	if (!g_sci || idx.w <= 0 || idx.h <= 0)
		return nullptr;

	IndexImage scaled = scaleNearest(idx, 6);
	const int sw = scaled.w, sh = scaled.h;
	if (sw <= 0 || sh <= 0)
		return nullptr;

	const Graphics::PixelFormat fmt(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *surf = new Graphics::Surface();
	surf->create((uint16)sw, (uint16)sh, fmt);
	if (!surf->getPixels()) { delete surf; return nullptr; }

	const Palette &pal = g_sci->_gfxPalette16->_sysPalette;

	// Hard edges, index -> palette RGBA (ck -> transparent).
	for (int y = 0; y < sh; y++) {
		uint32 *dst = (uint32 *)surf->getBasePtr(0, y);
		for (int x = 0; x < sw; x++) {
			const byte v = scaled.pixels[(uint32)(y * sw + x)];
			if (v == ck) dst[x] = fmt.ARGBToColor(0, 0, 0, 0);
			else { const Color &c = pal.colors[v]; dst[x] = fmt.ARGBToColor(255, c.r, c.g, c.b); }
		}
	}
	return surf;
#else
	(void)idx; (void)penColor; (void)ck;
	return nullptr;
#endif
}

// ---------------------------------------------------------------------------
// generateTextSurface
// ---------------------------------------------------------------------------
Graphics::Surface *RogerAssetGen::generateTextSurface(const Common::String &text, int fontId, byte penColor) {
	if (text.empty())
		return nullptr;

#ifdef ENABLE_SCI
	if (!g_sci)
		return nullptr;
	GfxCache *gfxCache = g_sci->_gfxCache;
	if (!gfxCache)
		return nullptr;
	GfxFont *font = gfxCache->getFont((GuiResourceId)fontId);
	if (!font)
		return nullptr;

	const int h = font->getHeight();
	int w = 0;
	for (uint i = 0; i < text.size(); i++)
		w += font->getCharWidth((byte)text[i]);
	if (w <= 0 || h <= 0)
		return nullptr;

	// Clear-key sentinel index for the transparent background (must differ from the
	// pen so glyph pixels are never treated as transparent). scale6x only copies
	// existing indices, so the sentinel survives scaling and no new index appears.
	const byte ck = (penColor != 0xFF) ? 0xFF : 0xFE;

	IndexImage idx;
	idx.w = w;
	idx.h = h;
	idx.pixels.resize((uint32)(w * h), ck);

	int x = 0;
	for (uint i = 0; i < text.size(); i++) {
		const uint16 c = (byte)text[i];
		font->drawToBuffer(c, 0, (int16)x, penColor, false, idx.pixels.begin(), (int16)w, (int16)h);
		x += font->getCharWidth(c);
	}

	Graphics::Surface *surf = finishGlyphSurface(idx, penColor, ck);
	return surf;
#else
	(void)fontId; (void)penColor;
	return nullptr;
#endif
}

} // namespace Roger
} // namespace Sci
