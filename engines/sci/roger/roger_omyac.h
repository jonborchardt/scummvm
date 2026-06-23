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

#ifndef SCI_ROGER_ROGER_OMYAC_H
#define SCI_ROGER_ROGER_OMYAC_H

#include "common/array.h"
#include "common/scummsys.h"
#include "sci/roger/roger_pic_native.h"

namespace Sci {
namespace Roger {

// Output of the omyac upscaler: a 1920x1140 (OMYAC_HYBRID_W * OMYAC_HYBRID_H)
// doubled-nibble pixel buffer plus a parallel cmdType buffer
// (CMD_NONE/CMD_LINE/CMD_FILL). Port of render-omyac-upscaler.ts
// OmyacUpscalerResult (without the optional wireframe capture).
struct OmyacResult {
	Common::Array<byte> pixels;        // OMYAC_HYBRID_W * OMYAC_HYBRID_H
	Common::Array<byte> cmdType;       // OMYAC_HYBRID_W * OMYAC_HYBRID_H
	// For each hires pixel, the native cell (y*OMYAC_NATIVE_W + x) whose color it
	// shows, or -1 if untouched. Rides the SAME anchors/lines/enhance geometry as
	// `pixels`, so a parallel channel (e.g. priority bands) can be upscaled with
	// edges identical to the visual plate. Additive: does not affect color/type.
	Common::Array<int32> srcNativeIdx; // OMYAC_HYBRID_W * OMYAC_HYBRID_H
};

// Default enhance pass sequence: 3x fill, 1x line, 2x fill, 4x all.
// MODE_BY_NAME: fill=2, line=1, all=0. (Task 8 orchestrator decides whether to
// use this or a custom list; renderOmyac itself runs exactly the passes given.)
Common::Array<int> defaultPasses();

// Run the omyac upscaler pipeline on a Task-4 native pre-render:
//   build anchors -> detect line endings -> connect line/fill anchors ->
//   hybrid 6x Bresenham -> enhance passes -> null-fill.
// Each entry in `passes` is a MODE_BY_NAME value (0=all, 1=line, 2=fill). An
// EMPTY array runs zero enhance passes (raw wireframe), distinct from "use
// default" (the caller supplies defaultPasses() for the default behavior).
// Port of render-omyac-upscaler.ts renderOmyacUpscaler (lines 886-910).
OmyacResult renderOmyac(const NativeRef &ref, const Common::Array<int> &passes);

} // namespace Roger
} // namespace Sci
#endif
