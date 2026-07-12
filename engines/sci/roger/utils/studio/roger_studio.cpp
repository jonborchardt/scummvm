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

#include "sci/roger/utils/studio/roger_studio.h"
#include "sci/roger/utils/studio/roger_sweep_svg.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/cursorman.h"
#include "graphics/fontman.h"
#include "graphics/font.h"
#include "graphics/managed_surface.h"
#include "sci/roger/png_loader.h"
#include "sci/roger/gen/roger_passes.h"

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
	_iniPasses = effectivePasses(ConfMan.hasKey("roger_omyac_passes"),
	                             ConfMan.hasKey("roger_omyac_passes") ? ConfMan.get("roger_omyac_passes") : "");
	// Pic-enhance mode list: the curated registry (best-first), with the
	// ini-effective passes select-or-added so both slots open on the config.
	for (int i = 0; i < goodPassPatternCount(); i++)
		_picModes.push_back(parsePassString(goodPassPattern(i).compact));
	const int iniSel = selectOrAddPicMode(_iniPasses);
	_slots[0].picSel = _slots[1].picSel = iniSel;
	_slots[0].passes = _iniPasses;
	_slots[1].passes = _iniPasses;
	_buildPasses = _iniPasses;
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

// Point picSel at the _picModes entry equal to `passes`, appending it as a
// new mode when absent (the "add" contract, same as the tune panel's
// tuneSelectOrAddMode). Returns the selected index.
int RogerStudio::selectOrAddPicMode(const Common::Array<int> &passes) {
	for (uint i = 0; i < _picModes.size(); i++)
		if (passesEqual(_picModes[i], passes))
			return (int)i;
	// Appending shifts the trailing "nearest" position up by one; re-point any
	// slot parked on it first, or its picSel would alias the new mode (wrong
	// label, dead cycling - the cached plateMode masks it until the next apply).
	for (int i = 0; i < 2; i++)
		if (picSelIsNearest(_slots[i].picSel))
			_slots[i].picSel = (int)_picModes.size() + 1;
	_picModes.push_back(passes);
	return (int)_picModes.size() - 1;
}

// Derive the render inputs from the slot's pic-enhance selection: the trailing
// slot is the nearest plate (passes untouched - it has no pass list); a pass
// mode loads its sequence and returns to the omyac pipeline.
void RogerStudio::applyPicSel(Slot &slot) {
	if (picSelIsNearest(slot.picSel)) {
		slot.plateMode = kPlateNearestRef;
	} else {
		slot.plateMode = kPlateOmyac;
		slot.passes = _picModes[slot.picSel];
	}
}

