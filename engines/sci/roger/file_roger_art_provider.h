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

#ifndef SCI_ROGER_FILE_ROGER_ART_PROVIDER_H
#define SCI_ROGER_FILE_ROGER_ART_PROVIDER_H

#include "sci/roger/roger_art_provider.h"
#include "sci/roger/null_roger_art_provider.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_capabilities.h"
#include "sci/roger/roger_compositor.h"
#include "sci/roger/roger_ui_layer.h"
#include "sci/roger/roger_input.h"
#include "common/array.h"
#include "common/str.h"
#include "common/path.h"

namespace Graphics { struct Surface; class ManagedSurface; }

namespace Sci {
namespace Roger { struct Sprite; class RogerCompositor; class ViewCache; class RogerUiLayer; class RogerTextRenderer; }

class FileRogerArtProvider : public RogerArtProvider {
public:
	// gameId: ScummVM game ID string (e.g. "sq3", "qfg1")
	// gamePath: path to the game directory, as a Common::Path so native
	//   separators are parsed correctly (use ConfMan.getPath("path"), NOT
	//   ConfMan.get("path") — the latter is a raw string with backslashes on
	//   Windows that Common::Path's '/' separator cannot split).
	FileRogerArtProvider(const Common::String &gameId, const Common::Path &gamePath);
	~FileRogerArtProvider();

	bool isOverlayVisible() const override;
	bool hasBackground(GuiResourceId pictureId) const override;
	void precacheAll() override;
	bool precacheOnePic(GuiResourceId picId, uint32 &ms) override;
	bool precacheOneView(int viewId) override;
	void pushHiresBackground(GuiResourceId pictureId) override;
	void renderFromAnimateList(const AnimateList &list) override;
	void onNativePicture() override;
	void onMouseMoved() override;
	void onDrawCel(const Common::Rect &globalRect, int viewId, int loopNo, int celNo) override;
	void onAddToPicCel(int viewId, int loopNo, int celNo,
	                   const Common::Rect &celRect, int priority) override;
	void onInitCel(int viewId, int loopNo, int celNo,
	               const Common::Rect &celRect, int priority, uint32 owner) override;
	void beginNativeDraw() override;
	void endNativeDraw() override;
	void onNativeShowRect(const Common::Rect &screenRect, uint32 ownerToken) override;
	void onNativeText(const Common::Rect &nativeRect, const char *text,
	                  int fontId, int penColor, int align,
	                  int nativeFontH, int nativeTextW, uint32 winToken) override;
	void onNativeEraseRect(const Common::Rect &nativeRect) override;
	void snapshotNativeBaseline() override;
	void onTransition(int sciType, const Common::Rect &picRect) override;
	void onShake(int shakeCount, int directions) override;
	void onCursorShape(int cursorId) override;
	void onCursorHidden(bool hidden) override;
	void onCursorView(int viewId, int loopNo, int celNo) override;
	void remapComparisonMouse(Common::Point &mousePos) override;
	void toggleOverlay() override;   // Ctrl+Shift+U: upscaled overlay <-> original native
	void toggleDebugLog() override;  // Ctrl+Shift+L: per-frame Roger diagnostic logging
	void tuneEnhancePasses(int delta, int which) override; // Ctrl+Shift+]/[ add/remove fill; '/; add/remove all
	void reloadGenConfig() override; // Ctrl+Shift+R: re-read roger_omyac_passes from ConfMan
	void cycleBodyFont() override; // Ctrl+Shift+F: rotate dialog font through the shortlist

