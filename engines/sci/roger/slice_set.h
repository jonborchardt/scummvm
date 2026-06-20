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

#ifndef SCI_ROGER_SLICE_SET_H
#define SCI_ROGER_SLICE_SET_H

#include "common/str.h"
#include "common/array.h"

namespace Graphics { struct Surface; }

namespace Sci {
namespace Roger {

// Map a CSS hex color (e.g. "#ffffff") to the nearest SCI0 EGA priority band (0..15).
int bandForColor(const Common::String &hex);

// One foreground slice piece: a surface loaded from a color_*.png, its hires
// position within the manifest's coordinate space, and the SCI priority band
// derived from its assigned color.
struct SlicePiece {
	Graphics::Surface *surface; // owned by SliceSet — do NOT free externally
	int x, y;                   // top-left in hires manifest space
	int band;                   // SCI priority 0..15
};

// Loads a sliced overlay manifest (manifest.json by default) together with all
// its color_*.png piece images. Constructed with the directory that contains
// both the manifest and the piece PNGs.
//
// Ownership: SliceSet owns all Graphics::Surface* in pieces(); they are freed
// in the destructor. SliceSet is non-copyable to prevent double-free.
class SliceSet {
public:
	// dir  — path to the "sliced/" directory containing the manifest + PNGs.
	// manifestName — filename of the JSON manifest within dir (default "manifest.json").
	SliceSet(const Common::String &dir, const Common::String &manifestName = "manifest.json");
	~SliceSet();

	// Non-copyable: owns raw Surface pointers.
	SliceSet(const SliceSet &) = delete;
	SliceSet &operator=(const SliceSet &) = delete;

	// Parse the manifest and load each piece surface.
	// Returns false if the manifest file cannot be found, read, or parsed.
	// Malformed individual pieces are silently skipped (no crash).
	bool load();

	const Common::Array<SlicePiece> &pieces() const { return _pieces; }

private:
	Common::String _dir, _manifest;
	Common::Array<SlicePiece> _pieces;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_SLICE_SET_H
