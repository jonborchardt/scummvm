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

#ifndef SCI_ROGER_ROGER_ASSET_GEN_H
#define SCI_ROGER_ROGER_ASSET_GEN_H

// SCI-FREE header: no SCI includes, no GuiResourceId (it is just int).
// Follows the same isolation pattern as engines/sci/roger/view_cache.h.

#include "common/str.h"
#include "common/array.h"
#include "sci/roger/roger_omyac.h"
#include "sci/roger/roger_scale.h"

namespace Graphics { struct Surface; }

namespace Sci {
namespace Roger {

// EGA cel de-undither. ScummVM's GfxView::getBitmap() undithers EGA cels by
// collapsing a two-colour dither checkerboard into a single combined byte
// (high<<4 | low, value 16..254), which then resolves to a washed-out *blended*
// palette entry (the pink/orange look). The reference pipeline (sci.js) never
// undithers — it keeps the original dither. Re-expand a combined byte back into
// the two-colour checkerboard at pixel (x,y) so upscaled cels keep saturated EGA
// colours (consistent with how pic backgrounds preserve dither). Bytes <= 0x0f
// (true palette indices, including the clearKey) pass through unchanged.
inline byte egaDeUndither(byte b, int x, int y, byte clearKey) {
	if (b <= 0x0f || b == clearKey)
		return b;
	const byte lo = b & 0x0f;
	const byte hi = b >> 4;
	return ((x ^ y) & 1) ? hi : lo;
}

enum GenMode {
	kGenPrebuilt, // Default: return nullptr, let provider load prebuilt PNG.
	kGenCache,    // Load from disk cache on hit; generate + write on miss.
	kGenMemory,   // Always generate; never write to disk.
	kGenAlways    // Always generate, even when a cache file exists (re-gen + overwrite).
};

// Pipeline version embedded in every cache key. Bump whenever the generation
// pipeline changes in a way that invalidates previously cached files — stale
// files are then simply not found and regenerated on next use.
//   v1: initial omyac plate + scale6x view-cel pipeline.
//   v2: in-engine art path; overlay occlusion from native priority bands.
//   v3: view cels de-undither EGA bytes (egaDeUndither).
//   v4: view cels pack pixels via PixelFormat::ARGBToColor.
//   v5: fillNullPixels backfills unclaimed pixels with their own native cell's
//       colour (OmyacParams::backfillOwnCell) instead of the scan-order majority
//       flood, which cascaded foreign colours down-right across cells (the SQ3
//       pod-door cyan artifact). Superseded same day by v6 — pure own-cell reads
//       too blocky under sparse pass lists.
//   v6: bounded direction-neutral backfill — backfillFloodRounds breadth-first
//       majority rounds (frozen src per round), then own-cell fill. Keeps the
//       flood-rasterized smooth look, kills the unbounded cascade.
//   v7: foreign fill fringes eroded to a 1 px anchored rim (before + after the
//       backfill) — a fill colour never sits deeper than 1 px inside a drawn
//       native cell of another colour (the pod-door dashed-seam residue).
//   v8: zero rim in LINE cells — a fill colour never enters a line-drawn cell
//       (fills no longer thin lines; kills the remaining 1 px pod-door seam).
static const int kTransformVersion = 8;

/**
 * Orchestrates on-the-fly omyac plate generation and scale6x view-cel
 * generation, backed by a content-hash disk cache.
 *
 * Default mode is kGenPrebuilt, which makes every method a no-op (nullptr)
 * so existing behavior is entirely unchanged unless the knob is set.
 *
 * This class is SCI-engine-free in its header; the .cpp guards all engine
 * state access under #ifdef ENABLE_SCI.
 */
class RogerAssetGen {
public:
	/**
	 * @param gameId   ScummVM game-id string (e.g. "sq3"); part of cache key.
	 * @param cacheDir Directory for .png cache files (e.g. "<roger-art>/cache").
	 * @param mode     Generation / caching policy.
	 */
	RogerAssetGen(const Common::String &gameId,
	              const Common::String &cacheDir,
	              GenMode mode);

	/**
	 * Override the enhance-pass sequence fed to renderOmyac().
	 * Empty array means "use defaultPasses()" (not "zero passes").
	 */
	void setEnhancePasses(const Common::Array<int> &passes);
	const Common::Array<int> &enhancePasses() const { return _passes; }
	GenMode mode() const { return _mode; }
	void setMode(GenMode m) { _mode = m; }

	// Studio tuning support. Non-default params force kGenMemory: params are
	// NOT part of the cache key, so caching a tuned render would poison the
	// content-hash cache for normal launches.
	void setOmyacParams(const OmyacParams &p) {
		_omyacParams = p;
		if (!p.isDefault() && _mode != kGenMemory)
			_mode = kGenMemory;
	}
	const OmyacParams &omyacParams() const { return _omyacParams; }

	// De-undithered native cel pixels as an IndexImage (pre-upscale). False on
	// any failure. Extracted from generateViewCel so the studio can feed the
	// SAME source pixels through alternative scaler variants.
	bool nativeCelIndexImage(int viewId, int loopNo, int celNo,
	                         IndexImage &out, byte &outClearKey);

