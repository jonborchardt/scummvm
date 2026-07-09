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

#ifndef SCI_ROGER_LAUNCHER_ROGER_PICKER_MODEL_H
#define SCI_ROGER_LAUNCHER_ROGER_PICKER_MODEL_H

// Pure model/layout helpers for the Roger game picker v2. SCI-free, GUI-free,
// ConfMan-free — unit-tested in test/sci/roger/test_picker_model.h. The view
// (roger_picker_view) draws and hit-tests EXCLUSIVELY from PickerLayout, so
// the geometry under every click is covered by tests.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

// Widget kinds for the packed PanelWidget ids (widId/widKind, roger_widgets.h).
// Kind 0 is the reserved "none" (hitTestWidgets miss).
enum PickerWidgetKind {
	kPickNone = 0,
	kPickRow,          // index = state row index (scroll already applied)
	kPickRowPrecache,  // index = state row; doubles as Cancel while precaching
	kPickRowRemove,    // index = state row
	kPickAddGame,
	kPickPasses,       // the dropdown field
	kPickPassOption,   // index = option index in the dialog's pass-option list
	kPickDebugToggle,
	kPickLaunch
};

// ── Cache stamp ─────────────────────────────────────────────────────────────
// "Ready for current passes" is a completion stamp in the game's ini section
// (roger_cache_stamp), written when a precache run drains its queue.
// Format: "v<kTransformVersion>:<passes>".
Common::String cacheStamp(int version, const Common::String &passes);
bool cacheStampMatches(const Common::String &stamp, int version, const Common::String &passes);

// The roger_omyac_passes three-state rule projected to the stamp's passes part:
// unset -> defaultPasses; set (even empty after trim) -> the trimmed ini value.
Common::String stampPasses(bool hasKey, const Common::String &iniValue, const char *defaultPasses);

// Split "Space Quest III: ... (DOS/English)" into title + "DOS/English".
// No trailing parenthesized group -> outTitle = desc, outSubtitle empty.
void splitGameDescription(const Common::String &desc,
                          Common::String &outTitle, Common::String &outSubtitle);

// ── Layout ──────────────────────────────────────────────────────────────────
struct PickerRowLayout {
	Common::Rect card;      // full row card
	Common::Rect badge;     // cache-status badge area
	Common::Rect precache;  // Precache/Cancel button slot (drawn only when relevant)
	Common::Rect remove;    // Remove button slot
};

struct PickerLayout {
	Common::Rect titleBox, subtitleBox, descBox;
	Common::Rect listPanel, listHeader;
	Common::Array<PickerRowLayout> rows;  // one per VISIBLE row; state row = scrollOffset + i
	int rowsVisible = 0;                  // capacity (rows that fit), >= rows.size()
	Common::Rect addGame;
	Common::Rect settingsPanel, settingsTitle;
	Common::Rect passesLabel, passesField, passesHint;
	Common::Rect debugLabel, debugToggle, debugHint;
	Common::Rect launch;
};

// All rects in widget-local coordinates (0,0)-(w,h).
PickerLayout layoutPicker(int w, int h, int gameCount, int scrollOffset);

// Rect of dropdown option `index` of `count`, stacked below passesField and
// shifted up as a block if the stack would run past canvasH.
Common::Rect passOptionRect(const PickerLayout &l, int index, int count, int canvasH);

int clampScroll(int scroll, int gameCount, int rowsVisible);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_LAUNCHER_ROGER_PICKER_MODEL_H
