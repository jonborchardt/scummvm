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

#ifndef SCI_ROGER_ROGER_MMPX_H
#define SCI_ROGER_ROGER_MMPX_H

#include "sci/roger/roger_scale.h"

namespace Sci {
namespace Roger {

/**
 * MMPX Style-Preserving Pixel Art Magnification (McGuire & Gagiu, JCGT 2021),
 * ported to Roger's 8-bit EGA index domain. Doubles each dimension.
 *
 * MMPX only copies existing pixels (never blends), so it is index-domain
 * safe. The reference's ARGB luma comparisons are answered from a fixed
 * EGA-palette luma LUT; `clearKey` (the transparent index) takes the
 * reference's alpha-0 luma (sorts above every opaque colour). Pass a byte
 * value not present in the image to mean "no transparency".
 *
 * Reference implementation (MIT license, GPL-compatible):
 * Copyright 2020 Morgan McGuire & Mara Gagiu.
 * https://casual-effects.com/research/McGuire2021PixelArt/index.html
 */
IndexImage mmpx2x(const IndexImage &in, byte clearKey);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_MMPX_H
