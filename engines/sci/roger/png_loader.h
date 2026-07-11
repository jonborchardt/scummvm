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

#ifndef SCI_ROGER_PNG_LOADER_H
#define SCI_ROGER_PNG_LOADER_H

#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"
#include "graphics/surface.h"

namespace Sci {
namespace Roger {

// Loads a PNG file and returns raw 8-bit pixel data (one byte per pixel).
// For grayscale PNGs: returns pixel values directly.
// For RGB/RGBA PNGs: returns the red channel (grayscale-equivalent).
// Returns an empty array on any failure (file not found, decode error, etc.).
// Width x height pixels returned in row-major order (left-to-right, top-to-bottom).
Common::Array<byte> loadGrayscale8(const Common::String &path);

// Decode a PNG file at `path` to a newly-allocated 32-bit RGBA
// Graphics::Surface. Caller owns the result: call ->free() then delete.
// Returns nullptr on failure.
Graphics::Surface *loadSurfaceRGBA(const Common::String &path);

// Write a Graphics::Surface to a PNG file at `path`. Returns false on failure
// (could not open the file, or the PNG encoder failed). Accepts any format
// Image::writePNG supports (CLUT8 / RGB24 / RGBA32, or others via conversion).
// Used by the .rin capture path (dumpAutoshot) to dump the composited scene.
bool dumpSurfacePng(const Graphics::Surface &surf, const Common::String &path);

// True when a file exists and is readable at `path` (absolute or relative,
// same path handling as loadSurfaceRGBA). Existence check only -- no decode.
bool fileExists(const Common::String &path);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_PNG_LOADER_H
