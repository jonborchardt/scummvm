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

	// Called when a room transition begins — provider may prefetch assets.
	// Currently a no-op for the filesystem provider (room entry generates the plate
	// synchronously in pushHiresBackground, and roger_precache warms the cache up
	// front so entry is normally all hits).
	//
	// Transition seam (parent spec roadmap B5): when the room-transition feature
	// lands, prefetch should generate the *next* room's plate ahead of the
	// transition — ideally asynchronously — so the ~1.8 s/pic cold-generation cost
	// never blocks the transition itself. Until then, precache covers it.
	virtual void prefetch(GuiResourceId pictureId) {}

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

	// Called when kDrawPic draws a picture WITHOUT clearing the screen first
	// (addToFlag): the pic's commands paint over the scene the previous
	// kDrawPic(s) produced (e.g. the SQ3 intro title/scanner overlays). The
	// provider must ADD the pic to the displayed scene, not replace it.
	// Base implementation is a no-op.
	virtual void pushHiresBackgroundAddTo(GuiResourceId pictureId) {}

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

	// roger_diag accessor (revertible instrumentation): lets engine hook sites
	// (paint16) emit gated ROGER-DIAG trace lines. False in the base.
	virtual bool diagEnabled() const { return false; }

	// roger_cycle_log accessor: gates the per-kernelAnimate "ROGER-CYCLE" telemetry
	// line (see animate.cpp) for the scripted verification loop. False in the base.
	virtual bool cycleLogEnabled() const { return false; }

	// Standalone cel draw (kDrawCel) — e.g. an inventory item's "look at" close-up.
	// If an upscaled cel exists (views/<id>/view.<id>.loop.<loop>.png), composite it
	// into the overlay at globalRect (320x200 space); otherwise no-op (native shows).
	virtual void onDrawCel(const Common::Rect &globalRect, int viewId, int loopNo, int celNo) {}

	// Init-time cel (a cel drawn while _picNotValid): a view cel drawn during the room's
	// first setup that bakes into the native picture (QFG1 first-visit signs/decorations).
	// Captured so it can be re-shown persistently at hires. `owner` is an opaque token
	// identifying the drawing animate-list object (0 = not a cast draw, e.g. kDrawCel):
	// the capture is shown only while its owner is absent from the live animate list —
	// a disposed-after-baking prop promotes, a live actor (the ego) never does.
	// No-op in base.
	virtual void onInitCel(int viewId, int loopNo, int celNo,
	                       const Common::Rect &celRect, int priority, uint32 owner) {}

	// addToPic cel (kAddToPic) — a static view baked into the room's native picture.
	// Roger captures it as a persistent per-room sprite so it appears in the overlay at
	// hires (it is NOT part of the omyac plate and NOT in the animate list). celRect is
	// picture-window-local 320x190 space (same as animate Sprite::celRect). No-op in base.
	virtual void onAddToPicCel(int viewId, int loopNo, int celNo,
	                           const Common::Rect &celRect, int priority) {}

	// Generic native show (Feeder B): SCI is about to blit `screenRect` (320x200 screen
	// coords) of its native visual buffer to the display through a path Roger does not
	// hook semantically. Recorded for the per-frame generic composite. `ownerToken` scopes
	// the capture to the window it was drawn in (controls namespace 0x40000000 | window id,
	// 0 = no owning window): captures die with their window (GfxPorts::removeWindow), the
	// same lifetime rule as text/control captures. Without it, a pixel stamp queued while a
	// blocking window froze the game cycle is only processed AFTER the window is disposed —
	// it then stamps the restored native background over the hires plate, permanently
	// (the "un-enhanced band where the typed-command box was" bug). No-op in base.
	virtual void onNativeShowRect(const Common::Rect &screenRect, uint32 ownerToken) {}

	// Generic text-out capture (game-agnostic): SCI drew `text` at native `nativeRect`
	// in font `fontId`, color `penColor`, alignment `align`. nativeFontH is the SCI font
	// cell height (px) and nativeTextW is the single-line string width (0 = multi-line).
	// `winToken` scopes the text to the window/port it was drawn in (generic namespace
	// 0x60000000 | port->id), so the text is dropped when that window is disposed
	// (GfxPorts::removeWindow) — exactly like controls16/menu text. Default no-op.
	virtual void onNativeText(const Common::Rect &nativeRect, const char *text,
	                          int fontId, int penColor, int align,
	                          int nativeFontH, int nativeTextW, uint32 winToken) {}

	// Feeder B diff backstop: snapshot the native visual buffer as the "known" state
	// (plate-source + addToPic + animate sprites), taken right after SCI's updateScreen.
	// A later composite diffs against it to catch native draws no hook recorded. No-op base.
	virtual void snapshotNativeBaseline() {}

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
	virtual void uiPushWindow(const Common::Rect &globalRect, int backColor, int penColor,
	                          uint16 wndStyle, uint32 token) {}
	// textRole: 0 = body (dialog/message/list text), 1 = heading (titles); see
	// Roger::UiTextRole. useAltFont: render with the header/menu font.
	// nativeFontH: SCI font cell height (px) for the line; 0 = unknown.
	// nativeTextW: native single-line string width (px); 0 = multi-line/unknown.
	virtual void uiPushText(const Common::Rect &globalRect, const char *text, int penColor,
	                        int backColor, int fontId, int align, uint32 token,
	                        int textRole = 0, bool useAltFont = false,
	                        int nativeFontH = 0, int nativeTextW = 0) {}
	virtual void uiPushButton(const Common::Rect &globalRect, const char *text, int fontId,
	                          int style, uint32 token,
	                          int nativeFontH = 0, int nativeTextW = 0) {}
	virtual void uiPushTextEdit(const Common::Rect &globalRect, const char *text, int fontId,
	                            int style, int cursorPos, uint32 token,
	                            int nativeFontH = 0, int nativeTextW = 0) {}
	virtual void uiPushIcon(const Common::Rect &globalRect, int viewId, int loopNo, int celNo,
	                        uint32 token) {}
	// Score/title status banner (top strip): rendered hires (exact fit, opaque) so it
	// occludes the native low-res bar instead of showing through the overlay strip.
	virtual void uiPushStatus(const Common::Rect &globalRect, const char *text, int fontId,
	                          int penColor, int backColor, uint32 token,
	                          int nativeFontH = 0, int nativeTextW = 0) {}
	virtual void uiClearToken(uint32 token) {}
	virtual void uiClearAll() {}
	// Bracket a multi-element UI re-push (e.g. a menu dropdown: clear + window + one
	// text per row) so the per-push present barrier coalesces into ONE present at
	// endUiBatch. Without this, each push during a FROZEN cycle (blocking menu/dialog
	// loop, which never ticks kernelAnimate) flushes its own full present — a present
	// storm per menu highlight change. Depth-counted; no-op in the base provider.
	virtual void beginUiBatch() {}
	virtual void endUiBatch() {}
	// kGraphFrameBox selection highlight: frame-only (no fill), room-scoped.
	// globalRect is already in global 320x200 screen space. Any previous frame
	// pushed under the same token is replaced so the highlight tracks movement.
	virtual void uiPushFrameBox(const Common::Rect &globalRect, int penColor) {}

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
