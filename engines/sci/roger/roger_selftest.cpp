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

#include "sci/roger/roger_selftest.h"
#include "common/textconsole.h"

namespace Sci {
namespace Roger {

SelfTestResult evaluateInvariants(const SelfTestInputs &in, const RogerCapabilities &caps) {
	SelfTestResult r;
	if (!caps.isEga)            { r.firstFailure = "not EGA SCI0"; return r; }
	if (!in.overlayEnabled)     { r.firstFailure = "overlay disabled (native fallthrough)"; return r; }
	if (!in.plateGenerated)     { r.firstFailure = "plate not generated"; return r; }
	if (in.plateW <= 0 || in.plateH <= 0) { r.firstFailure = "plate has no dimensions"; return r; }
	if (in.plateW != in.expectW || in.plateH != in.expectH) { r.firstFailure = "plate dimension mismatch"; return r; }
	if (!in.priorityMapPresent) { r.firstFailure = "priority map missing"; return r; }
	r.pass = true;
	return r;
}

void logSelfTest(const char *gameId, int picId, const SelfTestResult &r) {
	if (r.pass)
		warning("ROGER selftest[%s] pic %d: PASS", gameId ? gameId : "?", picId);
	else
		warning("ROGER selftest[%s] pic %d: FAIL (%s)", gameId ? gameId : "?", picId,
		        r.firstFailure ? r.firstFailure : "?");
}

} // namespace Roger
} // namespace Sci
