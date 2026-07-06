/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCI_ROGER_ROGER_COORDS_H
#define SCI_ROGER_ROGER_COORDS_H

#include "common/frac.h"
#include "common/rect.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

/**
 * Stretch-mode ids, mirroring the STRETCH_* enum in backends/graphics/windowed.h.
 * The values travel through OSystem::getStretchMode() as plain ints; the enum
 * itself lives in a backend header engine code must not include. Every desktop
 * SDL backend (surfacesdl, openglsdl) shares these ids.
 */
enum {
	kStretchCenter = 0,
	kStretchIntegral = 1,
	kStretchIntegralAR = 2,
	kStretchFit = 3,
	kStretchStretch = 4,
	kStretchFitForceAspect = 5
};

/**
 * Fit a srcW x srcH box into a W x H area, preserving aspect, centered
 * (letterbox / pillarbox). Returns the centered destination rect in W x H space.
 */
inline Common::Rect fitCentered(int srcW, int srcH, int W, int H) {
	if (srcW <= 0 || srcH <= 0)
		return Common::Rect(0, 0, (int16)W, (int16)H);
	// scale = min(W/srcW, H/srcH), computed without floats crossing the branch.
	const float scaleW = (float)W / srcW;
	const float scaleH = (float)H / srcH;
	const float scale = (scaleW < scaleH) ? scaleW : scaleH;
	const int fitW = (int)(srcW * scale);
	const int fitH = (int)(srcH * scale);
	const int x = (W - fitW) / 2;
	const int y = (H - fitH) / 2;
	return Common::Rect((int16)x, (int16)y, (int16)(x + fitW), (int16)(y + fitH));
}

/**
 * The on-screen rect (in overlay pixels) where the native 320x200 SCI game is
 * displayed. This MUST match the backend's own game placement so the overlay
 * (drawn full-window, alpha-blended over the game) lines up 1:1 — toggling the
 * overlay then produces no positional shift, and the backend's game-space mouse
 * mapping (which follows its own draw rect) agrees with what Roger paints.
 *
 * A faithful port of WindowedGraphicsManager::populateDisplayAreaDrawRect for
 * the desktop case (no rotation, no insets, center alignment, safe area = whole
 * window), including the frac_t rounding, so results are pixel-identical:
 *  - stretchMode selects Center / Pixel-perfect / Even-pixels / Fit / Stretch /
 *    Fit-4:3 semantics (kStretch* above, from g_system->getStretchMode());
 *  - renderScale is the software-scaler factor (g_system->getScaleFactor(),
 *    == the backend's getGameRenderScale() on both SDL backends), which sizes
 *    the Center/Integral modes;
 *  - with aspect-ratio correction the 200 game lines are shown at 4:3 (as if
 *    320x240); otherwise at the native 320x200 (16:10).
 */
inline Common::Rect computeGameRect(int overlayW, int overlayH, bool aspectCorrected,
                                    int stretchMode = kStretchFit, int renderScale = 1) {
	if (overlayW <= 0 || overlayH <= 0)
		return Common::Rect(0, 0, (int16)MAX(overlayW, 0), (int16)MAX(overlayH, 0));
	if (renderScale < 1)
		renderScale = 1;
	const frac_t displayAspect = aspectCorrected ? (intToFrac(4) / 3)
	                                             : (intToFrac(320) / 200);
	const int originalWidth = 320 * renderScale;
	const int originalHeight = 200 * renderScale;

	int width = 0, height = 0;
	if (stretchMode == kStretchCenter || stretchMode == kStretchIntegral || stretchMode == kStretchIntegralAR) {
		width = originalWidth;
		height = intToFrac(width) / displayAspect;
		if (width > overlayW || height > overlayH) {
			int fac = 1 + MAX((width - 1) / overlayW, (height - 1) / overlayH);
			width /= fac;
			height /= fac;
		} else if (stretchMode == kStretchIntegral) {
			int fac = MIN(overlayW / width, overlayH / height);
			width *= fac;
			height *= fac;
		} else if (stretchMode == kStretchIntegralAR) {
			int targetHeight = height;
			int horizontalFac = overlayW / width;
			do {
				width = originalWidth * horizontalFac;
				int verticalFac = (targetHeight * horizontalFac + originalHeight / 2) / originalHeight;
				height = originalHeight * verticalFac;
				--horizontalFac;
			} while (horizontalFac > 0 && height > overlayH);
			if (height > overlayH)
				height = targetHeight;
		}
	} else {
		const frac_t windowAspect = intToFrac(overlayW) / overlayH;
		width = overlayW;
		height = overlayH;
		if (stretchMode == kStretchFitForceAspect) {
			const frac_t ratio = intToFrac(4) / 3;
			if (windowAspect < ratio)
				height = intToFrac(width) / ratio;
			else if (windowAspect > ratio)
				width = fracToInt(height * ratio);
		} else if (stretchMode != kStretchStretch) {
			// kStretchFit (and any unknown future id, matching the backend's else-chain)
			if (windowAspect < displayAspect)
				height = intToFrac(width) / displayAspect;
			else if (windowAspect > displayAspect)
				width = fracToInt(height * displayAspect);
		}
	}

	const int x = (overlayW - width) / 2;
	const int y = (overlayH - height) / 2;
	return Common::Rect((int16)x, (int16)y, (int16)(x + width), (int16)(y + height));
}

