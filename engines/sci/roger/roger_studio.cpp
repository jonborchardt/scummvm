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

RogerStudio::RogerStudio(const Common::String &gameId)
	: _gen(gameId, "", kGenMemory) {
	_slots[0].passes = defaultPasses();
	_slots[1].passes = defaultPasses();
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
	const StudioDefaults d = studioDefaultsForGame(gameId);
	_celX = d.celX; _celY = d.celY;
	if (d.picId >= 0)
		for (uint i = 0; i < _picIds.size(); i++)
			if (_picIds[i] == d.picId) { _picIdx = (int)i; break; }
	if (d.viewId >= 0)
		for (uint i = 0; i < _viewIds.size(); i++)
			if (_viewIds[i] == d.viewId) { _viewIdx = (int)i; _loopNo = d.loopNo; _celNo = d.celNo; break; }
}

RogerStudio::~RogerStudio() {
	if (_display) { _display->free(); delete _display; }
	for (int i = 0; i < 2; i++)
		if (_slots[i].render) { _slots[i].render->free(); delete _slots[i].render; }
}

void RogerStudio::renderSlot(Slot &slot) {
	slot.stale = false;
	if (slot.render) { slot.render->free(); delete slot.render; slot.render = nullptr; }
	if (_picIds.empty()) { _status = "no pic resources"; markDirty(); return; }

	// Clamp loop/cel against real counts before extraction (v1 renderViewMode block).
#ifdef ENABLE_SCI
	if (_showView && !_viewIds.empty() && g_sci && g_sci->_gfxCache) {
		GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)_viewIds[_viewIdx]);
		if (view) {
			_loopNo = CLIP<int>(_loopNo, 0, MAX(0, (int)view->getLoopCount() - 1));
			_celNo = CLIP<int>(_celNo, 0, MAX(0, (int)view->getCelCount((int16)_loopNo) - 1));
		}
	}
#endif

	_gen.setEnhancePasses(slot.passes);
	_gen.setOmyacParams(slot.params);
	uint32 ms = 0;
	Graphics::Surface *plate = (slot.plateMode == kPlateNearestRef)
		? _gen.generatePlateNearest(_picIds[_picIdx], ms)
		: _gen.generatePlate(_picIds[_picIdx], ms);
	if (!plate) {
		_status = Common::String::format("pic %d: generation FAILED", _picIds[_picIdx]);
		markDirty();
		return;
	}

	Graphics::ManagedSurface composed(plate->w, plate->h, plate->format);
	composed.blitFrom(*plate);
	plate->free(); delete plate;

	if (_showView && !_viewIds.empty()) {
		IndexImage cel;
		byte clearKey = 0;
		if (_gen.nativeCelIndexImage(_viewIds[_viewIdx], _loopNo, _celNo, cel, clearKey)) {
			IndexImage scaled = applyScalerVariant(slot.variant, cel);
			Graphics::Surface *celSurf = _gen.surfaceFromIndex(scaled, clearKey);
			if (celSurf) {
				// Bottom-centre anchor at (_celX, _celY) native.
				const int dx = _celX * 6 - celSurf->w / 2;
				const int dy = _celY * 6 - celSurf->h;
				composed.blendBlitFrom(*celSurf,
					Common::Rect(0, 0, celSurf->w, celSurf->h),
					Common::Rect(dx, dy, dx + celSurf->w, dy + celSurf->h),
					Graphics::FLIP_NONE);
				celSurf->free(); delete celSurf;
			} else {
				_status = "cel render failed; plate only";
			}
		} else {
			_status = "cel extraction failed; plate only";
		}
	}

	slot.render = new Graphics::Surface();
	slot.render->copyFrom(composed.rawSurface());
	slot.renderMs = ms;
	markDirty();
}

Common::String RogerStudio::slotStamp(const Slot &slot) const {
	Common::String s = omyacParamStamp(slot.params) + "-" + omyacPassStamp(slot.passes);
	if (slot.plateMode == kPlateNearestRef)
		s += "-nref";
	return s;
}

Common::Rect RogerStudio::sceneArea() const {
	return Common::Rect(0, 0, _display->w, _display->h - kPanelH);
}

void RogerStudio::fitView() {
	const Common::Rect area = sceneArea();
	const float sx = (float)area.width() / (float)OMYAC_HYBRID_W;
	const float sy = (float)area.height() / (float)OMYAC_HYBRID_H;
	_fitScale = MIN(sx, sy);
	_viewScale = _fitScale;
	_panX = (area.width() - (int)(OMYAC_HYBRID_W * _viewScale)) / 2;
	_panY = (area.height() - (int)(OMYAC_HYBRID_H * _viewScale)) / 2;
	markDirty();
}

bool RogerStudio::displayToNative(int mx, int my, int &nx, int &ny) const {
	const Common::Rect area = sceneArea();
	if (!area.contains((int16)mx, (int16)my))
		return false;
	const float px = (mx - area.left - _panX) / _viewScale; // plate coords
	const float py = (my - area.top - _panY) / _viewScale;
	nx = (int)(px / 6.0f); ny = (int)(py / 6.0f);
	if (nx < 0 || nx > 319 || ny < 0 || ny > 189)
		return false;
	return true;
}

void RogerStudio::run() {
	g_system->showOverlay(false);
	_display = new Graphics::ManagedSurface(
		g_system->getOverlayWidth(), g_system->getOverlayHeight(),
		g_system->getOverlayFormat());
	fitView();
	ensureFresh(_slots[0]);
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
	default:
		return; // mouse handling arrives in Tasks 6-7
	}

	switch (ev.kbd.keycode) {
	case Common::KEYCODE_ESCAPE:
		_quit = true;
		break;
	case Common::KEYCODE_e:
		exportShown();
		break;
	default:
		break;
	}
}

