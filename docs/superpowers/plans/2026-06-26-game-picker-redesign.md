# Roger Launcher Dialog Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the RogerLauncherDialog — fix layout overlaps, remove Skip, add Delete, fix SQ3 name, disable Launch until selected, auto-precache on first launch.

**Architecture:** All behavior changes are in `roger_launcher_dialog.{h,cpp}`. Minor changes to `roger_launcher.cpp` (warning log + comment). No new files. Build verification replaces unit tests (the dialog is pure UI).

**Tech Stack:** ScummVM C++11, `GUI::Dialog`/`GUI::Widget`, `common/config-manager.h`, `engines/metaengine.h` (for `EngineMan` + `QualifiedGameList`).

## Global Constraints

- C++11, tabs (width 4), K&R braces, no exceptions, no RTTI
- Pointer/ref on the right: `int *ptr`, `void foo(int &bar)`
- No column limit
- No new files — all changes go into existing files
- Build with `.\build_and_run.ps1` (Windows/MSVC); or open the generated `.sln` and build `scummvm`

---

### Task 1: Refactor — file-scope constants + `addSettingsRow` helper

Pure refactor: no behavior change. Eliminates the three copies of the popup value arrays (constructor, `rebuildSettings`, `handleCommand`) and the fragile parallel-array settings row loop.

**Files:**
- Modify: `engines/sci/roger/roger_launcher_dialog.h`
- Modify: `engines/sci/roger/roger_launcher_dialog.cpp`

**Interfaces:**
- Produces: `addSettingsRow(int y, int M, int LH, const char *label, uint32 cmd) → GUI::PopUpWidget *`
- Produces: file-scope constants `kPrecacheVals`, `kEnhanceVals`, `kFontVals`, `kFallbackVals` (each `static const char *[]`)

- [ ] **Step 1: Add `addSettingsRow` declaration to the header**

In `roger_launcher_dialog.h`, inside `class RogerLauncherDialog`, add to the `private:` section after `rebuildSettings`:

```cpp
GUI::PopUpWidget *addSettingsRow(int y, int M, int LH, const char *label, uint32 cmd);
```

- [ ] **Step 2: Add file-scope constants at the top of the cpp**

In `roger_launcher_dialog.cpp`, after the `namespace Roger {` line and before the constructor, add:

```cpp
static const char *kPrecacheVals[] = { "off", "pics", "views", "all" };
static const char *kEnhanceVals[]  = { "off", "fast", "balanced", "quality" };
static const char *kFontVals[]     = {
    "ms_sans_serif.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf",
    "LiberationSerif-Regular.ttf", "GoMono-Regular.ttf",
    "LiberationMono-Regular.ttf", "SourceCodeVariable-Roman.ttf"
};
static const char *kFallbackVals[] = { "prebuilt", "cache", "memory", "always" };
```

- [ ] **Step 3: Implement `addSettingsRow`**

Add after the constants (before the constructor):

```cpp
GUI::PopUpWidget *RogerLauncherDialog::addSettingsRow(int y, int M, int LH,
                                                       const char *label, uint32 cmd) {
    const int labelW = _w / 5;
    const int popW   = _w / 4;
    new GUI::StaticTextWidget(this, M, y, labelW, LH,
                              Common::U32String(label), Graphics::kTextAlignLeft);
    return new GUI::PopUpWidget(this, M + labelW + M/2, y, popW, LH,
                                Common::U32String(), cmd);
}
```

- [ ] **Step 4: Replace the constructor's parallel-array settings loop**

In the constructor, replace the entire `for (int i = 0; i < 4; ++i)` loop (and the `labels[]`, `pops[]`, `popCmds[]` arrays above it) with:

