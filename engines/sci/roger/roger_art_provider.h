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

	// Optional one-time startup warm-up: when roger_precache is set (and a
	// generating roger_gen_mode is active), generate every art-backed pic's
	// plate into the content cache up front, so in-game room entry is all hits
	// (no first-visit generation stall). No-op otherwise.
	virtual void precacheAll() {}

	// Returns true if replacement assets exist for this picture resource.
	virtual bool hasBackground(GuiResourceId pictureId) const = 0;

	// Obsolete: under in-engine generation the priority/control buffers are filled
	// by SCI's own native render (the hybrid drawPicture hook still draws the native
	// picture), so the provider no longer replaces them from prebuilt PNGs. Kept as a
	// documented no-op for source/ABI stability. Walkability + native occlusion ride
	// SCI's native buffers; overlay sprite occlusion is derived in-engine (priorityBands).
	virtual bool loadBuffers(GuiResourceId /*pictureId*/, GfxScreen * /*screen*/) { return false; }

	// Shows the hires visual for this picture in ScummVM's OSystem overlay
	// (a higher-resolution layer composited above the 320x200 game surface).
	// Base implementation is a no-op.
	virtual void pushHiresBackground(GuiResourceId pictureId) {}

	// Called each frame by the GfxAnimate hook: translates the sorted animate
	// list to Sprites and composites the hires scene into the OSystem overlay.
	// Default no-op; FileRogerArtProvider overrides with the real compositor.
	virtual void renderFromAnimateList(const AnimateList &list) {}

	// Called when a full-screen picture with NO replacement art is drawn: drop any
	// hires overlay left over from a previous room so it does not bleed through.
	virtual void onNativePicture() {}

	// Called from the SCI event loop when the mouse has moved. Roger composites its
	// cursor into the overlay, so it re-presents here to keep the cursor tracking the
	// pointer (especially during blocking dialogs/menus that do not tick animate).
	virtual void onMouseMoved() {}

	// Standalone cel draw (kDrawCel) — e.g. an inventory item's "look at" close-up.
	// If an upscaled cel exists (views/<id>/view.<id>.loop.<loop>.png), composite it
	// into the overlay at globalRect (320x200 space); otherwise no-op (native shows).
	virtual void onDrawCel(const Common::Rect &globalRect, int viewId, int loopNo, int celNo) {}

	// UI display-list capture (Roger hires dialogs). SCI's high-level UI draw calls
	// push resolution-independent elements (global 320x200 rects) here; the provider
	// composites them over the cached hires scene. All default to no-op so the base
	// provider (and null provider) are unaffected; FileRogerArtProvider overrides.
	virtual void uiPushWindow(const Common::Rect &globalRect, int backColor, int penColor,
	                          uint16 wndStyle, uint32 token) {}
	// textRole: 0 = body (dialog/message/list text), 1 = heading (titles); see
	// Roger::UiTextRole. useAltFont: render with the header/menu font.
	virtual void uiPushText(const Common::Rect &globalRect, const char *text, int penColor,
	                        int backColor, int fontId, int align, uint32 token,
	                        int textRole = 0, bool useAltFont = false) {}
	virtual void uiPushButton(const Common::Rect &globalRect, const char *text, int fontId,
	                          int style, uint32 token) {}
	virtual void uiPushTextEdit(const Common::Rect &globalRect, const char *text, int fontId,
	                            int style, int cursorPos, uint32 token) {}
	virtual void uiPushIcon(const Common::Rect &globalRect, int viewId, int loopNo, int celNo,
	                        uint32 token) {}
	// Score/title status banner (top strip): rendered hires (exact fit, opaque) so it
	// occludes the native low-res bar instead of showing through the overlay strip.
	virtual void uiPushStatus(const Common::Rect &globalRect, const char *text, int penColor,
	                          int backColor, uint32 token) {}
	virtual void uiClearToken(uint32 token) {}
	virtual void uiClearAll() {}

	// Debug/runtime toggles, invoked from the SCI event loop (see event.cpp):
	// toggleOverlay flips between the upscaled overlay and the original native
	// 320x200 render (A/B comparison); toggleDebugLog flips per-frame logging.
	virtual void toggleOverlay() {}
	virtual void toggleDebugLog() {}

	// Live enhance-pass tuning (roger_omyac generation): adjust the count of a
	// pass type (which: 0=fill, 1=line, 2=all) by delta and regenerate in place.
	virtual void tuneEnhancePasses(int delta, int which) {}
	virtual void reloadGenConfig() {}

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