/**
 * The picture sub-rect within the game rect. SCI0 reserves the top `statusBarH`
 * screen rows (of `screenLines`, normally 200) for the status/menu bar; the
 * upscaled picture plate occupies the rest. Leaving the reserved strip
 * untouched (transparent) lets the native status bar show through the overlay.
 */
inline Common::Rect computePictureRect(const Common::Rect &gameRect, int statusBarH, int screenLines = 200) {
	if (screenLines <= 0 || statusBarH <= 0)
		return gameRect;
	const int stripPx = gameRect.height() * statusBarH / screenLines;
	return Common::Rect(gameRect.left, (int16)(gameRect.top + stripPx),
	                    gameRect.right, gameRect.bottom);
}

/**
 * Scale a 320x200 SCI cel rect into hires overlay pixel coordinates.
 *
 * scaleX = overlayW / 320.0, scaleY = overlayH / 200.0
 */
inline Common::Rect sciCelRectToOverlay(const Common::Rect &r, int overlayW, int overlayH) {
	const float sx = (float)overlayW / 320.0f;
	const float sy = (float)overlayH / 200.0f;
	return Common::Rect((int16)(r.left * sx), (int16)(r.top * sy),
	                    (int16)(r.right * sx), (int16)(r.bottom * sy));
}

/**
 * Map a 320x200 SCI screen-space rect into a destination game rect (the
 * on-screen placement of the native game, from computeGameRect). Used for UI
 * elements, which live in the full 320x200 space (status bar included).
 */
inline Common::Rect sciRectToDest(const Common::Rect &nr, const Common::Rect &gameRect) {
	const int gw = gameRect.width(), gh = gameRect.height();
	return Common::Rect(
		(int16)(gameRect.left + nr.left   * gw / 320),
		(int16)(gameRect.top  + nr.top    * gh / 200),
		(int16)(gameRect.left + nr.right  * gw / 320),
		(int16)(gameRect.top  + nr.bottom * gh / 200));
}

/**
 * Full overlay paint extent of a UI element pushed at native rect `nr`: the
 * compositor paints kUiWindow fills at nr.grow(2) native px, and TTF glyphs can
 * overshoot the native box by ~2 overlay px. Marks made with this cover every
 * pixel the element's draw can touch (the 2026-07-02 shipped overdraw math).
 */
inline Common::Rect uiPaintExtent(const Common::Rect &nr, const Common::Rect &gameRect) {
	Common::Rect n = nr;
	n.grow(2);
	Common::Rect d = sciRectToDest(n, gameRect);
	d.grow(2);
	return d;
}

/**
 * Overlay extent a REMOVED element must invalidate: exact native rect + the TTF
 * overshoot pad only. The compositor-overdraw ring beyond it is covered by
 * bitsRestore's exact erase rect (§3.1). Live callers are the two documented
 * duty-3 exceptions (markVacatedDirty from uiClearToken / uiPushFrameBox), where
 * no bitsRestore rect ever fires. Gate greens cannot verify this either way
 * (Phase 2: layered redundancy) — coverage is soak-verified.
 */
inline Common::Rect uiVacatedExtent(const Common::Rect &nr, const Common::Rect &gameRect) {
	Common::Rect d = sciRectToDest(nr, gameRect);
	d.grow(2);
	return d;
}

/**
 * Return the SCI0 priority band (0..14) for a given y coordinate.
 *
 * Band 0 for y < 42 (above the horizon); bands 1..14 are distributed
 * evenly across the range [42, gameHeight-1].
 */
inline int sciPriorityBand(int y, int gameHeight = 190) {
	const int topBand = 42;
	if (y < topBand)
		return 0;
	const int range = gameHeight - topBand;
	if (range <= 0)
		return 14;
	int band = ((y - topBand) * 14) / range + 1;
	if (band > 14) band = 14;
	return band;
}

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_COORDS_H