```cpp
_precachePop = addSettingsRow(settTop + LH + 0*(LH + M/3), M, LH, "Pre-cache",   kPrecachePopCmd);
_enhancePop  = addSettingsRow(settTop + LH + 1*(LH + M/3), M, LH, "Enhancement", kEnhancePopCmd);
_fontPop     = addSettingsRow(settTop + LH + 2*(LH + M/3), M, LH, "Font",        kFontPopCmd);
_fallbackPop = addSettingsRow(settTop + LH + 3*(LH + M/3), M, LH, "Fallback",    kFallbackPopCmd);
```

- [ ] **Step 5: Replace inline arrays in `rebuildSettings` with file-scope constants**

Replace the body of `rebuildSettings()` with:

```cpp
void RogerLauncherDialog::rebuildSettings() {
    const LauncherSettings &s = _state.settings;

    for (int i = 0; i < 4; ++i)
        if (s.precache == kPrecacheVals[i]) { _precachePop->setSelectedTag((uint32)i); break; }
    for (int i = 0; i < 4; ++i)
        if (s.enhancement == kEnhanceVals[i]) { _enhancePop->setSelectedTag((uint32)i); break; }
    for (int i = 0; i < 7; ++i)
        if (s.font == kFontVals[i]) { _fontPop->setSelectedTag((uint32)i); break; }
    for (int i = 0; i < 4; ++i)
        if (s.fallback == kFallbackVals[i]) { _fallbackPop->setSelectedTag((uint32)i); break; }
}
```

- [ ] **Step 6: Replace inline arrays in the popup `handleCommand` cases**

Replace the four popup handler cases in `handleCommand` with:

```cpp
case kPrecachePopCmd: {
    uint32 tag = _precachePop->getSelectedTag();
    if (tag < 4) _state.settings.precache = kPrecacheVals[tag];
    break;
}
case kEnhancePopCmd: {
    uint32 tag = _enhancePop->getSelectedTag();
    if (tag < 4) _state.settings.enhancement = kEnhanceVals[tag];
    break;
}
case kFontPopCmd: {
    uint32 tag = _fontPop->getSelectedTag();
    if (tag < 7) _state.settings.font = kFontVals[tag];
    break;
}
case kFallbackPopCmd: {
    uint32 tag = _fallbackPop->getSelectedTag();
    if (tag < 4) _state.settings.fallback = kFallbackVals[tag];
    break;
}
```

- [ ] **Step 7: Build**

Run `.\build_and_run.ps1` (or build `scummvm` in MSVC). Expected: clean build, no errors. The dialog must behave identically to before.

- [ ] **Step 8: Commit**

```
git add engines/sci/roger/roger_launcher_dialog.h engines/sci/roger/roger_launcher_dialog.cpp
git commit -m "refactor: extract file-scope popup constants and addSettingsRow helper"
```

---

### Task 2: Layout redesign

Replace the constructor's widget creation: remove Skip, narrow the game list to leave a right column for Add/Delete, move progress label above the bottom buttons. Launch and Delete start disabled (enabled in Task 3).

**Files:**
- Modify: `engines/sci/roger/roger_launcher_dialog.h`
- Modify: `engines/sci/roger/roger_launcher_dialog.cpp`

**Interfaces:**
- Consumes: `addSettingsRow` from Task 1
- Produces: `_deleteBtn` (new member, disabled at start), `_launchBtn` (disabled at start), `_skipBtn` removed

- [ ] **Step 1: Add `_deleteBtn` and `kDeleteCmd` to the header**

In `roger_launcher_dialog.h`:

Add `_deleteBtn` to the private member list (after `_addGameBtn`):
```cpp
GUI::ButtonWidget    *_deleteBtn    = nullptr;
```

Remove `_skipBtn` from the member list.

Add `kDeleteCmd` to the enum:
```cpp
kDeleteCmd   = 'RDEL',
```

Remove `kSkipCmd` from the enum.

- [ ] **Step 2: Rewrite the constructor in `roger_launcher_dialog.cpp`**

Replace the entire constructor body (everything between `{` and `}` after `_launcher(launcher), _state(launcher.state())`) with:

