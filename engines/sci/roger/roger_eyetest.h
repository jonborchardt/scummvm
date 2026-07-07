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

#ifndef SCI_ROGER_ROGER_EYETEST_H
#define SCI_ROGER_ROGER_EYETEST_H

// TEMPORARY EXPERIMENT (eye-test genetic pass search, 2026-07-06). Studio-style
// interactive tool: renders small batches of OMYAC pass-sequence candidates for
// one pic and asks the user "Which looks better?" — the choices drive a simple
// genetic search (see roger_eyetest_search.h). Launched via ROGER_EYETEST=1
// (build_and_run.ps1 -EyeTest) at the same sci.cpp seam as RogerStudio; owns the
// overlay; never touches the generation disk cache (kGenMemory). DELETE this
// pair + the sci.cpp hook + the -EyeTest switch when the pass search is done.

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"
#include "sci/roger/roger_asset_gen.h"
#include "sci/roger/roger_eyetest_search.h"

namespace Graphics { class ManagedSurface; struct Surface; }
namespace Common { struct Event; }

namespace Sci {
namespace Roger {

class RogerEyeTest {
public:
	explicit RogerEyeTest(const Common::String &gameId);
	~RogerEyeTest();

	// Blocking loop; returns when the user finishes (Esc from the report screen).
	void run();

private:
	enum Phase { kPhaseCompare, kPhaseBanner, kPhaseDone };

	void seedGeneration0();                  // seeds.txt in _outDir, else base + mutations
	void importPriorSeen(const Common::String &shotsDir); // harvest judged seqs from old run dirs
	void renderCandidate(uint i);            // plate + eval cel -> PNG + kept surface
	void renderNewCandidates(uint firstIdx); // renderCandidate for _all[firstIdx..]
	void startCompareQueue(uint firstIdx);   // challengers = firstIdx.. vs champion
	void handleEvent(const Common::Event &ev);
	void choose(int choice);                 // EyeChoice for the CURRENT pair
	void endOfGeneration();                  // converged / budget banner or breed
	void nextGeneration();
	void finish();                           // idempotent: summary + report screen
	void writeManifests();                   // rewrite manifest.json + comparisons.json
	void writeSummary();
	void drawFrame();
	void drawProgress(const Common::String &msg); // full-screen status during renders
	void pushDisplay();

	int sameInLastWindow() const;

	static const int kBarH = 150;            // bottom prompt/button bar, overlay px

	RogerAssetGen _assetGen;                 // kGenMemory, empty cache dir
	Graphics::ManagedSurface *_display = nullptr;
	EyeRng _rng;
	Common::String _gameId;
	Common::Array<int> _picPool;             // per-game evaluation pics (see ctor)
	int _picId = 2;                          // CURRENT generation's pic (re-rolled per gen)
	Graphics::Surface *_celSurf = nullptr;   // shared eval cel (view 0/0/0), lazily generated
	Common::String _outDir;                  // <screenshotpath>/eyetest-<gameId>/

	Common::Array<EyeCandidate> _all;
	Common::Array<Graphics::Surface *> _surf; // parallel to _all; freed -> nullptr
	Common::Array<Common::String> _seen;      // compact strings ever generated
	Common::Array<EyeComparison> _history;
	Common::Array<int> _choices;              // EyeChoice history (convergence)

	int _champion = 0;                        // index into _all
	Common::Array<int> _queue;                // challenger indices, current gen
	int _qPos = 0;
	bool _champIsA = true;                    // which pair member is labeled A (randomized per pair)
	bool _showingB = false;                   // eye-exam flip state: currently displaying B
	int _genNo = 0;
	int _phase = kPhaseCompare;
	Common::String _banner;
	bool _summaryWritten = false;

	Common::Rect _btn[4];                     // A / B / Same / Neither hit rects
	Common::Rect _btnFlip;                    // Flip A<->B button
	Common::Rect _imageArea;                  // the single in-place image (click = flip)
	int _mouseX = 0, _mouseY = 0;
	bool _dirty = true;
	bool _quit = false;
};

} // namespace Roger
} // namespace Sci

#endif // SCI_ROGER_ROGER_EYETEST_H
