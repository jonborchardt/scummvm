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
#include "graphics/cursorman.h"
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

// Sanitize an export stamp: keep [a-z0-9-], map everything else to '_'.
static Common::String sanitize(const Common::String &detail) {
	Common::String safe;
	for (uint i = 0; i < detail.size(); i++) {
		const char c = detail[i];
		safe += (Common::isAlnum(c) || c == '-') ? c : '_';
	}
	return safe;
}

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
	if (_diffSurf) { _diffSurf->free(); delete _diffSurf; }
}

void RogerStudio::renderSlot(Slot &slot) {
	_status.clear(); // Important 1: transient status ("FAILED"/"copied A->B") must not persist
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
		slot.backfillMask.clear();
		_gen.setEnhancePasses(slot.passes);
		_gen.setOmyacParams(slot.params);
		uint32 ms = 0;
		// Nearest-ref plates have no backfill mask (no omyac pass ran) -> no pink.
		Graphics::Surface *plate = (slot.plateMode == kPlateNearestRef)
			? _gen.generatePlateNearest(_picIds[_picIdx], ms)
			: _gen.generatePlateWithBackfill(_picIds[_picIdx], slot.backfillMask, ms);
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

	// Feature 1: recolour "unfilled" pixels — those fillNullPixels backfilled
	// (nothing official painted) — hot pink, before the cel goes on. Toggling
	// this is invalidateCelOnly()-tier (plate cache is reused). The mask matches
	// the plate 1:1 (OMYAC_HYBRID_W*OMYAC_HYBRID_H); nearest-ref plates carry an
	// empty mask, so no pink there.
	if (_showBackfill && (int)slot.backfillMask.size() == composed.w * composed.h) {
		const uint32 pink = composed.format.RGBToColor(255, 105, 180);
		for (int y = 0; y < composed.h; y++) {
			uint32 *row = (uint32 *)composed.getBasePtr(0, y);
			const byte *mrow = slot.backfillMask.begin() + (size_t)y * composed.w;
			for (int x = 0; x < composed.w; x++)
				if (mrow[x])
					row[x] = pink;
		}
	}

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
	g_system->showOverlay(true); // inGUI: mouse events arrive in overlay coords
	// Hide the system hardware cursor: it is invisible/wrong over the hires overlay
	// (same finding as FileRogerArtProvider), so the studio composites its own
	// crosshair (drawCursor) at the reported mouse position instead.
	CursorMan.showMouse(false);
	_display = new Graphics::ManagedSurface(
		g_system->getOverlayWidth(), g_system->getOverlayHeight(),
		g_system->getOverlayFormat());
	fitView();
	ensureFresh(_slots[0]);
	// Seed the crosshair at the current pointer so it is visible before first move.
	{
		const Common::Point p = g_system->getEventManager()->getMousePos();
		_mouseX = p.x; _mouseY = p.y;
	}
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
		// ev.mouse arrives in overlay (_display) space because run() calls
		// showOverlay(true): WindowedGraphicsManager sets _activeArea to the
		// overlay dims when inGUI, and convertWindowToVirtual() maps window px
		// into that space (backends/graphics/windowed.h:71-90, :277). With
		// inGUI=false it would be GAME space (320x200) - do not "simplify".
		const int ox = ev.mouse.x;
		const int oy = ev.mouse.y;
		_mouseX = ox; _mouseY = oy; markDirty(); // track for the composited crosshair
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
		// ev.mouse is already overlay-space (see EVENT_MOUSEMOVE): identity.
		const int ox = ev.mouse.x;
		const int oy = ev.mouse.y;
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
		const int ox = ev.mouse.x; // overlay-space (see EVENT_MOUSEMOVE)
		const int oy = ev.mouse.y;
		_panning = true; _dragLastX = ox; _dragLastY = oy;
		return;
	}
	case Common::EVENT_RBUTTONUP:
		_panning = false;
		return;
	case Common::EVENT_WHEELUP:
	case Common::EVENT_WHEELDOWN: {
		const int ox = ev.mouse.x; // overlay-space (see EVENT_MOUSEMOVE)
		const int oy = ev.mouse.y;
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

// Blit one render into a sub-area of the scene, scaled by _viewScale and panned
// by (_panX,_panY). Reuses v1's split-blit src/dst mapping math.
void RogerStudio::blitRender(const Graphics::Surface &render, const Common::Rect &subArea) {
	const float scale = _viewScale;
	Common::Rect dst(subArea.left + _panX, subArea.top + _panY,
	                 subArea.left + _panX + (int)(render.w * scale),
	                 subArea.top + _panY + (int)(render.h * scale));
	dst.clip(subArea);
	if (!dst.isEmpty()) {
		Common::Rect src((int)((dst.left - subArea.left - _panX) / scale),
		                 (int)((dst.top - subArea.top - _panY) / scale),
		                 (int)((dst.right - subArea.left - _panX) / scale),
		                 (int)((dst.bottom - subArea.top - _panY) / scale));
		src.clip(Common::Rect(render.w, render.h));
		if (!src.isEmpty())
			_display->blitFrom(render, src, dst);
	}
}

// Feature 2: draw a light 1-screen-px grid at every plate-pixel boundary within
// subArea, using the same (_panX,_panY,_viewScale) transform as blitRender.
// Only meaningful when a plate pixel spans several screen px, so callers gate on
// _viewScale >= 3. Display-time only (bare markDirty tier — no render regen).
void RogerStudio::drawPixelGrid(const Common::Rect &subArea) {
	if (_viewScale < 3.0f)
		return;
	const uint32 grid = _display->format.RGBToColor(200, 200, 200);
	// Vertical lines: plate x-boundary px maps to display px = subArea.left + panX + px*scale.
	// Walk plate columns whose boundary falls inside subArea.
	const int x0 = subArea.left, x1 = subArea.right;
	const int y0 = subArea.top, y1 = subArea.bottom;
	// First plate column boundary at/after subArea.left.
	int startPx = (int)((x0 - subArea.left - _panX) / _viewScale);
	if (startPx < 0)
		startPx = 0;
	for (int px = startPx;; px++) {
		const int dx = subArea.left + _panX + (int)(px * _viewScale);
		if (dx >= x1)
			break;
		if (dx >= x0)
			_display->vLine(dx, y0, y1 - 1, grid);
		if (px > OMYAC_HYBRID_W)
			break; // safety bound
	}
	int startPy = (int)((y0 - subArea.top - _panY) / _viewScale);
	if (startPy < 0)
		startPy = 0;
	for (int py = startPy;; py++) {
		const int dy = subArea.top + _panY + (int)(py * _viewScale);
		if (dy >= y1)
			break;
		if (dy >= y0)
			_display->hLine(x0, dy, x1 - 1, grid);
		if (py > OMYAC_HYBRID_H)
			break; // safety bound
	}
}

void RogerStudio::ensureDiff() {
	Slot &a = _slots[0], &b = _slots[1];
	ensureFresh(a); ensureFresh(b);
	if (!a.render || !b.render) {
		_status = "diff: a slot render is missing";
		return;
	}
	if (a.render->w != b.render->w || a.render->h != b.render->h) {
		_status = "diff: slot renders differ in size";
		return;
	}
	if (!_diffStale && _diffSurf)
		return;

	const int w = a.render->w, h = a.render->h;
	if (!_diffSurf || _diffSurf->w != w || _diffSurf->h != h) {
		if (_diffSurf) { _diffSurf->free(); delete _diffSurf; }
		_diffSurf = new Graphics::Surface();
		_diffSurf->create(w, h, a.render->format);
	}
	diffMapRGBA((const byte *)a.render->getPixels(), (const byte *)b.render->getPixels(),
	            w, h, (byte *)_diffSurf->getPixels());
	int dx = 0, dy = 0;
	estimateOffsetSAD((const byte *)a.render->getPixels(), (const byte *)b.render->getPixels(),
	                  w, h, 3, dx, dy);
	_offsetReadout = Common::String::format("best align: dx=%+d dy=%+d overlay px (1/6 native)", dx, dy);
	// Evidence line (run log): only emitted when the diff is actually rebuilt.
	debug("ROGER-STUDIO diff offset dx=%d dy=%d", dx, dy);
	_diffStale = false;
}

void RogerStudio::drawFrame() {
	const Graphics::PixelFormat fmt = _display->format;
	_display->fillRect(Common::Rect(_display->w, _display->h),
	                   fmt.RGBToColor(48, 48, 48));

	const Common::Rect area = sceneArea();

	if (_displayMode == kShowSplit) {
		ensureFresh(_slots[0]); ensureFresh(_slots[1]);
		const int halfW = area.width() / 2;
		const Common::Rect leftArea(area.left, area.top, area.left + halfW, area.bottom);
		const Common::Rect rightArea(area.left + halfW, area.top, area.right, area.bottom);
		if (_slots[0].render)
			blitRender(*_slots[0].render, leftArea);
		if (_slots[1].render)
			blitRender(*_slots[1].render, rightArea);
		_display->vLine(area.left + halfW, area.top, area.bottom - 1, fmt.RGBToColor(255, 255, 255));
		const Graphics::Font *lf = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		if (lf) {
			const uint32 white = fmt.RGBToColor(255, 255, 255);
			lf->drawString(_display, Common::String("A ") + slotStamp(_slots[0]),
			               leftArea.left + 4, area.top + 4, halfW - 8, white);
			lf->drawString(_display, Common::String("B ") + slotStamp(_slots[1]),
			               rightArea.left + 4, area.top + 4, halfW - 8, white);
		}
	} else if (_displayMode == kShowDiff) {
		// Important 2: while dragging the cel, skip the per-frame diff/SAD rebuild
		// (49-offset SAD over 1920x1140 is a stall loop at drag rate). _diffStale
		// stays set (invalidateCelOnly keeps it true), so the diff + offset readout
		// rebuild once on the first frame after the drag ends. During the drag we
		// re-show the last diff surface (positionally stale but cheap).
		if (!_draggingCel)
			ensureDiff();
		if (_diffSurf) {
			blitRender(*_diffSurf, area);
		} else {
			// Diff unavailable: fall back to Show A behavior.
			ensureFresh(_slots[0]);
			if (_slots[0].render)
				blitRender(*_slots[0].render, area);
		}
	} else {
		Slot &slot = (_displayMode == kShowB) ? _slots[1] : _slots[0];
		ensureFresh(slot);
		if (slot.render)
			blitRender(*slot.render, area);
	}

	// Feature 2: pixel-boundary grid over the scene area (both Split halves share
	// the same transform, so a single pass over the whole area covers both).
	if (_showGrid)
		drawPixelGrid(area);

	drawPanel();
	drawCursor();

	g_system->copyRectToOverlay(_display->getPixels(), _display->pitch,
	                            0, 0, _display->w, _display->h);
	g_system->updateScreen();
}

// Composite a crosshair pointer at (_mouseX,_mouseY) into _display. The native
// hardware cursor is invisible over the overlay (same finding as
// FileRogerArtProvider, which draws its own arrow), so the studio draws its own.
// Cheap: two short lines each frame, only inside drawFrame (which runs only when
// _dirty, and a mouse move sets _dirty). No per-frame cost when the mouse is idle.
void RogerStudio::drawCursor() {
	const int r = 14;                     // crosshair arm length (overlay px)
	const int gap = 3;                    // centre gap so the exact point stays clear
	const uint32 white = _display->format.RGBToColor(255, 255, 255);
	const uint32 black = _display->format.RGBToColor(0, 0, 0);
	const int x = CLIP<int>(_mouseX, 0, _display->w - 1);
	const int y = CLIP<int>(_mouseY, 0, _display->h - 1);
	// 3px black underlay for contrast against light plates, then a 1px white core.
	for (int t = -1; t <= 1; t++) {
		_display->hLine(CLIP<int>(x - r, 0, _display->w - 1), CLIP<int>(y + t, 0, _display->h - 1),
		                CLIP<int>(x - gap, 0, _display->w - 1), black);
		_display->hLine(CLIP<int>(x + gap, 0, _display->w - 1), CLIP<int>(y + t, 0, _display->h - 1),
		                CLIP<int>(x + r, 0, _display->w - 1), black);
		_display->vLine(CLIP<int>(x + t, 0, _display->w - 1), CLIP<int>(y - r, 0, _display->h - 1),
		                CLIP<int>(y - gap, 0, _display->h - 1), black);
		_display->vLine(CLIP<int>(x + t, 0, _display->w - 1), CLIP<int>(y + gap, 0, _display->h - 1),
		                CLIP<int>(y + r, 0, _display->h - 1), black);
	}
	_display->hLine(CLIP<int>(x - r, 0, _display->w - 1), y, CLIP<int>(x - gap, 0, _display->w - 1), white);
	_display->hLine(CLIP<int>(x + gap, 0, _display->w - 1), y, CLIP<int>(x + r, 0, _display->w - 1), white);
	_display->vLine(x, CLIP<int>(y - r, 0, _display->h - 1), CLIP<int>(y - gap, 0, _display->h - 1), white);
	_display->vLine(x, CLIP<int>(y + gap, 0, _display->h - 1), CLIP<int>(y + r, 0, _display->h - 1), white);
}

void RogerStudio::drawPanel() {
	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (!font) {
		if (!_hudFontWarned) {
			_hudFontWarned = true;
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
	st.celX = _celX; st.celY = _celY;
	st.variantName = scalerVariantName(s.variant);
	st.plateNearest = s.plateMode == kPlateNearestRef;
	st.showView = _showView;
	st.showBackfill = _showBackfill;
	st.showGrid = _showGrid;
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

	// Bottom line: hover help (when hovering a param/chip widget) or render ms + status + offset readout.
	Common::String bottomLine;
	{
		const int hk = widKind(_hoverWid);
		const int hi2 = widIndex(_hoverWid);
		const char *hoverHelp = nullptr;
		if (hk == kWidParamMinus || hk == kWidParamPlus || hk == kWidParamToggle) {
			hoverHelp = omyacParamDesc(hi2).help;
		} else if (hk == kWidChipAddF) {
			hoverHelp = "insert fill pass at caret";
		} else if (hk == kWidChipAddL) {
			hoverHelp = "insert line pass at caret";
		} else if (hk == kWidChipAddA) {
			hoverHelp = "insert anti-alias pass at caret";
		} else if (hk == kWidChipX) {
			hoverHelp = "remove this pass";
		} else if (hk == kWidChipLeft) {
			hoverHelp = "move selected pass left";
		} else if (hk == kWidChipRight) {
			hoverHelp = "move selected pass right";
		} else if (hk == kWidChipClear) {
			hoverHelp = "remove all passes (wireframe)";
		} else if (hk == kWidChipReset) {
			hoverHelp = "restore default pass list";
		} else if (hk == kWidShowBackfill) {
			hoverHelp = "recolour hot pink the pixels nothing drew (backfilled)";
		} else if (hk == kWidShowGrid) {
			hoverHelp = "show a light grid at plate-pixel borders (zoom >= 3x)";
		}
		if (hoverHelp && *hoverHelp)
			bottomLine = Common::String::format("? %s", hoverHelp);
		else
			bottomLine = Common::String::format("%ums  %s  %s",
				s.renderMs, _status.c_str(), _offsetReadout.c_str());
	}
	font->drawString(&small, bottomLine, 4, smallH - kStudioRowH + 4, smallW - 8, hi);

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
	// Pink recolour happens at compose time over the cached plate -> cel-only tier.
	case kWidShowBackfill: _showBackfill = !_showBackfill; invalidateCelOnly(); break;
	// Grid is drawn at display time only -> redraw, no render regen.
	case kWidShowGrid: _showGrid = !_showGrid; markDirty(); break;
	case kWidFit: fitView(); break;
	case kWidTabA: _activeSlot = 0; _selectedChip = -1; markDirty(); break;
	case kWidTabB: _activeSlot = 1; _selectedChip = -1; markDirty(); break;
	case kWidShowA: _displayMode = kShowA; _offsetReadout.clear(); markDirty(); break;
	case kWidShowB: _displayMode = kShowB; _offsetReadout.clear(); markDirty(); break;
	case kWidSplit: _displayMode = kShowSplit; _offsetReadout.clear(); markDirty(); break;
	case kWidDiff: _displayMode = kShowDiff; markDirty(); break;
	case kWidCopyAB: {
		Slot &b = _slots[1];
		b.params = _slots[0].params; b.passes = _slots[0].passes;
		b.variant = _slots[0].variant; b.plateMode = _slots[0].plateMode;
		b.stale = b.plateStale = true; // settings differ -> plate genuinely changes
		_diffStale = true;
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
	case kWidChipClear:
		s.passes.clear(); _selectedChip = -1; invalidateActive(); break;
	case kWidChipReset:
		s.passes = defaultPasses(); _selectedChip = -1; invalidateActive(); break;
	default: break;
	}
}

void RogerStudio::exportShown() {
	const int picId = _picIds.empty() ? 0 : _picIds[_picIdx];
	Common::String name;
	Graphics::Surface *tmp = nullptr;        // composed export needing free
	const Graphics::Surface *src = nullptr;
	if (_displayMode == kShowSplit || _displayMode == kShowDiff) {
		ensureFresh(_slots[0]); ensureFresh(_slots[1]);
		if (!_slots[0].render || !_slots[1].render) { _status = "nothing to export"; markDirty(); return; }
		const Common::String sa = sanitize(slotStamp(_slots[0]));
		const Common::String sb = sanitize(slotStamp(_slots[1]));
		if (_displayMode == kShowDiff) {
			// Build the diff on demand (E key can reach export before it was displayed).
			ensureDiff();
			if (!_diffSurf) { _status = "nothing to export"; markDirty(); return; }
			name = studioCompareExportName(picId, true, sa, sb);
			src = _diffSurf;
		} else {
			// Full-res side-by-side compose (independent of window/zoom).
			const Graphics::Surface *a = _slots[0].render, *b = _slots[1].render;
			Graphics::ManagedSurface side(a->w + b->w, MAX(a->h, b->h), a->format);
			side.blitFrom(*a, Common::Point(0, 0));
			side.blitFrom(*b, Common::Point(a->w, 0));
			tmp = new Graphics::Surface();
			tmp->copyFrom(side.rawSurface());
			name = studioCompareExportName(picId, false, sa, sb);
			src = tmp;
		}
	} else {
		Slot &s = _slots[_displayMode == kShowB ? 1 : 0];
		ensureFresh(s);
		if (!s.render) { _status = "nothing to export"; markDirty(); return; }
		name = studioSceneExportName(picId, _displayMode == kShowB ? 'B' : 'A',
		                             sanitize(slotStamp(s)));
		src = s.render;
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

	const Common::String path = dir + name;
	if (Roger::dumpSurfacePng(*src, path)) {
		if (useDefault)
			_status = "exported " + dir + name;
		else
			_status = "exported " + name;
	} else {
		_status = "export FAILED: cannot open " + name;
	}
	if (tmp) { tmp->free(); delete tmp; }
	markDirty();
}

} // namespace Roger
} // namespace Sci
