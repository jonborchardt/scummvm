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

#ifndef SCI_ROGER_ROGER_ART_PROVIDER_H
#define SCI_ROGER_ROGER_ART_PROVIDER_H

#include "common/scummsys.h"
#include "sci/graphics/helpers.h"

namespace Sci {

class GfxScreen;

class RogerArtProvider {
public:
	virtual ~RogerArtProvider() {}

	// Called when a room transition begins — provider may prefetch assets.
	// No-op in Stage 1 (Emscripten VFS reads are synchronous).
	virtual void prefetch(GuiResourceId pictureId) {}

	// Returns true if replacement assets exist for this picture resource.
	virtual bool hasBackground(GuiResourceId pictureId) const = 0;

	// Fills GfxScreen's priority and control buffers from pre-generated PNGs.
	// Visual buffer is left untouched (roger-canvas covers it in Stage 1).
	// Returns false and writes nothing if assets are missing or wrong size.
	virtual bool loadBuffers(GuiResourceId pictureId, GfxScreen *screen) = 0;

	// Stage 1: pushes the hires visual PNG to the roger-canvas HTML overlay.
	// No-op in non-Emscripten builds (base implementation).
	virtual void pushHiresBackground(GuiResourceId pictureId) {}

	// Set to false to disable Roger without destroying the provider.
	// ScummVM native rendering is used when false.
	bool enabled;

protected:
	RogerArtProvider() : enabled(true) {}
};

// Global provider instance. Null means Roger is inactive.
// Set in SciEngine::run() after graphics init. Destroyed in SciEngine destructor.
extern RogerArtProvider *g_sciRogerProvider;

} // namespace Sci

#endif // SCI_ROGER_ROGER_ART_PROVIDER_H
