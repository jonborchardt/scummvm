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

#ifndef SCI_ROGER_LAUNCHER_ROGER_STANDALONE_H
#define SCI_ROGER_LAUNCHER_ROGER_STANDALONE_H

// Pre-engine entry for the Roger game picker: base/main.cpp's launcher round
// calls this in place of the stock ScummVM launcher (fork-only seam).
// Deliberately include-free so base/ can include it without pulling SCI
// headers.

namespace Sci {
namespace Roger {

// Runs the Roger picker as this launcher round if the gate is open
// (roger_no_launcher in [scummvm] / ROGER_NO_LAUNCHER env both opt out).
// Returns true if the picker ran: a launch has set the active ConfMan domain
// and the caller's loop boots it; a close/quit left it unset. Returns false
// when the gate is closed -- caller falls through to the stock launcher.
bool rogerStandaloneLauncher();

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_LAUNCHER_ROGER_STANDALONE_H