```cpp
const int W = _w, H = _h;
const int M  = W / 30;   // margin
const int LH = H / 20;   // line height

// ── Title ─────────────────────────────────────────────────────────────────
new GUI::StaticTextWidget(this, M, M, W / 4, LH,
                          Common::U32String("ROGER"), Graphics::kTextAlignLeft);

// ── Games section ─────────────────────────────────────────────────────────
const int listTop = M + LH + M;
const int listH   = H * 2 / 5;
const int btnColW = W / 5;                       // width of Add/Delete buttons
const int listW   = W - 2*M - btnColW - M;       // list narrowed for right column

new GUI::StaticTextWidget(this, M, listTop, W / 4, LH,
                          Common::U32String("GAMES"), Graphics::kTextAlignLeft);

_gameList = new GUI::ListWidget(this, M, listTop + LH, listW, listH - LH,
                                Common::U32String(), kGameSelCmd);

// Right column: stacked Add / Delete buttons flush with list top.
const int rightX = M + listW + M;
_addGameBtn = new GUI::ButtonWidget(this, rightX, listTop + LH, btnColW, LH,
                                    Common::U32String("+ Add Game"),
                                    Common::U32String(), kAddGameCmd);
_deleteBtn  = new GUI::ButtonWidget(this, rightX, listTop + LH*2 + M/2, btnColW, LH,
                                    Common::U32String("Delete"),
                                    Common::U32String(), kDeleteCmd);

// ── Settings section ──────────────────────────────────────────────────────
const int settTop = listTop + listH + M;
new GUI::StaticTextWidget(this, M, settTop, W - 2*M, LH,
                          Common::U32String("SETTINGS"), Graphics::kTextAlignLeft);

_precachePop = addSettingsRow(settTop + LH + 0*(LH + M/3), M, LH, "Pre-cache",   kPrecachePopCmd);
_enhancePop  = addSettingsRow(settTop + LH + 1*(LH + M/3), M, LH, "Enhancement", kEnhancePopCmd);
_fontPop     = addSettingsRow(settTop + LH + 2*(LH + M/3), M, LH, "Font",        kFontPopCmd);
_fallbackPop = addSettingsRow(settTop + LH + 3*(LH + M/3), M, LH, "Fallback",    kFallbackPopCmd);

// ── Bottom row ────────────────────────────────────────────────────────────
const int btnY = H - M - LH;

// Progress label sits in its own row above the buttons.
_progressLbl = new GUI::StaticTextWidget(this, M, btnY - LH - M/2, W - 2*M, LH,
                                         Common::U32String(""), Graphics::kTextAlignLeft);

_precacheBtn = new GUI::ButtonWidget(this, M, btnY, W/5, LH,
                                     Common::U32String("Precache Now"),
                                     Common::U32String(), kPrecacheCmd);
_launchBtn   = new GUI::ButtonWidget(this, W - M - W/6, btnY, W/6, LH,
                                     Common::U32String("Launch"),
                                     Common::U32String(), kLaunchCmd);

// ── Populate popup options ─────────────────────────────────────────────────
_precachePop->appendEntry(Common::U32String("off"),   0);
_precachePop->appendEntry(Common::U32String("pics"),  1);
_precachePop->appendEntry(Common::U32String("views"), 2);
_precachePop->appendEntry(Common::U32String("all"),   3);

_enhancePop->appendEntry(Common::U32String("off"),      0);
_enhancePop->appendEntry(Common::U32String("fast"),     1);
_enhancePop->appendEntry(Common::U32String("balanced"), 2);
_enhancePop->appendEntry(Common::U32String("quality"),  3);

for (int i = 0; i < 7; ++i)
    _fontPop->appendEntry(Common::U32String(kFontVals[i]), (uint32)i);

_fallbackPop->appendEntry(Common::U32String("native (prebuilt)"), 0);
_fallbackPop->appendEntry(Common::U32String("cache"),             1);
_fallbackPop->appendEntry(Common::U32String("memory"),            2);
_fallbackPop->appendEntry(Common::U32String("always"),            3);

// Launch and Delete start disabled; enabled on game selection.
_launchBtn->setEnabled(false);
_deleteBtn->setEnabled(false);
```

