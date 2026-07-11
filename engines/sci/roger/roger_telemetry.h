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

#ifndef SCI_ROGER_ROGER_TELEMETRY_H
#define SCI_ROGER_ROGER_TELEMETRY_H

#include "common/scummsys.h"

namespace Sci {
namespace Roger {

// Per-cycle ROGER-CYCLE telemetry state. Kept as member state on the provider
// (non-const function statics are forbidden: they survive return-to-launcher /
// in-process restart and cannot be reset from outside the function).
//
// period = kernelAnimate entry-to-entry, measured between EMITTED cycles (an
// early-return cycle folds into the next period)  --  the walking-speed number.
// busy   = this cycle's entry to end-of-composite span.
// Arm/consume: frameStart arms; the first frameRendered consumes. A
// reAnimate-driven composite outside a cycle therefore never emits.
struct CycleTelemetry {
	uint32 prevT0 = 0;   // entry time of the last EMITTED cycle
	uint32 t0 = 0;       // entry time of the current cycle
	bool armed = false;

	void frameStart(uint32 nowMs) {
		t0 = nowMs;
		armed = true;
	}

	// Returns true when a telemetry line should be emitted for this cycle,
	// filling period/busy. Consumes the arm.
	bool frameRendered(uint32 nowMs, uint32 &period, uint32 &busy) {
		if (!armed)
			return false;
		armed = false;
		period = prevT0 ? t0 - prevT0 : 0;
		busy = nowMs - t0;
		prevT0 = t0;
		return true;
	}
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_TELEMETRY_H