	// Palette-map an IndexImage to a new RGBA32 surface (caller owns; ->free()
	// then delete). clearKey pixels get alpha 0. Extracted generateViewCel tail.
	Graphics::Surface *surfaceFromIndex(const IndexImage &img, byte clearKey);

	/**
	 * Generate (or load from cache) the omyac RGBA plate for pictureId.
	 * Returns a new Graphics::Surface (caller owns: ->free() then delete) or
	 * nullptr meaning "fall back to the prebuilt PNG".
	 * @param id    SCI picture resource number (int == GuiResourceId).
	 * @param outMs Wall-clock milliseconds spent generating (0 on cache hit).
	 */
	Graphics::Surface *generatePlate(int id, uint32 &outMs);

	// As generatePlate, but also returns the omyac doubled-nibble index buffer
	// (OMYAC_HYBRID_W*OMYAC_HYBRID_H) in outIndex — the pre-blend color source used by
	// live palette re-apply. outIndex is cleared on any failure / cache-only path where
	// the index is unavailable (caller must check !outIndex.empty()).
	Graphics::Surface *generatePlateWithIndex(int id, Common::Array<byte> &outIndex, uint32 &outMs);

	// As generatePlate, but also returns the omyac backfill mask
	// (OMYAC_HYBRID_W*OMYAC_HYBRID_H bytes: 1 where fillNullPixels painted the
	// pixel, else 0) in outBackfill — the studio "unfilled pixels" diagnostic
	// (recoloured hot pink). Studio-only; the mask is NEVER cached, so a plate
	// served from the disk cache leaves outBackfill empty (no pink). The studio
	// runs kGenMemory, so plates always generate live and the mask is available.
	Graphics::Surface *generatePlateWithBackfill(int id, Common::Array<byte> &outBackfill, uint32 &outMs);

	/**
	 * Reference plate for shift diagnosis: the native 320x190 pre-render
	 * (NativeRef.refPixel) replicated x6 nearest-neighbour — geometrically
	 * exact by construction (every native pixel -> exactly one 6x6 block).
	 * Studio-only; never cached. Caller owns (->free() then delete).
	 */
	Graphics::Surface *generatePlateNearest(int id, uint32 &outMs);

	/**
	 * Generate a scale6x RGBA cel from the native GfxView cel.
	 * Returns nullptr on any failure or in kGenPrebuilt mode.
	 * @param outMs Generation time in ms (0 on cache hit).
	 */
	Graphics::Surface *generateViewCel(int viewId, int loopNo, int celNo, uint32 &outMs);

	/**
	 * Render `text` with the game's native SCI font `fontId` (pen colour `penColor`,
	 * transparent background) and upscale 6x. Game-agnostic: glyphs come from the
	 * engine's font resource, so custom glyphs (e.g. a stylized title glyph) render
	 * faithfully. Returns a new Graphics::Surface (caller owns: ->free() then delete),
	 * or nullptr on failure / in builds without ENABLE_SCI. Not disk-cached.
	 */
	Graphics::Surface *generateTextSurface(const Common::String &text, int fontId, byte penColor);

	// Native priority bands (320x190, one SCI band per pixel) for overlay occlusion,
	// derived from the in-engine native pre-render. Returns false on any miss.
	bool priorityBands(int picId, Common::Array<byte> &outBands, int &outW, int &outH);

	// Hires omyac-derived priority bands (OMYAC_HYBRID_W x OMYAC_HYBRID_H = 1920x1140)
	// whose band edges align with the generated plate, backed by the same content-hash
	// disk cache (transform "omyacprio", keyed by pic bytes + passes + kTransformVersion).
	// Returns false on any miss or in kGenPrebuilt; sprites then draw without occlusion.
	// @param outMs generation time in ms (0 on cache hit).
	bool generatePriorityMap(int picId, Common::Array<byte> &outBands,
	                         int &outW, int &outH, uint32 &outMs);

	// White-box test shim: exposes the private cacheKey() for unit tests.
	Common::String testKey(const char *transform, uint32 resourceHash) const {
		return cacheKey(transform, resourceHash);
	}

private:
	/**
	 * Build a deterministic cache key string.
	 * Format: "<gameId>.<transform>.v<kTransformVersion>.<hashHex>.<passesString>"
	 */
	Common::String cacheKey(const char *transform, uint32 resourceHash) const;

	// Shared plate-generation core behind generatePlateWithIndex /
	// generatePlateWithBackfill. Fills outIndex (pre-blend index buffer) and
	// outBackfill (fillNullPixels mask); both cleared on failure/cache-only.
	Graphics::Surface *generatePlateCore(int id, Common::Array<byte> &outIndex,
	                                     Common::Array<byte> &outBackfill, uint32 &outMs);

	GenMode        _mode;
	Common::String _gameId;
	Common::String _cacheDir;
	Common::Array<int> _passes; // empty => use defaultPasses() at generation time
	OmyacParams _omyacParams; // default-constructed == today's constants
	// Shared tail for native-font glyph rendering: nearest-upscale 6x + palette->RGBA.
	// `ck` is the transparent clear-key index. Caller owns.
	Graphics::Surface *finishGlyphSurface(const IndexImage &idx, int penColor, byte ck);
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_ASSET_GEN_H