	// UI display-list capture (Roger hires dialogs) — see roger_art_provider.h.
	void uiPushWindow(const Common::Rect &globalRect, int backColor, int penColor,
	                  uint16 wndStyle, uint32 token) override;
	void uiPushText(const Common::Rect &globalRect, const char *text, int penColor,
	                int backColor, int fontId, int align, uint32 token,
	                int textRole, bool useAltFont,
	                int nativeFontH, int nativeTextW) override;
	void uiPushButton(const Common::Rect &globalRect, const char *text, int fontId,
	                  int style, uint32 token,
	                  int nativeFontH, int nativeTextW) override;
	void uiPushTextEdit(const Common::Rect &globalRect, const char *text, int fontId,
	                    int style, int cursorPos, uint32 token,
	                    int nativeFontH, int nativeTextW) override;
	void uiPushIcon(const Common::Rect &globalRect, int viewId, int loopNo, int celNo,
	                uint32 token) override;
	void uiPushStatus(const Common::Rect &globalRect, const char *text, int fontId,
	                  int penColor, int backColor, uint32 token,
	                  int nativeFontH, int nativeTextW) override;
	void uiClearToken(uint32 token) override;
	void uiClearAll() override;
	void uiPushFrameBox(const Common::Rect &globalRect, int penColor) override;

	// Compose and present the current room to the OSystem overlay.
	// Called each frame by the GfxAnimate hook (Task 7).
	void renderFrame(const Common::Array<Roger::Sprite> &sprites);

	// Accessor used by the GfxAnimate hook to translate AnimateEntry → Sprite.
	Roger::ViewCache *viewCache() { return _viewCache; }

private:
	Common::String _basePath;       // absolute path to <gameid>-roger/ directory
	Common::String _gameId;         // ScummVM game id (e.g. "sq3", "qfg1"); stored for cache paths + debug dumps

	Roger::RogerAssetGen *_assetGen = nullptr;
	Roger::RogerCompositor *_compositor = nullptr;
	Roger::ViewCache *_viewCache = nullptr;
	Graphics::Surface *_plate = nullptr;
	int _loadedPicId = -1;
	int _bodyFontIdx = -1; // index into the body-font shortlist (-1 = config/default font)
	Roger::RogerCapabilities _caps;   // probed once on first room load; read-only after
	bool _capsProbed = false;
	Roger::CompareDisplayMode _mode = Roger::kModeEnhanced; // F10 cycles enhanced/original/side-by-side
	bool overlayShown() const { return _mode != Roger::kModeOriginal; } // overlay visible (enhanced OR side-by-side)
	bool _debugLog = false;      // per-frame diagnostic logging
	uint32 _lastUiDiagSig = 0;   // ROGER-UI diag dump dedup: signature of the last dumped UI display-list
	bool _diag = false;          // roger_diag: one-line overlay-state trace at room-load/present/transition seams (revertible instrumentation)
	void diagDumpState(const char *where);
public:
	bool diagEnabled() const override { return _diag; }
	bool cycleLogEnabled() const override { return _cycleLog; }
private:
	bool _autoshot = false;      // roger_autoshot: dump the composited scene to PNG on room load (verification harness)
	bool _selfTest = false;      // roger_selftest: log structural invariant PASS/FAIL per room (off by default)
	bool _diffBackstop = false;  // roger_diff_backstop: Feeder B pixel-diff backstop (default off; per-frame full-buffer diff is costly and can stamp blocky native pixels over the plate around moving sprites)
	bool _diffCheck = false;     // roger_diff_check: gated in-engine native-vs-overlay diff (off by default; once per pic; never on steady-state path)
	bool _truthCapture = false; // .rin captures grab the REAL overlay pixels (grabOverlay) instead of forcing a full recompose — evidence mode, default off; stale never-pushed regions are visible
	// Input automation (scripted verification loop / live control) — roger_input.h.
	Roger::InputScriptDriver *_inputDriver = nullptr;
	bool _cycleLog = false;
	void maybeScriptCapture(Graphics::ManagedSurface &scene, const Common::Rect &gameRect);
	bool _useHwCursor = false;   // roger_hw_cursor: try the native HW cursor over the overlay (invisible in practice); default false = composited arrow
	bool _debugCapture = false;  // roger_debug_capture: write manifest + PNGs to screenshots/ once per pic (off by default; inspection only)
	int _autoshotPicId = -1;     // last pic id already auto-shot (so we dump once per room, not per frame)
	int _debugDumpedPic = -1;    // last pic id whose capture was dumped (once-per-pic guard for dumpCaptureDebug)
	int _diffCheckedPic = -1;    // last pic id whose diff was run (once-per-pic guard for runDiffCheck)
	uint32 _lastUiSig = 0;       // signature of the last -ui autoshot's UI layer (throttle: dump only on change)
	int _statusBarH = 10;        // SCI0 status/menu bar height in screen rows (of 200); reserved at the top of the game rect (may change)
	Common::Array<byte> _priorityMap; // 1920x1140 omyac-aligned priority bands (from RogerAssetGen::generatePriorityMap), for overlay occlusion