RogerStudio::~RogerStudio() {
	if (_display) { _display->free(); delete _display; }
	for (int i = 0; i < 2; i++) {
		if (_slots[i].render) { _slots[i].render->free(); delete _slots[i].render; }
		if (_slots[i].plateCache) { _slots[i].plateCache->free(); delete _slots[i].plateCache; }
	}
	if (_diffSurf) { _diffSurf->free(); delete _diffSurf; }
	freeGridCels();
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
	// only recomposites the cel over it -- no omyac regen at drag rate.
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

	// Feature 1: recolour "unfilled" pixels -- those fillNullPixels backfilled
	// (nothing official painted) -- hot pink, before the cel goes on. Toggling
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
			// applyViewEnhanceMode6x lands every mode on the 6x plate grid
			// (registry modules via applyViewScalerTo6x, nearest via a plain
			// exact-rational resample) so the 1:1 blit below and the Diff
			// view stay pixel-exact.
			IndexImage scaled = applyViewEnhanceMode6x(slot.viewMode, cel, clearKey);
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

void RogerStudio::freeGridCels() {
	for (int i = 0; i < 6; i++) {
		if (_gcSurf[i]) { _gcSurf[i]->free(); delete _gcSurf[i]; _gcSurf[i] = nullptr; }
	}
	_gcView = _gcLoop = _gcCel = -1;
}

// Build one surface per registered scaler module for the current (view, loop, cel), each at the module's OWN factor; normalization to a common on-screen footprint happens at blit time.
void RogerStudio::ensureGridCels() {
#ifdef ENABLE_SCI
	if (_viewIds.empty())
		return;
	const int viewId = _viewIds[_viewIdx];
	if (_gcView == viewId && _gcLoop == _loopNo && _gcCel == _celNo && _gcSurf[0])
		return;
	freeGridCels();

	// Clamp against real counts (same block as renderSlot).
	if (g_sci && g_sci->_gfxCache) {
		GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)viewId);
		if (view) {
			_loopNo = CLIP<int>(_loopNo, 0, MAX(0, (int)view->getLoopCount() - 1));
			_celNo = CLIP<int>(_celNo, 0, MAX(0, (int)view->getCelCount((int16)_loopNo) - 1));
			const CelInfo *ci = view->getCelInfo((int16)_loopNo, (int16)_celNo);
			if (ci) { _gcDx = ci->displaceX; _gcDy = ci->displaceY; }
		}
	}

	IndexImage cel;
	byte clearKey = 0;
	if (!_gen.nativeCelIndexImage(viewId, _loopNo, _celNo, cel, clearKey)) {
		_status = "grid: cel extraction failed";
		return;
	}
	_gcW = cel.w; _gcH = cel.h;
	for (int i = 0; i < gridTileCount(); i++) {
		const int mode = gridPresetSlot(i);
		if (mode < 0)
			continue; // past the modes; tile stays empty
		if (viewEnhanceModeIsNearest(mode)) {
			// Nearest tile: plain 6x resample, no enhancement (the "before").
			IndexImage scaled = resampleNearestExact(cel, cel.w * 6, cel.h * 6);
			_gcSurf[i] = _gen.surfaceFromIndex(scaled, clearKey);
			_gcFactor[i] = 6;
		} else {
			IndexImage scaled = applyViewScaler(mode, cel, clearKey);
			_gcSurf[i] = _gen.surfaceFromIndex(scaled, clearKey);
			_gcFactor[i] = viewScaler(mode).factor;
		}
	}
	_gcView = viewId; _gcLoop = _loopNo; _gcCel = _celNo;
#endif
}

// Advance the shared cel index through the current loop, wrapping. Reuses the
// cel-only invalidation tier: plates stay cached, so a tick recomposites the
// cel (A/B/Split) or rebuilds the small grid cache (Grid). Engine-gated: with
// no engine or no views this is a no-op.
void RogerStudio::stepAnimCel() {
#ifdef ENABLE_SCI
	if (_viewIds.empty() || !g_sci || !g_sci->_gfxCache)
		return;
	GfxView *view = g_sci->_gfxCache->getView((GuiResourceId)_viewIds[_viewIdx]);
	if (!view)
		return;
	_loopNo = CLIP<int>(_loopNo, 0, MAX(0, (int)view->getLoopCount() - 1));
	const int cels = MAX(1, (int)view->getCelCount((int16)_loopNo));
	_celNo = (_celNo + 1) % cels;
	invalidateCelOnly();
#endif
}

Common::String RogerStudio::slotStamp(const Slot &slot) const {
	Common::String s = omyacParamStamp(slot.params) + "-" + omyacPassStamp(slot.passes);
	if (slot.plateMode == kPlateNearestRef)
		s += "-nref";
	if (_showView)
		s += Common::String("-") + viewEnhanceModeId(slot.viewMode);
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
		if (_animPlaying) {
			const uint32 now = g_system->getMillis();
			if (now - _lastAnimTick >= (uint32)animSpeedMs(_animSpeedIdx)) {
				_lastAnimTick = now;
				stepAnimCel();
			}
		}
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
// _viewScale >= 3. Display-time only (bare markDirty tier -- no render regen).
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
	if (!_animPlaying) {
		int dx = 0, dy = 0;
		estimateOffsetSAD((const byte *)a.render->getPixels(), (const byte *)b.render->getPixels(),
		                  w, h, 3, dx, dy);
		_offsetReadout = Common::String::format("best align: dx=%+d dy=%+d overlay px (1/6 native)", dx, dy);
		// Evidence line (run log): only emitted when the diff is actually rebuilt.
		debug("ROGER-STUDIO diff offset dx=%d dy=%d", dx, dy);
	}
	_diffStale = false;
}

