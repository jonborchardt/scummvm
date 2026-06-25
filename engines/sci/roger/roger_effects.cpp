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

#include "sci/roger/roger_effects.h"
#include "graphics/surface.h"
#include "common/util.h"

namespace Sci {
namespace Roger {

TransitionFamily transitionFamilyFor(int sciType) {
	switch (sciType) {
	case 10:                      return kFxFade;     // FADEPALETTE
	case 8: case 9:               return kFxDissolve; // BLOCKS, PIXELATION
	case 2: case 3: case 4: case 5:                   // STRAIGHT_FROM_*
	case 0: case 1: case 6: case 7:                   // ROLL_*_FROMCENTER / DIAGONAL
	case 300: case 301:           return kFxWipe;     // ROLL_*_TOCENTER
	case 11: case 12: case 13: case 14: return kFxScroll; // SCROLL_*
	case 15: case 100:            return kFxNone;     // NONE_LONGBOW, NONE
	default:                      return kFxFade;     // unknown -> safe default
	}
}

TransitionFamily effectiveFamily(TransitionFamily f) {
	return f; // no collapse; each family renders faithfully
}

int wipeDirectionFor(int sciType) {
	switch (sciType) {
	// Reveal from right: wipe moves left exposing new content on the right side
	case 2:   // STRAIGHT_FROM_RIGHT
	case 0:   // VERTICALROLL_FROMCENTER
	case 300: // VERTICALROLL_TOCENTER
	case 6:   // DIAGONALROLL_TOCENTER
	case 7:   // DIAGONALROLL_FROMCENTER
		return 0;
	// Reveal from left
	case 3:   // STRAIGHT_FROM_LEFT
	case 1:   // HORIZONTALROLL_FROMCENTER
	case 301: // HORIZONTALROLL_TOCENTER
		return 1;
	// Reveal from bottom
	case 5:   // STRAIGHT_FROM_BOTTOM
		return 2;
	// Reveal from top
	case 4:   // STRAIGHT_FROM_TOP
		return 3;
	default:
		return 0; // safe default: right
	}
}

int defaultDurationMs(TransitionFamily f) {
	switch (f) {
	case kFxFade:     return 250;
	case kFxDissolve: return 350;
	case kFxWipe:     return 300;
	case kFxScroll:   return 300;
	default:          return 0;
	}
}

static bool sameRGBA(const Graphics::Surface &a, const Graphics::Surface &b) {
	return a.w == b.w && a.h == b.h && a.format.bytesPerPixel == 4 &&
	       b.format.bytesPerPixel == 4;
}

// Scale an RGBA32 surface's RGB by factor k (0..1) into out (same dims/format).
static void scaleRGB(const Graphics::Surface &src, Graphics::Surface &out, float k) {
	for (int y = 0; y < src.h; y++) {
		for (int x = 0; x < src.w; x++) {
			uint8 a, r, g, b;
			src.format.colorToARGB(src.getPixel(x, y), a, r, g, b);
			r = (uint8)CLIP((int)(r * k + 0.5f), 0, 255);
			g = (uint8)CLIP((int)(g * k + 0.5f), 0, 255);
			b = (uint8)CLIP((int)(b * k + 0.5f), 0, 255);
			out.setPixel(x, y, out.format.ARGBToColor(255, r, g, b));
		}
	}
}

void blendFadeThroughBlack(const Graphics::Surface &from, const Graphics::Surface &to,
                           Graphics::Surface &out, float t) {
	if (!sameRGBA(from, to) || !sameRGBA(from, out))
		return;
	t = CLIP(t, 0.0f, 1.0f);
	if (t < 0.5f)
		scaleRGB(from, out, 1.0f - t * 2.0f);   // from -> black
	else
		scaleRGB(to, out, (t - 0.5f) * 2.0f);    // black -> to
}

// 4x4 ordered (Bayer) matrix, values 0..15 -> thresholds 0..1.
static const int kBayer4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

void blendDissolve(const Graphics::Surface &from, const Graphics::Surface &to,
                   Graphics::Surface &out, float t, int blockPx) {
	if (!sameRGBA(from, to) || !sameRGBA(from, out))
		return;
	if (blockPx < 1) blockPx = 1;
	t = CLIP(t, 0.0f, 1.0f);
	for (int y = 0; y < from.h; y++) {
		for (int x = 0; x < from.w; x++) {
			const int bx = (x / blockPx) & 3;
			const int by = (y / blockPx) & 3;
			const float threshold = (kBayer4[by * 4 + bx] + 0.5f) / 16.0f;
			const Graphics::Surface &src = (t >= threshold) ? to : from;
			out.setPixel(x, y, src.getPixel(x, y));
		}
	}
}

void blendWipe(const Graphics::Surface &from, const Graphics::Surface &to,
               Graphics::Surface &out, float t, int direction) {
	if (!sameRGBA(from, to) || !sameRGBA(from, out))
		return;
	t = CLIP(t, 0.0f, 1.0f);
	const int W = out.w, H = out.h;
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			float threshold;
			switch (direction) {
			case 1:  // reveal from left: threshold increases left->right
				threshold = (float)x / W;
				break;
			case 2:  // reveal from bottom: threshold increases bottom->top
				threshold = 1.0f - (float)y / H;
				break;
			case 3:  // reveal from top: threshold increases top->bottom
				threshold = (float)y / H;
				break;
			case 0:  // reveal from right: threshold increases right->left
			default:
				threshold = 1.0f - (float)x / W;
				break;
			}
			out.setPixel(x, y, (t >= threshold) ? to.getPixel(x, y) : from.getPixel(x, y));
		}
	}
}

} // namespace Roger
} // namespace Sci
