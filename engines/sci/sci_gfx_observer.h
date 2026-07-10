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

#ifndef SCI_SCI_GFX_OBSERVER_H
#define SCI_SCI_GFX_OBSERVER_H

#include "common/scummsys.h"
#include "common/list.h"
#include "common/rect.h"

namespace Sci {

struct Palette;

// Forward declaration of the SCI animate list so this header stays decoupled
// from the engine internals: it is included by every graphics hook site and,
// via roger_tokens.h, by SCI-free unit-test translation units. The concrete
// definition lives in sci/graphics/animate.h.
struct AnimateEntry;
typedef Common::List<AnimateEntry> AnimateList;

// Matches the typedef in sci/graphics/helpers.h (an identical redeclaration is
// legal C++); repeated here so this header does not pull the engine's
// helpers/detection headers into observer-only translation units.
typedef int GuiResourceId;

// ── Token scheme ────────────────────────────────────────────────────────────
// Every captured element is identified by a 32-bit token whose high nibble is
// a namespace. The load-bearing rule (learned the hard way, see the window
// token below): element LIFETIME is keyed off these tokens, never off
// geometry or per-game knowledge.
enum : uint32 {
	kGfxTokenNamespaceMask = 0xF0000000u,
	kGfxTokenStatus        = 0x10000000u, // status/menu-bar strip singleton
	kGfxTokenMenuDropdown  = 0x20000000u, // menu dropdown singleton
	kGfxTokenNsWindow      = 0x40000000u, // window/control captures: ns | windowId
	kGfxTokenNsIcon        = 0x50000000u, // standalone-cel (icon) captures: ns | portId
	kGfxTokenNsText        = 0x60000000u, // generic text captures: ns | portId
	kGfxTokenFrameBox      = 0x70000000u  // selection-frame singleton
};

// Window/control element token. The window token is THE dispose signal:
// GfxPorts::openWindow / removeWindow bracket every dialog, message, menu and
// the picture port itself; an element's lifetime equals its window's lifetime.
// Window ids are REUSED after dispose — an observer must clear a token's
// elements at window close or the next window reusing the id inherits ghosts.
inline uint32 gfxWindowToken(uint32 windowId) {
	return kGfxTokenNsWindow | windowId;
}

// Generic text-out token: text captured in GfxText16::Box is scoped to the
// port/window it was drawn in, so windowed text dies with its window while
// text on the persistent picture port survives until room change (this is why
// char-sheet stats persist while a popup over the sheet cannot wipe them).
inline uint32 gfxTextPortToken(uint32 portId) {
	return kGfxTokenNsText | portId;
}

// Standalone-cel (icon) capture token, port-scoped like generic text.
inline uint32 gfxIconToken(uint32 portId) {
	return kGfxTokenNsIcon | portId;
}

// Save-handle token: journal-checkpoint identity for onSave/onRestore/onFree.
// Packs a reg_t hunk handle as (segment << 16) | offset — deliberately
// unmasked, matching the historical packing so token streams are unchanged.
inline uint32 gfxHandleToken(uint32 segment, uint32 offset) {
	return (segment << 16) | offset;
}

// Animate-owner token for onCel(source=initBake): identifies the drawing
// animate-list OBJECT, not the resource. SCI0 offsets fit 16 bits; the mask
// makes the packing well-defined. Owner identity is the ONLY reliable
// discriminator between a baked prop and a live actor — see onCel.
inline uint32 gfxOwnerToken(uint32 segment, uint32 offset) {
	return (segment << 16) | (offset & 0xFFFFu);
}

inline uint32 gfxTokenNamespace(uint32 token) {
	return token & kGfxTokenNamespaceMask;
}

/**
 * SciGfxObserver — neutral display-layer observer seam for the SCI16 engine.
 *
 * A single registered observer (POD global g_sciGfxObserver, null by default)
 * receives structured notifications from SCI's graphics chokepoints, layered:
 *
 *   L1  frame lifecycle   (kernelAnimate boundaries, batching, mouse)
 *   L2  pixel truth       (bitsShow/bitsSave/bitsRestore/bitsFree/erase,
 *                          self-draw brackets, palette)
 *   L3  semantic events   (text, cels, windows, controls, pictures, menus)
 *   L4  claims            (documented overrides; return false = native runs)
 *
 * Contract for every event:
 *  - Null observer ⇒ SCI behavior is byte-identical to stock. Call sites are
 *    mechanical null-guarded calls with minimal marshalling; no observer
 *    logic, no game-specific branches, no concrete-observer types in SCI code.
 *  - Anything reachable per game cycle (from kernelAnimate) MUST be O(1) in
 *    the observer and must not force a full present/recompose unless the
 *    scene actually changed: SCI's cycle is a single synchronous heartbeat,
 *    so a heavy per-cycle path slows GAME LOGIC (a ~196 ms unconditional
 *    present in the bitsRestore path once caused a 2.7x walking slowdown).
 *  - Blocking calls (Print/Display menus) FREEZE the cycle: observer state
 *    tied to an element's lifetime must be updated at that element's DRAW
 *    event, never deferred to the animate cycle (deferred flushes reach the
 *    overlay after the window is disposed and ghost).
 *
 * Base implementations are all no-ops; the class is directly instantiable.
 */
class SciGfxObserver {
public:
	virtual ~SciGfxObserver() {}

