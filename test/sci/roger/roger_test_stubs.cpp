/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Test-only stubs for the few SCI-engine symbols referenced by the SCI-free
// Roger objects linked into the unit-test runner (see test/module.mk).
//
// The Windows test build (build_tests.ps1) compiles the same source set with
// MSVC, whose /Gy + /OPT:REF dead-stripping silently discards the
// never-called engine-touching functions together with their references to
// these symbols. GNU ld includes directly-listed objects wholesale and
// requires every relocation resolved, so this TU is the explicit equivalent
// of that dead-stripping.
//
// A stub actually being CALLED means a test has started exercising a
// live-engine path; that must fail loudly (error() aborts the runner)
// rather than silently returning a fake.

#include "sci/sci.h"
#include "sci/resource/resource.h"
#include "sci/graphics/cache.h"
#include "sci/graphics/view.h"
#include "common/textconsole.h"

namespace Sci {

// The engine singleton is null in unit tests; the Roger code paths the
// tests exercise never dereference it.
SciEngine *g_sci = nullptr;

Resource *ResourceManager::findResource(ResourceId id, bool lock) {
	error("roger_test_stubs: ResourceManager::findResource called from a unit test");
}

GfxFont *GfxCache::getFont(GuiResourceId fontId) {
	error("roger_test_stubs: GfxCache::getFont called from a unit test");
}

GfxView *GfxCache::getView(GuiResourceId viewId) {
	error("roger_test_stubs: GfxCache::getView called from a unit test");
}

const CelInfo *GfxView::getCelInfo(int16 loopNo, int16 celNo) const {
	error("roger_test_stubs: GfxView::getCelInfo called from a unit test");
}

const SciSpan<const byte> &GfxView::getBitmap(int16 loopNo, int16 celNo) {
	error("roger_test_stubs: GfxView::getBitmap called from a unit test");
}

} // End of namespace Sci