// 6-pipeline comparison grid: the current cel through each grid preset, on a
// neutral dark background. Every tile shows the same NATIVE footprint: screen
// px per native px = 6 * _viewScale regardless of the preset's factor, so 8x
// and 9x tiles align with the 6x ones and pan/zoom move all tiles in lockstep.
// blitFrom's scaled blit is display-only here (same as blitRender for plates).
void RogerStudio::drawGrid(const Common::Rect &area) {
	ensureGridCels();
	const Graphics::PixelFormat fmt = _display->format;
	const uint32 tileBg = fmt.RGBToColor(32, 32, 32);
	const uint32 border = fmt.RGBToColor(96, 96, 96);
	const uint32 white = fmt.RGBToColor(255, 255, 255);
	const Graphics::Font *lf = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	const int labelH = lf ? (lf->getFontHeight() + 4) : 0;
	const float sN = 6.0f * _viewScale; // screen px per NATIVE cel px

	for (int i = 0; i < gridTileCount(); i++) {
		const Common::Rect tile = gridTileRect(area, i);
		_display->fillRect(tile, tileBg);
		_display->frameRect(tile, border);
		const int slot = gridPresetSlot(i);
		if (lf && slot >= 0)
			lf->drawString(_display, viewEnhanceModeLabel(slot),
			               tile.left + 4, tile.top + 2, tile.width() - 8, white);

		if (!_gcSurf[i])
			continue;
		// Cel content area below the label strip.
		const Common::Rect content(tile.left, tile.top + labelH, tile.right, tile.bottom);
		// Anchor point: content centre-bottom third, shifted by shared pan.
		const int ax = content.left + content.width() / 2 + _panX;
		const int ay = content.top + (content.height() * 3) / 4 + _panY;
		const Common::Rect nat = celAnchorRect(_gcW, _gcH, _gcDx, _gcDy, 0, 0);
		Common::Rect dst((int16)(ax + nat.left * sN), (int16)(ay + nat.top * sN),
		                 (int16)(ax + nat.left * sN + _gcW * sN),
		                 (int16)(ay + nat.top * sN + _gcH * sN));
		Common::Rect clipped = dst;
		clipped.clip(content);
		if (clipped.isEmpty())
			continue;
		// Map the clipped screen rect back into the preset surface (own factor).
		const Graphics::Surface *s = _gcSurf[i];
		Common::Rect src((int)((clipped.left - dst.left) / sN * _gcFactor[i]),
		                 (int)((clipped.top - dst.top) / sN * _gcFactor[i]),
		                 (int)((clipped.right - dst.left) / sN * _gcFactor[i]),
		                 (int)((clipped.bottom - dst.top) / sN * _gcFactor[i]));
		src.clip(Common::Rect(s->w, s->h));
		if (!src.isEmpty())
			_display->blendBlitFrom(*s, src, clipped, Graphics::FLIP_NONE);
	}
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
	} else if (_displayMode == kShowGrid6) {
		drawGrid(area);
	} else {
		Slot &slot = (_displayMode == kShowB) ? _slots[1] : _slots[0];
		ensureFresh(slot);
		if (slot.render)
			blitRender(*slot.render, area);
	}

	// Feature 2: pixel-boundary grid over the scene area (both Split halves share
	// the same transform, so a single pass over the whole area covers both).
	// Meaningless in grid mode (plate-space; the tiles are cel-space).
	if (_showGrid && _displayMode != kShowGrid6)
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
	const int panelTop = _display->h - kPanelH;
	const int smallW = _display->w / 2, smallH = kPanelH / 2;

	// Full-res paint: widgets stay laid out in small (half-res) space so the
	// existing /2 hit-test mapping and buildStudioPanel are untouched; each
	// rect is scaled 2x at draw time and text renders at display resolution.
	const int sizes[kFontRoleCount] = {
		kStudioRowH * 2 * 3 / 4,  // title: unused today, sized anyway
		0,                        // sub: unused
		kStudioRowH * 2 * 3 / 5,  // body: widget labels (28px in a 48px row)
		kStudioRowH,              // small: unused today, sized anyway
		kStudioRowH * 2 * 3 / 5   // mono: unused today, sized anyway
	};
	_panelFonts.load(sizes);
	PanelPainter paint(*_display, _panelFonts);

	const Common::Rect panelR(0, panelTop, _display->w, _display->h);
	_display->fillRect(panelR, _display->format.RGBToColor(
		PanelStyle::kPanelFill.r, PanelStyle::kPanelFill.g, PanelStyle::kPanelFill.b));
	paint.strokeRect(panelR, PanelStyle::kPanelLine);

	// Rebuild widgets from current state (cheap; keeps rects in lockstep with state).
	StudioPanelState st;
	const Slot &s = _slots[_activeSlot];
	st.picId = _picIds.empty() ? -1 : _picIds[_picIdx];
	st.viewId = _viewIds.empty() ? -1 : _viewIds[_viewIdx];
	st.loopNo = _loopNo; st.celNo = _celNo;
	st.celX = _celX; st.celY = _celY;
	st.viewEnhanceLabel = viewEnhanceModeLabel(s.viewMode);
	st.picEnhanceLabel = picSelIsNearest(s.picSel)
	                     ? Common::String("nearest") : omyacPassStamp(s.passes);
	st.showView = _showView;
	st.showBackfill = _showBackfill;
	st.showGrid = _showGrid;
	st.animPlaying = _animPlaying;
	st.animMs = animSpeedMs(_animSpeedIdx);
	st.svgWidth = _svgWidth;
	st.activeSlot = _activeSlot;
	st.displayMode = _displayMode;
	st.buildPasses = _buildPasses;
	// "add" would change the active slot: it is on nearest, or its passes
	// differ from the built sequence.
	st.addPending = picSelIsNearest(s.picSel) || !passesEqual(_buildPasses, s.passes);
	for (int i = 0; i < omyacParamCount(); i++)
		st.paramValues.push_back(omyacParamGet(s.params, i));
	buildStudioPanel(Common::Rect(0, 0, smallW, smallH - kStudioRowH), st, _widgets);

	for (uint i = 0; i < _widgets.size(); i++) {
		const PanelWidget &wg = _widgets[i];
		const Common::Rect r(wg.rect.left * 2, panelTop + wg.rect.top * 2,
		                     wg.rect.right * 2, panelTop + wg.rect.bottom * 2);
		if (widKind(wg.id) == kWidNone || !wg.enabled) {
			// Plain text runs (labels, values, display-only chips).
			paint.drawTextIn(kFontBody, wg.label, r, PanelStyle::kText,
			                 Graphics::kTextAlignLeft);
		} else {
			paint.drawButton(r, wg.label,
			                 wg.on ? PanelStyle::kBlue : PanelStyle::kPanelLine,
			                 false, true, wg.id == _hoverWid, kFontBody);
		}
	}

	// Bottom line: hover help (when hovering a param/chip widget) or render ms + status + offset readout.
	Common::String bottomLine;
	{
		const int hk = widKind(_hoverWid);
		const int hi2 = widIndex(_hoverWid);
		const char *hoverHelp = nullptr;
		if (hk == kWidParamMinus || hk == kWidParamPlus || hk == kWidParamToggle) {
			hoverHelp = omyacParamDesc(hi2).help;
		} else if (hk == kWidViewEnhance) {
			hoverHelp = "cycle the view-cel upscaler (scaler modules + nearest)";
		} else if (hk == kWidPicEnhance) {
			hoverHelp = "cycle the plate mode (known pass sequences + nearest)";
		} else if (hk == kWidChipAddF) {
			hoverHelp = "append a fill pass to the built sequence";
		} else if (hk == kWidChipAddL) {
			hoverHelp = "append a line pass to the built sequence";
		} else if (hk == kWidChipAddA) {
			hoverHelp = "append an anti-alias pass to the built sequence";
		} else if (hk == kWidChipClear) {
			hoverHelp = "empty the built sequence (add -> wireframe)";
		} else if (hk == kWidChipAdd) {
			hoverHelp = "register the built sequence as a pic-enhance mode and use it";
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
	const Common::Rect blR(8, _display->h - kStudioRowH * 2 + 2,
	                       _display->w - 8, _display->h - 2);
	paint.drawTextIn(kFontBody, bottomLine, blR, PanelStyle::kAmber,
	                 Graphics::kTextAlignLeft);
}

void RogerStudio::dispatchWidget(uint32 id) {
	Slot &s = activeSlot();
	const int kind = widKind(id), idx = widIndex(id);
	switch (kind) {
	case kWidPicPrev: case kWidPicNext:
		if (_picIds.empty())
			break;
		_picIdx = (_picIdx + (kind == kWidPicPrev ? (int)_picIds.size() - 1 : 1)) % (int)_picIds.size();
		invalidateScene(); break;
	case kWidViewPrev: case kWidViewNext:
		if (_viewIds.empty())
			break;
		_viewIdx = (_viewIdx + (kind == kWidViewPrev ? (int)_viewIds.size() - 1 : 1)) % (int)_viewIds.size();
		_loopNo = _celNo = 0;
		invalidateCelOnly(); break; // view swap changes only the cel, not the plate
	case kWidLoopPrev: _loopNo = MAX(0, _loopNo - 1); _celNo = 0; invalidateCelOnly(); break;
	case kWidLoopNext: _loopNo++; _celNo = 0; invalidateCelOnly(); break; // clamped in renderSlot
	case kWidCelPrev: _celNo = MAX(0, _celNo - 1); invalidateCelOnly(); break;
	case kWidCelNext: _celNo++; invalidateCelOnly(); break;              // clamped in renderSlot
	case kWidViewEnhance:
		// Cycle the view-enhance modes (registry scalers + trailing nearest).
		// The cel recomposites over the cached plate: cel-only tier (the old
		// variant cycle needlessly regenerated the plate - fixed here).
		s.viewMode = (s.viewMode + 1) % viewEnhanceModeCount();
		invalidateCelOnly(); break;
	case kWidPicEnhance:
		// Cycle the pic-enhance modes (shared pass-mode list + trailing
		// nearest plate); the gen-level memory cache makes a revisited mode a
		// copy instead of an omyac regen.
		s.picSel = (s.picSel + 1) % picModeCount();
		applyPicSel(s);
		invalidateActive(); break;
	case kWidShowView: _showView = !_showView; invalidateCelOnly(); break;
	// Pink recolour happens at compose time over the cached plate -> cel-only tier.
	case kWidShowBackfill: _showBackfill = !_showBackfill; invalidateCelOnly(); break;
	// Grid is drawn at display time only -> redraw, no render regen.
	case kWidShowGrid: _showGrid = !_showGrid; markDirty(); break;
	case kWidFit: fitView(); break;
	case kWidTabA: _activeSlot = 0; markDirty(); break;
	case kWidTabB: _activeSlot = 1; markDirty(); break;
	case kWidShowA: _displayMode = kShowA; _offsetReadout.clear(); markDirty(); break;
	case kWidShowB: _displayMode = kShowB; _offsetReadout.clear(); markDirty(); break;
	case kWidSplit: _displayMode = kShowSplit; _offsetReadout.clear(); markDirty(); break;
	case kWidDiff: _displayMode = kShowDiff; markDirty(); break;
	case kWidGrid6: _displayMode = kShowGrid6; _offsetReadout.clear(); markDirty(); break;
	case kWidAnimPlay:
		_animPlaying = !_animPlaying;
		_lastAnimTick = g_system->getMillis(); // full first period after resume
		markDirty(); break;
	case kWidAnimSlower: _animSpeedIdx = animSpeedStep(_animSpeedIdx, -1); markDirty(); break;
	case kWidAnimFaster: _animSpeedIdx = animSpeedStep(_animSpeedIdx, +1); markDirty(); break;
	case kWidCopyAB: {
		Slot &b = _slots[1];
		b.params = _slots[0].params; b.passes = _slots[0].passes;
		b.picSel = _slots[0].picSel; b.plateMode = _slots[0].plateMode;
		b.viewMode = _slots[0].viewMode;
		b.stale = b.plateStale = true; // settings differ -> plate genuinely changes
		_diffStale = true;
		_status = "copied A settings to B";
		markDirty(); break;
	}
	case kWidExport: exportShown(); break;
	case kWidExportSvg: exportSweepSvg(); break;
	case kWidSvgSize:
		// Two supported export widths: 1920 (as-is) and 640 (/3).
		_svgWidth = (_svgWidth == 1920) ? 640 : 1920;
		markDirty();
		break;
	case kWidParamMinus:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) - omyacParamDesc(idx).step);
		invalidateActive(); break;
	case kWidParamPlus:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) + omyacParamDesc(idx).step);
		invalidateActive(); break;
	case kWidParamToggle:
		omyacParamSet(s.params, idx, omyacParamGet(s.params, idx) ? 0 : 1);
		invalidateActive(); break;
	// Builder ops touch only the built sequence, never a render: display tier.
	case kWidChipAddF: _buildPasses.push_back(2); markDirty(); break;
	case kWidChipAddL: _buildPasses.push_back(1); markDirty(); break;
	case kWidChipAddA: _buildPasses.push_back(0); markDirty(); break;
	case kWidChipClear: _buildPasses.clear(); markDirty(); break;
	case kWidChipAdd:
		// Register the built sequence as a pic-enhance mode, select it in the
		// active slot, and apply (the F12 panel's "add" contract).
		s.picSel = selectOrAddPicMode(_buildPasses);
		applyPicSel(s);
		invalidateActive(); break;
	default: break;
	}
}

