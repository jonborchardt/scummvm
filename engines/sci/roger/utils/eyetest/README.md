# Eye Exam — interactive OMYAC pass-sequence tuner

A quarantined dev utility that finds good OMYAC enhance-pass sequences the same
way an optometrist finds your prescription: it shows you one image, flips it
in place between two candidates ("1 or 2?"), and your choices are the fitness
function. Two modes: a **genetic search** (default) that evolves sequences from
your verdicts, and a **showdown** that round-robins a fixed list of finalists.

Everything is keyed to pass *sequences*: 10 positions, each `f` (fill), `l`
(line) or `a` (all) — e.g. the engine default `affffflaaa`.

---

## Launching

```powershell
.\build_and_run.ps1 -EyeTest              # SQ3 scenes (default)
.\build_and_run.ps1 -EyeTest -Game qfg1   # QFG1 scenes
```

`-EyeTest` sets the `ROGER_EYETEST` env var for that launch only; the env-gated
hook in `roger_register.cpp` (`FileRogerArtProvider::onEngineStartup()`)
starts the tool instead of the game (resources loaded, no game
scripts run). One game per run — the engine can only render the resources of
the game it booted, so the launch picks which scene pool (below) applies.

Everything reads/writes under `<screenshotpath>/eyetest-<gameId>/`
(e.g. `screenshots/eyetest-sq3/`). Control files (`seeds.txt`,
`showdown.txt`) go **in that folder**.

## The screen

- **One full-size image** — the current pair, eye-exam style. Flip between A
  and B **in place** with `Space`, `Tab`, clicking the image, or the Flip
  button. Both candidates occupy the same pixels, so differences pop. A
  "Showing: A/B" label sits top-left; every new pair starts on A.
- Every plate has the ego cel (view 0, loop 0, cel 0) composited at a fixed
  spot (native 160,150, bottom-centre), so plate + sprite treatment are judged
  together.
- **Choose** with `1` (A), `2` (B), `3` (Same), `4` (Neither/skip), or the
  bottom buttons. Which candidate is "A" is randomized per pair — trust the
  labels, not habit.
- **Undo** with `Backspace`, `U`, or the Undo button: reverts one judged pair
  per press, exactly as scored, repeatable back to the start of the current
  generation/round. When a generation/round finishes, a pause ("Enter = ...,
  Backspace = undo") keeps the last choice undoable until you commit it with
  `Enter`.
- **Esc** anywhere finishes: writes `summary.json` and shows the winner
  screen; `Esc` again quits.

## Genetic-search mode (default)

- **Generation 0**: the shipping default sequence (`kDefaultPassString` in
  `roger_passes.cpp` — the single swap point the game/picker also follow;
  currently `affffflaaa`) + 5 mutations of it — or your explicit `seeds.txt`
  list (below). The curated known-good winners live next to it as
  `goodPassPattern()` and appear as one-click picks in the game picker.
- Each generation you judge every candidate against the current **champion**
  (king-of-the-hill: beat the champion, become the champion).
- **Each generation rolls a random scene** from the per-game pool (defined in
  `roger_eyetest.cpp`, validated against the game's resources at startup); the
  champion is re-rendered on the new scene so every pair is apples-to-apples.
- Breeding per generation: 2 elites survive in the pool, 4 offspring from
  75% mutation / 15% crossover / 10% random restart. Mutations mostly change
  1 position (60/25/10/5% for 1/2/3/4–5 positions) and are position-biased
  toward the tail (weights `{1,1,1,2,1,1,2,5,5,5}` — see `kEyeTailBias`).
- Sequences already judged in previous runs of the same game
  (`eyetest-<gameId>*` folders — PNG filenames encode the sequence) are never
  re-proposed by breeding. Explicit seeds are exempt.
- **Stopping**: a banner suggests stopping when ≥50% of your last 12 choices
  were "Same" (convergence), or at generation 19 (budget). `Enter` overrides,
  `Esc` finishes. You can Esc at any moment.

### Adding seeds (`seeds.txt`)

Start the search from your own list instead of the base sequence: create
`<outDir>/seeds.txt` (e.g. `screenshots/eyetest-sq3/seeds.txt`):

```
# one compact sequence per line, '#' comments, blank lines ok
fffffflaaa   # first line = starting champion
ffflffaaaa
fffffflaal
```

- 1–16 entries; invalid/duplicate lines are warned and skipped.
- The listed sequences ARE generation 0 (they re-run even if previously
  judged); breeding takes over from generation 1.
- No file (or no valid lines) = default start. The file persists — edit or
  delete it before the next run, or it seeds that run too.

## Showdown mode (`showdown.txt`)

Pit specific finalists against each other — no breeding, no generations.
Create `<outDir>/showdown.txt` (same format as seeds.txt, **2–8 entrants**):

```
# finalists
ffffffflff   # sq new
fffffflaaa   # run-1 winner
ffflffaaaa   # original default
```

When the file exists, the launch arms showdown mode automatically (log:
`ROGER-EYETEST showdown armed: N entrants`) and `seeds.txt` is ignored.

- **Each round** = one random scene + the full round-robin (every pairing:
  4 entrants → 6 comparisons, 8 → 28).
- "Round done → Enter" rolls the next scene; keep going until the ranking
  feels settled.
- **Esc = final ranking**: winner screen, `ROGER-EYETEST showdown rank 1..N`
  lines in the run log, per-entrant W/T/L in `manifest.json`,
  `"showdown": true` in `summary.json`.
- Rename or delete `showdown.txt` to return to genetic-search runs.

## Output files (in `<outDir>`)

| File | What |
|---|---|
| `n<pic>_gen<G>_cand<K>_<source>__<sequence>.png` | every rendered candidate, named by scene + origin + sequence (the filenames ARE the record) |
| `manifest.json` | one object per candidate: sequence, source, parents, W/T/L — rewritten after every choice |
| `comparisons.json` | full choice history (A/B/same/skip, timestamps, champion after) — rewritten after every choice |
| `summary.json` | written on finish only: winner sequence/file/score, totals |

**A new run overwrites the three JSON files.** To keep a session's records,
copy the folder to a backup first (convention: `eyetest-sq3-run3` etc.).
Backups matching `eyetest-<gameId>*` still feed the never-re-propose dedup;
control files inside backups are ignored.

## Quarantine contract

Nothing in the engine may depend on `utils/eyetest/`. The only permitted
references: the env-gated `ROGER_EYETEST` hook in `roger_register.cpp`
(`onEngineStartup()`), the
`engines/sci/module.mk` object list, `build_tests.ps1`'s test registration
(`test/sci/roger/test_eyetest_search.h` covers the pure search module), and
the `build_and_run.ps1 -EyeTest` switch. This module may only consume stable
roger seams (`RogerAssetGen`, `png_loader`) — never provider/compositor
internals. It never touches the generation disk cache (`kGenMemory`), and a
launch without the env var is byte-identical to one where this folder doesn't
exist.
