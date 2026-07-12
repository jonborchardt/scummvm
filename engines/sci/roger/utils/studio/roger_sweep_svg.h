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
// authored at 1920 wide). Returns the empty string on invalid input.
// Output is pure ASCII; the embedded interaction script contains no XML
// significant characters (ported verbatim -- do not restyle it).
Common::String buildSweepSvgFromPngData(const byte *pngLeft, uint32 lenLeft,
                                        const byte *pngRight, uint32 lenRight,
                                        int width, int height, bool animated);

// Layer 2 (Task 3): downscale + encode surfaces, then delegate to layer 1.
// divisor is an integer divisor of the render (1|2|3|6 in the Studio UI).
Common::String buildSweepSvg(const Graphics::Surface &left,
                             const Graphics::Surface &right,
                             int divisor, bool animated);

// Nearest-neighbour integer downscale (top-left sample of each block).
// Caller owns the result (free() then delete). divisor 1 returns a copy.
// Exposed for unit tests. Returns nullptr on invalid divisor/size.
Graphics::Surface *downscaleNearest(const Graphics::Surface &src, int divisor);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_UTILS_STUDIO_ROGER_SWEEP_SVG_H