- [ ] **Step 3: Remove `kSkipCmd` case from `handleCommand`**

Delete the case:
```cpp
case kSkipCmd:
    close();
    break;
```

- [ ] **Step 4: Build and visually verify layout**

Run `.\build_and_run.ps1`. Launch the dialog. Verify:
- No Skip button
- Game list is narrower; Add Game / Delete stacked on the right
- Delete is grayed out
- Launch is grayed out
- Progress label is above Precache Now / Launch
- Settings section has no overlap with buttons

- [ ] **Step 5: Commit**

```
git add engines/sci/roger/roger_launcher_dialog.h engines/sci/roger/roger_launcher_dialog.cpp
git commit -m "feat: redesign launcher dialog layout — right-column Add/Delete, remove Skip"
```

---

### Task 3: Fix game selection — redraw + enable buttons

Clicking a different game row must update the settings popups visually and enable Launch + Delete.

**Files:**
- Modify: `engines/sci/roger/roger_launcher_dialog.cpp`

**Interfaces:**
- Consumes: `_deleteBtn`, `_launchBtn` from Task 2

- [ ] **Step 1: Update `open()` to set initial button state**

In `open()`, after the existing `rebuildGameList()` and `rebuildSettings()` calls, add:

```cpp
bool hasGames = !_state.games.empty();
_launchBtn->setEnabled(hasGames);
_deleteBtn->setEnabled(hasGames);
```

- [ ] **Step 2: Update `kGameSelCmd` handler in `handleCommand`**

Replace the existing case:
```cpp
case kGameSelCmd:
    _launcher.selectGame(_gameList->getSelected());
    rebuildSettings();
    break;
```

With:
```cpp
case kGameSelCmd: {
    int sel = _gameList->getSelected();
    if (sel >= 0) {
        _launcher.selectGame(sel);
        rebuildSettings();
        _launchBtn->setEnabled(true);
        _deleteBtn->setEnabled(true);
    }
    g_gui.scheduleTopDialogRedraw();
    break;
}
```

- [ ] **Step 3: Build and verify**

Run `.\build_and_run.ps1`. Click between games in the list. Verify:
- Settings popups update to reflect each game's ConfMan values
- Launch and Delete become enabled after clicking a row

- [ ] **Step 4: Commit**

```
git add engines/sci/roger/roger_launcher_dialog.cpp
git commit -m "fix: update settings on game selection + enable Launch/Delete"
```

---

### Task 4: Delete button — remove game from ConfMan

`kDeleteCmd` removes the selected game's domain from ConfMan, flushes to disk, and refreshes the list.

**Files:**
- Modify: `engines/sci/roger/roger_launcher_dialog.cpp`

**Interfaces:**
- Consumes: `_deleteBtn` (Task 2), `_state.games[_state.selectedIndex].targetName`
- Consumes: `ConfMan.removeGameDomain()`, `ConfMan.flushToDisk()`, `_launcher.discoverGames()`, `rebuildGameList()`

- [ ] **Step 1: Add `kDeleteCmd` case to `handleCommand`**

After the `kAddGameCmd` case (before `kGameSelCmd`), add:

```cpp
case kDeleteCmd: {
    if (_state.selectedIndex < 0 ||
        _state.selectedIndex >= (int)_state.games.size()) break;
    const Common::String dom = _state.games[_state.selectedIndex].targetName;
    ConfMan.removeGameDomain(dom);
    ConfMan.flushToDisk();
    _launcher.discoverGames();
    rebuildGameList();
    rebuildSettings();
    bool hasGames = !_state.games.empty();
    _launchBtn->setEnabled(hasGames);
    _deleteBtn->setEnabled(hasGames);
    g_gui.scheduleTopDialogRedraw();
    break;
}
```

