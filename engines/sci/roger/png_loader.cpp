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

#include "sci/roger/png_loader.h"
#include "image/png.h"
#include "common/fs.h"
#include "common/file.h"
#include "common/stream.h"
#include "graphics/surface.h"
#include "graphics/pixelformat.h"

namespace Sci {
namespace Roger {

Common::Array<byte> loadGrayscale8(const Common::String &path) {
	Common::Path fsPath(path);
	Common::FSNode node(fsPath);
	if (!node.exists() || !node.isReadable())
		return Common::Array<byte>();

	Common::SeekableReadStream *stream = node.createReadStream();
	if (!stream)
		return Common::Array<byte>();

	Image::PNGDecoder decoder;
	bool ok = decoder.loadStream(*stream);
	delete stream;
	if (!ok)
		return Common::Array<byte>();

	const Graphics::Surface *surf = decoder.getSurface();
	if (!surf)
		return Common::Array<byte>();

	const int w = surf->w;
	const int h = surf->h;
	const int bpp = surf->format.bytesPerPixel;

	Common::Array<byte> result;
	result.resize(w * h);

	for (int y = 0; y < h; y++) {
		const byte *row = (const byte *)surf->getBasePtr(0, y);
		for (int x = 0; x < w; x++) {
			if (bpp == 1) {
				// Grayscale or palette index — use directly
				result[y * w + x] = row[x];
			} else if (bpp >= 3) {
				// RGB or RGBA — take red channel (R == G == B for grayscale source)
				result[y * w + x] = row[x * bpp];
			} else {
				// Unexpected format — fill with zero
				result[y * w + x] = 0;
			}
		}
	}

	return result;
}

Graphics::Surface *loadSurfaceRGBA(Common::SeekableReadStream &stream) {
	Image::PNGDecoder decoder;
	if (!decoder.loadStream(stream) || !decoder.getSurface())
		return nullptr;
	const Graphics::PixelFormat rgba(4, 8, 8, 8, 8, 24, 16, 8, 0);
	// Use the no-palette overload: test fixtures and hires art are truecolor PNGs.
	Graphics::Surface *out = decoder.getSurface()->convertTo(rgba);
	return out;  // may be nullptr if conversion failed
}

Graphics::Surface *loadSurfaceRGBA(const Common::String &path) {
	Common::Path fsPath(path);
	Common::FSNode node(fsPath);
	if (!node.exists() || !node.isReadable())
		return nullptr;
	Common::SeekableReadStream *stream = node.createReadStream();
	if (!stream)
		return nullptr;
	Graphics::Surface *out = loadSurfaceRGBA(*stream);
	delete stream;
	return out;
}

bool dumpSurfacePng(const Graphics::Surface &surf, const Common::String &path) {
	Common::DumpFile out;
	if (!out.open(Common::Path(path)))
		return false;
	// Image::writePNG handles RGBA32/RGB24/CLUT8 directly and converts other
	// formats itself, so the composited RGBA scene can be passed straight through.
	const bool ok = Image::writePNG(out, surf);
	out.close();
	return ok;
}

bool fileExists(const Common::String &path) {
	Common::Path fsPath(path);
	Common::FSNode node(fsPath);
	return node.exists() && node.isReadable();
}

} // namespace Roger
} // namespace Sci
