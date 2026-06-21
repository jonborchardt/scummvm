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

	bool hasBackground(GuiResourceId pictureId) const override;
	bool loadBuffers(GuiResourceId pictureId, GfxScreen *screen) override;
	void pushHiresBackground(GuiResourceId pictureId) override;
	void renderFromAnimateList(const AnimateList &list) override;
	void hideOverlayForUI() override;
	void onNativePicture() override;
	void toggleOverlay() override;   // Ctrl+Shift+U: upscaled overlay <-> original native
	void toggleDebugLog() override;  // Ctrl+Shift+L: per-frame Roger diagnostic logging

	// UI display-list capture (Roger hires dialogs) — see roger_art_provider.h.
	void uiPushWindow(const Common::Rect &globalRect, int backColor, int penColor,
	                  uint16 wndStyle, uint32 token) override;
	void uiPushText(const Common::Rect &globalRect, const char *text, int penColor,
	                int backColor, int fontId, int align, uint32 token) override;
	void uiPushButton(const Common::Rect &globalRect, const char *text, int fontId,
	                  int style, uint32 token) override;
	void uiPushTextEdit(const Common::Rect &globalRect, const char *text, int fontId,
	                    int style, int cursorPos, uint32 token) override;
	void uiPushIcon(const Common::Rect &globalRect, int viewId, int loopNo, int celNo,
	                uint32 token) override;
	void uiClearToken(uint32 token) override;
	void uiClearAll() override;

	// Compose and present the current room to the OSystem overlay.
	// Called each frame by the GfxAnimate hook (Task 7).
	void renderFrame(const Common::Array<Roger::Sprite> &sprites);

	// Accessor used by the GfxAnimate hook to translate AnimateEntry → Sprite.
	Roger::ViewCache *viewCache() { return _viewCache; }

	// Test-only accessors — expose private path helpers for white-box testing
	Common::String testVisualPath(GuiResourceId id) const { return visualPath(id); }
	Common::String testPriorityPath(GuiResourceId id) const { return priorityPath(id); }
	Common::String testControlPath(GuiResourceId id) const { return controlPath(id); }

private:
	Common::String _basePath;       // absolute path to <gameid>-roger/ directory
	Common::String _visualVariant;  // hires visual variant, e.g. "omyac-upscaler" ("" = plain pic.<id>.png)
	Common::String _priorityVariant; // EGA-color priority map variant for overlay occlusion (default "baseline-native")

	Roger::RogerCompositor *_compositor = nullptr;
	Roger::ViewCache *_viewCache = nullptr;
	Graphics::Surface *_plate = nullptr;
	int _loadedPicId = -1;
	bool _overlayActive = true;  // false = show native 320x200 (A/B comparison toggle)
	bool _debugLog = false;      // per-frame diagnostic logging
	bool _autoshot = false;      // roger_autoshot: dump the composited scene to PNG on room load (verification harness)
	int _autoshotPicId = -1;     // last pic id already auto-shot (so we dump once per room, not per frame)
	int _statusBarH = 10;        // SCI0 status/menu bar height in screen rows (of 200); reserved at the top of the game rect (may change)
	Common::Array<byte> _priorityMap; // screen-space SCI priority (from loadBuffers), for overlay occlusion

	// Roger hires UI/dialog compositing (see roger_ui_layer / roger_text).
	Roger::RogerUiLayer *_uiLayer = nullptr;
	Roger::RogerTextRenderer *_textRenderer = nullptr;
	Graphics::ManagedSurface *_sceneCache = nullptr; // last composed room+sprites (no UI)
	Common::Array<Graphics::Surface *> _uiIcons;     // owned native-cel surfaces for kUiIcon
	bool _haveScene = false;                         // _sceneCache valid this room
	Common::Rect _lastGameRect;                      // gameRect used for the cached scene
	void ensureUi();                                 // lazily build _uiLayer + _textRenderer
	void presentWithUi();                            // compose _sceneCache + _uiLayer -> overlay

	Common::String picDir(GuiResourceId id) const;
	Common::String visualPath(GuiResourceId id) const;
	Common::String priorityPath(GuiResourceId id) const;
	Common::String controlPath(GuiResourceId id) const;
	Common::String occlusionPriorityPath(GuiResourceId id) const; // EGA-color priority map for overlay occlusion
	// Load an EGA-color-encoded priority PNG into a band-per-pixel buffer (0..15).
	// Returns true and fills outBands/outW/outH on success.
	bool loadPriorityBands(const Common::String &path, Common::Array<byte> &outBands, int &outW, int &outH) const;

	// Render a native SCI cel to a new RGBA surface. Caller owns and must free.
	// Returns nullptr on any failure (guard: sprite will be skipped).
	Graphics::Surface *renderNativeCel(int viewId, int loopNo, int celNo) const;
};

} // namespace Sci

#endif // SCI_ROGER_FILE_ROGER_ART_PROVIDER_H
