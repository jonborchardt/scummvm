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

#ifndef SCI_ROGER_UTILS_STUDIO_ROGER_SWEEP_SVG_H
#define SCI_ROGER_UTILS_STUDIO_ROGER_SWEEP_SVG_H

// ROGER STUDIO (kept dev utility, quarantined): self-contained sweep-SVG
// builder. Produces a plain-ASCII SVG document comparing two images with a
// draggable divider (and optionally an SMIL auto-sweep), a direct port of the
// pic-dev PicSweep.tsx download output. See
// docs/superpowers/specs/2026-07-11-roger-studio-sweep-svg-design.md and the
// -reference.md extract next to it (verbatim script + geometry constants).
// Quarantine contract: see roger_studio.h.

#include "common/str.h"
#include "graphics/surface.h"

namespace Sci {
namespace Roger {

// Layer 1: assemble the SVG document from already-PNG-encoded image bytes.
// left = slot A (revealed by the clip rect), right = slot B (full background).
// width/height set the viewBox and all proportional geometry (reference
// authored at 1920 wide). pixelatedLeft/Right choose each embedded image's
// image-rendering style: pixelated (crisp pixel art) or auto (smooth).
// Returns the empty string on invalid input. Output is pure ASCII; the
// embedded interaction script contains no XML significant characters
// (ported verbatim -- do not restyle it).
Common::String buildSweepSvgFromPngData(const byte *pngLeft, uint32 lenLeft,
                                        const byte *pngRight, uint32 lenRight,
                                        int width, int height, bool animated,
                                        bool pixelatedLeft, bool pixelatedRight);

// Layer 2: scale + encode surfaces, then delegate to layer 1. left/right must
// be the same size. targetWidth == left.w embeds the surfaces as-is; smaller
// widths area-resample both (height follows the aspect ratio). The Studio
// uses 1920 (as-is) and 640 (/3).
Common::String buildSweepSvg(const Graphics::Surface &left,
                             const Graphics::Surface &right,
                             int targetWidth, bool pixelatedLeft,
                             bool pixelatedRight, bool animated);

// General area resampler: each destination pixel is the overlap-area-weighted
// average of the source pixels it covers, computed in linear light with
// premultiplied alpha (sRGB -> linear -> premultiply -> average -> divide by
// alpha -> back to sRGB). Exact-replication inputs (pixel art upscales) come
// out as exact replications when the ratio divides the replication factor.
// Caller owns the result (free() then delete). Returns nullptr on invalid
// sizes (dstW/dstH < 1 or an upscale request) or a non-32bpp source.
Graphics::Surface *areaResample(const Graphics::Surface &src, int dstW, int dstH);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_UTILS_STUDIO_ROGER_SWEEP_SVG_H
