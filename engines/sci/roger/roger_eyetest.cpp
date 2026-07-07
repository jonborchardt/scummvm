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

#include "sci/roger/roger_eyetest.h"

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

namespace Sci {
namespace Roger {

// Mutation position bias, tuned from the 2026-07-06 session's data: the head
// structure f f f f f f l _ _ _ held up, so concentrate the search on the tail
// (positions 7-9), probe the l-slots at 3 and 6 occasionally, and keep every
// position reachable (eyeMutate requires all weights >= 1; the 60/25/10/5
// large-mutation escape hatch is untouched).
static const int kEyeTailBias[kEyeSeqLen] = {1, 1, 1, 2, 1, 1, 2, 5, 5, 5};

RogerEyeTest::RogerEyeTest(const Common::String &gameId)
	: _assetGen(gameId, "", kGenMemory), _rng(g_system->getMillis()) {
}

RogerEyeTest::~RogerEyeTest() {
	if (_display) { _display->free(); delete _display; }
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
	const Common::String prefix = Common::String::format("eyetest-n%03d", _picId);
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

// gen 0: when _outDir/seeds.txt exists, its sequences ARE the generation — one
// compact string per line ('#' comments, blank lines ok), first line becomes
// the starting champion, and listed sequences are re-run even if a prior run
// already judged them. Otherwise: the base sequence + (kEyePop - 1) mutations.
void RogerEyeTest::seedGeneration0() {
	Common::FSNode seedNode((Common::Path(_outDir + "seeds.txt")));
	if (seedNode.exists() && seedNode.isReadable()) {
		Common::SeekableReadStream *in = seedNode.createReadStream();
		if (in) {
			while (!in->eos() && (int)_all.size() < 16) {
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
					warning("ROGER-EYETEST seeds.txt: '%s' is not %d chars of f/l/a — skipped",
					        clean.c_str(), kEyeSeqLen);
					continue;
				}
				bool dupSeed = false;
				for (uint s = 0; s < _all.size(); s++)
					if (eyeSeqCompact(_all[s].seq) == clean) { dupSeed = true; break; }
				if (dupSeed)
					continue;
				EyeCandidate c;
				c.gen = 0; c.idx = (int)_all.size();
				c.seq = seq;
				c.source = "seed";
				_all.push_back(c);
				_surf.push_back(nullptr);
				bool inSeen = false;
				for (uint s = 0; s < _seen.size(); s++)
					if (_seen[s] == clean) { inSeen = true; break; }
				if (!inSeen)
					_seen.push_back(clean);
			}
			delete in;
		}
		if (!_all.empty()) {
			debug("ROGER-EYETEST seeded gen 0 from seeds.txt: %u candidates, champion %s",
			      (uint)_all.size(), eyeSeqCompact(_all[0].seq).c_str());
			return;
		}
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

void RogerEyeTest::renderNewCandidates(uint firstIdx) {
	for (uint i = firstIdx; i < _all.size(); i++) {
		drawProgress(Common::String::format(
			"rendering gen %d candidate %u/%u: %s",
			_all[i].gen, (uint)(i - firstIdx + 1), (uint)(_all.size() - firstIdx),
			eyeSeqCompact(_all[i].seq).c_str()));
		_assetGen.setEnhancePasses(_all[i].seq);
		uint32 ms = 0;
		Graphics::Surface *plate = _assetGen.generatePlate(_picId, ms);
		if (!plate) {
			warning("ROGER-EYETEST: pic %d generation FAILED for %s",
			        _picId, eyeSeqCompact(_all[i].seq).c_str());
			continue; // startCompareQueue skips surface-less candidates
		}
		_all[i].file = eyeCandidateFileName(_picId, _all[i]);
		if (!dumpSurfacePng(*plate, _outDir + _all[i].file))
			warning("ROGER-EYETEST: could not write %s", (_outDir + _all[i].file).c_str());
		_surf[i] = plate;
		debug("ROGER-EYETEST rendered %s (%u ms)", _all[i].file.c_str(), ms);
	}
	writeManifests(); // file names are now known
}

void RogerEyeTest::startCompareQueue(uint firstIdx) {
	_queue.clear();
	_qPos = 0;
	for (uint i = firstIdx; i < _all.size(); i++)
		if ((int)i != _champion && _surf[i])
			_queue.push_back((int)i);
	_champIsA = _rng.below(2) == 0;
	_showingB = false;
	_phase = kPhaseCompare;
	_dirty = true;
	// Drain input queued during the blocking render batch: stale clicks /
	// key-repeats must not score pairs the user never saw. Quit still counts.
	Common::Event stale;
	while (g_system->getEventManager()->pollEvent(stale)) {
		if (stale.type == Common::EVENT_QUIT || stale.type == Common::EVENT_RETURN_TO_LAUNCHER) {
			finish();
			_quit = true;
		}
	}
	if (_queue.empty())
		endOfGeneration(); // every render failed — don't strand the UI
}

void RogerEyeTest::choose(int choice) {
	if (_phase != kPhaseCompare || _qPos >= (int)_queue.size())
		return;
	const int champ = _champion;
	const int chall = _queue[_qPos];

	EyeComparison rec;
	rec.aId = _all[_champIsA ? champ : chall].id(); // A/B labels, as shown to the user
	rec.bId = _all[_champIsA ? chall : champ].id();
	rec.choice = choice;
	rec.millis = g_system->getMillis();

	int winner = -1, loser = -1;
	if (choice == kEyeChoiceA) {
		winner = _champIsA ? champ : chall;
		loser  = _champIsA ? chall : champ;
	} else if (choice == kEyeChoiceB) {
		winner = _champIsA ? chall : champ;
		loser  = _champIsA ? champ : chall;
	}
	if (winner >= 0) {
		_all[winner].wins++;
		_all[loser].losses++;
		if (winner == chall)
			_champion = chall; // king of the hill
	} else if (choice == kEyeChoiceSame) {
		_all[champ].ties++;
		_all[chall].ties++;
	} // skip: no score movement

	rec.championAfter = _all[_champion].id();
	_history.push_back(rec);
	_choices.push_back(choice);
	writeManifests();

	_qPos++;
	_champIsA = _rng.below(2) == 0;
	_showingB = false; // every new pair starts on A
	_dirty = true;
	if (_qPos >= (int)_queue.size())
		endOfGeneration();
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
	// Free surfaces we no longer show; the champion stays (comparison anchor).
	for (uint i = 0; i < _surf.size(); i++) {
		if ((int)i != _champion && _surf[i]) {
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
	renderNewCandidates(firstIdx);
	startCompareQueue(firstIdx);
}

void RogerEyeTest::finish() {
	if (_phase == kPhaseDone)
		return;
	_phase = kPhaseDone;
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
		"  \"manifest\": \"manifest.json\",\n"
		"  \"comparisons_file\": \"comparisons.json\"\n"
		"}\n",
		_picId, w.id().c_str(), w.file.c_str(),
		eyeSeqCompact(w.seq).c_str(), spaced.c_str(),
		w.wins, w.ties, w.losses, _genNo + 1,
		(uint)_all.size(), (uint)_history.size(),
		eyeConverged(_choices, kEyeSameWindow) ? "true" : "false"));
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
static Common::Rect fitRect(const Graphics::Surface *src, const Common::Rect &area) {
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
	const int chall = (_qPos < (int)_queue.size()) ? _queue[_qPos] : _champion;
	const int aIdx = _champIsA ? _champion : chall;
	const int bIdx = _champIsA ? chall : _champion;
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
	if (_phase == kPhaseBanner) {
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
		static const char *labels[4] = {"1  A", "2  B", "3  Same", "4  Neither / skip"};
		for (int i = 0; i < 4; i++) {
			const int bw = (i == 3) ? 460 : 300;
			_btn[i] = Common::Rect(bx, barTop + 56, bx + bw, barTop + 116);
			_display->frameRect(_btn[i], white);
			if (lf)
				lf->drawString(_display, labels[i], _btn[i].left + 20, _btn[i].top + 16,
				               bw - 40, white);
			bx += bw + 30;
		}
		if (lf)
			lf->drawString(_display, Common::String::format(
				"gen %d   comparison %d/%u   Same in last %d: %d   candidates %u   Esc = stop",
				_genNo, _qPos + 1, (uint)_queue.size(), kEyeSameWindow,
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
		if (_phase == kPhaseBanner)
			nextGeneration();
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
	_outDir = dir + Common::String::format("eyetest-n%03d/", _picId);

	importPriorSeen(dir);    // old runs' judged sequences never re-proposed by breeding
	seedGeneration0();
	writeManifests();        // createPath=true creates _outDir before the first PNG
	renderNewCandidates(0);
	startCompareQueue(1);    // champion = candidate 0 (base or first seed); challengers follow

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