	// Roger hires UI/dialog compositing (see roger_ui_layer / roger_text).
	Roger::RogerUiLayer *_uiLayer = nullptr;
	Roger::RogerTextRenderer *_textRenderer = nullptr;     // dialog font
	Roger::RogerTextRenderer *_altTextRenderer = nullptr;  // header/menu font
	Graphics::ManagedSurface *_sceneCache = nullptr; // last composed room+sprites (no UI)
	Graphics::ManagedSurface *_scratchScene = nullptr; // reused per-frame compose buffer (realloc only on size change)
	// Return a persistent scratch surface of (w,h) in RGBA32, reallocated only when
	// the overlay size changes — avoids a fresh ManagedSurface alloc/free every frame.
	Graphics::ManagedSurface *scratchScene(int w, int h);
	// Compose the current room background (plate, no sprites) into `out` at full overlay size.
	void composeRoomScene(Graphics::ManagedSurface &out);
	Common::Array<Graphics::Surface *> _uiIcons;     // owned native-cel surfaces for kUiIcon
	// addToPic cels captured for the current room (Feeder A). Cleared on room change;
	// merged with the animate list each frame and drawn via the hires Sprite path.
	Common::Array<Roger::Sprite> _staticSprites;
	// Cels drawn during room init (_picNotValid) that bake into the native picture (QFG1
	// first-visit signs), tagged with their owning animate object (one capture per owner,
	// latest wins; owner 0 = script kDrawCel). Cleared on room change; each frame the entries
	// whose owner is absent from the animate list are merged in as persistent hires statics —
	// a disposed-after-baking prop promotes, a live actor (the ego) never does. See onInitCel.
	Common::Array<Roger::Sprite> _initCels;
	// Native-foreground capture (QFG1 menu/character-creation stat labels, class buttons,
	// software cursor): regions recorded by the bitsShow hook (onNativeShowRect), turned
	// into persistent sprites so they survive past one frame (Feeder-A style). Each carries
	// its owning window token (0x40000000 | id, 0 = none): pending regions AND stamped
	// sprites owned by a window are dropped when that window is disposed (uiClearToken from
	// GfxPorts::removeWindow) — without this, a region queued while a blocking window froze
	// the game cycle is processed only after dispose and stamps the restored native
	// background over the plate for the rest of the room. Owner-less captures stay
	// room-scoped (cleared by clearTextSprites on room change).
	struct FgRegion { Common::Rect rect; uint32 owner; };
	Common::Array<FgRegion> _foregroundRegions;       // bitsShow rects pending capture this frame
	Common::Array<Roger::Sprite> _textSprites;        // persistent captured-foreground sprites (own celOverride; Sprite::owner = window token)
	Common::Array<Roger::UiElement> _genTextPending; // generic-text captures pending emit this frame (Task 3 consumes)
	// Cross-frame cache for non-ASCII glyph surfaces generated by buildGlyphs for generic
	// text (flushGenericText path). Keyed by (ch, fontId, penColor) so each distinct glyph
	// surface is generated at most once per room instead of once per frame. Owned here;
	// surfaces are freed on room change alongside _uiIcons.
	struct GenGlyphKey { byte ch; int fontId; int penColor; const Graphics::Surface *surf; };
	Common::Array<GenGlyphKey> _genericGlyphCache;
	// Cache for native-fallback cel surfaces used by onDrawCel (kDrawCel standalone cels).
	// Keyed by (viewId, loopNo, celNo) so the same cel drawn at N positions renders once
	// and is referenced N times. Owned via _uiIcons (freed on room change); this array is
	// just an index (cleared whenever _uiIcons is freed).
	struct DrawCelNativeKey { int viewId; int loopNo; int celNo; Graphics::Surface *surf; };
	Common::Array<DrawCelNativeKey> _drawCelNativeCache;
	int _nativeDrawDepth = 0;                 // >0 => inside a Roger-handled draw
	Common::Array<Common::Rect> _genRegions;  // Feeder B native rects captured this frame
	Common::Array<byte> _nativeBaseline;      // last "known" native visual buffer (Feeder B diff)
	bool _haveBaseline = false;
	bool _haveScene = false;                         // _sceneCache valid this room
	bool _transitionsEnabled = true;                 // roger_transitions knob (default on)
	bool _paletteLive = true;                        // roger_palette_live knob (default on)
	Common::Array<byte> _plateIndex;                 // current room's omyac doubled-nibble index map (or empty)
	byte _palSnapshot[48];                           // room-load EGA palette (16 RGB triples)
	bool _haveSnapshot = false;
	uint32 _lastPaletteCheckMs = 0;                  // whole-palette re-apply throttle
	// Per-frame: diff live palette vs snapshot; partial change -> re-blend changed regions
	// (dirty present), whole change -> throttled full re-blend + full present. No-op if
	// roger_palette_live is off or no index map is resident.
	void observeLivePalette();
	Graphics::Surface *_cursorSurf = nullptr;        // smooth hires arrow cursor (RGBA, owned)
	int _cursorShapeId = -1;   // last SCI0 cursor resource id received; -1 = unknown
	bool _cursorVisible = true; // false when the game called kernelHide()
	void ensureCursor();                             // build _cursorSurf once
	void compositeCursor(Graphics::ManagedSurface &scene, const Common::Rect &gameRect);
	// Decode the SCI0 cursor resource cursorId from g_sci->_resMan, scale 5x, store
	// result in _cursorSurf + hotspot in _cursorHotspot. No-op if resource unavailable.
	void buildCursorForShape(int cursorId);
	// Render native VIEW cel via renderNativeCel(), scale 5x (inline RGBA nearest-neighbour),
	// store in _cursorSurf. Hotspot computed from GfxView cel displaceX/displaceY.
	void buildCursorFromView(int viewId, int loopNo, int celNo);
	// Cursor-only fast redraw (composite cache). Holds scene+UI with no cursor baked in.
	// Rebuilt on scene/UI change; patched in-place on cursor-only moves.
	Graphics::ManagedSurface *_compositeCache = nullptr;  // scene+UI, no cursor
	bool _compositeCacheValid = false;                     // cleared on scene/UI change; set after rebuild
	Common::Rect _lastCursorDstRect;                       // overlay-space rect where cursor was last painted
	Common::Point _cursorHotspot;                          // active-point offset within _cursorSurf (overlay px)
	void ensureCompositeCache(int w, int h);
	Common::Rect _lastGameRect;                      // gameRect used for the cached scene
	void ensureUi();                                 // lazily build _uiLayer + _textRenderer
	void presentWithUi();                            // compose _sceneCache + _uiLayer -> overlay
	// Side-by-side compare mode: build enhanced(left)|original(right) into the overlay and
	// present full. Gated by _mode == kModeSideBySide; called from renderFrame/presentWithUi.
	void presentComparison();

