# Roger Launcher Dialog Redesign

**Date:** 2026-06-26
**Status:** Approved, ready for implementation

## Problem

The `RogerLauncherDialog` has several UX and correctness issues:

1. **Skip button** — closes the dialog without saving settings; intent is unclear
2. **Settings don't visually update** on game selection (missing `scheduleTopDialogRedraw()`)
3. **No way to remove a game** from the list
4. **SQ3 shows raw gameId** ("sq3") instead of a friendly name when launched from command line (no `description` key in active domain)
5. **Precache button overlaps Fallback label** — layout doesn't have enough vertical room
6. **Launch is always enabled** — should require a game to be selected first
7. **No auto-precache on first launch** — user must manually precache before launch works well
8. **Progress label placement** — currently fights for space with the settings rows

## Approach

Approach B: layout redesign with right-side action column for Add/Delete, clean vertical spacing that eliminates all overlaps, and new auto-precache-on-launch behavior.

## Layout

```
┌────────────────────────────────────────────────────────┐
│  ROGER                                                 │
│                                                        │
│  GAMES                                                 │
│  ┌──────────────────────────────────┐  [+ Add Game  ] │
│  │ Space Quest III   55 pics cached │  [× Delete    ] │
│  │ Hero's Quest      not cached     │                  │
│  │                                  │                  │
│  └──────────────────────────────────┘                  │
│                                                        │
│  SETTINGS  (for selected game)                         │
│  Pre-cache    [all      ▾]                             │
│  Enhancement  [balanced ▾]                             │
│  Font         [GoMono   ▾]                             │
│  Fallback     [cache    ▾]                             │
│                                                        │
│  Caching: 12 / 55                                      │
│                          [Precache Now]  [  Launch  ] │
└────────────────────────────────────────────────────────┘
```

**Add / Delete column:** stacked to the right of the game list, flush with the list top, same button width. Delete is disabled until a row is selected.

**Settings:** always visible below the list, update on game selection (with redraw). Grayed when list is empty.

**Progress label:** dedicated row directly above the bottom button row — no overlap with settings.

**Launch:** starts disabled; enables when a game is selected. Skip button removed.

## Behavior

### Game selection (`kGameSelCmd`)
- Call `selectGame()` → `rebuildSettings()` → `scheduleTopDialogRedraw()`
- Enable Launch button and Delete button

### Delete (`kDeleteCmd`)
- Call `ConfMan.removeGameDomain(targetName)` + `ConfMan.flushToDisk()`
- Call `discoverGames()` + `rebuildGameList()`
- Reset selection to index 0 (or -1 if list now empty); re-disable Launch + Delete if empty

### SQ3 / missing description fix
- In `discoverGames()`, when `description` is empty after reading ConfMan, call `EngineMan.findGame(gameId)` to retrieve the friendly name from detection tables
- Fall back to raw `gameId` only if detection also fails

### Launch flow (new)
```
kLaunchCmd:
  selectedIsCurrent = (selectedGame.targetName == ConfMan.getActiveDomainName())
  if (selectedIsCurrent &&
      selectedGame.cache.picCount == 0 && selectedGame.cache.viewCount == 0):
    _launchAfterPrecache = true
    buildPrecacheQueues()          // auto-start precache (uses current g_sci resMan)
    updateProgress()
    scheduleTopDialogRedraw()
  else:
    handleLaunch() → close()      // game-switch: no precache possible for other game

handleTickle() precache completion:
  if (_launchAfterPrecache):
    handleLaunch() → close()
```

The progress label shows "Caching: N / M" during auto-precache so the user knows why the dialog is still open.

**Constraint:** auto-precache only triggers when the selected game is the currently-running one (`g_sci` owns its resource manager). If the user selects a different game that has no cache, the launcher switches games immediately — the new game's own launcher invocation will handle precaching on that game's startup.

### Manual Precache Now
Unchanged: starts precache independently, sets Cancel label, updates progress. Does not auto-launch when done.

## Code Cleanup

### Constructor
- Extract 4 settings rows into a `addSettingsRow(label, popup, cmd)` helper — eliminates the parallel-array fragility
- Move popup value arrays (`precacheVals[]`, `fontVals[]`, `enhanceVals[]`, `fbVals[]`) to file-scope constants; currently duplicated between constructor and `handleCommand`

### `handleCommand`
- Remove `kSkipCmd` case entirely
- Fix `kLaunchCmd`: old code had `close()` in both branches of an if/else (dead code); replaced by new auto-precache logic
- All popup handlers use the file-scope constants

### `rebuildSettings`
- Use file-scope constants (no longer duplicates the arrays)

### `roger_launcher.cpp`
- `buildPrecacheQueues()`: add `warning()` log when it no-ops due to `!g_sci` so the failure is not silent
- `precacheStep()`: add comment noting the O(n) `remove_at(0)` is acceptable for these queue sizes

### New members in `RogerLauncherDialog`
```cpp
GUI::ButtonWidget *_deleteBtn       = nullptr;
bool               _launchAfterPrecache = false;

enum {
    kDeleteCmd = 'RDEL',
    // ... existing
};
```

## Documentation

**`docs/roger.md`:** Update launcher section — no Skip button, Launch requires selection, auto-precache on first launch, Delete button.

**`CLAUDE.md`:** Add a short entry in the Roger Project section describing the `RogerLauncherDialog` / `RogerLauncher` split and the auto-precache-on-launch flow.

## Files Changed

| File | Change |
|------|--------|
| `engines/sci/roger/roger_launcher_dialog.h` | Add `_deleteBtn`, `_launchAfterPrecache`, `kDeleteCmd` |
| `engines/sci/roger/roger_launcher_dialog.cpp` | Layout redesign, all behavior changes, code cleanup |
| `engines/sci/roger/roger_launcher.cpp` | Warning log in `buildPrecacheQueues`, comment in `precacheStep` |
| `engines/sci/roger/roger_launcher.h` | No change expected |
| `docs/roger.md` | Launcher section update |
| `CLAUDE.md` | Launcher entry in Roger Project section |
