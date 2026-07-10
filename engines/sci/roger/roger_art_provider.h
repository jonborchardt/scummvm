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
#include "sci/sci_gfx_observer.h"

namespace Sci {

class GfxScreen;

// AnimateEntry/AnimateList forward declarations now come from
// sci/sci_gfx_observer.h (same decoupling rationale).

// Strangler base (SciGfxObserver): during the observer migration this class
// derives from the neutral seam; each task moves a hook family from the old
// virtuals below onto SciGfxObserver and deletes the old virtual here. The
// same-name redeclarations that remain during migration (onMouseMoved,
// onCursorShape/onCursorView/onCursorHidden) intentionally OVERRIDE the base
// no-ops — identical signatures, identical no-op bodies.
class RogerArtProvider : public SciGfxObserver {
public:
	virtual ~RogerArtProvider() {}

	// Optional one-time startup warm-up: when roger_precache is set (and a
	// generating roger_gen_mode is active), generate every art-backed pic's
	// plate into the content cache up front, so in-game room entry is all hits
	// (no first-visit generation stall). No-op otherwise.
	virtual void precacheAll() {}

	// Single-step generation — called by the launcher's handleTickle() precache loop.
	// Generates (or cache-loads) one pic plate. Returns false if the provider cannot
	// generate (prebuilt mode or no asset gen). ms is generation time in milliseconds.
	virtual bool precacheOnePic(GuiResourceId /*picId*/, uint32 & /*ms*/) { return false; }
	// Generates one view's cels (all loops×cels for viewId). Returns false on failure.
	virtual bool precacheOneView(int /*viewId*/) { return false; }

	// Called from the SCI event loop when the mouse has moved. Roger composites its
	// cursor into the overlay, so it re-presents here to keep the cursor tracking the
	// pointer (especially during blocking dialogs/menus that do not tick animate).
	virtual void onMouseMoved() {}

	// R4: generic text-out capture migrated to SciGfxObserver::onText (source
	// kTextSourceBox); the old onNativeText virtual is deleted.

	// Called from GfxTransitions::doit() when a room transition is about to run (gated on
	// g_sciRogerProvider + enabled). The provider mirrors the effect in the overlay; SCI's
	// native transition is then finalized instantly (invisible under the opaque overlay).
	// sciType is the *normalized* transitions.h enum value (SCI_TRANSITIONS_*) — raw
	// game-script IDs have already been translated by GfxTransitions::doit() before this
	// hook fires. picRect is the 320x200 picture rect. blackoutSciType mirrors the
	// original's two-phase blackout form (SCI0 raw IDs 11-17): the screen first
	// animates old -> BLACK with that (normalized) type, then black -> new with
	// sciType. -1 = no blackout (plain old -> new).
	virtual void onTransition(int sciType, const Common::Rect &picRect, int blackoutSciType) {}
	// Called from kShakeScreen (gated). shakeCount jolts; directions bit0=vertical,
	// bit1=horizontal. The provider jolts the overlay; native shake is skipped.
	virtual void onShake(int shakeCount, int directions) {}

	// Called when SCI sets a new cursor shape (SCI0 kSetCursor resourceId).
	// The provider decodes the resource and rebuilds the hires cursor surface.
	// cursorId < 0 -> treat as hidden. No-op in base.
	virtual void onCursorShape(int cursorId) {}
	// Called when SCI shows or hides the cursor (kSetCursor hide/show paths).
	// When hidden the composited cursor is not drawn. No-op in base.
	virtual void onCursorHidden(bool hidden) {}
	// Called when SCI sets a VIEW-based cursor (SCI1 kSetCursor argc=3 path).
	// Provider renders the native cel scaled 5x and stores it as the cursor surface.
	virtual void onCursorView(int viewId, int loopNo, int celNo) {}

	// True while the provider's composited cursor owns the pointer visual — the
	// backend hardware cursor must not be drawn (it leaks at the overlay edge,
	// where it is composited ABOVE the overlay by every backend). Default false:
	// stock native cursor behavior when no provider / provider disabled.
	virtual bool hidesNativeCursor() const { return false; }

	// UI display-list capture (Roger hires dialogs). SCI's high-level UI draw calls
	// push resolution-independent elements (global 320x200 rects) here; the provider
	// composites them over the cached hires scene. All default to no-op so the base
	// provider (and null provider) are unaffected; FileRogerArtProvider overrides.
	// R4: uiPushText / uiPushStatus migrated to SciGfxObserver::onText (sources
	// kTextSourceControl/kTextSourceListRow/kTextSourceFill/kTextSourceMenuBar/
	// kTextSourceMenuRow and kTextSourceStatus respectively); those virtuals are
	// deleted.
	// R7/R8: uiPushButton / uiPushTextEdit migrated to SciGfxObserver::onControl
	// (kinds kControlButton/kControlTextEdit); uiPushFrameBox migrated to
	// SciGfxObserver::onFrameBox. Those virtuals are deleted here.

	// Debug/runtime toggles, invoked from the SCI event loop (see event.cpp):
	// toggleOverlay flips between the upscaled overlay and the original native
	// 320x200 render (A/B comparison); toggleDebugLog flips per-frame logging.
	virtual void toggleOverlay() {}
	virtual void toggleDebugLog() {}

	// Side-by-side compare mode: remap the game-space mouse coordinate so the LEFT panel
	// (the enhanced view) acts as the whole 320x200 game — clicks there hit the right spot.
	// No-op unless in side-by-side.
	virtual void remapComparisonMouse(Common::Point &mousePos) {}

	// DEBUG TOOL — tune-panel provider seams (spec 2026-07-05). MMPX judging
	// concluded 2026-07-06 (s2>s3 shipped); the panel is kept (no scheduled
	// deletion). F12 toggles the in-game quick-tune panel;
	// tunePanelMouse routes a button event at gamePos (320x200 game space) and
	// returns true when the panel consumed it (event.cpp then swallows it so
	// the game never sees clicks on the panel). No-ops in the base.
	virtual void toggleTunePanel() {}
	virtual bool tunePanelMouse(bool buttonDown, const Common::Point &gamePos) { return false; }

	// Returns true when the hires overlay is currently visible (i.e. F10 has not
	// hidden it). Used to gate overlay-specific effects (transitions, shake): when
	// the overlay is hidden the user is viewing the native 320x200 render, so native
	// SCI transitions and shake should run instead of being suppressed.
	virtual bool isOverlayVisible() const { return false; }

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