	// ── Present barrier (spec §3.2) ────────────────────────────────────────────
	// The ONLY entry point that pushes to the overlay outside transitions. O(1)
	// when nothing changed. Defers while the animate cycle is mid-draw
	// (_inAnimateCycle) — the end-of-renderFromAnimateList call flushes.
	void presentBarrier();
	void markUiDirty(const Common::Rect &nativeRect);      // element pushed/redrawn at nr
	void markVacatedDirty(const Common::Rect &nativeRect); // element removed at nr
	void markNativeDirty(const Common::Rect &nativeRect);  // §3.1: exact rect SCI touched
	void markFullDirty();                                  // room/F10/font/plate change
	// Overlay-space rect the cursor would occupy right now (empty when not drawable).
	// Extracted from compositeCursor so the barrier can detect cursor movement.
	Common::Rect cursorDstRect(const Common::Rect &gameRect);
	bool _barrierDirty = false;      // any mark since the last barrier present
	bool _inAnimateCycle = false;    // set at snapshotNativeBaseline, cleared at cycle end
	bool _frameJustComposed = false; // renderFrame composed this cycle (Task 4 uses it)

	// Last status/title banner so it can be re-applied on room load / F10 enable
	// (the game only redraws it on score/text change).
	bool _haveStatus = false;
	Common::Rect _statusRect;
	Common::String _statusText;
	int _statusPen = 0, _statusBack = 0;
	uint32 _statusToken = 0;
	int _statusFont = 0;                       // SCI font id the game drew the banner with
	int _statusNativeFontH = 0, _statusNativeTextW = 0; // native font metrics captured at push time
	void reapplyStatus(); // re-push the cached banner (no-op if none)
	// roger_autoshot helper: dump <screenshotpath>/roger-<id><suffix>-overlay.png and
	// -preview.png for the given composited scene (suffix "" = per-room scene, "-ui" =
	// dialog re-present). Verification harness only; no-op unless roger_autoshot is set.
	void dumpAutoshot(Graphics::ManagedSurface &scene, const Common::Rect &gameRect, const char *suffix);
	// roger_debug_capture helper: write a per-pic manifest (captured TEXT strings+rects and
	// GFX rects) plus a PNG per pixel-captured graphic to the gitignored screenshots/ folder.
	// Guarded by _debugCapture and a once-per-pic check (_debugDumpedPic). No-op when off.
	void dumpCaptureDebug();
	// roger_diff_check helper: downscale _compositeCache to 320x200, diff against the native
	// visual buffer, and log coalesced "present-in-native-but-missing-in-overlay" boxes via
	// ROGER-DIAG[diff]. Guarded by _diffCheck and a once-per-pic guard (_diffCheckedPic).
	// Off by default; never on the steady-state path.
	void runDiffCheck();

