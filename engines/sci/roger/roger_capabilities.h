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

#ifndef SCI_ROGER_ROGER_CAPABILITIES_H
#define SCI_ROGER_ROGER_CAPABILITIES_H

namespace Sci {
namespace Roger {

// What the *currently running* game can do, answered from live SCI engine state
// only — never from a game id (Hard Constraint 9). Populated once at provider
// startup and read-only thereafter. The single authoritative source for "what
// does this game support" at every render/UI decision point.
struct RogerCapabilities {
	bool isEga = false;        // EGA SCI0 view type (the only supported class)
	int  screenRows = 200;     // SCI script/play-area height in rows (SCI0 = 200)
	int  statusBarRows = 10;   // SCI0 status/menu bar height, derived from geometry
	bool hasParser = false;    // text-parser game (vs icon/menu-only)

	// Pure builder from explicit probe inputs. No engine access — unit-testable.
	// statusBarRows is the SCI0 status-bar height, a function of the screen class,
	// NOT of the game id.
	static RogerCapabilities fromProbes(bool isEga, bool hasParser, int sciScriptHeight) {
		RogerCapabilities c;
		c.isEga = isEga;
		c.hasParser = hasParser;
		c.screenRows = sciScriptHeight > 0 ? sciScriptHeight : 200;
		// SCI0's top menu/status bar is 10 rows of the 200-row screen.
		c.statusBarRows = 10;
		return c;
	}

	// Engine-reading entry point: probes live SCI state (g_sci / ResourceManager /
	// GfxScreen) and delegates to fromProbes(). Defined in roger_capabilities.cpp.
	static RogerCapabilities probe();
};

} // namespace Roger
} // namespace Sci

#endif
