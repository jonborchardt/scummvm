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

#ifndef SCI_ROGER_ROGER_TOKENS_H
#define SCI_ROGER_ROGER_TOKENS_H

#include "common/scummsys.h"

namespace Sci {
namespace Roger {

// UI element token namespaces (high nibble) — the single home for these values.
// See CLAUDE.md "Ports & Windows are THE UI lifetime model". Tests pin the raw
// hex on purpose; everything else names them.
static const uint32 kTokenNamespaceMask = 0xF0000000u;
static const uint32 kStatusToken        = 0x10000000u; // status/menu-bar strip singleton
static const uint32 kMenuDropdownToken  = 0x20000000u; // menu dropdown singleton
static const uint32 kControlTokenNs     = 0x40000000u; // controls16/window captures: ns | windowId
static const uint32 kDrawCelIconTokenNs = 0x50000000u; // kDrawCel icon captures
static const uint32 kGenericTextTokenNs = 0x60000000u; // GfxText16::Box generic captures: ns | portId
static const uint32 kFrameBoxToken      = 0x70000000u; // kGraphFrameBox singleton

// Persistent overlay singletons whose lifetime is owned by their token (explicit
// clearToken / reapply), NOT by any SCI save-under. Restore rollback AND the
// unknown-handle restore fallback must spare them: the status banner is redrawn
// while a menu/dialog is open (postdates the checkpoint) and nothing repaints it
// after a bare restore; the frame box is overlay-only. The menu dropdown is
// deliberately NOT here — its own save-under restore is exactly what removes it.
inline bool isSaveUnderExemptSingleton(uint32 token) {
	return token == kStatusToken || token == kFrameBoxToken;
}

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_TOKENS_H
