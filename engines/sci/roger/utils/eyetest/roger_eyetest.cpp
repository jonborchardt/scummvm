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

#include "sci/roger/utils/eyetest/roger_eyetest.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
#include "sci/roger/png_loader.h"

#ifdef ENABLE_SCI
#include "sci/sci.h"
#include "sci/resource/resource.h"
#endif

namespace Sci {
namespace Roger {

// Shared evaluation-cel anchor (native coords, bottom-centre — the studio's
// default placement): every candidate composites the SAME cel at the SAME
// spot, so pairs differ only by their pass sequence.
static const int kEyeCelX = 160;
static const int kEyeCelY = 150;

// Mutation position bias, tuned empirically: the head structure f f f f f f l
// _ _ _ held up, so concentrate the search on the tail
// (positions 7-9), probe the l-slots at 3 and 6 occasionally, and keep every
// position reachable (eyeMutate requires all weights >= 1; the 60/25/10/5
// large-mutation escape hatch is untouched).
static const int kEyeTailBias[kEyeSeqLen] = {1, 1, 1, 2, 1, 1, 2, 5, 5, 5};

RogerEyeTest::RogerEyeTest(const Common::String &gameId)
	: _assetGen(gameId, "", kGenMemory), _rng(g_system->getMillis()), _gameId(gameId) {
	// Per-game evaluation pic pools: scenes worth judging pass sequences on.
	// The launched game decides which pool applies
	// (sq3 is the default launch; use build_and_run.ps1 -Game qfg1 for qfg1's).
	static const int sq3Pics[] = {2, 3, 4, 5, 7, 8, 13, 14, 15, 25, 28, 49, 52, 62, 69, 74, 81, 153, 156};
	static const int qfg1Pics[] = {10, 13, 16, 21, 28, 29, 30, 37, 38, 39, 40, 54, 65, 82, 88, 93, 94, 96, 97, 300, 301, 310, 320, 460};
	const int *pool = sq3Pics;
	uint poolLen = ARRAYSIZE(sq3Pics);
	if (gameId.hasPrefix("qfg1")) {
		pool = qfg1Pics;
		poolLen = ARRAYSIZE(qfg1Pics);
	}
#ifdef ENABLE_SCI
	// Keep only pics the running game actually has (same probe as RogerStudio).
	Common::Array<int> have;
	if (g_sci && g_sci->getResMan()) {
		Common::List<ResourceId> pics = g_sci->getResMan()->listResources(kResourceTypePic);
		for (Common::List<ResourceId>::iterator it = pics.begin(); it != pics.end(); ++it)
			have.push_back(it->getNumber());
	}
	for (uint i = 0; i < poolLen; i++) {
		bool found = false;
		for (uint h = 0; h < have.size(); h++)
			if (have[h] == pool[i]) { found = true; break; }
		if (found)
			_picPool.push_back(pool[i]);
		else
			warning("ROGER-EYETEST: pic %d not in %s resources - dropped from pool",
			        pool[i], gameId.c_str());
	}
#else
	for (uint i = 0; i < poolLen; i++)
		_picPool.push_back(pool[i]);
#endif
	if (_picPool.empty())
		_picPool.push_back(pool[0]); // generatePlate will report the failure
}

RogerEyeTest::~RogerEyeTest() {
	if (_display) { _display->free(); delete _display; }
	if (_celSurf) { _celSurf->free(); delete _celSurf; }
	for (uint i = 0; i < _surf.size(); i++)
		if (_surf[i]) { _surf[i]->free(); delete _surf[i]; }
}

// Harvest compact sequences already judged in previous runs so breeding never
// re-proposes them. The candidate PNG names encode the sequence after "__", so
// scanning filenames is a full import with zero JSON parsing: every
// "eyetest-n<pic>*" directory under screenshotpath (manual backups like
// eyetest-n002-run1 included, plus leftovers in the active dir) contributes.
// Explicit seeds.txt entries are exempt — a listed sequence is always re-run.
void RogerEyeTest::importPriorSeen(const Common::String &shotsDir) {
	Common::FSNode root((Common::Path(shotsDir)));
	Common::FSList dirs;
	if (!root.exists() || !root.getChildren(dirs, Common::FSNode::kListDirectoriesOnly))
		return;
	// Scoped to THIS game's multi-pic era (eyetest-<gameId>*). The single-pic
	// era's eyetest-n002* backups are deliberately NOT harvested: those
	// verdicts were pic-2-only and the user keeps them as future seed material.
	const Common::String prefix = "eyetest-" + _gameId;
	int imported = 0;
	for (uint d = 0; d < dirs.size(); d++) {
		if (!dirs[d].getName().hasPrefix(prefix))
			continue;
		Common::FSList files;
		if (!dirs[d].getChildren(files, Common::FSNode::kListFilesOnly))
			continue;
		for (uint f = 0; f < files.size(); f++) {
			const Common::String name = files[f].getName();
			if (!name.hasSuffix(".png") || name.size() < (uint)kEyeSeqLen + 6)
				continue;
			const Common::String compact(name.c_str() + name.size() - 4 - kEyeSeqLen, kEyeSeqLen);
			EyeSeq seq;
			if (!eyeParseCompact(compact, seq))
				continue;
			bool dup = false;
			for (uint s = 0; s < _seen.size(); s++)
				if (_seen[s] == compact) { dup = true; break; }
			if (!dup) {
				_seen.push_back(compact);
				imported++;
			}
		}
	}
	if (imported)
		debug("ROGER-EYETEST imported %d previously judged sequences from %s%s*",
		      imported, shotsDir.c_str(), prefix.c_str());
}

// Read a compact-sequence list file: one sequence per line, '#' comments,
// blank lines ok, invalid lines warned and skipped, duplicates skipped.
static void readSeqList(const Common::String &path, uint maxN, Common::Array<EyeSeq> &out) {
	Common::FSNode node((Common::Path(path)));
	if (!node.exists() || !node.isReadable())
		return;
	Common::SeekableReadStream *in = node.createReadStream();
	if (!in)
		return;
	while (!in->eos() && out.size() < maxN) {
		Common::String line = in->readLine();
		Common::String clean;
		for (uint i = 0; i < line.size(); i++) {
			if (line[i] == '#')
				break;
			clean += line[i];
		}
		clean.trim();
		if (clean.empty())
			continue;
		EyeSeq seq;
		if (!eyeParseCompact(clean, seq)) {
			warning("ROGER-EYETEST %s: '%s' is not %d chars of f/l/a — skipped",
			        path.c_str(), clean.c_str(), kEyeSeqLen);
			continue;
		}
		bool dup = false;
		for (uint s = 0; s < out.size(); s++)
			if (eyeSeqCompact(out[s]) == clean) { dup = true; break; }
		if (!dup)
			out.push_back(seq);
	}
	delete in;
}

// gen 0: when _outDir/seeds.txt exists, its sequences ARE the generation — one
// compact string per line ('#' comments, blank lines ok), first line becomes
// the starting champion, and listed sequences are re-run even if a prior run
// already judged them. Otherwise: the base sequence + (kEyePop - 1) mutations.
void RogerEyeTest::seedGeneration0() {
	Common::Array<EyeSeq> seeds;
	readSeqList(_outDir + "seeds.txt", 16, seeds);
	if (!seeds.empty()) {
		for (uint k = 0; k < seeds.size(); k++) {
			EyeCandidate c;
			c.gen = 0; c.idx = (int)k;
			c.seq = seeds[k];
			c.source = "seed";
			_all.push_back(c);
			_surf.push_back(nullptr);
			const Common::String compact = eyeSeqCompact(c.seq);
			bool inSeen = false;
			for (uint s = 0; s < _seen.size(); s++)
				if (_seen[s] == compact) { inSeen = true; break; }
			if (!inSeen)
				_seen.push_back(compact);
		}
		debug("ROGER-EYETEST seeded gen 0 from seeds.txt: %u candidates, champion %s",
		      (uint)_all.size(), eyeSeqCompact(_all[0].seq).c_str());
		return;
	}

	EyeCandidate base;
	base.gen = 0; base.idx = 0;
	base.seq = eyeBaseSeq();
	base.source = "base";
	_all.push_back(base);
	_surf.push_back(nullptr);
	_seen.push_back(eyeSeqCompact(base.seq));
	for (int k = 1; k < kEyePop; k++) {
		EyeCandidate c;
		c.gen = 0; c.idx = k;
		bool fresh = false;
		for (int attempt = 0; attempt < 20 && !fresh; attempt++) {
			c.seq = eyeMutate(base.seq, _rng, c.source, kEyeTailBias);
			fresh = true;
			for (uint s = 0; s < _seen.size(); s++)
				if (_seen[s] == eyeSeqCompact(c.seq))
					fresh = false;
		}
		c.parents.push_back(base.id());
		_seen.push_back(eyeSeqCompact(c.seq));
		_all.push_back(c);
		_surf.push_back(nullptr);
	}
}

void RogerEyeTest::renderCandidate(uint i) {
	drawProgress(Common::String::format(
		"rendering gen %d pic %d: %s",
		_all[i].gen, _picId, eyeSeqCompact(_all[i].seq).c_str()));
	_assetGen.setEnhancePasses(_all[i].seq);
	uint32 ms = 0;
	Graphics::Surface *plate = _assetGen.generatePlate(_picId, ms);
	if (!plate) {
		warning("ROGER-EYETEST: pic %d generation FAILED for %s",
		        _picId, eyeSeqCompact(_all[i].seq).c_str());
		return; // startCompareQueue skips surface-less candidates
	}
	// Composite the shared evaluation cel (view 0 / loop 0 / cel 0 — the ego)
	// game-style over the plate, so plate and sprite treatment are judged
	// TOGETHER. The cel is pass-independent (scale6x, not omyac), so one
	// generated surface serves every candidate; same compose pattern as
	// RogerStudio::renderSlot.
	if (!_celSurf) {
		uint32 cms = 0;
		_celSurf = _assetGen.generateViewCel(0, 0, 0, cms);
		if (!_celSurf)
			warning("ROGER-EYETEST: view 0/0/0 cel generation failed - plates only");
	}
	if (_celSurf) {
		Graphics::ManagedSurface composed(plate->w, plate->h, plate->format);
		composed.blitFrom(*plate);
		const int dx = kEyeCelX * 6 - _celSurf->w / 2;
		const int dy = kEyeCelY * 6 - _celSurf->h;
		composed.blendBlitFrom(*_celSurf, Common::Rect(0, 0, _celSurf->w, _celSurf->h),
		                       Common::Rect(dx, dy, dx + _celSurf->w, dy + _celSurf->h),
		                       Graphics::FLIP_NONE);
		plate->copyFrom(composed.rawSurface());
	}
	if (_surf[i]) { _surf[i]->free(); delete _surf[i]; _surf[i] = nullptr; }
	_all[i].file = eyeCandidateFileName(_picId, _all[i]);
	if (!dumpSurfacePng(*plate, _outDir + _all[i].file))
		warning("ROGER-EYETEST: could not write %s", (_outDir + _all[i].file).c_str());
	_surf[i] = plate;
	debug("ROGER-EYETEST rendered %s (%u ms)", _all[i].file.c_str(), ms);
}

void RogerEyeTest::renderNewCandidates(uint firstIdx) {
	for (uint i = firstIdx; i < _all.size(); i++)
		renderCandidate(i);
	writeManifests(); // file names are now known
}

// Drain input queued during a blocking render batch: stale clicks /
// key-repeats must not score pairs the user never saw. Quit still counts.
void RogerEyeTest::drainStaleInput() {
	Common::Event stale;
	while (g_system->getEventManager()->pollEvent(stale)) {
		if (stale.type == Common::EVENT_QUIT || stale.type == Common::EVENT_RETURN_TO_LAUNCHER) {
			finish();
			_quit = true;
		}
	}
}

void RogerEyeTest::startCompareQueue(uint firstIdx) {
	_queue.clear();
	_qPos = 0;
	_undo.clear(); // undo window = the current generation
	for (uint i = firstIdx; i < _all.size(); i++)
		if ((int)i != _champion && _surf[i])
			_queue.push_back((int)i);
	_champIsA = _rng.below(2) == 0;
	_showingB = false;
	_phase = kPhaseCompare;
	_dirty = true;
	drainStaleInput();
	if (_queue.empty())
		endOfGeneration(); // every render failed — don't strand the UI
}

// showdown.txt (same format as seeds.txt, 2-8 entrants) arms showdown mode:
// no breeding, no generations — round after round of full round-robins, each
// on a fresh random scene, until Esc reports the final ranking.
bool RogerEyeTest::loadShowdown() {
	Common::Array<EyeSeq> entrants;
	readSeqList(_outDir + "showdown.txt", 8, entrants);
	if (entrants.size() < 2)
		return false;
	for (uint k = 0; k < entrants.size(); k++) {
		EyeCandidate c;
		c.gen = 0; c.idx = (int)k;
		c.seq = entrants[k];
		c.source = "showdown";
		_all.push_back(c);
		_surf.push_back(nullptr);
		_seen.push_back(eyeSeqCompact(c.seq));
	}
	debug("ROGER-EYETEST showdown armed: %u entrants", (uint)_all.size());
	return true;
}

void RogerEyeTest::startShowdownRound() {
	// New scene per round; every entrant re-renders on it.
	_picId = _picPool[_rng.below(_picPool.size())];
	for (uint i = 0; i < _surf.size(); i++) {
		if (_surf[i]) {
			_surf[i]->free();
			delete _surf[i];
			_surf[i] = nullptr;
		}
	}
	renderNewCandidates(0);
	_pairA.clear();
	_pairB.clear();
	for (uint i = 0; i < _all.size(); i++)
		for (uint j = i + 1; j < _all.size(); j++)
			if (_surf[i] && _surf[j]) {
				_pairA.push_back((int)i);
				_pairB.push_back((int)j);
			}
	_qPos = 0;
	_undo.clear(); // undo window = the current round
	_champIsA = _rng.below(2) == 0;
	_showingB = false;
	_phase = kPhaseCompare;
	_dirty = true;
	drainStaleInput();
	if (_pairA.empty())
		finish(); // nothing comparable this round — end with the ranking
}

// The pair at schedule position `pos`: GA mode pits the champion against the
// queued challenger; showdown mode walks the round-robin schedule. `pos` is
// clamped so the gen-done pause keeps displaying the LAST judged pair.
void RogerEyeTest::pairAt(int pos, int &pa, int &pb) const {
	const int n = pairCount();
	pos = CLIP(pos, 0, MAX(0, n - 1));
	if (_showdown) {
		pa = _pairA.empty() ? _champion : _pairA[pos];
		pb = _pairB.empty() ? _champion : _pairB[pos];
	} else {
		pa = _champion;
		pb = _queue.empty() ? _champion : _queue[pos];
	}
}

// Revert one judged pair, exactly as scored — repeatable back to the start of
// the current generation/round (a bred generation is final: its choices
// already shaped the offspring). The pair is re-shown for a fresh answer.
void RogerEyeTest::undoLast() {
	if (_phase == kPhaseDone || _undo.empty())
		return;
	const UndoRec u = _undo.back();
	_undo.pop_back();
	if (u.winner >= 0) {
		_all[u.winner].wins--;
		_all[u.loser].losses--;
	}
	if (u.tie) {
		_all[u.pa].ties--;
		_all[u.pb].ties--;
	}
	_champion = u.champBefore;
	if (!_history.empty())
		_history.pop_back();
	if (!_choices.empty())
		_choices.pop_back();
	_qPos--;
	_phase = kPhaseCompare;
	_champIsA = _rng.below(2) == 0;
	_showingB = false;
	writeManifests();
	_dirty = true;
}

void RogerEyeTest::choose(int choice) {
	if (_phase != kPhaseCompare || _qPos >= pairCount())
		return;
	int pa, pb;
	pairAt(_qPos, pa, pb);

	EyeComparison rec;
	rec.aId = _all[_champIsA ? pa : pb].id(); // A/B labels, as shown to the user
	rec.bId = _all[_champIsA ? pb : pa].id();
	rec.choice = choice;
	rec.millis = g_system->getMillis();

	UndoRec u;
	u.pa = pa;
	u.pb = pb;
	u.champBefore = _champion;

	int winner = -1, loser = -1;
	if (choice == kEyeChoiceA) {
		winner = _champIsA ? pa : pb;
		loser  = _champIsA ? pb : pa;
	} else if (choice == kEyeChoiceB) {
		winner = _champIsA ? pb : pa;
		loser  = _champIsA ? pa : pb;
	}
	if (winner >= 0) {
		_all[winner].wins++;
		_all[loser].losses++;
		u.winner = winner;
		u.loser = loser;
		if (!_showdown && winner == pb)
			_champion = pb; // king of the hill (GA mode only)
	} else if (choice == kEyeChoiceSame) {
		_all[pa].ties++;
		_all[pb].ties++;
		u.tie = true;
	} // skip: no score movement

	rec.championAfter = _all[_champion].id();
	_history.push_back(rec);
	_choices.push_back(choice);
	_undo.push_back(u);
	writeManifests();

	_qPos++;
	_champIsA = _rng.below(2) == 0;
	_showingB = false; // every new pair starts on A
	_dirty = true;
	if (_qPos >= pairCount()) {
		// Pause instead of breeding/rolling immediately: the last choice of a
		// generation/round stays undoable until Enter commits it.
		_phase = kPhaseGenDone;
		_banner = _showdown
			? "Round done.  Enter = new scene, Backspace = undo, Esc = final ranking."
			: "Generation done.  Enter = breed the next one, Backspace = undo, Esc = finish.";
	}
}

void RogerEyeTest::endOfGeneration() {
	if (eyeConverged(_choices, kEyeSameWindow)) {
		_phase = kPhaseBanner;
		_banner = "Looks converged (>=50% Same in the last 12). Enter = keep searching, Esc = finish.";
	} else if (_genNo + 1 >= kEyeMaxGens ||
	           (int)_all.size() + (kEyePop - kEyeElite) > kEyeMaxCandidates) {
		_phase = kPhaseBanner;
		_banner = "Budget reached (generations/candidates). Enter = continue anyway, Esc = finish.";
	} else {
		nextGeneration();
	}
	_dirty = true;
}

void RogerEyeTest::nextGeneration() {
	_genNo++;
	// New generation, new scene: every pair within a generation shares one
	// pool pic (re-rolled here), so comparisons stay apples-to-apples while
	// the search samples many rooms across the run.
	_picId = _picPool[_rng.below(_picPool.size())];
	// A pic change stales EVERY cached render, the champion's included — free
	// them all; the champion re-renders on the new pic below.
	for (uint i = 0; i < _surf.size(); i++) {
		if (_surf[i]) {
			_surf[i]->free();
			delete _surf[i];
			_surf[i] = nullptr;
		}
	}
	Common::Array<int> pool = eyeRankPool(_all, kEyePop);
	const uint firstIdx = _all.size();
	Common::Array<EyeCandidate> kids =
		eyeBreed(_all, pool, _genNo, kEyePop - kEyeElite, _rng, _seen, kEyeTailBias);
	for (uint k = 0; k < kids.size(); k++) {
		_all.push_back(kids[k]);
		_surf.push_back(nullptr);
	}
	writeManifests();
	renderCandidate((uint)_champion); // the anchor joins the new scene
	renderNewCandidates(firstIdx);
	startCompareQueue(firstIdx);
}

void RogerEyeTest::finish() {
	if (_phase == kPhaseDone)
		return;
	_phase = kPhaseDone;
	if (_showdown && !_all.empty()) {
		// Final ranking: net wins decide; the top entrant becomes the reported
		// winner. Full standings go to the run log (per-entrant W/T/L is also
		// in manifest.json).
		Common::Array<int> rank = eyeRankPool(_all, (int)_all.size());
		_champion = rank[0];
		for (uint r = 0; r < rank.size(); r++) {
			const EyeCandidate &c = _all[rank[r]];
			debug("ROGER-EYETEST showdown rank %u: %s score %d (W%d T%d L%d)",
			      r + 1, eyeSeqCompact(c.seq).c_str(), c.score(), c.wins, c.ties, c.losses);
		}
	}
	writeSummary();
	const EyeCandidate &w = _all[_champion];
	Common::String spaced;
	for (uint i = 0; i < w.seq.size(); i++)
		spaced += Common::String::format("%s%c", i ? " " : "", eyePassChar(w.seq[i]));
	debug("ROGER-EYETEST winner id=%s seq=[%s] file=%s score=%d (W%d T%d L%d) "
	      "comparisons=%u outDir=%s",
	      w.id().c_str(), spaced.c_str(), w.file.c_str(), w.score(),
	      w.wins, w.ties, w.losses, (uint)_history.size(), _outDir.c_str());
	_dirty = true;
}

void RogerEyeTest::writeManifests() {
	Common::DumpFile mf;
	if (mf.open(Common::Path(_outDir + "manifest.json"), true)) { // createPath makes _outDir
		Common::String out = "[\n";
		for (uint i = 0; i < _all.size(); i++)
			out += "  " + eyeCandidateJson(_picId, _all[i]) +
			       (i + 1 < _all.size() ? ",\n" : "\n");
		out += "]\n";
		mf.writeString(out);
		mf.close();
	}
	Common::DumpFile cf;
	if (cf.open(Common::Path(_outDir + "comparisons.json"), true)) {
		Common::String out = "[\n";
		for (uint i = 0; i < _history.size(); i++)
			out += "  " + eyeComparisonJson(_history[i]) +
			       (i + 1 < _history.size() ? ",\n" : "\n");
		out += "]\n";
		cf.writeString(out);
		cf.close();
	}
}

void RogerEyeTest::writeSummary() {
	if (_summaryWritten)
		return;
	_summaryWritten = true;
	const EyeCandidate &w = _all[_champion];
	Common::String spaced;
	for (uint i = 0; i < w.seq.size(); i++)
		spaced += Common::String::format("%s%c", i ? " " : "", eyePassChar(w.seq[i]));
	Common::DumpFile sf;
	if (!sf.open(Common::Path(_outDir + "summary.json"), true))
		return;
	sf.writeString(Common::String::format(
		"{\n"
		"  \"picture\": \"n%03d\",\n"
		"  \"winner_id\": \"%s\",\n"
		"  \"winner_file\": \"%s\",\n"
		"  \"winner_sequence\": \"%s\",\n"
		"  \"winner_sequence_spaced\": \"%s\",\n"
		"  \"wins\": %d, \"ties\": %d, \"losses\": %d,\n"
		"  \"generations\": %d,\n"
		"  \"candidates\": %u,\n"
		"  \"comparisons\": %u,\n"
		"  \"converged\": %s,\n"
		"  \"showdown\": %s,\n"
		"  \"manifest\": \"manifest.json\",\n"
		"  \"comparisons_file\": \"comparisons.json\"\n"
		"}\n",
		_picId, w.id().c_str(), w.file.c_str(),
		eyeSeqCompact(w.seq).c_str(), spaced.c_str(),
		w.wins, w.ties, w.losses, _genNo + 1,
		(uint)_all.size(), (uint)_history.size(),
		eyeConverged(_choices, kEyeSameWindow) ? "true" : "false",
		_showdown ? "true" : "false"));
	sf.close();
}

int RogerEyeTest::sameInLastWindow() const {
	int same = 0;
	const int n = (int)_choices.size();
	for (int i = MAX(0, n - kEyeSameWindow); i < n; i++)
		if (_choices[i] == kEyeChoiceSame)
			same++;
	return same;
}

void RogerEyeTest::pushDisplay() {
	g_system->copyRectToOverlay(_display->getPixels(), _display->pitch,
	                            0, 0, _display->w, _display->h);
	g_system->updateScreen();
}

void RogerEyeTest::drawProgress(const Common::String &msg) {
	_display->fillRect(Common::Rect(_display->w, _display->h),
	                   _display->format.RGBToColor(0, 0, 0));
	const Graphics::Font *lf = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	if (lf)
		lf->drawString(_display, msg, 40, _display->h / 2, _display->w - 80,
		               _display->format.RGBToColor(255, 255, 255));
	pushDisplay();
}

// Fit src into area preserving aspect, centered. Display-only scaled blit
// (same use as RogerStudio::blitRender; plate-alignment precision is moot here).
// Degenerate sources yield an empty rect (blitFrom with an empty dest is a no-op).
static Common::Rect fitRect(const Graphics::Surface *src, const Common::Rect &area) {
	if (!src || src->w <= 0 || src->h <= 0)
		return Common::Rect();
	const float sc = MIN((float)area.width() / src->w, (float)area.height() / src->h);
	const int w = (int)(src->w * sc), h = (int)(src->h * sc);
	const int x = area.left + (area.width() - w) / 2;
	const int y = area.top + (area.height() - h) / 2;
	return Common::Rect(x, y, x + w, y + h);
}

void RogerEyeTest::drawFrame() {
	const Graphics::PixelFormat fmt = _display->format;
	const uint32 black = fmt.RGBToColor(0, 0, 0);
	const uint32 white = fmt.RGBToColor(255, 255, 255);
	const uint32 grey = fmt.RGBToColor(150, 150, 150);
	_display->fillRect(Common::Rect(_display->w, _display->h), black);
	const Graphics::Font *lf = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
	const int barTop = _display->h - kBarH;

	if (_phase == kPhaseDone) {
		const Graphics::Surface *s = _surf[_champion];
		if (s) {
			const Common::Rect area(40, 40, _display->w - 40, barTop - 20);
			_display->blitFrom(*s, Common::Rect(0, 0, s->w, s->h), fitRect(s, area));
		}
		if (lf) {
			const EyeCandidate &w = _all[_champion];
			Common::String spaced;
			for (uint i = 0; i < w.seq.size(); i++)
				spaced += Common::String::format("%s%c", i ? " " : "", eyePassChar(w.seq[i]));
			lf->drawString(_display, Common::String::format(
				"WINNER %s   seq: %s   score %d (W%d T%d L%d)   %u comparisons",
				w.id().c_str(), spaced.c_str(), w.score(), w.wins, w.ties, w.losses,
				(uint)_history.size()), 40, barTop + 20, _display->w - 80, white);
			lf->drawString(_display, "saved: " + _outDir + w.file,
			               40, barTop + 60, _display->w - 80, grey);
			lf->drawString(_display, "manifest.json / comparisons.json / summary.json alongside.  Esc quits.",
			               40, barTop + 100, _display->w - 80, grey);
		}
		pushDisplay();
		return;
	}

	// Compare / banner: the current pair, eye-exam style — ONE image at a time,
	// flipped in place (Space / Tab / click on the image / Flip button) so both
	// candidates occupy the exact same pixels and differences pop.
	int pa, pb;
	pairAt(_qPos, pa, pb); // clamped: the gen-done pause keeps the last pair up
	const int aIdx = _champIsA ? pa : pb;
	const int bIdx = _champIsA ? pb : pa;
	const int shown = _showingB ? bIdx : aIdx;
	_imageArea = Common::Rect(10, 10, _display->w - 10, barTop - 10);
	if (_surf[shown])
		_display->blitFrom(*_surf[shown], Common::Rect(0, 0, _surf[shown]->w, _surf[shown]->h),
		                   fitRect(_surf[shown], _imageArea));
	if (lf)
		lf->drawString(_display, Common::String::format("Showing:  %s", _showingB ? "B" : "A"),
		               _imageArea.left + 10, _imageArea.top + 6, 400, white);

	// Bottom bar: prompt, buttons, status.
	_display->hLine(0, barTop, _display->w - 1, grey);
	if (_phase == kPhaseBanner || _phase == kPhaseGenDone) {
		if (lf)
			lf->drawString(_display, _banner, 40, barTop + 55, _display->w - 80, white);
	} else {
		if (lf)
			lf->drawString(_display, "Which looks better?  (Space or click the image flips A/B)",
			               40, barTop + 12, _display->w - 80, white);
		int bx = 40;
		// Flip button first: the eye-exam lens switch.
		_btnFlip = Common::Rect(bx, barTop + 56, bx + 380, barTop + 116);
		_display->frameRect(_btnFlip, white);
		if (lf)
			lf->drawString(_display, Common::String::format("Flip  (showing %s)", _showingB ? "B" : "A"),
			               _btnFlip.left + 20, _btnFlip.top + 16, 340, white);
		bx += 380 + 30;
		static const char *const labels[4] = {"1  A", "2  B", "3  Same", "4  Neither / skip"};
		for (int i = 0; i < 4; i++) {
			const int bw = (i == 3) ? 460 : 300;
			_btn[i] = Common::Rect(bx, barTop + 56, bx + bw, barTop + 116);
			_display->frameRect(_btn[i], white);
			if (lf)
				lf->drawString(_display, labels[i], _btn[i].left + 20, _btn[i].top + 16,
				               bw - 40, white);
			bx += bw + 30;
		}
		_btnUndo = Common::Rect(bx, barTop + 56, bx + 280, barTop + 116);
		_display->frameRect(_btnUndo, _undo.empty() ? grey : white);
		if (lf)
			lf->drawString(_display, "Undo (Bksp)", _btnUndo.left + 20, _btnUndo.top + 16,
			               240, _undo.empty() ? grey : white);
		bx += 280 + 30;
		if (lf)
			lf->drawString(_display, _showdown
				? Common::String::format(
					"round %d   pair %d/%d   pic %d   entrants %u   Esc = ranking",
					_genNo + 1, _qPos + 1, pairCount(), _picId, (uint)_all.size())
				: Common::String::format(
					"gen %d   pair %d/%d   pic %d   Same in last %d: %d   candidates %u   Esc = stop",
					_genNo, _qPos + 1, pairCount(), _picId, kEyeSameWindow,
					sameInLastWindow(), (uint)_all.size()),
				bx, barTop + 76, _display->w - bx - 20, grey);
	}

	// Crosshair (hardware cursor is invisible over the overlay — same as studio).
	_display->hLine(MAX(0, _mouseX - 8), _mouseY, MIN((int)_display->w - 1, _mouseX + 8), white);
	_display->vLine(_mouseX, MAX(0, _mouseY - 8), MIN((int)_display->h - 1, _mouseY + 8), white);

	pushDisplay();
}

void RogerEyeTest::handleEvent(const Common::Event &ev) {
	switch (ev.type) {
	case Common::EVENT_QUIT:
	case Common::EVENT_RETURN_TO_LAUNCHER:
		finish(); // summary is written even on window close
		_quit = true;
		return;
	case Common::EVENT_MOUSEMOVE:
		// Overlay coords (showOverlay(true) — same as RogerStudio::handleEvent).
		_mouseX = ev.mouse.x;
		_mouseY = ev.mouse.y;
		_dirty = true;
		return;
	case Common::EVENT_LBUTTONDOWN: {
		const int16 mx = ev.mouse.x, my = ev.mouse.y;
		if (_phase == kPhaseCompare) {
			for (int i = 0; i < 4; i++)
				if (_btn[i].contains(mx, my)) { choose(i); return; }
			if (_btnFlip.contains(mx, my)) { _showingB = !_showingB; _dirty = true; return; }
			if (_btnUndo.contains(mx, my)) { undoLast(); return; }
		}
		// Clicking the image flips the lens (compare AND banner — the pair stays up).
		if (_phase != kPhaseDone && _imageArea.contains(mx, my)) {
			_showingB = !_showingB;
			_dirty = true;
		}
		return;
	}
	case Common::EVENT_KEYDOWN:
		break;
	default:
		return;
	}

	switch (ev.kbd.keycode) {
	case Common::KEYCODE_ESCAPE:
		if (_phase == kPhaseDone)
			_quit = true;
		else
			finish();
		break;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		if (_phase == kPhaseGenDone) {
			// Commit the generation/round: past this point its choices are final.
			if (_showdown) {
				_genNo++;
				startShowdownRound();
			} else {
				endOfGeneration(); // converged/budget banner, or breed
			}
		} else if (_phase == kPhaseBanner) {
			nextGeneration();
		}
		break;
	case Common::KEYCODE_BACKSPACE:
	case Common::KEYCODE_u:
		undoLast(); // repeatable; no-op once the generation/round was committed
		break;
	case Common::KEYCODE_SPACE:
	case Common::KEYCODE_TAB:
		if (_phase != kPhaseDone) {
			_showingB = !_showingB;
			_dirty = true;
		}
		break;
	case Common::KEYCODE_1: case Common::KEYCODE_a:
		choose(kEyeChoiceA);
		break;
	case Common::KEYCODE_2: case Common::KEYCODE_b:
		choose(kEyeChoiceB);
		break;
	case Common::KEYCODE_3: case Common::KEYCODE_s:
		choose(kEyeChoiceSame);
		break;
	case Common::KEYCODE_4: case Common::KEYCODE_n:
		choose(kEyeChoiceSkip);
		break;
	default:
		break;
	}
}

void RogerEyeTest::run() {
	g_system->showOverlay(true); // inGUI: mouse events arrive in overlay coords
	CursorMan.showMouse(false);
	_display = new Graphics::ManagedSurface(
		g_system->getOverlayWidth(), g_system->getOverlayHeight(),
		g_system->getOverlayFormat());

	// Output dir: same screenshotpath lookup as RogerStudio::exportShown.
	Common::String dir;
	if (ConfMan.hasKey("screenshotpath"))
		dir = ConfMan.getPath("screenshotpath").toString('/');
	if (dir.empty())
		dir = "screenshots";
	if (dir.lastChar() != '/')
		dir += '/';
	_outDir = dir + Common::String::format("eyetest-%s/", _gameId.c_str());

	_picId = _picPool[_rng.below(_picPool.size())]; // generation 0's scene
	if (loadShowdown()) {
		// showdown.txt present: round-robin the listed finalists, no GA.
		_showdown = true;
		writeManifests();    // createPath=true creates _outDir before the first PNG
		startShowdownRound();
	} else {
		importPriorSeen(dir); // this game's judged sequences never re-proposed by breeding
		seedGeneration0();
		writeManifests();
		renderNewCandidates(0);
		startCompareQueue(1); // champion = candidate 0 (base or first seed); challengers follow
	}

	{
		const Common::Point p = g_system->getEventManager()->getMousePos();
		_mouseX = p.x;
		_mouseY = p.y;
	}
	Common::EventManager *em = g_system->getEventManager();
	while (!_quit && !em->shouldQuit()) {
		Common::Event ev;
		while (em->pollEvent(ev))
			handleEvent(ev);
		if (_dirty) {
			drawFrame();
			_dirty = false;
		}
		g_system->delayMillis(10);
	}
	finish(); // no-op if already done; guarantees summary.json exists
}

} // namespace Roger
} // namespace Sci
