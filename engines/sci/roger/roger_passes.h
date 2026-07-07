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

#ifndef SCI_ROGER_ROGER_PASSES_H
#define SCI_ROGER_ROGER_PASSES_H

#include "common/array.h"
#include "common/str.h"

namespace Sci {
namespace Roger {

// The pass vocabulary for roger_omyac_passes, shared by the provider, the
// game picker, the Studio, and the tune panel. SCI-free and ConfMan-free —
// callers pass in the config lookup results (see effectivePasses).
//
// A pass value is a renderOmyac MODE_BY_NAME int: fill=2, line=1, all=0.

// Canonical default enhance-pass sequence as a compact character string
// (one char per pass: f=fill, l=line, a=all): 3x fill, 1x line, 2x fill,
// 4x all. The single source of truth — defaultPasses() parses this.
extern const char *const kDefaultPassString; // "ffflffaaaa"

// Parse a roger_omyac_passes string.
//  - Compact form: a string with no separators whose every char is one of
//    f l a 2 1 0 (lowercase only) parses per-character ("ffflffaaaa", "21").
//  - Legacy form: otherwise, space/comma/tab-separated tokens fill|f|2,
//    line|l|1, all|a|0; unknown tokens warning() and are skipped.
//  - Empty string -> empty array (zero passes / wireframe).
Common::Array<int> parsePassString(const Common::String &s);

// Canonical compact formatting: "ffla". Empty array -> "" (display code that
// wants "none" adds it itself, e.g. omyacPassStamp).
Common::String passString(const Common::Array<int> &passes);

// The roger_omyac_passes three-state rule:
//   hasKey=false          -> parsePassString(kDefaultPassString)
//   hasKey=true, s empty  -> empty array (wireframe)
//   hasKey=true, s tokens -> parsePassString(s)
Common::Array<int> effectivePasses(bool hasKey, const Common::String &s);

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_PASSES_H
