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

#ifndef SCI_ROGER_ROGER_SELFTEST_H
#define SCI_ROGER_ROGER_SELFTEST_H

#include "sci/roger/roger_capabilities.h"

namespace Sci {
namespace Roger {

// Structural invariants checked per room for the *currently running* game. These
// are game-agnostic: they assert the generic pipeline produced a usable hires
// frame, with no per-game expectations.
struct SelfTestInputs {
	bool plateGenerated = false;   // hires plate generated/cached (no native fallthrough)
	int  plateW = 0, plateH = 0;   // actual plate dimensions
	int  expectW = 0, expectH = 0; // expected dimensions (pic geometry * upscale)
	bool priorityMapPresent = false; // omyacprio occlusion map generated + non-empty
	bool overlayEnabled = true;    // overlay still active (not disabled/rejected)
};

struct SelfTestResult {
	bool pass = false;
	const char *firstFailure = nullptr; // first invariant that failed, or nullptr
};

// Pure evaluation -- no engine access, unit-testable.
SelfTestResult evaluateInvariants(const SelfTestInputs &in, const RogerCapabilities &caps);

// Emit a single PASS/FAIL line keyed on gameId (data artifact, Constraint 9 OK).
void logSelfTest(const char *gameId, int picId, const SelfTestResult &r);

} // namespace Roger
} // namespace Sci

#endif
