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
#include "common/list.h"
#include "common/rect.h"
#include "sci/graphics/helpers.h"

namespace Sci {

class GfxScreen;

// Forward declaration of the SCI animate list, so this base header stays
// decoupled from the full SCI engine internals (it is included by sci.cpp /
// paint16.cpp / animate.cpp). The concrete definition lives in
// sci/graphics/animate.h, which only file_roger_art_provider.cpp includes.
struct AnimateEntry;
typedef Common::List<AnimateEntry> AnimateList;

class RogerArtProvider {
public:
	virtual ~RogerArtProvider() {}

	// Called when a room transition begins — provider may prefetch assets.
	// No-op for the filesystem provider (reads are synchronous).
	virtual void prefetch(GuiResourceId pictureId) {}

	// Returns true if replacement assets exist for this picture resource.
	virtual bool hasBackground(GuiResourceId pictureId) const = 0;

	// Fills GfxScreen's priority and control buffers from pre-generated 320x200
	// PNGs so game logic (pathfinding, occlusion) honors the replacement art.
	// The visual buffer is left untouched; the hires visual is shown via the
	// OSystem overlay in pushHiresBackground(). Returns false and writes nothing
	// if assets are missing or wrong size.
	virtual bool loadBuffers(GuiResourceId pictureId, GfxScreen *screen) = 0;

	// Shows the hires visual for this picture in ScummVM's OSystem overlay
	// (a higher-resolution layer composited above the 320x200 game surface).
	// Base implementation is a no-op.
	virtual void pushHiresBackground(GuiResourceId pictureId) {}

	// Called each frame by the GfxAnimate hook: translates the sorted animate
	// list to Sprites and composites the hires scene into the OSystem overlay.
	// Default no-op; FileRogerArtProvider overrides with the real compositor.
	virtual void renderFromAnimateList(const AnimateList &list) {}

	// Hides the OSystem overlay while a blocking SCI UI element (text box, menu)
	// is on screen. The overlay re-shows automatically on the next frame hook.
	// Default no-op; FileRogerArtProvider delegates to g_system->hideOverlay().
	virtual void hideOverlayForUI() {}

	// Called when a full-screen picture with NO replacement art is drawn: drop any
	// hires overlay left over from a previous room so it does not bleed through.
	virtual void onNativePicture() {}

	// UI display-list capture (Roger hires dialogs). SCI's high-level UI draw calls
	// push resolution-independent elements (global 320x200 rects) here; the provider
	// composites them over the cached hires scene. All default to no-op so the base
	// provider (and null provider) are unaffected; FileRogerArtProvider overrides.
	virtual void uiPushWindow(const Common::Rect &globalRect, int backColor, int penColor,
	                          uint16 wndStyle, uint32 token) {}
	virtual void uiPushText(const Common::Rect &globalRect, const char *text, int penColor,
	                        int backColor, int fontId, int align, uint32 token) {}
	virtual void uiPushButton(const Common::Rect &globalRect, const char *text, int fontId,
	                          int style, uint32 token) {}
	virtual void uiPushTextEdit(const Common::Rect &globalRect, const char *text, int fontId,
	                            int style, int cursorPos, uint32 token) {}
	virtual void uiPushIcon(const Common::Rect &globalRect, int viewId, int loopNo, int celNo,
	                        uint32 token) {}
	virtual void uiClearToken(uint32 token) {}
	virtual void uiClearAll() {}

	// Debug/runtime toggles, invoked from the SCI event loop (see event.cpp):
	// toggleOverlay flips between the upscaled overlay and the original native
	// 320x200 render (A/B comparison); toggleDebugLog flips per-frame logging.
	virtual void toggleOverlay() {}
	virtual void toggleDebugLog() {}

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