	// Provenance of an onText emission. The observer derives PRESENTATION
	// (heading vs body role, alternate font, bar styling) from this source —
	// the old uiPushText textRole/useAltFont params are deliberately not
	// carried. kTextSourceFill is a background-fill-only event (empty string):
	// the kDisplay save-under box pushes its fill here while the text itself
	// arrives per line as kTextSourceBox.
	enum TextSource {
		kTextSourceBox,      // GfxText16::Box per-line capture (all narration/dialog text)
		kTextSourceControl,  // kDrawControl text control
		kTextSourceStatus,   // score/title status banner (token kGfxTokenStatus)
		kTextSourceMenuBar,  // menu-bar title at bar-draw time
		kTextSourceMenuRow,  // dropdown row at menu-draw time (token kGfxTokenMenuDropdown)
		kTextSourceListRow,  // list-control row (selected row arrives inverted pen/back)
		kTextSourceFill      // background fill only; text is empty ""
	};

	// Provenance of an onCel emission.
	enum CelSource {
		kCelSourceAnimate,    // live animate-cast draw
		kCelSourceAddToPic,   // kAddToPic cel baked into the room picture
		kCelSourceInitBake,   // cel drawn while _picNotValid (room init) — may bake
		kCelSourceStandalone, // script kDrawCel (e.g. inventory look-at close-up)
		kCelSourceIcon        // kDrawControl icon control
	};

	// Control kind for onControl.
	enum ControlKind {
		kControlButton,
		kControlTextEdit
	};

	// ── L1: frame lifecycle ─────────────────────────────────────────────────

	// Fired at GfxAnimate::kernelAnimate entry — the start of one game cycle.
	// Early-return cycles (null cast list, script abort) fire this WITHOUT a
	// matching onAnimateFrame; observers must tolerate that (arm/consume).
	virtual void onFrameStart() {}

	// Fired when the native buffer holds the complete frame
	// {pic + addToPic + animate cels}, right after the animate cel shows and
	// before restoreAndDelete. This is the whole-frame native snapshot point
	// (e.g. for a side-by-side native mirror panel).
	virtual void onFrameEnd() {}

	// Fired with the sorted animate cast: once per cycle after
	// restoreAndDelete (the per-frame composite seam), and again from
	// reAnimate after a save-under background restore (dialog dismissal) so a
	// compositing observer can re-render. This is the LAST per-cycle event.
	virtual void onAnimateFrame(const AnimateList &list) {}

	// Bracket a multi-element re-push (e.g. a menu dropdown: clear + window +
	// one text per row) so a presenting observer coalesces into ONE present at
	// endBatch. Without this, each push during a FROZEN cycle (blocking
	// menu/dialog loop, which never ticks kernelAnimate) flushes its own full
	// present — a present storm per menu highlight change (the menu
	// mouse-crawl bug). Depth-counted; generalized to any frozen-loop re-push.
	virtual void beginBatch() {}
	virtual void endBatch() {}

	// Fired from the SCI event loop when the mouse moved. An observer that
	// composites its own cursor re-presents here: blocking dialogs/menus do
	// not tick kernelAnimate, so the cursor would otherwise freeze.
	virtual void onMouseMoved() {}

	// ── L2: pixel truth ─────────────────────────────────────────────────────
	// The completeness guarantee (verified over 92 copyRectToScreen callers,
	// FORK_AUDIT §5, scoped to non-Mac SCI16): every visible pixel change
	// crosses onShow or is bracketed by beginSelfDraw/endSelfDraw; every
	// visible LUT change crosses onPaletteChanged.

	// SCI is about to blit screenRect (global 320x200 screen coords) of its
	// native visual buffer to the display (GfxPaint16::bitsShow). `owner` is a
	// window token (gfxWindowToken) or 0: captures scoped to a window die with
	// it at onWindowClose. Without owner scoping, a pixel stamp queued while a
	// blocking window froze the cycle is processed only AFTER the window is
	// disposed — it then stamps the restored background over the observer's
	// scene, permanently (the "un-enhanced band where the typed-command box
	// was" bug). Suppressed while inside a self-draw bracket.
	virtual void onShow(const Common::Rect &screenRect, uint32 owner) {}

