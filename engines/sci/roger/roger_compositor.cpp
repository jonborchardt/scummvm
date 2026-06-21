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
#include "sci/roger/slice_set.h"
#include "sci/roger/roger_coords.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "common/system.h"
#include "common/textconsole.h"

namespace Sci {
namespace Roger {

void RogerCompositor::setRoom(Graphics::Surface *cleanPlate, SliceSet *slices, ViewCache *views) {
	_plate = cleanPlate;
	_slices = slices;
	_views = views;
}

// Compute the centered, aspect-preserving rectangle for a srcW x srcH image
// fitted inside a W x H destination (letterbox/pillarbox). Keeps the hires art
// from stretching when the overlay/window aspect differs from the art's.
static Common::Rect aspectFitRect(int srcW, int srcH, int W, int H) {
	if (srcW <= 0 || srcH <= 0)
		return Common::Rect(0, 0, (int16)W, (int16)H);
	// Scale to fit (min of the two ratios), preserving aspect.
	const float scale = MIN((float)W / srcW, (float)H / srcH);
	const int fitW = (int)(srcW * scale);
	const int fitH = (int)(srcH * scale);
	const int x = (W - fitW) / 2;
	const int y = (H - fitH) / 2;
	return Common::Rect((int16)x, (int16)y, (int16)(x + fitW), (int16)(y + fitH));
}

void RogerCompositor::renderScene(Graphics::ManagedSurface &dest, const Common::Array<Sprite> &sprites) {
	const int W = dest.w, H = dest.h;

	// The hires content occupies an aspect-preserving rect centered in the overlay
	// (the overlay/window may be a different shape than the 320x200 game art). All
	// of plate, sprites and slices map into this same "game rect" so they stay
	// aligned; the surrounding letterbox stays as cleared background.
	const Common::Rect gameRect = _plate
		? aspectFitRect(_plate->w, _plate->h, W, H)
		: Common::Rect(0, 0, (int16)W, (int16)H);
	const int GW = gameRect.width(), GH = gameRect.height();

	// SCI cel rects are in picture-window-local coordinates. SCI's script width is
	// always 320; the picture HEIGHT is the screen height minus the menu bar (190
	// for SCI0 SQ3/QFG1), which is exactly what the plate encodes. Derive it from
	// the plate's aspect (plate->h * 320 / plate->w) rather than assuming 200, or
	// sprites drift vertically against the background.
	const int PIC_W = 320;
	const int PIC_H = _plate ? (_plate->h * PIC_W / _plate->w) : 200;

	// 0) Clear so letterbox borders are clean (transparent in an alpha overlay).
	dest.clear(0);

	// 1) Clean plate, scaled into the game rect (aspect preserved).
	if (_plate)
		dest.blitFrom(*_plate, Common::Rect(0, 0, _plate->w, _plate->h), gameRect);

	// Manifest piece space (authored at plate/320x200 res) -> game-rect scale.
	const float msx = _plate ? (float)GW / _plate->w : 1.0f;
	const float msy = _plate ? (float)GH / _plate->h : 1.0f;

	// 2) Sprites back-to-front, with slices above each sprite re-drawn for occlusion.
	for (uint i = 0; i < sprites.size(); i++) {
		const Sprite &s = sprites[i];
		const Graphics::Surface *cel = _views ? _views->getCel(s.viewId, s.loopNo, s.celNo) : nullptr;
		if (!cel)
			cel = s.celOverride;
		if (!cel) {
			warning("ROGER: missing hires cel view=%d loop=%d cel=%d (skipped)", s.viewId, s.loopNo, s.celNo);
			continue;
		}
		// Map the picture-window-local cel rect (320 x PIC_H) into the game rect.
		Common::Rect dst(
			(int16)(gameRect.left + (int)s.celRect.left   * GW / PIC_W),
			(int16)(gameRect.top  + (int)s.celRect.top    * GH / PIC_H),
			(int16)(gameRect.left + (int)s.celRect.right  * GW / PIC_W),
			(int16)(gameRect.top  + (int)s.celRect.bottom * GH / PIC_H));
		// Alpha-aware blit: respects each pixel's alpha so transparent non-black
		// pixels (common in exported spritesheets) do not render opaque (halos).
		dest.blendBlitFrom(*cel, Common::Rect(0, 0, cel->w, cel->h), dst,
		                   s.mirror ? Graphics::FLIP_H : Graphics::FLIP_NONE);

		// Foreground occlusion: any slice whose band is strictly above this
		// sprite's band, overlapping the sprite's dest rect, is re-drawn on top.
		if (_slices) {
			const Common::Array<SlicePiece> &ps = _slices->pieces();
			for (uint j = 0; j < ps.size(); j++) {
				if (ps[j].band <= s.priority || !ps[j].surface)
					continue;
				Common::Rect sdst((int16)(gameRect.left + ps[j].x * msx),
				                  (int16)(gameRect.top + ps[j].y * msy),
				                  (int16)(gameRect.left + (ps[j].x + ps[j].surface->w) * msx),
				                  (int16)(gameRect.top + (ps[j].y + ps[j].surface->h) * msy));
				if (!sdst.intersects(dst))
					continue;
				dest.blendBlitFrom(*ps[j].surface,
				                   Common::Rect(0, 0, ps[j].surface->w, ps[j].surface->h), sdst);
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

} // namespace Roger
} // namespace Sci
