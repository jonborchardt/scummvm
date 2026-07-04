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

#include "common/rect.h"

namespace Sci {
namespace Roger {

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
 * displayed: a centered, aspect-preserving box. This MUST match the backend's own
 * game placement so the overlay (drawn full-window, alpha-blended over the game)
 * lines up 1:1 — toggling the overlay then produces no positional shift.
 *
 * With aspect-ratio correction the 200 game lines are shown at 4:3 (as if
 * 320x240); otherwise at the native 320x200. Assumes the backend's default
 * centered/fit stretch mode (not integer-scaling).
 */
inline Common::Rect computeGameRect(int overlayW, int overlayH, bool aspectCorrected) {
	return fitCentered(320, aspectCorrected ? 240 : 200, overlayW, overlayH);
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