	// bitsSave: a save-under is a journal CHECKPOINT (token = gfxHandleToken
	// of the hunk handle) — a later onRestore must be able to roll back
	// everything drawn over the saved region since this moment.
	virtual void onSave(uint32 token, const Common::Rect &rect) {}

	// bitsRestore: roll back elements appended since the matching checkpoint
	// and invalidate the region (a rollback REVEALS saved pixels; they are
	// never re-captured). token 0 = no checkpoint: used by the documented
	// no-save-under window-disposal exception (transparent windows have no
	// hunk to restore, so the dispose path plants the reveal explicitly —
	// without it the subsequent show re-stamps the restored background).
	// Rollback must spare persistent singletons (kGfxTokenStatus,
	// kGfxTokenFrameBox) that repaint while a save-under is open — they
	// postdate the checkpoint but are NOT save-under content.
	virtual void onRestore(uint32 token, const Common::Rect &rect) {}

	// bitsFree: a save-under freed WITHOUT restore — drop the checkpoint.
	virtual void onFree(uint32 token) {}

	// A native region was erased/redrawn in place (kGraphRedrawBox). rect is
	// global. Geometric (containment) removal of persisted captures is the
	// correct rule HERE — token-scoped removal is the rule at window dispose;
	// the two are complementary, not interchangeable.
	virtual void onErase(const Common::Rect &rect) {}

	// Re-entrancy bracket: SCI wraps draws the observer composites
	// SEMANTICALLY (picture render, animate cel shows, kDisplay text flush) so
	// the generic onShow pixel capture does not double-composite them.
	// Depth-counted, must stay balanced, and fires regardless of observer
	// display state. PLACEMENT IS LOAD-BEARING: e.g. GfxText16::Box must stay
	// OUTSIDE the kDisplay flush bracket or its per-line text capture is
	// depth-suppressed (doubled/missing intro credits class).
	virtual void beginSelfDraw() {}
	virtual void endSelfDraw() {}

	// The live palette changed (GfxPalette::copySysPaletteToScreen funnel):
	// per-tick palVary fades and color cycling change only the LUT and emit NO
	// pixel event, so their content is unrecoverable from L2 pixels. step/total
	// mirror the vary progress (0/0 = plain set).
	virtual void onPaletteChanged(const Palette &palette, int16 step, int16 total) {}

	// ── L3: semantic enrichment ─────────────────────────────────────────────

	// Text drawn at rect (global 320x200; the DRAWN extent, per line — never
	// the caller's requested box: requested-box rects re-wrapped at observer
	// metrics drift off their native rows and escape save-under restore
	// containment). On SCI0 EGA text is drawn with show == false and flushed
	// later — there is deliberately NO "shown" gate on this event.
	// nativeFontH = SCI font cell height (px); nativeTextW = single-line
	// string width (0 = unknown/multi-line); token = gfxTextPortToken /
	// kGfxTokenStatus / kGfxTokenMenuDropdown per source. itemId = menu row
	// item id when source == kTextSourceMenuRow; 0 otherwise (menu-row selection
	// is keyed by the SCI item id — ordinals are unsafe, separator rows skip
	// ids). Rects arrive
	// globalized (hook sites apply offsetRect before emitting — port-local
	// rects never match global erase rects).
	virtual void onText(const Common::Rect &rect, const char *text,
	                    int fontId, int penColor, int backColor, int align,
	                    int nativeFontH, int nativeTextW, uint32 token,
	                    TextSource source, uint16 itemId) {}

	// A view cel was drawn at rect. Coordinate space follows the native draw:
	// animate/addToPic/initBake rects are picture-local celRects; standalone/
	// icon rects are globalized. owner is the
	// animate-object token for cast/init draws (gfxOwnerToken); the
	// owning-window token for source == kCelSourceIcon; 0 = none. For
	// kCelSourceInitBake the owner is
	// the promotion discriminator: an init-frame draw bakes into the
	// picture only if its OWNER OBJECT leaves the animate list — no resource
	// identity (view/loop/cel) can separate a baked prop from a live actor
	// (SCI0 packs both into one per-room view; promoting by identity either
	// froze a duplicate ego or wiped the room signs).
	virtual void onCel(const Common::Rect &rect, int viewId, int loopNo,
	                   int celNo, int priority, uint32 owner, CelSource source) {}

	// A window opened (GfxPorts::drawWindow). globalRect is the window's
	// global dims; title is the caption text or "" (subsumes a separate
	// title-text event); token = gfxWindowToken(id). Only windows with a
	// frame style get a border — the status banner is frameless (SCI
	// NOFRAME); framing every window drew a line under the bar.
	virtual void onWindowOpen(const Common::Rect &globalRect, uint16 style,
	                          int backColor, int penColor, const char *title,
	                          uint32 token) {}

