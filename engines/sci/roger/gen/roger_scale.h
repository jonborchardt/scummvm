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

#ifndef SCI_ROGER_ROGER_SCALE_H
#define SCI_ROGER_ROGER_SCALE_H

#include "common/array.h"
#include "common/types.h"

namespace Sci {
namespace Roger {

/**
 * An 8-bit indexed-colour image (palette indices, not RGBA).
 * Downstream tasks map through the live SCI palette to produce RGBA.
 */
struct IndexImage {
	Common::Array<byte> pixels;
	int w;
	int h;
};

/**
 * EPX/Scale2x -- doubles each dimension.
 * Ported verbatim from scale2x.ts (lines 18-43) in sci.js.
 */
IndexImage scale2x(const IndexImage &in);

/**
 * Scale3x -- triples each dimension using the EPX-9 rule.
 * Ported verbatim from scale3x.ts + epx.ts (epx9) + s9.ts (s9).
 */
IndexImage scale3x(const IndexImage &in);

/**
 * Scale6x -- scale3x(scale2x(in)).
 * Matches create-pic-pipeline.ts lines 60-63.
 */
IndexImage scale6x(const IndexImage &in);

/**
 * Integer nearest-neighbour upscale (pixel replication) by `factor` in each axis.
 * Crisp/blocky -- preserves the source pixels exactly (no edge smoothing). For text
 * this avoids the EPX corner-rounding that makes glyphs look "bubbly".
 */
IndexImage scaleNearest(const IndexImage &in, int factor);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_SCALE_H
