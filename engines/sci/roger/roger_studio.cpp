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

#include "sci/roger/roger_studio.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "graphics/managed_surface.h"

#ifdef ENABLE_SCI
#include "sci/sci.h"
#include "sci/resource/resource.h"
#endif

namespace Sci {
namespace Roger {

static const float ZOOM_STEPS[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
static const int ZOOM_COUNT = ARRAYSIZE(ZOOM_STEPS);

RogerStudio::RogerStudio(const Common::String &gameId)
	: _gen(gameId, "", kGenMemory) {
	_passes = defaultPasses();
#ifdef ENABLE_SCI
	if (g_sci && g_sci->getResMan()) {
		ResourceManager *resMan = g_sci->getResMan();
		Common::List<ResourceId> pics = resMan->listResources(kResourceTypePic);
		for (Common::List<ResourceId>::iterator it = pics.begin(); it != pics.end(); ++it)
			_picIds.push_back(it->getNumber());
		Common::sort(_picIds.begin(), _picIds.end());
		Common::List<ResourceId> views = resMan->listResources(kResourceTypeView);
		for (Common::List<ResourceId>::iterator it = views.begin(); it != views.end(); ++it)
			_viewIds.push_back(it->getNumber());
		Common::sort(_viewIds.begin(), _viewIds.end());
	}
#endif
}

RogerStudio::~RogerStudio() {
	if (_display) { _display->free(); delete _display; }
	if (_current) { _current->free(); delete _current; }
	if (_previous) { _previous->free(); delete _previous; }
	if (_baseline) { _baseline->free(); delete _baseline; }
}

void RogerStudio::setCurrent(Graphics::Surface *s, const Common::String &label) {
	if (_previous) { _previous->free(); delete _previous; }
	_previous = _current;
	_previousLabel = _currentLabel;
	_current = s;
	_currentLabel = label;
	_showPrevious = false;
	markDirty();
}

void RogerStudio::rerender() {
	switch (_mode) {
	case kModePic:      renderPicMode(); break;
	case kModeView:     /* Task 7 */ _status = "view mode: Task 7"; markDirty(); break;
	case kModeCombined: /* Task 8 */ _status = "combined mode: Task 8"; markDirty(); break;
	}
}

void RogerStudio::renderPicMode() {
	_status.clear();
	if (_picIds.empty()) {
		_status = "no pic resources found";
		markDirty();
		return;
	}
	const int picId = _picIds[_picIdx];
	_gen.setEnhancePasses(_passes);
	_gen.setOmyacParams(_params);
	uint32 ms = 0;
	Graphics::Surface *plate = _gen.generatePlate(picId, ms);
	_lastRenderMs = ms;
	if (!plate) {
		_status = Common::String::format("pic %d: generation FAILED (parse/render error)", picId);
		markDirty();
		return;
	}
	setCurrent(plate, Common::String::format("pic %d  %s  passes:%s", picId,
		omyacParamStamp(_params).c_str(), omyacPassStamp(_passes).c_str()));
}

void RogerStudio::run() {
	g_system->showOverlay(false);
	_display = new Graphics::ManagedSurface(
		g_system->getOverlayWidth(), g_system->getOverlayHeight(),
		g_system->getOverlayFormat());
	rerender();
	Common::EventManager *em = g_system->getEventManager();
	while (!_quit && !em->shouldQuit()) {
		Common::Event ev;
		while (em->pollEvent(ev))
			handleEvent(ev);
		if (_dirty) {
			drawFrame();
			_dirty = false;
		}
		g_system->delayMillis(10);
	}
}

void RogerStudio::handleEvent(const Common::Event &ev) {
	switch (ev.type) {
	case Common::EVENT_QUIT:
	case Common::EVENT_RETURN_TO_LAUNCHER:
		_quit = true;
		return;
	case Common::EVENT_KEYDOWN:
		break; // handled below
	case Common::EVENT_WHEELUP:
		_zoomIdx = MIN(_zoomIdx + 1, ZOOM_COUNT - 1); markDirty(); return;
	case Common::EVENT_WHEELDOWN:
		_zoomIdx = MAX(_zoomIdx - 1, 0); markDirty(); return;
	case Common::EVENT_LBUTTONDOWN:
		_dragging = true; _dragX = ev.mouse.x; _dragY = ev.mouse.y; return;
	case Common::EVENT_LBUTTONUP:
		_dragging = false; return;
	case Common::EVENT_MOUSEMOVE:
		if (_dragging) {
			_panX += ev.mouse.x - _dragX;
			_panY += ev.mouse.y - _dragY;
			_dragX = ev.mouse.x; _dragY = ev.mouse.y;
			markDirty();
		}
		return;
	default:
		return;
	}

	// Global keys (mode-specific keys are added by Tasks 5-8).
	switch (ev.kbd.keycode) {
	case Common::KEYCODE_ESCAPE:
		_quit = true; break;
	case Common::KEYCODE_TAB:
		_mode = (Mode)((_mode + 1) % 3);
		rerender();
		break;
	case Common::KEYCODE_F1:
		_showKeymap = !_showKeymap; markDirty(); break;
	case Common::KEYCODE_0:
		// plain 0 = reset view (global); Shift+0 = ')' is pic-mode reorder (below)
		if (!(ev.kbd.flags & Common::KBD_SHIFT)) {
			_zoomIdx = 2; _panX = _panY = 0; markDirty();
		}
		break;
	case Common::KEYCODE_PLUS:
	case Common::KEYCODE_EQUALS:
		_zoomIdx = MIN(_zoomIdx + 1, ZOOM_COUNT - 1); markDirty(); break;
	case Common::KEYCODE_MINUS:
		_zoomIdx = MAX(_zoomIdx - 1, 0); markDirty(); break;
	default:
		break;
	}

	// Pic-mode keys (after global; consumes only what global layer ignored).
	if (_mode == kModePic) {
		switch (ev.kbd.keycode) {
		case Common::KEYCODE_PAGEUP:
			if (_picIds.empty()) break;
			_picIdx = (_picIdx + (int)_picIds.size() - 1) % (int)_picIds.size();
			rerender(); break;
		case Common::KEYCODE_PAGEDOWN:
			if (_picIds.empty()) break;
			_picIdx = (_picIdx + 1) % (int)_picIds.size();
			rerender(); break;
		case Common::KEYCODE_UP:
			_paramCursor = (_paramCursor + omyacParamCount() - 1) % omyacParamCount();
			markDirty(); break;
		case Common::KEYCODE_DOWN:
			_paramCursor = (_paramCursor + 1) % omyacParamCount();
			markDirty(); break;
		case Common::KEYCODE_LEFT:
			omyacParamSet(_params, _paramCursor,
				omyacParamGet(_params, _paramCursor) - omyacParamDesc(_paramCursor).step);
			rerender(); break;
		case Common::KEYCODE_RIGHT:
			omyacParamSet(_params, _paramCursor,
				omyacParamGet(_params, _paramCursor) + omyacParamDesc(_paramCursor).step);
			rerender(); break;
		case Common::KEYCODE_LEFTBRACKET:
			if (_passCursor > 0) _passCursor--;
			markDirty(); break;
		case Common::KEYCODE_RIGHTBRACKET:
			if (_passCursor + 1 < (int)_passes.size()) _passCursor++;
			markDirty(); break;
		case Common::KEYCODE_f:
			_passes.insert_at(MIN(_passCursor, (int)_passes.size()), 2); rerender(); break;
		case Common::KEYCODE_l:
			_passes.insert_at(MIN(_passCursor, (int)_passes.size()), 1); rerender(); break;
		case Common::KEYCODE_a: // Shift+A inserts an 'all' pass; plain 'a' is the global A/B flip
			if (ev.kbd.flags & Common::KBD_SHIFT) {
				_passes.insert_at(MIN(_passCursor, (int)_passes.size()), 0);
				rerender();
			}
			break;
		case Common::KEYCODE_DELETE:
		case Common::KEYCODE_BACKSPACE:
			if (!_passes.empty() && _passCursor < (int)_passes.size()) {
				_passes.remove_at(_passCursor);
				if (_passCursor >= (int)_passes.size() && _passCursor > 0) _passCursor--;
				rerender();
			}
			break;
		case Common::KEYCODE_9: // Shift+9 = '(' moves pass left; plain 9 unused
			if ((ev.kbd.flags & Common::KBD_SHIFT) &&
			    _passCursor > 0 && _passCursor < (int)_passes.size()) {
				SWAP(_passes[_passCursor], _passes[_passCursor - 1]);
				_passCursor--; rerender();
			}
			break;
		case Common::KEYCODE_0: // Shift+0 = ')' moves pass right; plain 0 handled globally
			if ((ev.kbd.flags & Common::KBD_SHIFT) &&
			    _passCursor + 1 < (int)_passes.size()) {
				SWAP(_passes[_passCursor], _passes[_passCursor + 1]);
				_passCursor++; rerender();
			}
			break;
		case Common::KEYCODE_r: // reset params + passes to defaults
			_params = OmyacParams();
			_passes = defaultPasses();
			_passCursor = 0;
			rerender();
			break;
		default:
			break;
		}
	}
}

void RogerStudio::drawFrame() {
	const Graphics::PixelFormat fmt = _display->format;
	_display->fillRect(Common::Rect(_display->w, _display->h),
	                   fmt.RGBToColor(24, 24, 24));

	const Graphics::Surface *shown = _showPrevious ? _previous : _current;
	const int hudH = kHudH; // bottom HUD strip (2x-scaled text lives here)
	const Common::Rect imageArea(0, 0, _display->w, _display->h - hudH);

	if (shown) {
		const float zoom = ZOOM_STEPS[_zoomIdx];
		// Destination rect of the (zoomed, panned) image; blitFrom scales + converts.
		if (!_split || !_baseline) {
			Common::Rect dst(_panX, _panY,
			                 _panX + (int)(shown->w * zoom), _panY + (int)(shown->h * zoom));
			dst.clip(imageArea);
			if (!dst.isEmpty()) {
				// Map the visible dst back to the src region.
				Common::Rect src((int)((dst.left - _panX) / zoom), (int)((dst.top - _panY) / zoom),
				                 (int)((dst.right - _panX) / zoom), (int)((dst.bottom - _panY) / zoom));
				src.clip(Common::Rect(shown->w, shown->h));
				if (!src.isEmpty())
					_display->blitFrom(*shown, src, dst);
			}
		} else {
			// Split: current left, baseline right, same zoom/pan each side.
			const int halfW = imageArea.width() / 2;
			const Common::Rect leftArea(0, 0, halfW, imageArea.bottom);
			const Common::Rect rightArea(halfW, 0, imageArea.right, imageArea.bottom);
			const Graphics::Surface *sides[2] = { _current, _baseline };
			const Common::Rect areas[2] = { leftArea, rightArea };
			for (int i = 0; i < 2; i++) {
				if (!sides[i])
					continue;
				Common::Rect dst(areas[i].left + _panX, areas[i].top + _panY,
				                 areas[i].left + _panX + (int)(sides[i]->w * zoom),
				                 areas[i].top + _panY + (int)(sides[i]->h * zoom));
				dst.clip(areas[i]);
				if (dst.isEmpty())
					continue;
				Common::Rect src((int)((dst.left - areas[i].left - _panX) / zoom),
				                 (int)((dst.top - areas[i].top - _panY) / zoom),
				                 (int)((dst.right - areas[i].left - _panX) / zoom),
				                 (int)((dst.bottom - areas[i].top - _panY) / zoom));
				src.clip(Common::Rect(sides[i]->w, sides[i]->h));
				if (!src.isEmpty())
					_display->blitFrom(*sides[i], src, dst);
			}
			_display->vLine(halfW, 0, imageArea.bottom, fmt.RGBToColor(255, 255, 255));
		}
	}

	drawHud();

	g_system->copyRectToOverlay(_display->getPixels(), _display->pitch,
	                            0, 0, _display->w, _display->h);
	g_system->updateScreen();
}

void RogerStudio::drawHud() {
	// Render HUD text small, then blit 2x so it is readable at hires overlays.
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (!font) {
		static bool warned = false;
		if (!warned) {
			warned = true;
			warning("RogerStudio: kBigGUIFont unavailable, HUD disabled");
		}
		return;
	}
	const int hudH = kHudH;
	const int smallW = _display->w / 2, smallH = hudH / 2;
	Graphics::ManagedSurface small(smallW, smallH, _display->format);
	const uint32 bg = _display->format.RGBToColor(0, 0, 0);
	const uint32 fg = _display->format.RGBToColor(220, 220, 220);
	const uint32 hi = _display->format.RGBToColor(255, 255, 0);
	small.fillRect(Common::Rect(smallW, smallH), bg);

	static const char *MODE_NAMES[] = { "PIC", "VIEW", "COMBINED" };
	int y = 2;
	const int lh = font->getFontHeight() + 2;
	small.frameRect(Common::Rect(smallW, smallH), fg);
	font->drawString(&small, Common::String::format(
		"[%s]  %s  render %ums   Tab=mode A=flip P=pin S=split E=export F1=keys Esc=quit",
		MODE_NAMES[_mode], _currentLabel.c_str(), _lastRenderMs), 4, y, smallW - 8, fg);
	y += lh;
	if (!_status.empty()) {
		font->drawString(&small, _status, 4, y, smallW - 8, hi);
		y += lh;
	}
	// Mode-specific HUD lines (Tasks 5-8).
	if (_mode == kModePic) {
		for (int i = 0; i < omyacParamCount(); i++) {
			const OmyacParamDesc d = omyacParamDesc(i);
			Common::String line = Common::String::format("%c %-20s %d",
				i == _paramCursor ? '>' : ' ', d.name, omyacParamGet(_params, i));
			font->drawString(&small, line, 4, y, smallW - 8,
			                 i == _paramCursor ? hi : fg);
			y += lh;
		}
		// Pass strip with cursor: "passes: f f f [l] f f a a a a"
		Common::String strip = "passes: ";
		for (uint i = 0; i < _passes.size(); i++) {
			const char c = _passes[i] == 2 ? 'f' : _passes[i] == 1 ? 'l' : 'a';
			if ((int)i == _passCursor)
				strip += Common::String::format("[%c] ", c);
			else
				strip += Common::String::format("%c ", c);
		}
		if (_passes.empty())
			strip += "(none - wireframe)";
		font->drawString(&small, strip, 4, y, smallW - 8, fg);
		y += lh;
	}

	if (_showKeymap) {
		// Full keymap block (kept current as later tasks add keys).
		static const char *KEYS[] = {
			"Global: Tab mode | A flip prev | P pin baseline | S split | E export PNG",
			"        +/-/wheel zoom | drag pan | 0 reset view | F1 this help | Esc quit",
			"Pic:    PgUp/PgDn pic | Up/Dn param | Lt/Rt adjust | [ ] pass cursor",
			"        f/l insert pass, Shift+A insert all | Del remove | Shift+9/0 reorder | R reset",
			"View:   PgUp/PgDn view | Home/End loop | ,/. cel",
			"Combined: V variant (6x only) | arrows move cel (Shift x10)",
		};
		for (uint i = 0; i < ARRAYSIZE(KEYS); i++) {
			font->drawString(&small, KEYS[i], 4, y, smallW - 8, fg);
			y += lh;
		}
	}

	const Common::Rect srcR(0, 0, smallW, smallH);
	const Common::Rect dstR(0, _display->h - hudH, _display->w, _display->h);
	_display->blitFrom(small.rawSurface(), srcR, dstR);
}

} // namespace Roger
} // namespace Sci