	// The window died (GfxPorts::removeWindow) — THE dispose signal. Drop
	// every element scoped to this token (controls AND generic text). Do not
	// rely on save-under restores instead: transparent / no-save-under
	// windows and reanimate==false disposals never fire one.
	virtual void onWindowClose(uint32 token) {}

	// A dialog control was drawn (kDrawControl). cursorPos is the caret
	// position for kControlTextEdit and -1 for kControlButton. Live typing
	// re-emits textEdit with the same token/rect (replace-in-place identity).
	virtual void onControl(ControlKind kind, const Common::Rect &rect,
	                       const char *text, int fontId, int style,
	                       int cursorPos, uint32 token,
	                       int nativeFontH, int nativeTextW) {}

	// kGraphFrameBox / control-selection frame: frame-only (no fill),
	// singleton token kGfxTokenFrameBox, replaced on re-push so the highlight
	// tracks movement. Persistent until room change or explicit erase — no
	// bitsRestore ever covers it (documented duty-3 exception).
	virtual void onFrameBox(const Common::Rect &rect, int penColor) {}

	// A picture (room background) was drawn. addToFlag = the pic paints OVER
	// the previous pics WITHOUT clearing (SQ3 intro title/scanner overlays):
	// the observer must ADD it to the displayed scene, never replace the base
	// scene (treating an addTo pic as a room entry produced white title
	// screens and black starfields). !addToFlag = a real room entry.
	virtual void onPicture(GuiResourceId picId, bool addToFlag) {}

	// A full-screen picture with NO observer replacement is being drawn: drop
	// any stale observer scene from the previous room so it does not bleed
	// through.
	virtual void onPictureAbsent() {}

	// The highlighted menu row changed (invertMenuSelection). The observer
	// keeps its own retained row state (from onText(menuRow) between the
	// dropdown's onWindowOpen/onWindowClose) and re-composites.
	virtual void onMenuHighlight(uint16 itemId) {}

	// ── L4: claims ──────────────────────────────────────────────────────────
	// Documented overrides. Returning false means the native path runs
	// unchanged. Each claim states what native behavior is skipped and what
	// the observer must guarantee in exchange.

	// Room transition. sciType is the NORMALIZED transitions.h enum value
	// (raw game-script ids already translated); picRect is the 320x200
	// picture rect; blackoutSciType mirrors the two-phase blackout form
	// (old -> black -> new), -1 = plain. Returning true skips SCI's animated
	// transition (the caller finalizes the screen instantly); the observer
	// must render an equivalent blocking effect itself, or the user sees an
	// instant cut. Skipping avoids invisible double-blocking dead time when
	// an opaque observer layer covers the native animation.
	virtual bool claimTransition(int sciType, const Common::Rect &picRect,
	                             int blackoutSciType) { return false; }

	// kShakeScreen. Returning true skips the native (blocking) shake; the
	// observer must provide its own equivalent jolt. directions: bit0 =
	// vertical, bit1 = horizontal.
	virtual bool claimShake(int count, int directions) { return false; }

	// True while the observer owns the pointer visual: the backend hardware
	// cursor is suppressed (it composites ABOVE any overlay on every backend
	// and leaks at the letterbox edge). The observer must draw its own cursor
	// and keep it tracking via onMouseMoved.
	virtual bool claimCursor() const { return false; }

	// Cursor notifications (fire alongside the native cursor calls so the
	// observer's cursor mirrors shape/visibility even while claimed).
	virtual void onCursorShape(int cursorId) {}                       // kSetCursor SCI0; < 0 = hidden
	virtual void onCursorView(int viewId, int loopNo, int celNo) {}   // SCI1 view cursor
	virtual void onCursorHidden(bool hidden) {}

	// kernelTexteditChange: returning true relaxes the native "does the next
	// glyph fit the nsRect pixel width?" early-return, letting typing continue
	// past the native cap (an observer rendering the field wider needs the
	// keystrokes). The script-side maxChars buffer bound still applies — the
	// observer guarantees nothing overflows; only the pixel-width cap lifts.
	virtual bool wantsUnclampedTextEdit() const { return false; }
};

// Global observer slot. Null means no observer (stock behavior, byte-identical).
// Set in SciEngine::run() after graphics init; cleared in the SciEngine
// destructor BEFORE the owning provider is deleted. POD pointer global
// (no constructor), per the reentrancy rules.
extern SciGfxObserver *g_sciGfxObserver;

// Registration seam (Stage 3): the single slot is set/cleared through this, so the
// concrete observer type never appears in SCI engine wiring. A future observer
// LIST is a mechanical change here.
void setSciGfxObserver(SciGfxObserver *observer);
SciGfxObserver *sciGfxObserver();

} // namespace Sci

#endif // SCI_SCI_GFX_OBSERVER_H
