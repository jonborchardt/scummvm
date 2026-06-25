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

#include "sci/roger/roger_compositor.h"
#include "sci/roger/view_cache.h"
#include "sci/roger/roger_coords.h"
#include "sci/roger/roger_text.h"
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
// persistent buffer in a flat loop — no allocation, no per-pixel function calls.
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

void RogerCompositor::setRoom(Graphics::Surface *cleanPlate, ViewCache *views) {
	_plate = cleanPlate;
	_views = views;
	// Invalidate the static-background cache so the new room rebuilds it. (The plate
	// pointer can be freed+reallocated to the same address across rooms, so an identity
	// check alone could go stale — null it here on every room load to be safe.)
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
	const uint32 _perfT0 = g_system ? g_system->getMillis() : 0; // temp perf timing
	const int W = dest.w, H = dest.h;

	// The picture (plate + sprites) is drawn into _pictureDest — the overlay-space
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
	//      into _bgCache and seed each frame with a straight copy — re-scaling the
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
	if (bgValid) {
		// Seed only the game region: the static black letterbox in dest persists from the
		// last _bgRebuilt frame (this scratch surface is reused, and sprites only draw
		// inside picRect ⊂ gameRect), so we skip copying the letterbox AND avoid copyFrom's
		// per-frame free+malloc of the whole overlay. Empty gameRect (tests) -> full copy.
		if (_bgGameRect.isEmpty()) {
			dest.copyFrom(*_bgCache);
		} else {
			Common::Rect gr = _bgGameRect;
			gr.clip(Common::Rect(0, 0, (int16)W, (int16)H));
			dest.surfacePtr()->copyRectToSurface(*_bgCache->surfacePtr(), gr.left, gr.top, gr);
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
			if (gt > 0) dest.fillRect(Common::Rect(0, 0, (int16)W, gt), black);
			if (gb < H) dest.fillRect(Common::Rect(0, gb, (int16)W, (int16)H), black);
			if (gl > 0) dest.fillRect(Common::Rect(0, gt, gl, gb), black);
			if (gr < W) dest.fillRect(Common::Rect(gr, gt, (int16)W, gb), black);
		}
		// Clean plate, scaled into the game rect (aspect preserved).
		if (_plate)
			dest.blitFrom(*_plate, Common::Rect(0, 0, _plate->w, _plate->h), picRect);

		// Snapshot this fully-drawn background into the cache for subsequent frames —
		// but only when a plate was actually drawn, so we never cache an empty picRect.
		if (_plate) {
			if (!_bgCache || _bgCache->w != W || _bgCache->h != H || _bgCache->format != fmt) {
				if (_bgCache) { _bgCache->free(); delete _bgCache; }
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
		// Map the picture-window-local cel rect (PIC_W x PIC_H) into the game rect.
		Common::Rect dst(
			(int16)(picRect.left + (int)s.celRect.left   * GW / PIC_W),
			(int16)(picRect.top  + (int)s.celRect.top    * GH / PIC_H),
			(int16)(picRect.left + (int)s.celRect.right  * GW / PIC_W),
			(int16)(picRect.top  + (int)s.celRect.bottom * GH / PIC_H));
		// Alpha-aware blit: respects each pixel's alpha so transparent non-black
		// pixels (common in exported spritesheets) do not render opaque (halos).
		dest.blendBlitFrom(*cel, Common::Rect(0, 0, cel->w, cel->h), dst,
		                   s.mirror ? Graphics::FLIP_H : Graphics::FLIP_NONE);

		// Per-pixel priority occlusion against the plate.
		if (_priority && _plate) {
			const int x0 = MAX<int>(dst.left, picRect.left);
			const int y0 = MAX<int>(dst.top, picRect.top);
			const int x1 = MIN<int>(dst.right, picRect.right);
			const int y1 = MIN<int>(dst.bottom, picRect.bottom);

			// CRITICAL: sample the plate with the SAME integer scaler ScummVM's
			// blitFrom used to draw the background plate above (see
			// graphics/managed_surface.cpp::blitFromInner: scaleX = 256*srcW/dstW,
			// then srcX = i*scaleX/256). A plain `i*srcW/dstW` resample uses a
			// *different* rounding and drifts from the scaler by up to ~10px across a
			// wide rect, so the splatted foreground pixels would not match the
			// displayed background (the "off by a few pixels" bug). Matching the
			// scaler makes a splatted pixel byte-identical to the background at (ox,oy).
			// With the hires priority map, _priorityW/_priorityH == _plate->w/h, so
			// the overlay->plate->priority mapping below collapses to a 1:1 lookup at
			// the displayed plate pixel — the few-px drift of the old native-res map
			// (320x190 sampled /6) is gone. The math still generalises if they differ.
			const int SCALE = 0x100; // == SCALE_THRESHOLD in managed_surface.cpp
			const int scaleX = SCALE * _plate->w / GW;
			const int scaleY = SCALE * _plate->h / GH;
			const int pw = _plate->w, ph = _plate->h;
			const byte spritePri = s.priority;

			// Hot path (runs for every pixel of every sprite, every frame): when the
			// scene and plate share the exact RGBA32 layout, an occluded pixel is a raw
			// 32-bit word copy via row pointers — no per-pixel virtual getPixel/setPixel
			// (each of those does format dispatch + bounds checks). The plate column is
			// advanced incrementally (accX += scaleX) instead of a multiply+divide per
			// pixel, and picX collapses to plX when the priority map matches the plate
			// resolution (the normal hires case). Falls back to getPixel/setPixel if the
			// formats ever differ. Same priority>sprite rule, identical output.
			const bool fast = (destSurf->format == _plate->format &&
			                   destSurf->format.bytesPerPixel == 4);
			const bool priMatchesPlate = (_priorityW == pw);

			for (int oy = y0; oy < y1; oy++) {
				// Plate row the background scaler drew at this overlay row.
				const int plY = (oy - picRect.top) * scaleY / SCALE;
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

				int accX = (x0 - picRect.left) * scaleX;
				for (int ox = x0; ox < x1; ox++, accX += scaleX) {
					const int plX = accX / SCALE;
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

	if (g_system)
		_renderMs = g_system->getMillis() - _perfT0; // temp perf timing
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
	if (OW <= 0 || OH <= 0)
		return;
	const uint32 _perfT0 = g_system->getMillis(); // temp perf timing

	// Push only the region that can contain dynamic content. Everything that changes
	// frame-to-frame (sprites, dialogs, the composited cursor) is inside gameRect; the
	// letterbox outside it is static black, so it only needs pushing when the background
	// was just (re)built (_bgRebuilt: room/geometry change / first frame). Converting and
	// pushing just gameRect each frame skips the letterbox area on the kAnimate path.
	const Common::Rect fullRect(0, 0, (int16)(s->w < OW ? s->w : OW), (int16)(s->h < OH ? s->h : OH));
	Common::Rect region = fullRect;
	if (!_bgRebuilt && !_bgGameRect.isEmpty()) {
		region = _bgGameRect;
		region.clip(fullRect);
		if (region.isEmpty())
			region = fullRect; // safety: never skip the whole frame
	}
	_bgRebuilt = false;

	const bool fastPath = (s->format == overlayFmt);
	uint32 convMs = 0;
	if (fastPath) {
		g_system->copyRectToOverlay(s->getBasePtr(region.left, region.top), s->pitch,
		                            region.left, region.top, region.width(), region.height());
	} else if (s->format.bytesPerPixel == 4 && overlayFmt.bytesPerPixel == 4) {
		// Fast path: allocation-free 32->32 channel shuffle into a persistent buffer,
		// then push only the region. (convertTo allocated/freed a full-overlay surface
		// every frame — the bulk of present cost at this resolution.)
		const uint32 tc = g_system->getMillis();
		if (!_overlayConv || _overlayConv->w != s->w || _overlayConv->h != s->h ||
		    _overlayConv->format != overlayFmt) {
			if (_overlayConv) { _overlayConv->free(); delete _overlayConv; }
			_overlayConv = new Graphics::Surface();
			_overlayConv->create((uint16)s->w, (uint16)s->h, overlayFmt);
		}
		if (_overlayConv->getPixels()) {
			convert32((byte *)_overlayConv->getBasePtr(region.left, region.top),
			          (const byte *)s->getBasePtr(region.left, region.top),
			          _overlayConv->pitch, s->pitch, region.width(), region.height(),
			          overlayFmt, s->format);
			convMs = g_system->getMillis() - tc;
			g_system->copyRectToOverlay(_overlayConv->getBasePtr(region.left, region.top),
			                            _overlayConv->pitch, region.left, region.top,
			                            region.width(), region.height());
		}
	} else {
		// Fallback for non-32bpp overlays (e.g. RGB565): generic convertTo.
		const uint32 tc = g_system->getMillis();
		Graphics::Surface sub = scene.surfacePtr()->getSubArea(region); // view, no copy
		Graphics::Surface *conv = sub.convertTo(overlayFmt);
		convMs = g_system->getMillis() - tc;
		if (conv) {
			g_system->copyRectToOverlay(conv->getPixels(), conv->pitch,
			                            region.left, region.top, region.width(), region.height());
			conv->free();
			delete conv;
		}
	}
	g_system->showOverlay(false);

	// Temp perf instrumentation: average renderScene vs present, split into convert vs
	// push, plus overlay geometry/format, so we target the real per-frame bottleneck.
	const int kPerfWindow = 240;
	const uint32 presentMs = g_system->getMillis() - _perfT0;
	_accRenderMs += _renderMs;
	_accPresentMs += presentMs;
	_accConvertMs += convMs;
	_accPushMs += (presentMs - convMs); // copyRectToOverlay + showOverlay + setup
	if (++_perfFrames >= kPerfWindow) {
		warning("ROGER perf (%d-frame avg): render %.2f / present %.2f ms [convert %.2f, push %.2f]  overlay %dx%d bpp%d region %dx%d %s",
		        kPerfWindow, (double)_accRenderMs / kPerfWindow, (double)_accPresentMs / kPerfWindow,
		        (double)_accConvertMs / kPerfWindow, (double)_accPushMs / kPerfWindow,
		        OW, OH, overlayFmt.bytesPerPixel, region.width(), region.height(),
		        fastPath ? "fmt-match" : "convert");
		_accRenderMs = _accPresentMs = _accConvertMs = _accPushMs = 0;
		_perfFrames = 0;
	}
}

void RogerCompositor::renderUiLayer(Graphics::ManagedSurface &dest,
                                    const Common::Array<UiElement> &elems,
                                    const byte *palette, const Common::Rect &gameRect,
                                    const RogerTextRenderer *text, const RogerTextRenderer *altText) {
	const Graphics::PixelFormat &fmt = dest.surfacePtr()->format;
	// Role -> target on-screen cell height, in dest pixels. Expressed as a height in
	// the native 320x200 space scaled up by the game-rect mapping, so it is the SAME
	// physical size for every element regardless of its own (tiny, varying) rect, and
	// it tracks the overlay resolution. One body size + one slightly larger heading.
	const int bodyPx    = kRoleBodyNativeH    * gameRect.height() / 200;
	const int headingPx = kRoleHeadingNativeH * gameRect.height() / 200;
	for (uint i = 0; i < elems.size(); i++) {
		const UiElement &e = elems[i];
		Common::Rect nr = e.nativeRect;
		if (e.type == kUiWindow && e.hasFrame) {
			// SCI often positions a dialog's controls (buttons, edit fields, message
			// text) flush with — or a few rows past — the window's own dims rect. Expand
			// the drawn window to the union of every element sharing its token so the box
			// and its bold border actually contain them.
			for (uint j = 0; j < elems.size(); j++) {
				if (j != i && elems[j].token == e.token)
					nr.extend(elems[j].nativeRect);
			}
			nr.grow(2); // a little padding so controls are not flush against the border
		}
		const Common::Rect d = sciRectToDest(nr, gameRect);
		if (d.isEmpty())
			continue;
		// Pick the font renderer for this element (header/menu use the alt font).
		const RogerTextRenderer *tr = (e.useAltFont && altText) ? altText : text;
		const int targetPx = (e.textRole == kRoleHeading) ? headingPx : bodyPx;

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

		// Frame (1px native -> scaled): windows, edit fields, selected text/buttons.
		const bool frame = e.hasFrame || e.type == kUiTextEdit ||
		                   (e.type == kUiText && (e.style & 0x8)) ||
		                   e.type == kUiButton || e.type == kUiWindow;
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
			int thick = isWindow ? (gameRect.height() / 200) : 1;
			if (thick < 1) thick = 1;
			for (int t = 0; t < thick; t++) {
				Common::Rect fr = d; fr.grow(-t);
				if (fr.isEmpty()) break;
				dest.frameRect(fr, col);
			}
		}

		// Text + caret.
		if (tr && (e.type == kUiText || e.type == kUiButton || e.type == kUiTextEdit)
		    && !e.text.empty()) {
			const byte *pc = palette ? palette + (e.penColor >= 0 ? e.penColor : 0) * 3 : nullptr;
			const uint32 col = pc ? fmt.ARGBToColor(255, pc[0], pc[1], pc[2])
			                      : fmt.ARGBToColor(255, 255, 255, 255);
			tr->drawPx(dest, e.text, d, col, e.align, targetPx, e.vAlignTop, &e.glyphs);
		}
		if (tr && e.type == kUiTextEdit && (e.style & 0x8)) { // SELECTED -> caret
			const int cx = d.left + tr->caretPx(e.text, e.cursorPos, d, targetPx);
			const byte *pc = palette ? palette + (e.penColor >= 0 ? e.penColor : 0) * 3 : nullptr;
			const uint32 col = pc ? fmt.ARGBToColor(255, pc[0], pc[1], pc[2])
			                      : fmt.ARGBToColor(255, 255, 255, 255);
			dest.vLine(cx, d.top + 1, d.bottom - 2, col);
		}
	}
}

} // namespace Roger
} // namespace Sci
