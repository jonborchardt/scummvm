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
	for (int i = 0; i < 2; i++) {
		if (_slots[i].render) { _slots[i].render->free(); delete _slots[i].render; }
		if (_slots[i].plateCache) { _slots[i].plateCache->free(); delete _slots[i].plateCache; }
	}
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

	// Plate cache: regenerate the (expensive) omyac plate only when plateStale.
	// A cel place/drag or loop/cel/view/showView change reuses the cached plate and
	// only recomposites the cel over it — no omyac regen at drag rate.
	if (slot.plateStale || !slot.plateCache) {
		if (slot.plateCache) { slot.plateCache->free(); delete slot.plateCache; slot.plateCache = nullptr; }
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
		slot.plateCache = plate;
		slot.renderMs = ms;
		slot.plateStale = false;
	}

	Graphics::ManagedSurface composed(slot.plateCache->w, slot.plateCache->h, slot.plateCache->format);
	composed.blitFrom(*slot.plateCache);

	_celW = _celH = 0;
	if (_showView && !_viewIds.empty()) {
		IndexImage cel;
		byte clearKey = 0;
		if (_gen.nativeCelIndexImage(_viewIds[_viewIdx], _loopNo, _celNo, cel, clearKey)) {
			IndexImage scaled = applyScalerVariant(slot.variant, cel);
			Graphics::Surface *celSurf = _gen.surfaceFromIndex(scaled, clearKey);
			if (celSurf) {
				// Bottom-centre anchor at (_celX, _celY) native.
				_celW = celSurf->w; _celH = celSurf->h;
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
	case Common::EVENT_MOUSEMOVE: {
		// ev.mouse is in game space (0-320, 0-200); scale to overlay px.
		const int ox = ev.mouse.x * _display->w / 320;
		const int oy = ev.mouse.y * _display->h / 200;
		const int panelTop = _display->h - kPanelH;
		uint32 h = 0;
		if (oy >= panelTop)
			h = hitTestWidgets(_widgets, ox / 2, (oy - panelTop) / 2);
		if (h != _hoverWid) { _hoverWid = h; markDirty(); }
		// Scene-area cel-drag / pan (overlay px throughout).
		if (_draggingCel) {
			int nx, ny;
			if (displayToNative(ox, oy, nx, ny)) {
				if (nx != _celX || ny != _celY) {
					_celX = nx; _celY = ny;
					invalidateCelOnly();
				}
			}
		} else if (_panning) {
			_panX += ox - _dragLastX;
			_panY += oy - _dragLastY;
			_dragLastX = ox; _dragLastY = oy;
			markDirty();
		}
		return;
	}
	case Common::EVENT_LBUTTONDOWN: {
		// ev.mouse is in game space (0-320, 0-200); scale to overlay px.
		const int ox = ev.mouse.x * _display->w / 320;
		const int oy = ev.mouse.y * _display->h / 200;
		const int panelTop = _display->h - kPanelH;
		if (oy >= panelTop) {
			const uint32 id = hitTestWidgets(_widgets, ox / 2, (oy - panelTop) / 2);
			if (id != (uint32)kWidNone)
				dispatchWidget(id);
			return;
		}
		// Scene-area click: place the cel (bottom-centre at the click) and begin drag.
		int nx, ny;
		if (displayToNative(ox, oy, nx, ny)) {
			if (_showView) {
				_celX = CLIP(nx, 0, 319);
				_celY = CLIP(ny, 0, 189);
				_draggingCel = true;
				invalidateCelOnly();
			}
		}
		return;
	}
	case Common::EVENT_LBUTTONUP:
		_draggingCel = false;
		return;
	case Common::EVENT_RBUTTONDOWN: {
		const int ox = ev.mouse.x * _display->w / 320;
		const int oy = ev.mouse.y * _display->h / 200;
		_panning = true; _dragLastX = ox; _dragLastY = oy;
		return;
	}
	case Common::EVENT_RBUTTONUP:
		_panning = false;
		return;
	case Common::EVENT_WHEELUP:
	case Common::EVENT_WHEELDOWN: {
		const int ox = ev.mouse.x * _display->w / 320;
		const int oy = ev.mouse.y * _display->h / 200;
		const Common::Rect area = sceneArea();
		if (!area.contains((int16)ox, (int16)oy))
			return;
		const float oldScale = _viewScale;
		_viewScale = CLIP(_viewScale * (ev.type == Common::EVENT_WHEELUP ? 1.25f : 0.8f),
		                  _fitScale * 0.5f, 8.0f);
		// Keep the plate point under the cursor fixed.
		const float k = _viewScale / oldScale;
		_panX = (int)(ox - area.left - k * (ox - area.left - _panX));
		_panY = (int)(oy - area.top - k * (oy - area.top - _panY));
		markDirty();
		return;
	}
	default:
		return;
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

		// Record the last-drawn cel rect in _display coords (for hover/drag/debug).
		// Bottom-centre anchor at (_celX*6, _celY*6) in plate space; cel spans
		// _celW x _celH plate px. Map plate -> display via (_panX/_panY, scale).
		if (_showView && _celW > 0 && _celH > 0) {
			const int px = _celX * 6 - _celW / 2; // plate-space top-left
			const int py = _celY * 6 - _celH;
			const int dl = area.left + _panX + (int)(px * scale);
			const int dt = area.top + _panY + (int)(py * scale);
			const int dr = area.left + _panX + (int)((px + _celW) * scale);
			const int db = area.top + _panY + (int)((py + _celH) * scale);
			_celScreenRect = Common::Rect(dl, dt, dr, db);
		}
	}

	drawPanel();

	g_system->copyRectToOverlay(_display->getPixels(), _display->pitch,
	                            0, 0, _display->w, _display->h);
	g_system->updateScreen();
}

void RogerStudio::drawPanel() {
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (!font) {
		static bool warned = false;
		if (!warned) {
			warned = true;
			warning("RogerStudio: kBigGUIFont unavailable, panel text disabled");
		}
		return;
	}
	const int smallW = _display->w / 2, smallH = kPanelH / 2;
	Graphics::ManagedSurface small(smallW, smallH, _display->format);
	const uint32 bg = _display->format.RGBToColor(0, 0, 0);
	const uint32 fg = _display->format.RGBToColor(220, 220, 220);
	const uint32 hi = _display->format.RGBToColor(255, 255, 0);
	const uint32 hov = _display->format.RGBToColor(90, 90, 140);
	small.fillRect(Common::Rect(smallW, smallH), bg);

	// Rebuild widgets from current state (cheap; keeps rects in lockstep with state).
	StudioPanelState st;
	const Slot &s = _slots[_activeSlot];
	st.picId = _picIds.empty() ? -1 : _picIds[_picIdx];
	st.viewId = _viewIds.empty() ? -1 : _viewIds[_viewIdx];
	st.loopNo = _loopNo; st.celNo = _celNo;
	st.variantName = scalerVariantName(s.variant);
	st.plateNearest = s.plateMode == kPlateNearestRef;
	st.showView = _showView;
	st.activeSlot = _activeSlot;
	st.displayMode = _displayMode;
	st.selectedChip = _selectedChip;
	st.passes = s.passes;
	for (int i = 0; i < omyacParamCount(); i++)
		st.paramValues.push_back(omyacParamGet(s.params, i));
	buildStudioPanel(Common::Rect(0, 0, smallW, smallH - kStudioRowH), st, _widgets);

	for (uint i = 0; i < _widgets.size(); i++) {
		const StudioWidget &wg = _widgets[i];
		if (wg.enabled) {
			if (wg.id == _hoverWid)
				small.fillRect(wg.rect, hov);
			small.frameRect(wg.rect, wg.on ? hi : fg);
		}
		font->drawString(&small, wg.label, wg.rect.left + 4, wg.rect.top + 2,
		                 wg.rect.width() - 6, wg.on ? hi : fg);
	}

	// Bottom line: render ms + status + (Task 8) offset readout.
	font->drawString(&small, Common::String::format("%ums  %s  %s",
		s.renderMs, _status.c_str(), _offsetReadout.c_str()),
		4, smallH - kStudioRowH + 4, smallW - 8, hi);

	const Common::Rect srcR(0, 0, smallW, smallH);
	const Common::Rect dstR(0, _display->h - kPanelH, _display->w, _display->h);
	_display->blitFrom(small.rawSurface(), srcR, dstR);
}

void RogerStudio::dispatchWidget(uint32 id) {
	Slot &s = activeSlot();
	const int kind = widKind(id), idx = widIndex(id);
	switch (kind) {
	case kWidPicPrev: case kWidPicNext:
		if (_picIds.empty()) break;
		_picIdx = (_picIdx + (kind == kWidPicPrev ? (int)_picIds.size() - 1 : 1)) % (int)_picIds.size();
		invalidateScene(); break;
	case kWidViewPrev: case kWidViewNext:
		if (_viewIds.empty()) break;
		_viewIdx = (_viewIdx + (kind == kWidViewPrev ? (int)_viewIds.size() - 1 : 1)) % (int)_viewIds.size();
		_loopNo = _celNo = 0;
		invalidateCelOnly(); break; // view swap changes only the cel, not the plate
	case kWidLoopPrev: _loopNo = MAX(0, _loopNo - 1); _celNo = 0; invalidateCelOnly(); break;
	case kWidLoopNext: _loopNo++; _celNo = 0; invalidateCelOnly(); break; // clamped in renderSlot
	case kWidCelPrev: _celNo = MAX(0, _celNo - 1); invalidateCelOnly(); break;
	case kWidCelNext: _celNo++; invalidateCelOnly(); break;              // clamped in renderSlot
	case kWidVariantCycle:
		do { s.variant = (s.variant + 1) % kScalerCount; }
		while (scalerVariantFactor(s.variant) != 6);
		invalidateActive(); break;
	case kWidPlateMode:
		s.plateMode = (s.plateMode == kPlateOmyac) ? kPlateNearestRef : kPlateOmyac;
		invalidateActive(); break;
	case kWidShowView: _showView = !_showView; invalidateCelOnly(); break;
	case kWidFit: fitView(); break;
	case kWidTabA: _activeSlot = 0; _selectedChip = -1; markDirty(); break;
	case kWidTabB: _activeSlot = 1; _selectedChip = -1; markDirty(); break;
	case kWidShowA: _displayMode = kShowA; markDirty(); break;
	case kWidShowB: _displayMode = kShowB; markDirty(); break;
	case kWidSplit: _displayMode = kShowSplit; markDirty(); break;
	case kWidDiff: _displayMode = kShowDiff; markDirty(); break;
	case kWidCopyAB: {
		Slot &b = _slots[1];
		b.params = _slots[0].params; b.passes = _slots[0].passes;
		b.variant = _slots[0].variant; b.plateMode = _slots[0].plateMode;
		b.stale = b.plateStale = true; // settings differ -> plate genuinely changes
		_status = "copied A settings to B";
		markDirty(); break;
	}
	case kWidExport: exportShown(); break;
	case kWidParamMinus:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) - omyacParamDesc(idx).step);
		invalidateActive(); break;
	case kWidParamPlus:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) + omyacParamDesc(idx).step);
		invalidateActive(); break;
	case kWidParamToggle:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) ? 0 : 1);
		invalidateActive(); break;
	case kWidChip: _selectedChip = idx; markDirty(); break;
	case kWidChipX: { int sel = idx; passRemoveAt(s.passes, sel); _selectedChip = sel; invalidateActive(); break; }
	case kWidChipLeft:  if (passMove(s.passes, _selectedChip, -1)) invalidateActive(); break;
	case kWidChipRight: if (passMove(s.passes, _selectedChip, +1)) invalidateActive(); break;
	case kWidChipAddF: passInsertAfter(s.passes, _selectedChip, 2); invalidateActive(); break;
	case kWidChipAddL: passInsertAfter(s.passes, _selectedChip, 1); invalidateActive(); break;
	case kWidChipAddA: passInsertAfter(s.passes, _selectedChip, 0); invalidateActive(); break;
	case kWidChipReset:
		s.passes = defaultPasses(); _selectedChip = -1; invalidateActive(); break;
	default: break;
	}
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