- [ ] **Step 2: Build and verify**

Run `.\build_and_run.ps1`. With two games in the list:
- Select one, click Delete → it disappears from the list
- Verify the other game is still shown and selected
- Verify `scummvm.ini` no longer contains the deleted domain

- [ ] **Step 3: Commit**

```
git add engines/sci/roger/roger_launcher_dialog.cpp
git commit -m "feat: Delete button removes game from ConfMan"
```

---

### Task 5: Fix missing game description (SQ3 / command-line launch)

When a game is launched via `scummvm -p /path gameid`, no `description` key exists in the active domain. Fall back to `EngineMan.findGamesMatching()` to get the friendly name.

**Files:**
- Modify: `engines/sci/roger/roger_launcher.cpp`

**Interfaces:**
- Consumes: `EngineMan.findGamesMatching(engineId, gameId) → QualifiedGameList`; `QualifiedGameDescriptor::description`
- The fix applies to both the persistent-domain loop and the active-domain block in `discoverGames()`

- [ ] **Step 1: Add the `engines/metaengine.h` include**

At the top of `roger_launcher.cpp`, after the existing `#include` lines, add:

```cpp
#include "engines/metaengine.h"
```

- [ ] **Step 2: Extract a description-lookup helper lambda inside `discoverGames()`**

At the top of `RogerLauncher::discoverGames()`, before the domains loop, add a local lambda that resolves a friendly description when ConfMan doesn't have one:

```cpp
auto resolveDesc = [](const Common::String &gameId,
                       const Common::String &confDesc) -> Common::String {
    if (!confDesc.empty()) return confDesc;
    QualifiedGameList matches = EngineMan.findGamesMatching("sci", gameId);
    if (!matches.empty()) return matches[0].description;
    return gameId;  // last resort: raw gameId
};
```

- [ ] **Step 3: Use `resolveDesc` in the persistent-domain loop**

The current persistent-domain `tryAddEntry` call passes `ConfMan.get("description", dom)` or `""`. Change it to:

```cpp
tryAddEntry(dom,
            ConfMan.getPath("path", dom),
            ConfMan.hasKey("gameid", dom) ? ConfMan.get("gameid", dom) : dom,
            resolveDesc(
                ConfMan.hasKey("gameid", dom) ? ConfMan.get("gameid", dom) : dom,
                ConfMan.hasKey("description", dom) ? ConfMan.get("description", dom) : ""));
```

- [ ] **Step 4: Use `resolveDesc` in the active-domain block**

The current active-domain `tryAddEntry` call passes `ConfMan.get("description")` or `""`. Change it to:

```cpp
const Common::String activeGameId = ConfMan.hasKey("gameid") ? ConfMan.get("gameid") : active;
tryAddEntry(active,
            ConfMan.getPath("path"),
            activeGameId,
            resolveDesc(activeGameId,
                        ConfMan.hasKey("description") ? ConfMan.get("description") : ""));
```

- [ ] **Step 5: Build and verify**

Run `.\build_and_run.ps1`. Verify that SQ3 (or any game launched via command-line without a persistent ConfMan domain) shows its full name (e.g., "Space Quest III: The Pirates of Pestulon") instead of "sq3" in the game list.

- [ ] **Step 6: Commit**

```
git add engines/sci/roger/roger_launcher.cpp
git commit -m "fix: show friendly game name when ConfMan has no description key"
```

---

### Task 6: Auto-precache on launch when cache is empty

