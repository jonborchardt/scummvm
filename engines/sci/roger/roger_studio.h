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

#ifndef SCI_ROGER_ROGER_STUDIO_H
#define SCI_ROGER_ROGER_STUDIO_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_studio_render.h"

namespace Graphics { class ManagedSurface; struct Surface; }
namespace Common { struct Event; }

namespace Sci {
namespace Roger {

// Debug-only interactive tuning environment (ROGER_STUDIO=1 /
// build_and_run.ps1 -Studio). Owns the overlay; never touches the disk cache
// (own RogerAssetGen in kGenMemory with an empty cache dir). See
// docs/superpowers/specs/2026-07-02-roger-studio-design.md.
class RogerStudio {
public:
	explicit RogerStudio(const Common::String &gameId);
	~RogerStudio();

	// Blocking loop; returns when the user quits (Esc / window close).
	void run();

private:
	enum Mode { kModePic, kModeView, kModeCombined };

	// Frame / input
	void handleEvent(const Common::Event &ev);
	void drawFrame();               // compose _display + push to overlay
	void drawHud();
	void markDirty() { _dirty = true; }

	// Rendering (filled in by Tasks 5-8)
	void rerender();                // dispatch by _mode; updates _current/_previous
	void renderPicMode();
	void setCurrent(Graphics::Surface *s, const Common::String &label);

	static const int kHudH = 380;  // HUD strip height (2x-scaled; fits 2+omyacParamCount()+2 lines)

	RogerAssetGen        _gen;      // kGenMemory, empty cache dir
	Graphics::ManagedSurface *_display = nullptr; // overlay-format compose target

	Mode  _mode = kModePic;
	bool  _dirty = true;
	bool  _quit = false;
	bool  _showKeymap = false;

	// Current / previous / baseline renders (RGBA fmt(4,8,8,8,8,24,16,8,0)).
	Graphics::Surface *_current = nullptr;
	Graphics::Surface *_previous = nullptr;
	Graphics::Surface *_baseline = nullptr;
	Common::String _currentLabel, _previousLabel, _baselineLabel;
	bool _showPrevious = false;     // A/B flip
	bool _split = false;            // split view vs pinned baseline

	// View transform
	int _zoomIdx = 2;               // index into ZOOM_STEPS; 2 == 1.0
	int _panX = 0, _panY = 0;
	bool _dragging = false;
	int _dragX = 0, _dragY = 0;

	// Tuning state
	OmyacParams        _params;
	Common::Array<int> _passes;     // starts = defaultPasses()
	int _paramCursor = 0;
	int _passCursor = 0;

	// Resource browsing
	Common::Array<int> _picIds;     int _picIdx = 0;
	Common::Array<int> _viewIds;    int _viewIdx = 0;
	int _loopNo = 0, _celNo = 0;
	int _variant = kScaler6x;       // view-mode highlight / combined-mode active
	int _spriteX = 160, _spriteY = 120; // combined-mode cel position (native coords)

	uint32 _lastRenderMs = 0;
	Common::String _status;         // one-line HUD status / error line
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_STUDIO_H
