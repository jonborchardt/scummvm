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
#include "sci/roger/roger_ui_layer.h"
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
	void pushHiresBackground(GuiResourceId pictureId) override;
	void renderFromAnimateList(const AnimateList &list) override;
	void onNativePicture() override;
	void onMouseMoved() override;
	void onDrawCel(const Common::Rect &globalRect, int viewId, int loopNo, int celNo) override;
	void onTransition(int sciType, const Common::Rect &picRect) override;
	void onShake(int shakeCount, int directions) override;
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

	// Compose and present the current room to the OSystem overlay.
	// Called each frame by the GfxAnimate hook (Task 7).
	void renderFrame(const Common::Array<Roger::Sprite> &sprites);

	// Accessor used by the GfxAnimate hook to translate AnimateEntry → Sprite.
	Roger::ViewCache *viewCache() { return _viewCache; }

private:
	Common::String _basePath;       // absolute path to <gameid>-roger/ directory

	Roger::RogerAssetGen *_assetGen = nullptr;
	Roger::RogerCompositor *_compositor = nullptr;
	Roger::ViewCache *_viewCache = nullptr;
	Graphics::Surface *_plate = nullptr;
	int _loadedPicId = -1;
	int _bodyFontIdx = -1; // index into the body-font shortlist (-1 = config/default font)
	bool _overlayActive = true;  // false = show native 320x200 (A/B comparison toggle)
	bool _debugLog = false;      // per-frame diagnostic logging
	bool _autoshot = false;      // roger_autoshot: dump the composited scene to PNG on room load (verification harness)
	bool _useHwCursor = false;   // roger_hw_cursor: try the native HW cursor over the overlay (invisible in practice); default false = composited arrow
	int _autoshotPicId = -1;     // last pic id already auto-shot (so we dump once per room, not per frame)
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
	void ensureCursor();                             // build _cursorSurf once
	void compositeCursor(Graphics::ManagedSurface &scene, const Common::Rect &gameRect);
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

	// Render a native SCI cel to a new RGBA surface. Caller owns and must free.
	// Returns nullptr on any failure (guard: sprite will be skipped).
	Graphics::Surface *renderNativeCel(int viewId, int loopNo, int celNo) const;
};

} // namespace Sci

#endif // SCI_ROGER_FILE_ROGER_ART_PROVIDER_H