When the user clicks Launch and the selected game (which must be the currently-running game) has zero cached files, start precaching automatically and launch when done. If the selected game is different from the running game, launch immediately (the new game's launcher invocation will handle precaching).

**Files:**
- Modify: `engines/sci/roger/roger_launcher_dialog.h`
- Modify: `engines/sci/roger/roger_launcher_dialog.cpp`

**Interfaces:**
- Produces: `_launchAfterPrecache` (new `bool` member, default `false`)
- Consumes: `_state.games[_state.selectedIndex].cache`, `ConfMan.getActiveDomainName()`, `_launcher.buildPrecacheQueues()`, `_launcher.handleLaunch()`

- [ ] **Step 1: Add `_launchAfterPrecache` to the header**

In `roger_launcher_dialog.h`, in the `private:` member list, add:

```cpp
bool _launchAfterPrecache = false;
```

- [ ] **Step 2: Replace `kLaunchCmd` handler**

In `handleCommand`, replace the current `kLaunchCmd` case:
```cpp
case kLaunchCmd:
    if (_launcher.handleLaunch())
        close();
    else
        close();
    break;
```

With:

```cpp
case kLaunchCmd: {
    if (_state.games.empty()) break;
    const GameEntry &g    = _state.games[_state.selectedIndex];
    const bool isCurrent  = (g.targetName == ConfMan.getActiveDomainName());
    const bool noCache    = (g.cache.picCount == 0 && g.cache.viewCount == 0);
    if (isCurrent && noCache) {
        _launchAfterPrecache = true;
        _launcher.buildPrecacheQueues();
        _precacheBtn->setLabel(Common::U32String("Cancel"));
        updateProgress();
        g_gui.scheduleTopDialogRedraw();
    } else {
        if (_launcher.handleLaunch())
            close();
        else
            close();
    }
    break;
}
```

- [ ] **Step 3: Update `handleTickle` to launch after precache completes**

In `handleTickle()`, inside the `if (!more)` block, after `rebuildGameList()` and `scheduleTopDialogRedraw()`, add:

```cpp
if (_launchAfterPrecache) {
    _launchAfterPrecache = false;
    if (_launcher.handleLaunch())
        close();
    else
        close();
}
```

The full `handleTickle` should read:

```cpp
void RogerLauncherDialog::handleTickle() {
    if (_state.precaching) {
        bool more = _launcher.precacheStep();
        updateProgress();
        g_gui.scheduleTopDialogRedraw();
        if (!more) {
            _precacheBtn->setLabel(Common::U32String("Precache Now"));
            for (uint i = 0; i < _state.games.size(); ++i)
                _launcher.inspectCacheStatus(_state.games[i]);
            rebuildGameList();
            g_gui.scheduleTopDialogRedraw();
            if (_launchAfterPrecache) {
                _launchAfterPrecache = false;
                if (_launcher.handleLaunch())
                    close();
                else
                    close();
            }
        }
    }
    GUI::Dialog::handleTickle();
}
```

- [ ] **Step 4: Build and verify**

Run `.\build_and_run.ps1` with a game that has no cache files in its `<gameid>-roger/cache/` directory:
- Dialog opens
- Click Launch
- Progress label shows "Caching: N / M"
- After caching completes, the game launches automatically

Also verify: selecting a different game (non-active) and clicking Launch switches games immediately without precaching.

- [ ] **Step 5: Commit**

```
git add engines/sci/roger/roger_launcher_dialog.h engines/sci/roger/roger_launcher_dialog.cpp
git commit -m "feat: auto-precache on launch when cache is empty"
```

---

### Task 7: Cleanup in `roger_launcher.cpp`

Two minor maintenance items: a warning log when `buildPrecacheQueues` no-ops, and an explanatory comment on the O(n) queue pop.

**Files:**
- Modify: `engines/sci/roger/roger_launcher.cpp`

- [ ] **Step 1: Add warning log in `buildPrecacheQueues`**

Change the early-return guard from:

```cpp
if (!g_sci) return;
```

To:

```cpp
if (!g_sci) {
    warning("RogerLauncher::buildPrecacheQueues: g_sci is null, queues not built");
    return;
}
```

- [ ] **Step 2: Add comment in `precacheStep` on the O(n) pop**

In `precacheStep()`, change:

```cpp
GuiResourceId id = _state.picQueue.front();
_state.picQueue.remove_at(0);
```

To:

```cpp
GuiResourceId id = _state.picQueue.front();
_state.picQueue.remove_at(0);  // O(n) but pic counts stay well under 200
```

And similarly for the view queue:

```cpp
int id = _state.viewQueue.front();
_state.viewQueue.remove_at(0);  // O(n) but view counts stay well under 200
```

- [ ] **Step 3: Build**

Run `.\build_and_run.ps1`. Expected: clean build.

- [ ] **Step 4: Commit**

```
git add engines/sci/roger/roger_launcher.cpp
git commit -m "chore: warning log on null g_sci in buildPrecacheQueues; comment O(n) pop"
```

---

### Task 8: Documentation

Update `docs/roger.md` and `CLAUDE.md` to reflect the new launcher behavior.

**Files:**
- Modify: `docs/roger.md`
- Modify: `CLAUDE.md`

- [ ] **Step 1: Update the launcher section in `docs/roger.md`**

Find the section describing the launcher (search for "launcher" or "precache" in that file). Update it to describe:

- The dialog now requires clicking **Launch** to proceed (no Skip button)
- **Game list** shows cache status per game (e.g., "55 pics cached")
- **Delete** button (right of the list) removes the selected game from ScummVM's config
- **Settings** (Pre-cache, Enhancement, Font, Fallback) update when a different game is selected
- **Auto-precache on first launch**: if the selected game has no cache files, the dialog precaches automatically before launching — progress is shown in the dialog
- **Precache Now** button still available for manual precaching

- [ ] **Step 2: Update `CLAUDE.md`**

In the Roger Project section (under **Key files** table or after the config knobs paragraph), add a short entry describing the launcher:

```
**Launcher:** `engines/sci/roger/roger_launcher.{h,cpp}` + `roger_launcher_dialog.{h,cpp}`.
`RogerLauncher` discovers SCI game domains from ConfMan, manages the `LauncherState`
(selected game, precache queues, settings), and is called at engine startup via
`FileRogerArtProvider`. `RogerLauncherDialog` is a `GUI::Dialog` that presents the game
list, per-game settings, and precaching controls. On launch with no cache, precaching
runs automatically in `handleTickle` before `handleLaunch` is called.
```

- [ ] **Step 3: Build and verify docs look sensible**

Run `.\build_and_run.ps1`. No code changes — just confirm the build still passes.

- [ ] **Step 4: Commit**

```
git add docs/roger.md CLAUDE.md
git commit -m "docs: update launcher docs for redesigned dialog"
```

---

## Self-Review

**Spec coverage check:**

| Spec requirement | Covered by |
|---|---|
| Remove Skip button | Task 2 |
| Settings update on game selection | Task 3 |
| Delete button (removes from ConfMan) | Task 4 |
| SQ3 name fix (EngineMan fallback) | Task 5 |
| Fix precache/fallback overlap | Task 2 (layout redesign) |
| Launch disabled until game selected | Task 2 (starts disabled) + Task 3 (enabled on click) |
| Game list shows cache status | Already implemented in `rebuildGameList`; no new task needed |
| Auto-precache on launch (current game, no cache) | Task 6 |
| Precache progress visible during auto-precache | Task 6 (`updateProgress()` + redraw) |
| File-scope constants / settings row helper | Task 1 |
| `buildPrecacheQueues` warning log | Task 7 |
| `precacheStep` O(n) comment | Task 7 |
| `docs/roger.md` update | Task 8 |
| `CLAUDE.md` update | Task 8 |

All spec requirements covered.

**Placeholder scan:** No TBDs or "implement later" — every step has actual code.

**Type consistency:** `_deleteBtn` declared in Task 2 header, used in Tasks 2, 3, 4. `_launchAfterPrecache` declared in Task 6 header, set/cleared in Task 6 handleCommand/handleTickle. `kDeleteCmd` declared in Task 2, handled in Task 4. `kFontVals` etc. declared in Task 1, used in Tasks 1 and 2 (appendEntry loop). All consistent.