	// Parse a roger_omyac_passes string (or the three-state unset/empty/tokens
	// logic) into an enhance-pass array. Call with hasKey=false for the "unset"
	// case (returns defaultPasses); hasKey=true with an empty string for wireframe
	// (returns empty); hasKey=true with tokens for a parsed list.
	Common::Array<int> parseOmyacPasses(bool hasKey, const Common::String &passStr) const;

	// Regenerate the current room's plate in place and re-push the overlay.
	// No-op if no room is loaded (_loadedPicId < 0) or _assetGen is null.
	void regenInPlace();

	// Render each unique non-ASCII byte of `text` as a glyph surface from the game's
	// SCI font (fontId, penColor), own it in _uiIcons, and append {byte,surface} to
	// `out`. ASCII-only text yields an empty list (pure TTF path).
	void buildGlyphs(const char *text, int fontId, int penColor, Common::Array<Roger::UiGlyph> &out);

	// Feeder B: upscale captured native generic regions (_genRegions) onto scene.
	// scene is the current frame's composite surface (RGBA32). picRect is the
	// overlay-space picture rect (computePictureRect). Clears _genRegions after use.
	// Returns true if it upscaled at least one generic region into `scene` this frame. Those
	// regions are outside the sprite-rect seed union, so a true result forces the scene-cache
	// copies to be full (otherwise the bounded copy would miss the freshly drawn region).
	bool drawGenericRegions(Graphics::ManagedSurface &scene, const Common::Rect &picRect);

	// Render a native SCI cel to a new RGBA surface. Caller owns and must free.
	// Returns nullptr on any failure (guard: sprite will be skipped).
	Graphics::Surface *renderNativeCel(int viewId, int loopNo, int celNo) const;

	Graphics::Surface *snapshotNativeRegion(const Common::Rect &nativeRect) const; // region -> RGBA surface
	void processForegroundCaptures(const Common::Array<Common::Rect> &liveSpriteRects); // _foregroundRegions -> _textSprites
	void clearTextSprites();                                                       // free celOverride + clear
	void flushGenericText();                                                       // emit _genTextPending into _uiLayer, deduped
};

} // namespace Sci

#endif // SCI_ROGER_FILE_ROGER_ART_PROVIDER_H
