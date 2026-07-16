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

#include "sci/roger/overlay/roger_compositor.h"
#include "sci/roger/overlay/roger_tokens.h"
#include "sci/roger/overlay/view_cache.h"
#include "sci/roger/overlay/roger_coords.h"
#include "sci/roger/overlay/roger_text.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "common/system.h"
#include "common/textconsole.h"

namespace Sci {
namespace Roger {

void coalesceDirtyRects(const Common::Array<Common::Rect> &in, const Common::Rect &bounds,
                        Common::Array<Common::Rect> &out) {
	out.clear();
	// 1) clamp to bounds, drop empties
	for (uint i = 0; i < in.size(); i++) {
		Common::Rect r = in[i];
		r.clip(bounds);
		if (!r.isEmpty())
			out.push_back(r);
	}
	// 2) merge intersecting rects into their union, repeat until stable. n is small
	//    (a handful of sprites + cursor + dialog), so O(n^2) per pass is fine.
	bool merged = true;
	while (merged) {
		merged = false;
		for (uint i = 0; i < out.size() && !merged; i++) {
			for (uint j = i + 1; j < out.size() && !merged; j++) {
				if (out[i].intersects(out[j])) {
					out[i].extend(out[j]);     // out[i] becomes the bounding union
					out.remove_at(j);
					merged = true;
				}
			}
		}
	}
}

void mergeSpritesByPriority(const Common::Array<Sprite> &animate,
                            const Common::Array<Sprite> &staticSprites,
                            Common::Array<Sprite> &out) {
	out.clear();
	for (uint i = 0; i < staticSprites.size(); i++)
		out.push_back(staticSprites[i]);
	for (uint i = 0; i < animate.size(); i++)
		out.push_back(animate[i]);
	// Stable insertion sort by ascending priority.
	for (uint i = 1; i < out.size(); i++) {
		Sprite key = out[i];
		int j = (int)i - 1;
		while (j >= 0 && out[j].priority > key.priority) {
			out[j + 1] = out[j];
			j--;
		}
		out[j + 1] = key;
	}
}

void buildInitFrameSpriteSet(const Common::Array<Sprite> &statics,
                             const Common::Array<Sprite> &initCels,
                             Common::Array<Sprite> &out) {
	Common::Array<Sprite> all = statics;
	for (uint i = 0; i < initCels.size(); i++) {
		const Sprite &c = initCels[i];
		bool dup = false;
		for (uint j = 0; j < all.size(); j++)
			if (all[j].viewId == c.viewId && all[j].loopNo == c.loopNo &&
			    all[j].celNo == c.celNo && all[j].celRect == c.celRect) {
				dup = true;
				break;
			}
		if (!dup)
			all.push_back(c);
	}
	Common::Array<Sprite> none;
	mergeSpritesByPriority(none, all, out);
}

Common::Rect mapNativeRectToOverlay(const Common::Rect &nativeRect,
                                    const Common::Rect &picRect,
                                    int picW, int picH, int picScreenTop) {
	if (picW <= 0 || picH <= 0)
		return Common::Rect();
	const int GW = picRect.width(), GH = picRect.height();
	const int lx0 = nativeRect.left;
	const int lx1 = nativeRect.right;
	const int ly0 = nativeRect.top - picScreenTop;
	const int ly1 = nativeRect.bottom - picScreenTop;
	return Common::Rect(
		(int16)(picRect.left + mapNativeEdge(lx0, picW, GW)),
		(int16)(picRect.top  + mapNativeEdge(ly0, picH, GH)),
		(int16)(picRect.left + mapNativeEdge(lx1, picW, GW)),
		(int16)(picRect.top  + mapNativeEdge(ly1, picH, GH)));
}

void upscaleNativeRegionNearest(Graphics::Surface &dest, const Common::Rect &overlayRect,
                                const byte *visual, int visualPitch,
                                const Common::Rect &nativeRect, const byte *palette) {
	const int ow = overlayRect.width(), oh = overlayRect.height();
	const int nw = nativeRect.width(), nh = nativeRect.height();
	if (ow <= 0 || oh <= 0 || nw <= 0 || nh <= 0)
		return;
	for (int dy = 0; dy < oh; dy++) {
		const int sy = nativeRect.top + dy * nh / oh;
		const byte *srcRow = visual + (uint)sy * visualPitch;
		for (int dx = 0; dx < ow; dx++) {
			const int sx = nativeRect.left + dx * nw / ow;
			const byte idx = srcRow[sx];
			const byte *c = palette + (uint)idx * 3;
			dest.setPixel(overlayRect.left + dx, overlayRect.top + dy,
			              dest.format.ARGBToColor(255, c[0], c[1], c[2]));
		}
	}
}

static Common::Rect fitAspectCentered(const Common::Rect &box) {
	// Largest 8:5 rect that fits inside box, centered.
	const int bw = box.width(), bh = box.height();
	int w = bw, h = w * 5 / 8;
	if (h > bh) {
		h = bh;
		w = h * 8 / 5;
	}
	const int x = box.left + (bw - w) / 2;
	const int y = box.top + (bh - h) / 2;
	return Common::Rect((int16)x, (int16)y, (int16)(x + w), (int16)(y + h));
}

void comparePanelRects(int overlayW, int overlayH,
                       Common::Rect &leftFrame, Common::Rect &rightFrame) {
	const int half = overlayW / 2;
	leftFrame  = fitAspectCentered(Common::Rect(0, 0, (int16)half, (int16)overlayH));
	rightFrame = fitAspectCentered(Common::Rect((int16)half, 0, (int16)overlayW, (int16)overlayH));
}

void scaleBlitNearest(Graphics::Surface &dest, const Common::Rect &destRect,
                      const Graphics::Surface &src) {
	scaleBlitNearest(dest, destRect, src, Common::Rect(0, 0, (int16)src.w, (int16)src.h));
}

void scaleBlitNearest(Graphics::Surface &dest, const Common::Rect &destRect,
                      const Graphics::Surface &src, const Common::Rect &srcRect) {
	const int dw = destRect.width(), dh = destRect.height();
	const int sw = srcRect.width(), sh = srcRect.height();
	if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
		return;
	// EXACT rational mapping (sx = srcRect.left + dx*srcW/dstW), deliberately NOT
	// ManagedSurface's truncated 8.8 fixed-point step (scaleX = 256*srcW/dstW): that
	// truncation drifts by up to ~1 src px per 256 dest px (~12 overlay px across a
	// 2862-wide plate), which visibly skewed the plate down-right relative to the
	// exactly-placed cels.
	// Fast path: same 32bpp format -> row pointers + a precomputed column map.
	if (dest.format == src.format && dest.format.bytesPerPixel == 4) {
		Common::Array<int> colMap;
		colMap.resize(dw);
		for (int dx = 0; dx < dw; dx++)
			colMap[dx] = srcRect.left + dx * sw / dw;
		for (int dy = 0; dy < dh; dy++) {
			const int sy = srcRect.top + dy * sh / dh;
			const uint32 *srcRow = (const uint32 *)src.getBasePtr(0, sy);
			uint32 *destRow = (uint32 *)dest.getBasePtr(destRect.left, destRect.top + dy);
			for (int dx = 0; dx < dw; dx++)
				destRow[dx] = srcRow[colMap[dx]];
		}
		return;
	}
	for (int dy = 0; dy < dh; dy++) {
		const int sy = srcRect.top + dy * sh / dh;
		for (int dx = 0; dx < dw; dx++) {
			const int sx = srcRect.left + dx * sw / dw;
			dest.setPixel(destRect.left + dx, destRect.top + dy, src.getPixel(sx, sy));
		}
	}
}

void blendScaleBlitNearest(Graphics::ManagedSurface &dest, const Graphics::Surface &cel,
                           const Common::Rect &destRect, bool flipH,
                           const Common::Rect *clip) {
	const int dw = destRect.width(), dh = destRect.height();
	if (dw <= 0 || dh <= 0 || cel.w <= 0 || cel.h <= 0)
		return;
	Graphics::Surface *d = dest.surfacePtr();
	// Fallback for formats the fast path can't take (never hit in practice: the scene
	// and all cel sources are RGBA32). blendBlitFrom scales with the truncated step,
	// but on a mismatched-format path exact geometry is already lost to conversion.
	// (`clip` is not honored here -- acceptable on a path that never runs.)
	if (d->format != cel.format || d->format.bytesPerPixel != 4) {
		dest.blendBlitFrom(cel, Common::Rect(0, 0, cel.w, cel.h), destRect,
		                   flipH ? Graphics::FLIP_H : Graphics::FLIP_NONE);
		return;
	}
	// EXACT rational mapping (sx = dx*celW/dstW) so the cel's content occupies exactly
	// the dest rect the caller computed with the same rational math -- matching the
	// plate's scaleBlitNearest. Src-over per pixel (cel alpha is 0/255 in practice).
	// The paint window is the dest surface intersected with `clip`; source coords stay
	// relative to the FULL destRect, so off-window extent is cropped, never squished.
	int cl = 0, ct = 0, cr = d->w, cb = d->h;
	if (clip) {
		cl = MAX<int>(cl, clip->left);
		ct = MAX<int>(ct, clip->top);
		cr = MIN<int>(cr, clip->right);
		cb = MIN<int>(cb, clip->bottom);
	}
	const int x0 = MAX<int>(destRect.left, cl), x1 = MIN<int>(destRect.right, cr);
	const int y0 = MAX<int>(destRect.top, ct), y1 = MIN<int>(destRect.bottom, cb);
	if (x0 >= x1 || y0 >= y1)
		return;
	Common::Array<int> colMap;
	colMap.resize(x1 - x0);
	for (int ox = x0; ox < x1; ox++) {
		const int dx = ox - destRect.left;
		const int sx = dx * cel.w / dw;
		colMap[ox - x0] = flipH ? (cel.w - 1 - sx) : sx;
	}
	const int aShift = d->format.aShift;
	for (int oy = y0; oy < y1; oy++) {
		const int sy = (oy - destRect.top) * cel.h / dh;
		const uint32 *srcRow = (const uint32 *)cel.getBasePtr(0, sy);
		uint32 *destRow = (uint32 *)d->getBasePtr(0, oy);
		for (int ox = x0; ox < x1; ox++) {
			const uint32 s = srcRow[colMap[ox - x0]];
			const uint32 a = (s >> aShift) & 0xFF;
			if (a == 0)
				continue;
			if (a == 255) {
				destRow[ox] = s;
				continue;
			}
			// Rare general case: integer src-over on each 8-bit channel.
			const uint32 dPix = destRow[ox];
			uint32 out = 0;
			for (int shift = 0; shift <= 24; shift += 8) {
				const uint32 sc = (s >> shift) & 0xFF;
				const uint32 dc = (dPix >> shift) & 0xFF;
				const uint32 oc = (shift == aShift) ? MIN<uint32>(255, sc + dc * (255 - a) / 255)
				                                    : (sc * a + dc * (255 - a)) / 255;
				out |= oc << shift;
			}
			destRow[ox] = out;
		}
	}
}

void extractChangedBoxes(const byte *prev, const byte *cur, int w, int h,
                         Common::Array<Common::Rect> &out) {
	Common::Array<Common::Rect> runs;
	for (int y = 0; y < h; y++) {
		int x = 0;
		while (x < w) {
			if (prev[(uint)y * w + x] != cur[(uint)y * w + x]) {
				const int start = x;
				while (x < w && prev[(uint)y * w + x] != cur[(uint)y * w + x])
					x++;
				runs.push_back(Common::Rect((int16)start, (int16)y, (int16)x, (int16)(y + 1)));
			} else {
				x++;
			}
		}
	}
	coalesceDirtyRects(runs, Common::Rect(0, 0, (int16)w, (int16)h), out);
}

void filterForegroundCaptureRegions(const Common::Array<Common::Rect> &captured,
                                    const Common::Array<Common::Rect> &liveSpriteRects,
                                    Common::Array<Common::Rect> &out) {
	for (uint i = 0; i < captured.size(); i++) {
		bool overlaps = false;
		for (uint j = 0; j < liveSpriteRects.size(); j++) {
			if (captured[i].intersects(liveSpriteRects[j])) {
				overlaps = true;
				break;
			}
		}
		if (!overlaps)
			out.push_back(captured[i]);
	}
}

void collectUiTextRects(const Common::Array<UiElement> &elems, uint32 genericToken,
                        Common::Array<Common::Rect> &out) {
	(void)genericToken;
	for (uint i = 0; i < elems.size(); i++)
		if (elems[i].type == kUiText)
			out.push_back(elems[i].nativeRect);
}

int rectCoverageFraction(const Common::Rect &inner, const Common::Rect &outer) {
	const int area = inner.width() * inner.height();
	if (area <= 0)
		return 0;
	Common::Rect isect = inner.findIntersectingRect(outer);
	const int cov = isect.width() * isect.height();
	if (cov <= 0)
		return 0;
	return (int)((cov * 100) / area);
}

bool restoreReclaimsStamp(const Common::Rect &restoreRect, const Common::Rect &stampRect,
                          int minCoveragePct) {
	// Symmetric with the capture-time reveal suppression: the stamp is reclaimed when the
	// restore covers >= minCoveragePct of it. Empty stamp -> not reclaimed (nothing to drop).
	if (stampRect.isEmpty())
		return false;
	return rectCoverageFraction(stampRect, restoreRect) >= minCoveragePct;
}

void filterForegroundCaptureRegionsCovered(const Common::Array<Common::Rect> &captured,
                                           const Common::Array<Common::Rect> &exclude,
                                           int minCoveragePct, Common::Array<Common::Rect> &out) {
	for (uint i = 0; i < captured.size(); i++) {
		bool drop = false;
		for (uint j = 0; j < exclude.size(); j++) {
			if (rectCoverageFraction(captured[i], exclude[j]) >= minCoveragePct) {
				drop = true;
				break;
			}
		}
		if (!drop)
			out.push_back(captured[i]);
	}
}

bool regionIsCapturedWindowBody(const Common::Array<UiElement> &elems, uint32 owner,
                                const Common::Rect &region, int minMutualPct) {
	if (owner == 0)
		return false;
	for (uint i = 0; i < elems.size(); i++) {
		if (elems[i].type != kUiWindow || elems[i].token != owner)
			continue;
		if (rectCoverageFraction(region, elems[i].nativeRect) >= minMutualPct &&
		    rectCoverageFraction(elems[i].nativeRect, region) >= minMutualPct)
			return true;
	}
	return false;
}

void dedupeGenericTextElements(Common::Array<UiElement> &elems, uint32 genericToken) {
	// genericToken is a namespace prefix (high nibble): generic text is tokened per window
	// (0x60000000 | port->id), so match on the namespace, not an exact value.
	const uint32 ns = kTokenNamespaceMask;
	for (uint i = 0; i < elems.size();) {
		bool drop = false;
		if ((elems[i].token & ns) == genericToken) {
			for (uint j = 0; j < elems.size(); j++) {
				if (j == i)
					continue;
				if ((elems[j].token & ns) == genericToken)
					continue;
				const UiElementType jt = elems[j].type;
				// The dedup exists to reconcile ONE native text draw seen by two hooks: the
				// semantic control hook (kControlTokenNs) and the generic Box hook. So the
				// covering element that lets us drop the generic copy must itself be a genuine
				// control-namespace text element. A non-control, non-generic element -- e.g. a
				// bitsShow region captured with a raw save-under-handle token (namespace 0,
				// empty text) that the intro's decorative drop-cap/frame animation draws over
				// the upper caption rows -- is NOT a text duplicate and must never drop the
				// caption line it merely overlaps (it was silently deleting every caption line
				// but the last).
				const bool jRendersText = (elems[j].token & ns) == kControlTokenNs &&
				                          (jt == kUiText || jt == kUiButton || jt == kUiTextEdit);
				// A control that renders the same text usually draws its label at a small
				// offset inside its box, so dedup on substantial overlap (>=70%), not strict
				// containment. Window/icon never drop the text they enclose.
				if (jRendersText && rectCoverageFraction(elems[i].nativeRect, elems[j].nativeRect) >= 70) {
					drop = true; break;
				}
			}
		}
		if (drop)
			elems.remove_at(i);
		else
			i++;
	}
}

bool windowShouldHugContent(const Common::Rect &winRect, int screenW, int screenH) {
	if (screenW <= 0 || screenH <= 0)
		return true;
	// >= 60% of the screen area: this is a full-screen "screen" window, keep SCI's dims.
	const int64 winArea = (int64)winRect.width() * winRect.height();
	return winArea * 10 < (int64)screenW * screenH * 6;
}

RogerCompositor::~RogerCompositor() {
	if (_bgCache) {
		_bgCache->free();
		delete _bgCache;
	}
	if (_overlayConv) {
		_overlayConv->free();
		delete _overlayConv;
	}
}

// Tight 32bpp -> 32bpp channel shuffle (both 4 bytes/pixel, 8-bit channels). Extracts
// each channel by the source PixelFormat's shift and repacks by the destination's, so
// colours are correct by construction. Used instead of Surface::convertTo for the
// per-frame overlay conversion: convertTo allocates+frees a full-overlay surface every
// frame (~21 MB at this resolution), which dominated present cost. This writes into a
// persistent buffer in a flat loop -- no allocation, no per-pixel function calls.
static void convert32(byte *dstP, const byte *srcP, int dstPitch, int srcPitch,
                      int w, int h, const Graphics::PixelFormat &df, const Graphics::PixelFormat &sf) {
	const int sR = sf.rShift, sG = sf.gShift, sB = sf.bShift, sA = sf.aShift;
	const int dR = df.rShift, dG = df.gShift, dB = df.bShift, dA = df.aShift;
	const bool srcHasA = sf.aBits() != 0;
	const bool dstHasA = df.aBits() != 0;
	for (int y = 0; y < h; y++) {
		const uint32 *src = (const uint32 *)(srcP + (uint)y * srcPitch);
		uint32 *dst = (uint32 *)(dstP + (uint)y * dstPitch);
		for (int x = 0; x < w; x++) {
			const uint32 p = src[x];
			uint32 o = (((p >> sR) & 0xFF) << dR) | (((p >> sG) & 0xFF) << dG) | (((p >> sB) & 0xFF) << dB);
			if (dstHasA)
				o |= (srcHasA ? ((p >> sA) & 0xFF) : 0xFFu) << dA;
			dst[x] = o;
		}
	}
}

// Roger UI type scale: target on-screen cell heights expressed in native 320x200
// rows (the compositor scales them to the overlay). One body size for dialog /
// message / input / list / button text, one larger heading size for the score
// banner and menu titles. The user's roger_ui_font_scale multiplies both.
static const int kRoleBodyNativeH    = 9;
static const int kRoleHeadingNativeH = 11;
// Minimum clearance (dest pixels) between text/caret and the inner edge of a
// frame border, applied in addition to the border thickness so text never
// touches the border regardless of overlay resolution.
static const int kUiTextPad = 1;

void RogerCompositor::setRoom(Graphics::Surface *cleanPlate, ViewCache *views) {
	_plate = cleanPlate;
	_views = views;
	// Invalidate the static-background cache so the new room rebuilds it. (The plate
	// pointer can be freed+reallocated to the same address across rooms, so an identity
	// check alone could go stale -- null it here on every room load to be safe.)
	_bgPlate = nullptr;
}

void RogerCompositor::setPicture(int picW, int picH, int picScreenTop) {
	_picW = picW;
	_picH = picH;
	_picScreenTop = picScreenTop;
}

void RogerCompositor::setPriorityMask(const byte *priority, int priW, int priH) {
	_priority = priority;
	_priorityW = priW;
	_priorityH = priH;
}

void RogerCompositor::renderScene(Graphics::ManagedSurface &dest, const Common::Array<Sprite> &sprites,
                                  const Common::Rect &gameRect) {
	const int W = dest.w, H = dest.h;

	// Roll the sprite dirty set at renderScene (scene) granularity -- NOT present granularity.
	// A UI-only present (presentWithUi: cursor/dialog, no renderScene) must not discard these,
	// or a sprite that moves across that present leaves a stale "shadow." presentToOverlay's
	// dirtyUnion adds _sceneDirtyCur union _sceneDirtyPrev, so the vacated position is repainted.
	_sceneDirtyPrev.clear();
	for (uint i = 0; i < _sceneDirtyCur.size(); i++)
		_sceneDirtyPrev.push_back(_sceneDirtyCur[i]);
	_sceneDirtyCur.clear();

	// The picture (plate + sprites) is drawn into _pictureDest -- the overlay-space
	// rect the caller computed (via roger_coords::computeGameRect/computePictureRect)
	// to coincide with the native game's on-screen PICTURE region, i.e. below the
	// status bar. The overlay is alpha-blended over the still-rendered native game,
	// so everything outside this rect (letterbox + the reserved status-bar strip) is
	// left transparent and the native pixels show through there. Falls back to the
	// full surface when unset (unit tests, or no caller geometry).
	const Common::Rect picRect = _pictureDest.isEmpty()
		? Common::Rect(0, 0, (int16)W, (int16)H)
		: _pictureDest;
	const int GW = picRect.width(), GH = picRect.height();

	// Cel rects are in SCI picture-window-local coords (_picW x _picH, 320x190 for
	// SCI0); the plate encodes that same picture, so both map into picRect.
	const int PIC_W = _picW, PIC_H = _picH;

	// 0+1) Static background = opaque-black letterbox (everything OUTSIDE the game rect,
	//      so the native render/cursor can't leak there; the status strip inside it stays
	//      transparent for the native Sierra menu icon) + the clean plate scaled into the
	//      game rect. None of this changes within a room, so build it ONCE per geometry
	//      into _bgCache and seed each frame with a straight copy -- re-scaling the
	//      1920x1140 plate every frame was wasted work on SCI's kAnimate (game-clock) path.
	//      Output is byte-identical to the old per-frame clear+letterbox+scale.
	//      SAFETY: only cache once the plate is actually present and blitted, so a
	//      transient null-plate frame can never bake a black/transparent picRect into the
	//      cache (which would then persist). _bgPlate is nulled in setRoom on room change.
	const Graphics::PixelFormat fmt = dest.surfacePtr()->format;
	const uint32 black = fmt.ARGBToColor(255, 0, 0, 0);
	const bool bgValid = _bgCache && _bgPlate && _bgPlate == _plate &&
	                     _bgCache->w == W && _bgCache->h == H && _bgCache->format == fmt &&
	                     _bgPicRect == picRect && _bgGameRect == gameRect;

	// Pre-pass: compute each sprite's overlay dest-rect BEFORE seeding, so the seed can be
	// bounded to the union of where sprites are now plus where they were last frame. The dst
	// math (and the empty/skip rules) is IDENTICAL to the draw loop below -- the loop reuses
	// these exact values (see spriteDst), so _sceneDirtyCur and the drawn pixels are unchanged.
	const int PIC_W2 = _picW, PIC_H2 = _picH;
	Common::Array<Common::Rect> spriteDst; // parallel to `sprites`; empty rect == skipped
	spriteDst.reserve(sprites.size());
	for (uint i = 0; i < sprites.size(); i++) {
		const Sprite &s = sprites[i];
		const Graphics::Surface *cel = _views ? _views->getCel(s.viewId, s.loopNo, s.celNo) : nullptr;
		if (!cel)
			cel = s.celOverride;
		Common::Rect dst; // empty by default (skipped sprites: missing cel / bad PIC dims)
		if (cel && PIC_W2 > 0 && PIC_H2 > 0) {
			dst = Common::Rect(
				(int16)(picRect.left + (int)s.celRect.left   * GW / PIC_W2),
				(int16)(picRect.top  + (int)s.celRect.top    * GH / PIC_H2),
				(int16)(picRect.left + (int)s.celRect.right  * GW / PIC_W2),
				(int16)(picRect.top  + (int)s.celRect.bottom * GH / PIC_H2));
			// Game view cels sit flush in plate openings whose omyac-smoothed
			// boundary can poke 1-2 px past the exact edge (the SQ3 pod-door cyan
			// seam through the cel's transparent margin). Draw them a few px larger
			// so the opaque content covers the fringe on every side -- a <1% content
			// stretch on normal cels. Capped to 1/8 of the dest size so tiny cels
			// (projectiles, sparks) don't visibly fatten. Feeder B stamps and
			// exact-geometry tests keep coverGrow off.
			// NEVER clip dst itself to picRect: amputating the off-screen extent
			// from the rect while the full cel scales into what remains compressed
			// sprites at every screen edge (the "views squish as they exit" bug).
			// The blit crops to picRect instead (blendScaleBlitNearest's clip arg).
			static const int kCelCoverPx = 3;
			if (s.coverGrow && !dst.isEmpty()) {
				const int g = MIN<int>(kCelCoverPx, MIN<int>(dst.width() / 8, dst.height() / 8));
				if (g > 0)
					dst.grow((int16)g);
			}
		}
		spriteDst.push_back(dst);
		if (!dst.isEmpty())
			_sceneDirtyCur.push_back(dst); // sprite region (scene-granularity dirty; see header)
	}

	// fullSeed: re-seed the WHOLE game region (current behavior) on a static-background
	// rebuild (room/geometry change), the no-geometry test path, or a periodic heal --
	// otherwise the persistent scratch background outside the seed union is stale. Each layer
	// self-heals independently of presentToOverlay's present heal.
	static const int kSceneHealFrames = 300; // ~5s at 60fps; cheap insurance
	const bool fullSeed = !bgValid || _bgGameRect.isEmpty() ||
	                      (_framesSinceFullSeed >= kSceneHealFrames);
	if (fullSeed)
		_framesSinceFullSeed = 0;
	else
		_framesSinceFullSeed++;

	// Coalesced union (clamped to the game rect by coalesceDirtyRects) of everything that may
	// hold stale DYNAMIC pixels in the persistent scratch surface and must be re-seeded with
	// clean background this frame:
	//   - current + just-vacated sprite rects (_sceneDirtyCur union _sceneDirtyPrev), and
	//   - the previous present's UI/cursor/generic-region rects (_dirtyPrev) -- under the
	//     SOFTWARE cursor (the default), compositeCursor paints the cursor into this same
	//     scratch surface AFTER renderScene, so last present's cursor position sits here and
	//     would trail if not re-seeded. _dirtyCur (this present's) is excluded: those pixels
	//     are (re)painted later this frame anyway, and the caches are snapshotted cursor-free
	//     BEFORE compositeCursor, so seeding the vacated rect leaves the right (clean) pixels.
	// The bounded seed and the provider's scene-cache copies both ride this union.
	_lastSeedUnion.clear();
	{
		Common::Array<Common::Rect> raw;
		for (uint i = 0; i < _sceneDirtyCur.size(); i++)
			raw.push_back(_sceneDirtyCur[i]);
		for (uint i = 0; i < _sceneDirtyPrev.size(); i++)
			raw.push_back(_sceneDirtyPrev[i]);
		for (uint i = 0; i < _dirtyPrev.size(); i++)
			raw.push_back(_dirtyPrev[i]);
		// Deferred mid-cycle content-removal rects (present-barrier ghost fix): the fresh-frame
		// present that consumes this frame's scratch would otherwise push these from stale last-
		// frame pixels, since _dirtyCur is excluded from the seed. Re-seed clean background over
		// them here. Empty in the common no-deferred-marks case (no added cost on the walking path).
		for (uint i = 0; i < _sceneDeferredDirty.size(); i++)
			raw.push_back(_sceneDeferredDirty[i]);
		_sceneDeferredDirty.clear();
		Common::Rect bounds = _bgGameRect;
		bounds.clip(Common::Rect(0, 0, (int16)W, (int16)H));
		coalesceDirtyRects(raw, bounds, _lastSeedUnion);
	}
	_lastSceneFull = fullSeed;

	if (_diag)
		warning("ROGER-DIAG[renderScene]: fullSeed=%d seedUnion=%u framesSinceFullSeed=%d sprites=%u",
		        fullSeed ? 1 : 0, (unsigned)_lastSeedUnion.size(), _framesSinceFullSeed, (unsigned)sprites.size());

	if (bgValid) {
		// Seed the game region from _bgCache to lay down clean background where sprites are
		// now and where they were last frame. The static black letterbox AND the untouched
		// interior of the persistent scratch surface stay correct between frames (this scratch
		// is reused, the static bg outside the union does not change, and sprites only draw
		// inside their own dst <= union), so a bounded seed is pixel-identical to the full one.
		// Empty gameRect (tests) -> full copy. fullSeed -> whole game region (rebuild/heal).
		if (_bgGameRect.isEmpty()) {
			dest.copyFrom(*_bgCache);
		} else if (fullSeed) {
			Common::Rect gr = _bgGameRect;
			gr.clip(Common::Rect(0, 0, (int16)W, (int16)H));
			dest.surfacePtr()->copyRectToSurface(*_bgCache->surfacePtr(), gr.left, gr.top, gr);
		} else {
			for (uint i = 0; i < _lastSeedUnion.size(); i++) {
				const Common::Rect &r = _lastSeedUnion[i]; // already clipped to game rect
				dest.surfacePtr()->copyRectToSurface(*_bgCache->surfacePtr(), r.left, r.top, r);
			}
		}
	} else {
		_bgRebuilt = true; // letterbox redrawn this frame -> present full overlay once
		if (gameRect.isEmpty()) {
			dest.clear(black); // no geometry (unit tests): whole surface is a solid blocker
		} else {
			dest.clear(0); // transparent base; status strip + (later) plate keep/overwrite it
			const int16 gt = (int16)MAX<int>(0, gameRect.top);
			const int16 gb = (int16)MIN<int>(H, gameRect.bottom);
			const int16 gl = (int16)MAX<int>(0, gameRect.left);
			const int16 gr = (int16)MIN<int>(W, gameRect.right);
			if (gt > 0)
				dest.fillRect(Common::Rect(0, 0, (int16)W, gt), black);
			if (gb < H)
				dest.fillRect(Common::Rect(0, gb, (int16)W, (int16)H), black);
			if (gl > 0)
				dest.fillRect(Common::Rect(0, gt, gl, gb), black);
			if (gr < W)
				dest.fillRect(Common::Rect(gr, gt, (int16)W, gb), black);
		}
		// Clean plate, scaled into the game rect (aspect preserved). EXACT nearest
		// scale -- NOT ManagedSurface::blitFrom, whose truncated 8.8 fixed-point step
		// (scaleX = 256*srcW/dstW) drifted plate content down-right by up to ~12
		// overlay px across the screen while cel dest rects use exact rational math
		// (the SQ3 pod-door "cyan gap" misalignment).
		if (_plate)
			scaleBlitNearest(*dest.surfacePtr(), picRect, *_plate);

		// Snapshot this fully-drawn background into the cache for subsequent frames --
		// but only when a plate was actually drawn, so we never cache an empty picRect.
		if (_plate) {
			if (!_bgCache || _bgCache->w != W || _bgCache->h != H || _bgCache->format != fmt) {
				if (_bgCache) {
					_bgCache->free();
					delete _bgCache;
				}
				_bgCache = new Graphics::ManagedSurface(W, H, fmt);
			}
			_bgCache->copyFrom(dest);
			_bgPlate = _plate;
			_bgPicRect = picRect;
			_bgGameRect = gameRect;
		}
	}

	Graphics::Surface *destSurf = dest.surfacePtr();

	// 2) Sprites back-to-front. Each cel is drawn, then occluded per-pixel by the
	//    priority map (SCI's own model): wherever the priority there is greater than
	//    the sprite's priority, the plate's baked-in foreground is restored over the
	//    cel, hiding the sprite. No slices, no flicker.
	for (uint i = 0; i < sprites.size(); i++) {
		const Sprite &s = sprites[i];
		const Graphics::Surface *cel = _views ? _views->getCel(s.viewId, s.loopNo, s.celNo) : nullptr;
		if (!cel)
			cel = s.celOverride;
		if (!cel) {
			warning("ROGER: missing hires cel view=%d loop=%d cel=%d (skipped)", s.viewId, s.loopNo, s.celNo);
			continue;
		}
		if (PIC_W <= 0 || PIC_H <= 0)
			continue;
		// Reuse the dest-rect computed in the pre-pass (same integer-scale math as before, and
		// it was already pushed to _sceneDirtyCur and folded into the bounded background seed).
		const Common::Rect &dst = spriteDst[i];
		// Alpha-aware blit: respects each pixel's alpha so transparent non-black
		// pixels (common in exported spritesheets) do not render opaque (halos).
		// Exact-rational scaling (blendScaleBlitNearest), matching the plate's
		// scaleBlitNearest -- blendBlitFrom's truncated 8.8 step squished large cels
		// by a few px toward their top-left.
		// No FLIP_H here: GfxView::getBitmap() already mirrors a mirrored loop's pixels,
		// and BOTH cel sources (renderNativeCel + the hires ViewCache's generateViewCel)
		// read through it, so the cel arrives already-mirrored. s.mirror stays false.
		// picRect clips the PAINT (crop at the screen edge / letterbox boundary); the
		// dst rect keeps its off-screen extent so the cel is never compressed.
		blendScaleBlitNearest(dest, *cel, dst, s.mirror, &picRect);

		// Per-pixel priority occlusion against the plate.
		if (_priority && _plate) {
			const int x0 = MAX<int>(dst.left, picRect.left);
			const int y0 = MAX<int>(dst.top, picRect.top);
			const int x1 = MIN<int>(dst.right, picRect.right);
			const int y1 = MIN<int>(dst.bottom, picRect.bottom);

			// CRITICAL: sample the plate with the SAME mapping the compositor's
			// scaleBlitNearest used to draw the background plate above -- the exact
			// rational `i*srcW/dstW`. (Historically this matched blitFrom's truncated
			// 8.8 fixed-point step instead; that kept the splat consistent with the
			// displayed background but baked blitFrom's ~12px down-right content drift
			// into the scene -- the plate-vs-cel misalignment. Both now use exact math.)
			// With the hires priority map, _priorityW/_priorityH == _plate->w/h, so
			// the overlay->plate->priority mapping below collapses to a 1:1 lookup at
			// the displayed plate pixel. The math still generalises if they differ.
			const int pw = _plate->w, ph = _plate->h;
			const byte spritePri = s.priority;

			// Hot path (runs for every pixel of every sprite, every frame): when the
			// scene and plate share the exact RGBA32 layout, an occluded pixel is a raw
			// 32-bit word copy via row pointers -- no per-pixel virtual getPixel/setPixel
			// (each of those does format dispatch + bounds checks). The plate column is
			// advanced incrementally (accX += pw, plX = accX/GW -- exact rational, no
			// per-pixel multiply), and picX collapses to plX when the priority map
			// matches the plate resolution (the normal hires case). Falls back to
			// getPixel/setPixel if the formats ever differ. Same priority>sprite rule.
			const bool fast = (destSurf->format == _plate->format &&
			                   destSurf->format.bytesPerPixel == 4);
			const bool priMatchesPlate = (_priorityW == pw);

			for (int oy = y0; oy < y1; oy++) {
				// Plate row the background scaler drew at this overlay row.
				const int plY = (oy - picRect.top) * ph / GH;
				if (plY < 0 || plY >= ph)
					continue;
				// Sample the priority at the SAME scene location the displayed plate
				// occupies, so the occlusion boundary tracks the displayed plate.
				const int prY = plY * _priorityH / ph + _picScreenTop;
				if (prY < 0 || prY >= _priorityH)
					continue;
				const byte *priRow = _priority + (uint)prY * _priorityW;
				const uint32 *plateRow = fast ? (const uint32 *)_plate->getBasePtr(0, plY) : nullptr;
				uint32 *destRow = fast ? (uint32 *)destSurf->getBasePtr(0, oy) : nullptr;

				int accX = (x0 - picRect.left) * pw;
				for (int ox = x0; ox < x1; ox++, accX += pw) {
					const int plX = accX / GW;
					if (plX < 0 || plX >= pw)
						continue;
					const int picX = priMatchesPlate ? plX : (plX * _priorityW / pw);
					if (picX < 0 || picX >= _priorityW)
						continue;
					if (priRow[picX] > spritePri) {
						if (fast)
							destRow[ox] = plateRow[plX];
						else
							destSurf->setPixel(ox, oy, _plate->getPixel(plX, plY));
					}
				}
			}
		}
	}
}

void RogerCompositor::dirtyUnion(const Common::Rect &bounds, Common::Array<Common::Rect> &out) const {
	Common::Array<Common::Rect> raw;
	for (uint i = 0; i < _dirtyCur.size(); i++)
		raw.push_back(_dirtyCur[i]);
	for (uint i = 0; i < _dirtyPrev.size(); i++)
		raw.push_back(_dirtyPrev[i]);
	for (uint i = 0; i < _sceneDirtyCur.size(); i++)
		raw.push_back(_sceneDirtyCur[i]);
	for (uint i = 0; i < _sceneDirtyPrev.size(); i++)
		raw.push_back(_sceneDirtyPrev[i]);
	coalesceDirtyRects(raw, bounds, out);
}

void RogerCompositor::rollPresentDirty() {
	_dirtyPrev.clear();
	for (uint i = 0; i < _dirtyCur.size(); i++)
		_dirtyPrev.push_back(_dirtyCur[i]);
	_dirtyCur.clear();
}

void RogerCompositor::presentToOverlay(Graphics::ManagedSurface &scene) {
	// The scene is composited in RGBA32 (so the alpha-aware blendBlitFrom works -
	// it only accepts an RGBA32 destination). The OSystem overlay, however, uses
	// g_system->getOverlayFormat(), which is often NOT RGBA32 (e.g. RGB565), so
	// convert before handing the pixels to copyRectToOverlay.
	const Graphics::Surface *s = scene.surfacePtr();
	const Graphics::PixelFormat overlayFmt = g_system->getOverlayFormat();
	// Never push more than the current overlay can hold: the backend's
	// copyRectToOverlay asserts x+w <= overlayW / y+h <= overlayH (a hard crash if a
	// stale, over-sized scene survives a resize). Clamp defensively as a backstop to
	// the per-present rescale in the providers.
	const int OW = g_system->getOverlayWidth();
	const int OH = g_system->getOverlayHeight();
	if (OW <= 0 || OH <= 0) {
		_dirtyCur.clear();
		_dirtyPrev.clear();
		_sceneDirtyCur.clear();
		_sceneDirtyPrev.clear();
		return;
	}
	const Common::Rect fullRect(0, 0, (int16)(s->w < OW ? s->w : OW), (int16)(s->h < OH ? s->h : OH));

	// Decide the regions to push this frame.
	// Full present when: dirty present is off, the background was just (re)built
	// (room/geometry/F10/first frame), no game rect yet, or the periodic heal is due
	// (heals any region a missed dirty rect would have left stale, bounded to ~5s).
	bool full = !_dirtyPresent || _bgRebuilt || _bgGameRect.isEmpty() ||
	            _framesSinceFullPresent >= kHealFrames;

	Common::Array<Common::Rect> push;
	if (full) {
		// Lay the whole game region (or whole overlay on a bg rebuild, to cover letterbox).
		Common::Rect r = (_bgRebuilt || _bgGameRect.isEmpty()) ? fullRect : _bgGameRect;
		r.clip(fullRect);
		if (r.isEmpty())
			r = fullRect;
		push.push_back(r);
		_framesSinceFullPresent = 0;
	} else {
		// Push only what changed: this present's + last present's UI/cursor rects AND this
		// renderScene's + last renderScene's sprite rects (so a moved sprite/cursor/closed
		// dialog repaints the clean background it vacated). See dirtyUnion.
		dirtyUnion(fullRect, push);
		_framesSinceFullPresent++;
	}

	_bgRebuilt = false;

	// Convert + upload each region.
	for (uint i = 0; i < push.size(); i++) {
		const Common::Rect &region = push[i];
		if (region.isEmpty())
			continue;
		if (s->format == overlayFmt) {
			g_system->copyRectToOverlay(s->getBasePtr(region.left, region.top), s->pitch,
			                            region.left, region.top, region.width(), region.height());
		} else if (s->format.bytesPerPixel == 4 && overlayFmt.bytesPerPixel == 4) {
			if (!_overlayConv || _overlayConv->w != s->w || _overlayConv->h != s->h ||
			    _overlayConv->format != overlayFmt) {
				if (_overlayConv) {
					_overlayConv->free();
					delete _overlayConv;
				}
				_overlayConv = new Graphics::Surface();
				_overlayConv->create((uint16)s->w, (uint16)s->h, overlayFmt);
			}
			if (_overlayConv->getPixels()) {
				convert32((byte *)_overlayConv->getBasePtr(region.left, region.top),
				          (const byte *)s->getBasePtr(region.left, region.top),
				          _overlayConv->pitch, s->pitch, region.width(), region.height(),
				          overlayFmt, s->format);
				g_system->copyRectToOverlay(_overlayConv->getBasePtr(region.left, region.top),
				                            _overlayConv->pitch, region.left, region.top,
				                            region.width(), region.height());
			}
		} else {
			Graphics::Surface sub = scene.surfacePtr()->getSubArea(region);
			Graphics::Surface *conv = sub.convertTo(overlayFmt);
			if (conv) {
				g_system->copyRectToOverlay(conv->getPixels(), conv->pitch,
				                            region.left, region.top, region.width(), region.height());
				conv->free();
				delete conv;
			}
		}
	}

	// Roll this present's UI/cursor dirty set into "previous" for the next present. (Sprite
	// rects roll separately, in renderScene -- see _sceneDirtyCur.)
	rollPresentDirty();

	if (_diag)
		warning("ROGER-DIAG[present]: full=%d regions=%u", full ? 1 : 0, (unsigned)push.size());

	if (_presentLog) {
		uint32 area = 0;
		for (uint i = 0; i < push.size(); i++)
			area += (uint32)push[i].width() * (uint32)push[i].height();
		warning("ROGER-PRESENT full=%d regions=%u area=%u", full ? 1 : 0, (unsigned)push.size(), area);
	}

	g_system->showOverlay(false);
}

void RogerCompositor::renderUiLayer(Graphics::ManagedSurface &dest,
                                    const Common::Array<UiElement> &elems,
                                    const byte *palette, const Common::Rect &gameRect,
                                    const RogerTextRenderer *text, const RogerTextRenderer *altText) {
	const Graphics::PixelFormat &fmt = dest.surfacePtr()->format;
	// Per-element target cell height comes from each element's NATIVE SCI font height
	// (scaled to the overlay), so the crisp text occupies the same footprint as the
	// original. Elements without a captured metric (nativeFontH == 0) fall back to the
	// legacy role heights so no path regresses.
	const int overlayH = gameRect.height();
	const int fallbackBodyPx    = nativeRowsToOverlay(kRoleBodyNativeH,    overlayH);
	const int fallbackHeadingPx = nativeRowsToOverlay(kRoleHeadingNativeH, overlayH);

	// Shared type-scale pass: size every text element by its own caps first, then
	// let applySharedGroupScale shrink each sizing group together so siblings on
	// one screen/window render at one consistent scale (preserving native size
	// ratios) instead of each string fitting its own rect independently. Grouping
	// (textScaleGroup): generic text per screen, controls per window, multi-line
	// wrap-fit elements as singletons, alt-font always separate.
	Common::Array<TextSizeFit> fits;
	fits.resize(elems.size());
	for (uint i = 0; i < elems.size(); i++) {
		const UiElement &e = elems[i];
		if (e.type != kUiText && e.type != kUiButton && e.type != kUiTextEdit)
			continue;
		const RogerTextRenderer *tr = (e.useAltFont && altText) ? altText : text;
		if (!tr)
			continue;
		const Common::Rect d = sciRectToDest(e.nativeRect, gameRect);
		if (d.isEmpty())
			continue;
		const bool frame = e.hasFrame || e.type == kUiTextEdit ||
		                   (e.type == kUiText && (e.style & 0x8)) ||
		                   e.type == kUiButton;
		Common::Rect textRect = d;
		textRect.grow(-((frame ? 1 : 0) + kUiTextPad));
		if (textRect.isEmpty())
			continue;
		int targetPx = rogerTargetPx(e.nativeFontH, overlayH, 100);
		if (targetPx <= 0)
			targetPx = (e.textRole == kRoleHeading) ? fallbackHeadingPx : fallbackBodyPx;
		const int wCap = e.nativeTextW > 0 ? nativeRowsToOverlay(e.nativeTextW, overlayH) : 0;
		fits[i].group = textScaleGroup(e.token, e.useAltFont, wCap <= 0, i);
		fits[i].idealPx = tr->scaledIdealPx(targetPx);
		fits[i].fitPx = tr->fitPx(e.text, textRect.width(), textRect.height(),
		                          fits[i].idealPx, wCap, &e.glyphs);
		// Per-text-element sizing trace: the decisive tool for dialog-text-size faults --
		// shows each element's namespace token, native metric (font height + width cap),
		// sizing group and the chosen fit. Two rows for the same rect with different tokens
		// = a control/generic duplicate (the dialog-text-size-flip class).
		if (_diag)
			warning("ROGER-DIAG[textFit]: subsetN=%u i=%u tok=%08x type=%d nRect=(%d,%d,%d,%d) "
			        "nFontH=%d nTextW=%d wCap=%d group=%08x ideal=%d fit=%d '%.24s'",
			        elems.size(), i, e.token, (int)e.type,
			        e.nativeRect.left, e.nativeRect.top, e.nativeRect.right, e.nativeRect.bottom,
			        e.nativeFontH, e.nativeTextW,
			        wCap, fits[i].group, fits[i].idealPx, fits[i].fitPx, e.text.c_str());
	}
	applySharedGroupScale(fits);

	for (uint i = 0; i < elems.size(); i++) {
		const UiElement &e = elems[i];
		Common::Rect nr = e.nativeRect;
		if (e.type == kUiWindow && e.hasFrame) {
			// Union of the controls (text/buttons/edit/icons) sharing this window's token.
			Common::Rect content;
			bool haveContent = false;
			for (uint j = 0; j < elems.size(); j++) {
				if (j != i && elems[j].token == e.token) {
					if (!haveContent) {
						content = elems[j].nativeRect;
						haveContent = true;
					} else {
						content.extend(elems[j].nativeRect);
					}
				}
			}
			// SCI dialog/message windows (GfxPorts windows, token bit 0x40000000) reserve
			// more vertical space than their text needs -- SCI's window dims sit well above
			// the text, leaving a large empty band. Shrink-wrap the box to its actual
			// content so it hugs the text like a native SCI message window. The status/menu
			// bar (token 0x10000000) keeps its full SCI dims (it must span the screen), and
			// so does a near-full-screen window (windowShouldHugContent): hugging the QFG1
			// char-creation screen drew its border mid-screen and left the native render
			// leaking below the hug box.
			const int rows = _caps.screenRows > 0 ? _caps.screenRows : 200;
			if (haveContent && (e.token & kControlTokenNs) &&
			    windowShouldHugContent(e.nativeRect, 320, rows))
				nr = content;          // hug the controls; ignore SCI's oversized window dims
			else if (haveContent)
				nr.extend(content);    // status/menu bar etc.: window dims union controls
			nr.grow(2); // a little padding so controls are not flush against the border
		}
		const Common::Rect d = sciRectToDest(nr, gameRect);
		addDirtyRect(d); // UI element region (dirty-rect present)
		if (d.isEmpty())
			continue;
		// Pick the font renderer for this element (header/menu use the alt font).
		const RogerTextRenderer *tr = (e.useAltFont && altText) ? altText : text;
		// Final render size from the shared type-scale pass above (0 for non-text
		// elements and degenerate rects, which never reach the text path anyway).
		const int finalPx = fits[i].fitPx;

		// Background fill (opaque) for windows / buttons / edit fields.
		if (palette && e.backColor >= 0) {
			const byte *bc = palette + e.backColor * 3;
			dest.fillRect(d, fmt.ARGBToColor(255, bc[0], bc[1], bc[2]));
		}

		// Icon: blit the borrowed RGBA cel scaled into the rect (nearest).
		if (e.type == kUiIcon && e.iconSurface) {
			dest.blitFrom(*e.iconSurface,
				Common::Rect(0, 0, e.iconSurface->w, e.iconSurface->h), d);
		}

		// Frame (1px native -> scaled): framed windows, edit fields, selected
		// text/buttons. Windows honor hasFrame (SCI NOFRAME style bit): the
		// full-width status banner is pushed frameless, and framing it anyway
		// drew its bottom border as a dark line across the top of every scene.
		const bool frame = e.hasFrame || e.type == kUiTextEdit ||
		                   (e.type == kUiText && (e.style & 0x8)) ||
		                   e.type == kUiButton;
		if (palette && frame) {
			const bool isWindow = (e.type == kUiWindow);
			uint32 col;
			if (isWindow) {
				col = fmt.ARGBToColor(255, 0, 0, 0); // dialogs: always a black border like native SCI windows
			} else {
				const byte *pc = palette + (e.penColor >= 0 ? e.penColor : 0) * 3;
				col = fmt.ARGBToColor(255, pc[0], pc[1], pc[2]);
			}
			// 1 native px scaled to the overlay (min 1) so the border is visible at hires.
			int thick = isWindow ? nativeRowsToOverlay(1, gameRect.height()) : 1;
			if (thick < 1)
				thick = 1;
			for (int t = 0; t < thick; t++) {
				Common::Rect fr = d;
				fr.grow(-t);
				if (fr.isEmpty())
					break;
				dest.frameRect(fr, col);
			}
		}

		// Inner text rect: inset d by border thickness + kUiTextPad so text and
		// caret never render into the frame border. Non-window frames are always
		// 1px in dest pixels; kUiWindow never reaches the text+caret path.
		const int textThick = (frame && e.type != kUiWindow) ? 1 : 0;
		Common::Rect textRect = d;
		textRect.grow(-(textThick + kUiTextPad));

		// Text + caret.
		if (tr && finalPx > 0 && (e.type == kUiText || e.type == kUiButton || e.type == kUiTextEdit)
		    && !e.text.empty() && !textRect.isEmpty()) {
			const byte *pc = palette ? palette + (e.penColor >= 0 ? e.penColor : 0) * 3 : nullptr;
			const uint32 col = pc ? fmt.ARGBToColor(255, pc[0], pc[1], pc[2])
			                      : fmt.ARGBToColor(255, 255, 255, 255);
			tr->drawAtPx(dest, e.text, textRect, col, e.align, finalPx, e.vAlignTop, &e.glyphs);
		}
		// Caret on ANY edit field, blink-phase gated (the provider drives ~500 ms
		// flips mirroring native's kernelTexteditChange). Not gated on the
		// SELECTED style bit: that comes from the script's `state` selector and
		// fan-made games (Betrayed Alliance) leave it unset on the one active
		// edit -- and an SCI0 dialog only ever shows its focused edit control.
		// Drawn as 1 native px scaled to the overlay -- the old 1-dest-px vLine
		// was invisible at hires widths.
		if (tr && finalPx > 0 && e.type == kUiTextEdit && !textRect.isEmpty() && _caretBlinkOn) {
			const int cx = textRect.left + tr->caretAtPx(e.text, e.cursorPos, finalPx);
			const byte *pc = palette ? palette + (e.penColor >= 0 ? e.penColor : 0) * 3 : nullptr;
			const uint32 col = pc ? fmt.ARGBToColor(255, pc[0], pc[1], pc[2])
			                      : fmt.ARGBToColor(255, 255, 255, 255);
			int cw = nativeRowsToOverlay(1, gameRect.height());
			if (cw < 2)
				cw = 2;
			Common::Rect cr(cx, textRect.top + 1, cx + cw, textRect.bottom - 1);
			cr.clip(textRect);
			if (!cr.isEmpty())
				dest.fillRect(cr, col);
		}
	}
}

void RogerCompositor::runTransition(Graphics::ManagedSurface &from, Graphics::ManagedSurface &to,
                                    Graphics::ManagedSurface &scratch, TransitionFamily fam, int durationMs,
                                    int sciTypeHint) {
	fam = effectiveFamily(fam);
	if (fam == kFxNone || durationMs <= 0) {
		_bgRebuilt = true;
		presentToOverlay(to);
		g_system->updateScreen();
		return;
	}
	const int blockPx = blockPxForSciType(sciTypeHint);
	// Wipe direction derived from the raw SCI type so directional transitions are faithful.
	const int wipeDir = wipeDirectionFor(sciTypeHint);
	const uint32 start = g_system->getMillis();
	for (;;) {
		const uint32 now = g_system->getMillis();
		float t = (now - start) / (float)durationMs;
		bool last = false;
		if (t >= 1.0f) {
			t = 1.0f;
			last = true;
		}
		switch (fam) {
		case kFxDissolve: blendDissolve(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t, blockPx); break;
		case kFxWipe:     blendWipe(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t, wipeDir); break;
		case kFxScroll:   blendScroll(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t,
		                              scrollDirectionFor(sciTypeHint)); break;
		case kFxSplitV:   blendSplitVertical(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t,
		                                     splitFromCenter(sciTypeHint)); break;
		case kFxSplitH:   blendSplitHorizontal(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t,
		                                       splitFromCenter(sciTypeHint)); break;
		case kFxDiagonal: blendDiagonal(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t,
		                                splitFromCenter(sciTypeHint)); break;
		case kFxFade:
		default:          blendFadeThroughBlack(*from.surfacePtr(), *to.surfacePtr(), *scratch.surfacePtr(), t); break;
		}
		_bgRebuilt = true;          // force full present for this effect frame
		presentToOverlay(scratch);
		g_system->updateScreen();
		if (last)
			break;
		g_system->delayMillis(2);
	}
}

void RogerCompositor::runShake(Graphics::ManagedSurface &scene, Graphics::ManagedSurface &scratch,
                               int shakeCount, int directions, int magnitudePx) {
	if (shakeCount < 1)
		shakeCount = 1;
	if (magnitudePx < 1)
		magnitudePx = 1;
	const int dx = (directions & 2) ? magnitudePx : 0; // bit1 = horizontal
	const int dy = (directions & 1) ? magnitudePx : 0; // bit0 = vertical (default)
	for (int i = 0; i < shakeCount; i++) {
		// Jolt: draw the scene shifted by (dx,dy), then back to rest. Each half ~20ms.
		for (int phase = 0; phase < 2; phase++) {
			const int ox = phase ? 0 : dx;
			const int oy = phase ? 0 : dy;
			scratch.fillRect(Common::Rect(0, 0, scratch.w, scratch.h),
			                 scratch.surfacePtr()->format.ARGBToColor(255, 0, 0, 0));
			scratch.blitFrom(*scene.surfacePtr(), Common::Rect(0, 0, scene.w, scene.h),
			                 Common::Point((int16)ox, (int16)oy));
			_bgRebuilt = true;
			presentToOverlay(scratch);
			g_system->updateScreen();
			g_system->delayMillis(20);
		}
	}
	// Restore the un-shaken scene.
	_bgRebuilt = true;
	presentToOverlay(scene);
	g_system->updateScreen();
}

void expandRegionsToElements(Common::Array<Common::Rect> &regions,
                             const Common::Array<UiElement> &elems,
                             const Common::Rect &gameRect, const Common::Rect &bounds,
                             Common::Array<uint> &outElemIndices) {
	Common::Array<bool> selected;
	for (uint i = 0; i < elems.size(); i++)
		selected.push_back(false);
	Common::Array<Common::Rect> extents;
	for (uint i = 0; i < elems.size(); i++)
		extents.push_back(uiPaintExtent(elems[i].nativeRect, gameRect));

	bool changed = true;
	while (changed) {
		changed = false;
		for (uint i = 0; i < elems.size(); i++) {
			if (selected[i])
				continue;
			bool hit = false;
			for (uint r = 0; r < regions.size() && !hit; r++)
				if (extents[i].intersects(regions[r]))
					hit = true;
			if (!hit)
				continue;
			// Select the whole token group so the window-border content union is
			// computed from the same set a full redraw would see. Grouping is by raw
			// token equality -- including a default 0 token -- deliberately mirroring
			// renderUiLayer's own unconditional token match; a zero-token guard here
			// would diverge from the border logic and break byte-identity.
			for (uint j = 0; j < elems.size(); j++) {
				if (selected[j] || elems[j].token != elems[i].token)
					continue;
				selected[j] = true;
				regions.push_back(extents[j]);
				changed = true;
			}
		}
		if (changed) {
			Common::Array<Common::Rect> coalesced;
			coalesceDirtyRects(regions, bounds, coalesced);
			regions = coalesced;
		}
	}
	for (uint i = 0; i < elems.size(); i++)
		if (selected[i])
			outElemIndices.push_back(i);
}

void RogerCompositor::patchCompositeRegions(Graphics::ManagedSurface &composite,
                                            Graphics::ManagedSurface &sceneNoUi,
                                            const Common::Array<UiElement> &elems,
                                            const Common::Array<Common::Rect> &regions,
                                            const byte *palette, const Common::Rect &gameRect,
                                            const RogerTextRenderer *text, const RogerTextRenderer *altText) {
	if (regions.empty())
		return;
	const Common::Rect bounds(0, 0, (int16)composite.w, (int16)composite.h);
	Common::Array<Common::Rect> expanded;
	coalesceDirtyRects(regions, bounds, expanded);
	Common::Array<uint> idx;
	expandRegionsToElements(expanded, elems, gameRect, bounds, idx);

	// Seed the expanded regions with the clean (UI-free) scene. Every selected
	// element's full paint extent lies inside `expanded` (that is what the
	// expansion guarantees), so re-rendering them whole below cannot double-blend
	// over a previously rendered copy of themselves.
	for (uint i = 0; i < expanded.size(); i++) {
		Common::Rect r = expanded[i];
		r.clip(bounds);
		if (r.isEmpty())
			continue;
		composite.copyRectToSurface(*sceneNoUi.surfacePtr(), r.left, r.top, r);
	}

	if (idx.empty())
		return;
	Common::Array<UiElement> subset;
	for (uint i = 0; i < idx.size(); i++)
		subset.push_back(elems[idx[i]]);
	// Side-effect: renderUiLayer calls addDirtyRect for each rendered element, so
	// the patched rects also land in _dirtyCur -- callers get them pushed for free.
	renderUiLayer(composite, subset, palette, gameRect, text, altText);
}

} // namespace Roger
} // namespace Sci