void RogerStudio::exportShown() {
	const int picId = _picIds.empty() ? 0 : _picIds[_picIdx];
	Common::String name;
	Graphics::Surface *tmp = nullptr;        // composed export needing free
	const Graphics::Surface *src = nullptr;
	if (_displayMode == kShowGrid6) {
		ensureGridCels();
		if (!_gcSurf[0]) { _status = "nothing to export"; markDirty(); return; }
		// Window-independent compose: every tile at 8 screen px per native px.
		const int kS = 8, kPad = 8;
		const Graphics::Font *lf = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
		const int labelH = lf ? (lf->getFontHeight() + 4) : 0;
		const int tw = _gcW * kS + 2 * kPad, th = _gcH * kS + labelH + 2 * kPad;
		Graphics::ManagedSurface gridOut(3 * tw, 2 * th, _slots[0].render
			? _slots[0].render->format : _display->format);
		gridOut.fillRect(Common::Rect(gridOut.w, gridOut.h), gridOut.format.RGBToColor(32, 32, 32));
		const uint32 white = gridOut.format.RGBToColor(255, 255, 255);
		for (int i = 0; i < gridTileCount(); i++) {
			if (!_gcSurf[i])
				continue;
			const int col = i % 3, row = i / 3;
			const int tx = col * tw, ty = row * th;
			if (lf) {
				const int slot = gridPresetSlot(i);
				if (slot >= 0)
					lf->drawString(&gridOut, viewEnhanceModeLabel(slot),
					               tx + kPad, ty + 2, tw - 2 * kPad, white);
			}
			const Common::Rect d(tx + kPad, ty + labelH + kPad,
			                     tx + kPad + _gcW * kS, ty + labelH + kPad + _gcH * kS);
			gridOut.blendBlitFrom(*_gcSurf[i], Common::Rect(0, 0, _gcSurf[i]->w, _gcSurf[i]->h), d,
			                      Graphics::FLIP_NONE);
		}
		tmp = new Graphics::Surface();
		tmp->copyFrom(gridOut.rawSurface());
		src = tmp;
		const int viewId = _viewIds.empty() ? 0 : _viewIds[_viewIdx];
		name = studioExportName("grid", viewId,
			sanitize(Common::String::format("l%d-c%d", _loopNo, _celNo)));
	} else if (_displayMode == kShowSplit || _displayMode == kShowDiff) {
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

void RogerStudio::exportSweepSvg() {
	ensureFresh(_slots[0]);
	ensureFresh(_slots[1]);
	if (!_slots[0].render || !_slots[1].render) {
		_status = "nothing to export";
		markDirty();
		return;
	}
	const Graphics::Surface *a = _slots[0].render;
	const Graphics::Surface *b = _slots[1].render;
	if (a->w != b->w || a->h != b->h) {
		_status = "export FAILED: A/B size mismatch";
		markDirty();
		return;
	}

	Common::String dir;
	if (ConfMan.hasKey("screenshotpath"))
		dir = ConfMan.getPath("screenshotpath").toString('/');
	if (dir.empty())
		dir = "screenshots";
	if (dir.lastChar() != '/')
		dir += '/';
	// Sweep SVGs land in their own subfolder (they are large and paired).
	dir += ".svg/";

	const int picId = _picIds.empty() ? 0 : _picIds[_picIdx];
	const Common::String sa = sanitize(slotStamp(_slots[0]));
	const Common::String sb = sanitize(slotStamp(_slots[1]));
	// Per-slot pixelated flag: a nearest-plate slot is pixel art and must
	// stay crisp (image-rendering:pixelated; the /3 resample reproduces its
	// 6x replication exactly as 2x). View-enhance mode is deliberately
	// ignored -- the plate dominates the image.
	const bool pixelatedA = _slots[0].plateMode == kPlateNearestRef;
	const bool pixelatedB = _slots[1].plateMode == kPlateNearestRef;

	// Sweep left = A (revealed), right = B (background). Both variants
	// always: the animated file auto-sweeps until grabbed, the interactive
	// file starts at centre. Labels in the files are placeholders (A/B) --
	// edit them at the EDIT LABELS marker near the end of each file.
	for (int variant = 0; variant < 2; variant++) {
		const bool animated = (variant == 0);
		const Common::String svg = buildSweepSvg(*a, *b, _svgWidth, pixelatedA, pixelatedB, animated);
		const Common::String name = studioSweepExportName(picId, sa, sb, _svgWidth, animated);
		if (svg.empty()) {
			_status = "export FAILED: svg build " + name;
			markDirty();
			return;
		}
		Common::DumpFile out;
		if (!out.open(Common::Path(dir + name), true)) {
			_status = "export FAILED: cannot open " + name;
			markDirty();
			return;
		}
		const uint32 wrote = out.write(svg.c_str(), svg.size());
		out.flush();
		const bool writeFailed = wrote != svg.size() || out.err();
		out.close();
		if (writeFailed) {
			_status = "export FAILED: write " + name;
			markDirty();
			return;
		}
	}
	_status = Common::String::format("exported sweep %d (animated + interactive)", _svgWidth);
	markDirty();
}

} // namespace Roger
} // namespace Sci