void RogerStudio::drawFrame() {
	const Graphics::PixelFormat fmt = _display->format;
	_display->fillRect(Common::Rect(_display->w, _display->h),
	                   fmt.RGBToColor(48, 48, 48));

	// Split/Diff are treated as A until Task 8 fills them in.
	Slot &slot = (_displayMode == kShowB) ? _slots[1] : _slots[0];
	ensureFresh(slot);

	const Common::Rect area = sceneArea();
	_celScreenRect = Common::Rect();
	if (slot.render) {
		const float scale = _viewScale;
		// Destination rect of the (scaled, panned) plate within the scene area;
		// blitFrom scales + converts. (v1 drawFrame src/dst mapping math.)
		Common::Rect dst(area.left + _panX, area.top + _panY,
		                 area.left + _panX + (int)(slot.render->w * scale),
		                 area.top + _panY + (int)(slot.render->h * scale));
		dst.clip(area);
		if (!dst.isEmpty()) {
			Common::Rect src((int)((dst.left - area.left - _panX) / scale),
			                 (int)((dst.top - area.top - _panY) / scale),
			                 (int)((dst.right - area.left - _panX) / scale),
			                 (int)((dst.bottom - area.top - _panY) / scale));
			src.clip(Common::Rect(slot.render->w, slot.render->h));
			if (!src.isEmpty())
				_display->blitFrom(*slot.render, src, dst);
		}

		// Record the last-drawn cel rect in _display coords (for Task 7 drag hit).
		if (_showView) {
			// Plate-space bottom-centre anchor at (_celX*6, _celY*6); the exact cel
			// dimensions are re-derived per render, so record the anchor point only.
			const int ax = area.left + _panX + (int)(_celX * 6 * scale);
			const int ay = area.top + _panY + (int)(_celY * 6 * scale);
			_celScreenRect = Common::Rect(ax, ay, ax, ay);
		}
	}

	drawPanel();

	g_system->copyRectToOverlay(_display->getPixels(), _display->pitch,
	                            0, 0, _display->w, _display->h);
	g_system->updateScreen();
}

void RogerStudio::drawPanel() {
	// Task 5 stub: fill the panel strip black, draw the status line. Task 6 replaces
	// this with the widget layer. Text is rendered into a small surface then blitted
	// 2x so it is legible on the hires overlay (v1 drawHud small-font-then-2x pattern).
	const int panelH = kPanelH;
	const Common::Rect panelDst(0, _display->h - panelH, _display->w, _display->h);
	const Graphics::PixelFormat fmt = _display->format;
	_display->fillRect(panelDst, fmt.RGBToColor(0, 0, 0));

	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (!font) {
		static bool warned = false;
		if (!warned) {
			warned = true;
			warning("RogerStudio: kBigGUIFont unavailable, panel text disabled");
		}
		return;
	}

	const int smallW = _display->w / 2, smallH = panelH / 2;
	Graphics::ManagedSurface small(smallW, smallH, _display->format);
	const uint32 bg = _display->format.RGBToColor(0, 0, 0);
	const uint32 fg = _display->format.RGBToColor(220, 220, 220);
	small.fillRect(Common::Rect(smallW, smallH), bg);
	small.frameRect(Common::Rect(smallW, smallH), fg);

	const int lh = font->getFontHeight() + 2;
	int y = 2;
	Slot &slot = (_displayMode == kShowB) ? _slots[1] : _slots[0];
	font->drawString(&small, Common::String::format(
		"pic %d  view %d l%d c%d  slot %c  %s  render %ums   E=export Esc=quit",
		_picIds.empty() ? -1 : _picIds[_picIdx],
		_viewIds.empty() ? -1 : _viewIds[_viewIdx], _loopNo, _celNo,
		_activeSlot == 0 ? 'A' : 'B', slotStamp(slot).c_str(), slot.renderMs),
		4, y, smallW - 8, fg);
	y += lh;
	if (!_status.empty()) {
		const uint32 hi = _display->format.RGBToColor(255, 255, 0);
		font->drawString(&small, _status, 4, y, smallW - 8, hi);
		y += lh;
	}

	const Common::Rect srcR(0, 0, smallW, smallH);
	_display->blitFrom(small.rawSurface(), srcR, panelDst);
}

void RogerStudio::dispatchWidget(uint32 id) {
	// Task 6 wires the widget layer; nothing dispatched in Task 5.
	(void)id;
}

void RogerStudio::exportShown() {
	// Task 5 minimal version: export the shown slot (A or B; Split/Diff treated as A
	// until Task 8). v1 exportCurrent's sanitize + screenshotpath/dir-fallback logic
	// is preserved verbatim.
	const char slotChar = (_displayMode == kShowB) ? 'B' : 'A';
	Slot &slot = (_displayMode == kShowB) ? _slots[1] : _slots[0];
	ensureFresh(slot);
	if (!slot.render) {
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

	// Sanitize: keep [a-z0-9-], map everything else to '_'.
	const Common::String detail = slotStamp(slot);
	Common::String safe;
	for (uint i = 0; i < detail.size(); i++) {
		const char c = detail[i];
		safe += (Common::isAlnum(c) || c == '-') ? c : '_';
	}
	const int picId = _picIds.empty() ? 0 : _picIds[_picIdx];
	const Common::String name = studioSceneExportName(picId, slotChar, safe);
	const Common::String path = dir + name;
	if (Roger::dumpSurfacePng(*slot.render, path)) {
		if (useDefault)
			_status = "exported " + dir + name;
		else
			_status = "exported " + name;
	} else {
		_status = "export FAILED: cannot open " + name;
	}
	markDirty();
}

} // namespace Roger
} // namespace Sci
