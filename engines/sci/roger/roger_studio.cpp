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
#include "sci/roger/png_loader.h"

#ifdef ENABLE_SCI
#include "sci/sci.h"
#include "sci/resource/resource.h"
#include "sci/graphics/cache.h"
#include "sci/graphics/view.h"
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
	case kModeView:     renderViewMode(); break;
	case kModeCombined: renderCombinedMode(); break;
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

void RogerStudio::renderViewMode() {
	_status.clear();
	if (_viewIds.empty()) {
		_status = "no view resources found";
		markDirty();
		return;
	}
	const int viewId = _viewIds[_viewIdx];

#ifdef ENABLE_SCI
	if (g_sci && g_sci->_gfxCache) {
		GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
		if (view) {
			_loopNo = CLIP<int>(_loopNo, 0, MAX(0, (int)view->getLoopCount() - 1));
			_celNo = CLIP<int>(_celNo, 0, MAX(0, (int)view->getCelCount((int16)_loopNo) - 1));
		}
	}
#endif

	IndexImage cel;
	byte clearKey = 0;
	uint32 t0 = g_system->getMillis();
	if (!_gen.nativeCelIndexImage(viewId, _loopNo, _celNo, cel, clearKey)) {
		_status = Common::String::format("view %d l%d c%d: cel extraction FAILED", viewId, _loopNo, _celNo);
		markDirty();
		return;
	}

	// Render every variant, measure the grid.
	Common::Array<Graphics::Surface *> tiles;
	int tileW = 0, tileH = 0;
	for (int v = 0; v < kScalerCount; v++) {
		IndexImage scaled = applyScalerVariant(v, cel);
		Graphics::Surface *s = _gen.surfaceFromIndex(scaled, clearKey);
		tiles.push_back(s); // may be null; grid slot shows label only
		if (s) { tileW = MAX(tileW, (int)s->w); tileH = MAX(tileH, (int)s->h); }
	}
	_lastRenderMs = g_system->getMillis() - t0;
	if (tileW == 0) {
		_status = "all variants failed to render";
		markDirty();
		return;
	}

	// Compose: N columns of (tile + label strip), checkerboard behind alpha.
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	const int labelH = font ? font->getFontHeight() + 4 : 16;
	const int pad = 8;
	const Graphics::PixelFormat fmt(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::ManagedSurface grid((tileW + pad) * kScalerCount + pad, tileH + labelH + 2 * pad, fmt);
	grid.fillRect(Common::Rect(grid.w, grid.h), fmt.RGBToColor(40, 40, 40));
	for (int v = 0; v < kScalerCount; v++) {
		const int x0 = pad + v * (tileW + pad);
		// Checkerboard so transparency is visible.
		for (int cy = 0; cy < tileH; cy += 8)
			for (int cx = 0; cx < tileW; cx += 8)
				if (((cx / 8) ^ (cy / 8)) & 1)
					grid.fillRect(Common::Rect(x0 + cx, pad + cy,
						MIN(x0 + cx + 8, x0 + tileW), MIN(pad + cy + 8, pad + tileH)),
						fmt.RGBToColor(56, 56, 56));
		if (tiles[v])
			grid.blitFrom(*tiles[v], Common::Point(x0, pad));
		if (font)
			font->drawString(&grid, scalerVariantName(v), x0, pad + tileH + 2, tileW + pad,
			                 fmt.RGBToColor(255, 255, 255));
	}
	for (uint i = 0; i < tiles.size(); i++)
		if (tiles[i]) { tiles[i]->free(); delete tiles[i]; }

	// Hand the composed grid to setCurrent as a bare Surface copy.
	Graphics::Surface *composed = new Graphics::Surface();
	composed->copyFrom(grid.rawSurface());
	setCurrent(composed, Common::String::format("view %d loop %d cel %d - variant grid",
	                                            viewId, _loopNo, _celNo));
}

void RogerStudio::renderCombinedMode() {
	_status.clear();
	if (_picIds.empty() || _viewIds.empty()) {
		_status = "need at least one pic and one view";
		markDirty();
		return;
	}
	// Combined only accepts factor-6 variants (the plate is 6x).
	while (scalerVariantFactor(_variant) != 6)
		_variant = (_variant + 1) % kScalerCount;

	const int picId = _picIds[_picIdx];
	_gen.setEnhancePasses(_passes);
	_gen.setOmyacParams(_params);
	uint32 ms = 0;
	Graphics::Surface *plate = _gen.generatePlate(picId, ms);
	_lastRenderMs = ms;
	if (!plate) {
		_status = Common::String::format("pic %d: generation FAILED", picId);
		markDirty();
		return;
	}

	const int viewId = _viewIds[_viewIdx];
	IndexImage cel;
	byte clearKey = 0;
	Graphics::Surface *celSurf = nullptr;
	if (_gen.nativeCelIndexImage(viewId, _loopNo, _celNo, cel, clearKey)) {
		IndexImage scaled = applyScalerVariant(_variant, cel);
		celSurf = _gen.surfaceFromIndex(scaled, clearKey);
	}

	Graphics::ManagedSurface composed(plate->w, plate->h, plate->format);
	composed.blitFrom(*plate);
	plate->free(); delete plate;
	if (celSurf) {
		// Cel origin: top-left at (x,y)*6 for judging purposes (edge quality against
		// the plate, not game-exact anchoring). Alpha-aware blit so transparent pixels
		// (clearKey -> a=0 from surfaceFromIndex) don't stomp the background.
		composed.blendBlitFrom(*celSurf,
			Common::Rect(0, 0, celSurf->w, celSurf->h),
			Common::Rect(_spriteX * 6, _spriteY * 6,
				_spriteX * 6 + celSurf->w, _spriteY * 6 + celSurf->h),
			Graphics::FLIP_NONE);
		celSurf->free(); delete celSurf;
	} else {
		_status = "cel render failed; showing plate only";
	}

	Graphics::Surface *out = new Graphics::Surface();
	out->copyFrom(composed.rawSurface());
	setCurrent(out, Common::String::format("pic %d + view %d l%d c%d @(%d,%d) %s",
		picId, viewId, _loopNo, _celNo, _spriteX, _spriteY, scalerVariantName(_variant)));
}

void RogerStudio::exportCurrent() {
	if (!_current) {
		_status = "nothing to export";
		markDirty();
		return;
	}
	bool useDefault = false;
	Common::String dir;
	if (ConfMan.hasKey("screenshotpath"))
		dir = ConfMan.getPath("screenshotpath").toString('/');
	if (dir.empty()) {
		dir = "screenshots";
		useDefault = true;
	}
	if (!dir.empty() && dir.lastChar() != '/')
		dir += '/';
	Common::String detail, kind;
	int id = 0;
	if (_mode == kModePic) {
		kind = "pic";
		id = _picIds.empty() ? 0 : _picIds[_picIdx];
		detail = omyacParamStamp(_params) + "-" + omyacPassStamp(_passes);
	} else if (_mode == kModeView) {
		kind = "view";
		id = _viewIds.empty() ? 0 : _viewIds[_viewIdx];
		detail = Common::String::format("l%dc%d-grid", _loopNo, _celNo);
	} else {
		kind = "combo";
		id = _picIds.empty() ? 0 : _picIds[_picIdx];
		detail = Common::String::format("v%d-l%dc%d-%s",
			_viewIds.empty() ? 0 : _viewIds[_viewIdx], _loopNo, _celNo,
			scalerVariantName(_variant));
	}
	// Sanitize: keep [a-z0-9-], map everything else to '_'.
	Common::String safe;
	for (uint i = 0; i < detail.size(); i++) {
		const char c = detail[i];
		safe += (Common::isAlnum(c) || c == '-') ? c : '_';
	}
	const Common::String name = studioExportName(kind.c_str(), id, safe);
	const Common::String path = dir + name;
	if (Roger::dumpSurfacePng(*_current, path)) {
		if (useDefault)
			_status = "exported " + dir + name;
		else
			_status = "exported " + name;
	} else {
		_status = "export FAILED: cannot open " + name;
	}
	markDirty();
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
	case Common::KEYCODE_a:
		if (ev.kbd.flags & Common::KBD_SHIFT)
			break; // Shift+A = pic-mode insert-all-pass, handled there
		if (_previous) { _showPrevious = !_showPrevious; markDirty(); }
		else { _status = "no previous render to flip to"; markDirty(); }
		break;
	case Common::KEYCODE_p:
		if (_current) {
			if (_baseline) { _baseline->free(); delete _baseline; }
			_baseline = new Graphics::Surface();
			_baseline->copyFrom(*_current);
			_baselineLabel = _currentLabel;
			_status = "baseline pinned: " + _baselineLabel;
			markDirty();
		} else {
			_status = "nothing to pin (no current render)";
			markDirty();
		}
		break;
	case Common::KEYCODE_s:
		if (_baseline) { _split = !_split; markDirty(); }
		else { _status = "pin a baseline first (P)"; markDirty(); }
		break;
	case Common::KEYCODE_e:
		exportCurrent();
		break;
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

	// View-mode and Combined-mode shared keys (cel navigation; PgUp/PgDn gated to view only).
	if (_mode == kModeView || _mode == kModeCombined) {
		switch (ev.kbd.keycode) {
		case Common::KEYCODE_PAGEUP:
			if (_mode == kModeView) {
				if (_viewIds.empty()) break;
				_viewIdx = (_viewIdx + (int)_viewIds.size() - 1) % (int)_viewIds.size();
				_loopNo = _celNo = 0;
				rerender();
			}
			break;
		case Common::KEYCODE_PAGEDOWN:
			if (_mode == kModeView) {
				if (_viewIds.empty()) break;
				_viewIdx = (_viewIdx + 1) % (int)_viewIds.size();
				_loopNo = _celNo = 0;
				rerender();
			}
			break;
		case Common::KEYCODE_HOME: _loopNo = MAX(0, _loopNo - 1); _celNo = 0; rerender(); break;
		case Common::KEYCODE_END:  _loopNo = _loopNo + 1; _celNo = 0; rerender(); break; // clamped in render
		case Common::KEYCODE_COMMA:  _celNo = MAX(0, _celNo - 1); rerender(); break;
		case Common::KEYCODE_PERIOD: _celNo = _celNo + 1; rerender(); break; // clamped in render
		default: break;
		}
	}

	// Combined-mode keys: arrows move cel, V cycles 6x variant, PgUp/PgDn browse pics.
	if (_mode == kModeCombined) {
		const int d = (ev.kbd.flags & Common::KBD_SHIFT) ? 10 : 1;
		switch (ev.kbd.keycode) {
		case Common::KEYCODE_LEFT:  _spriteX = MAX(0, _spriteX - d); rerender(); break;
		case Common::KEYCODE_RIGHT: _spriteX = MIN(319, _spriteX + d); rerender(); break;
		case Common::KEYCODE_UP:    _spriteY = MAX(0, _spriteY - d); rerender(); break;
		case Common::KEYCODE_DOWN:  _spriteY = MIN(189, _spriteY + d); rerender(); break;
		case Common::KEYCODE_v:
			do { _variant = (_variant + 1) % kScalerCount; }
			while (scalerVariantFactor(_variant) != 6);
			rerender(); break;
		case Common::KEYCODE_PAGEUP:
			if (_picIds.empty()) break;
			_picIdx = (_picIdx + (int)_picIds.size() - 1) % (int)_picIds.size();
			rerender(); break;
		case Common::KEYCODE_PAGEDOWN:
			if (_picIds.empty()) break;
			_picIdx = (_picIdx + 1) % (int)_picIds.size();
			rerender(); break;
		default: break;
		}
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
	// When showing the previous render (A/B flip), display its label so the user
	// knows which version is on screen. The [PREV] prefix makes the state obvious.
	const Common::String headerLabel = (_showPrevious && _previous)
		? "[PREV] " + _previousLabel
		: _currentLabel;
	font->drawString(&small, Common::String::format(
		"[%s]  %s  render %ums   Tab=mode A=flip P=pin S=split E=export F1=keys Esc=quit",
		MODE_NAMES[_mode], headerLabel.c_str(), _lastRenderMs), 4, y, smallW - 8, fg);
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

	if (_mode == kModeView) {
		font->drawString(&small, Common::String::format(
			"view %d (%d/%u)  loop %d  cel %d   PgUp/PgDn view  Home/End loop  ,/. cel",
			_viewIds.empty() ? -1 : _viewIds[_viewIdx], _viewIdx + 1,
			(unsigned)_viewIds.size(), _loopNo, _celNo), 4, y, smallW - 8, fg);
		y += lh;
	}

	if (_mode == kModeCombined) {
		font->drawString(&small, Common::String::format(
			"cel @(%d,%d) variant %s   arrows move (Shift x10)  V variant  PgUp/PgDn pic",
			_spriteX, _spriteY, scalerVariantName(_variant)), 4, y, smallW - 8, fg);
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
			"Combined: V variant (6x only) | arrows move cel (Shift x10) | PgUp/PgDn pic",
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
