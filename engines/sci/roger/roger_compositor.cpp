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

void RogerCompositor::setRoom(Graphics::Surface *cleanPlate, ViewCache *views) {
	_plate = cleanPlate;
	_views = views;
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

void RogerCompositor::renderScene(Graphics::ManagedSurface &dest, const Common::Array<Sprite> &sprites) {
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

	// 0) Clear so letterbox borders are clean (transparent in an alpha overlay).
	dest.clear(0);

	// 1) Clean plate, scaled into the game rect (aspect preserved).
	if (_plate)
		dest.blitFrom(*_plate, Common::Rect(0, 0, _plate->w, _plate->h), picRect);

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
			const int SCALE = 0x100; // == SCALE_THRESHOLD in managed_surface.cpp
			const int scaleX = SCALE * _plate->w / GW;
			const int scaleY = SCALE * _plate->h / GH;

			for (int oy = y0; oy < y1; oy++) {
				// Plate row the background scaler drew at this overlay row.
				const int plY = (oy - picRect.top) * scaleY / SCALE;
				if (plY < 0 || plY >= _plate->h)
					continue;
				// Sample the priority at the SAME scene location the displayed plate
				// occupies — map overlay -> plate (scaler) -> priority — so the occlusion
				// boundary (which points get splatted) tracks the displayed plate instead
				// of a separately, exactly-scaled grid that drifts from it (that drift was
				// eating the sprite's edge). The priority>sprite rule is unchanged.
				const int prY = plY * _priorityH / _plate->h + _picScreenTop;
				if (prY < 0 || prY >= _priorityH)
					continue;
				for (int ox = x0; ox < x1; ox++) {
					const int plX = (ox - picRect.left) * scaleX / SCALE;
					if (plX < 0 || plX >= _plate->w)
						continue;
					const int picX = plX * _priorityW / _plate->w;
					if (picX < 0 || picX >= _priorityW)
						continue;
					if (_priority[prY * _priorityW + picX] > s.priority)
						destSurf->setPixel(ox, oy, _plate->getPixel(plX, plY));
				}
			}
		}
	}
}

void RogerCompositor::presentToOverlay(Graphics::ManagedSurface &scene) {
	// The scene is composited in RGBA32 (so the alpha-aware blendBlitFrom works -
	// it only accepts an RGBA32 destination). The OSystem overlay, however, uses
	// g_system->getOverlayFormat(), which is often NOT RGBA32 (e.g. RGB565), so
	// convert before handing the pixels to copyRectToOverlay.
	const Graphics::Surface *s = scene.surfacePtr();
	const Graphics::PixelFormat overlayFmt = g_system->getOverlayFormat();
	if (s->format == overlayFmt) {
		g_system->copyRectToOverlay(s->getPixels(), s->pitch, 0, 0, s->w, s->h);
	} else {
		Graphics::Surface *conv = s->convertTo(overlayFmt);
		if (conv) {
			g_system->copyRectToOverlay(conv->getPixels(), conv->pitch, 0, 0, conv->w, conv->h);
			conv->free();
			delete conv;
		}
	}
	g_system->showOverlay(false);
}

void RogerCompositor::renderUiLayer(Graphics::ManagedSurface &dest,
                                    const Common::Array<UiElement> &elems,
                                    const byte *palette, const Common::Rect &gameRect,
                                    const RogerTextRenderer *text) {
	const Graphics::PixelFormat &fmt = dest.surfacePtr()->format;
	for (uint i = 0; i < elems.size(); i++) {
		const UiElement &e = elems[i];
		const Common::Rect d = sciRectToDest(e.nativeRect, gameRect);
		if (d.isEmpty())
			continue;

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
			const byte *pc = palette + (e.penColor >= 0 ? e.penColor : 0) * 3;
			const uint32 col = fmt.ARGBToColor(255, pc[0], pc[1], pc[2]);
			dest.frameRect(d, col);
		}

		// Text + caret.
		if (text && (e.type == kUiText || e.type == kUiButton || e.type == kUiTextEdit)
		    && !e.text.empty()) {
			const byte *pc = palette ? palette + (e.penColor >= 0 ? e.penColor : 0) * 3 : nullptr;
			const uint32 col = pc ? fmt.ARGBToColor(255, pc[0], pc[1], pc[2])
			                      : fmt.ARGBToColor(255, 255, 255, 255);
			text->draw(dest, e.text, d, col, e.align);
		}
		if (text && e.type == kUiTextEdit && (e.style & 0x8)) { // SELECTED -> caret
			const int cx = d.left + text->caretX(e.text, e.cursorPos, d.width(), d.height());
			const byte *pc = palette ? palette + (e.penColor >= 0 ? e.penColor : 0) * 3 : nullptr;
			const uint32 col = pc ? fmt.ARGBToColor(255, pc[0], pc[1], pc[2])
			                      : fmt.ARGBToColor(255, 255, 255, 255);
			dest.vLine(cx, d.top + 1, d.bottom - 2, col);
		}
	}
}

} // namespace Roger
} // namespace Sci
