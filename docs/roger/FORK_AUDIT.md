# Fork Audit: changes outside engines/sci/roger/

**Branch:** jon-update-core-audit (off jon-update-core) vs origin/master. **Date:** 2026-07-10.
**Companion design spec:** docs/superpowers/specs/2026-07-09-sci-gfx-observer-generalization-design.md (untracked working doc).
**Purpose:** classify every out-of-roger change for defensibility, inventory every SCI graphics seam, and prove the generalized observer redesign shrinks the diff (baseline 830 lines; cap 1000).

Re-measured 2026-07-10 at 14509d438c3; plan's 2026-07-09 figures were 825/18/83.

## 1. Methodology

Buckets: **N** notification / **C** claim-override / **S** state-intrusion /
**G** generic-fix / **D** dev-tool / **W** wiring.
Seam classifications: **hooked** / **derivable-from-L2** / **real-gap** / **excluded**.
Gap-fill rule: a real-gap needs BOTH content-not-in-L2-pixels AND a non-Roger consumer story.

Change-site table format (every section-3 subsection uses these columns):

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|

Disposition values: `→ <event> (L1..L4)` / `observer-side` / `fork-only` / `standalone-PR` / `delete`.

## 2. Measured baseline

| File | Lines changed | Hunks |
|---|---|---|
| engines/sci/engine/kgraphics.cpp | 26 | 6 |
| engines/sci/event.cpp | 52 | 4 |
| engines/sci/graphics/animate.cpp | 74 | 14 |
| engines/sci/graphics/controls16.cpp | 94 | 9 |
| engines/sci/graphics/cursor.cpp | 3 | 2 |
| engines/sci/graphics/menu.cpp | 141 | 9 |
| engines/sci/graphics/menu.h | 22 | 3 |
| engines/sci/graphics/paint16.cpp | 167 | 17 |
| engines/sci/graphics/paint16.h | 4 | 1 |
| engines/sci/graphics/ports.cpp | 54 | 2 |
| engines/sci/graphics/scifont.cpp | 2 | 2 |
| engines/sci/graphics/scifont.h | 5 | 1 |
| engines/sci/graphics/text16.cpp | 44 | 4 |
| engines/sci/graphics/transitions.cpp | 27 | 2 |
| engines/sci/module.mk | 36 | 1 |
| engines/sci/sci.cpp | 67 | 4 |
| gui/EventRecorder.h | 6 | 1 |
| test/module.mk | 6 | 1 |
| **Total** | **830** | **83** |

## 3. Change-site classification

### 3.1 engines/sci/graphics/paint16.{cpp,h} (167+4 lines, 18 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| P1 | paint16.cpp:43 | `#include "sci/roger/roger_art_provider.h"` — pulls the concrete provider header into an SCI-engine file | W (+concern) | Not mechanical: SCI code should include a neutral observer header, never a `roger/` path — this is the diff's clearest "fork plumbing" tell | → replace with `sci_gfx_observer.h` (engine-owned); observer-side |
| P2 | paint16.cpp:91-108 | drawPicture pre-hook: `rogerReplace` gate (`enabled && hasBackground`), diag warning, `prefetch(pic)` on replace, `onNativePicture()` on unreplaced full-screen pic (stale-overlay drop) | N (+D for the diag line) | Mostly mechanical null-guarded observe + a claim-ish prefetch; the diag `warning()` is fork noise | → onPicture(picId, addToFlag) + onPictureAbsent() (L3); diag line: delete |
| P3 | paint16.cpp:110-114 | beginNativeDraw re-entrancy bracket opened before the native picture render | N | Mechanical self-draw bracket, already neutral | → beginSelfDraw() (L2) |
| P4 | paint16.cpp:135-149 | drawPicture post-hook: `pushHiresBackground` / `pushHiresBackgroundAddTo` on replace, then `endNativeDraw` bracket close | N | Mechanical observe + bracket close; picks the addTo variant by the same `addToFlag` the event already carries | → onPicture(picId, addToFlag) (L3) + endSelfDraw() (L2) |
| P5 | paint16.cpp:158-164 | drawCelAndShow entry: diag warning, then `beginNativeDraw` bracket (animate-cast cels composited semantically, not via Feeder B) | N (+D for the diag line) | Mechanical bracket; diag `warning()` is fork noise | → beginSelfDraw() (L2); diag line: delete |
| P6 | paint16.cpp:171-176 | `_picNotValid` init-cel capture: `onInitCel(view,loop,cel,rect,priority,owner=0)` for a script-drawn cel baking into the picture | N | Mechanical observe; owner=0 marshalling is one of the six `onCel` sources | → onCel(rect, view, loop, cel, priority, owner, source=initBake) (L3) |
| P7 | paint16.cpp:189-190 | drawCelAndShow `endNativeDraw` bracket close | N | Mechanical bracket close | → endSelfDraw() (L2) |
| P8 | paint16.cpp:227-230 | drawHiresCelAndShow `beginNativeDraw` bracket (KQ6 hires cel path; composited via onDrawCel) | N | Mechanical bracket | → beginSelfDraw() (L2) |
| P9 | paint16.cpp:250-251 | drawHiresCelAndShow `endNativeDraw` bracket close | N | Mechanical bracket close | → endSelfDraw() (L2) |
| P10 | paint16.cpp:369 + paint16.h:61-63 | `bitsShow(rect)` → `bitsShow(rect, uint32 rogerOwner = 0)` signature change + the header comment documenting the token param | N (+concern) | Default-arg keeps all native callers byte-identical, but a `rogerOwner` param on a public SCI method leaks the token scheme into the signature — a neutral observer would derive the owner observer-side | → onShow(rect, owner) (L2); owner derivation moves observer-side; drop the param |
| P11 | paint16.cpp:398-410 | bitsShow Feeder B body: `onNativeShowRect(workerRect, owner)`, deriving the window token from the caller arg or the current window port | N | Mechanical observe; the token-derivation branch is the marshalling §4.3 moves behind a helper | → onShow(rect, owner) (L2); token helper observer-side |
| P12 | paint16.cpp:428-433 | bitsSave journal checkpoint: `onNativeSaveRect(tok, rect)` keyed by the hunk handle | N | Mechanical observe; token = save-handle identity per §4.3 | → onSave(token, rect) (L2) |
| P13 | paint16.cpp:452-460 | bitsRestore journal rollback: `onNativeRestoreRect(tok, restored)` (subsumes the old uiClearToken + onNativeEraseRect pair) | N | Mechanical observe; already the consolidated restore path | → onRestore(token, rect) (L2) |
| P14 | paint16.cpp:471-475 | bitsFree drop: `onNativeFreeSave(tok)` before freeing the hunk | N | Mechanical observe | → onFree(token) (L2) |
| P15 | paint16.cpp:507-520 | kernelDrawCel standalone-cel hook: `onDrawCel(g, view, loop, cel)` with `offsetRect` globalization + diag warning | N (+D for the diag line) | Mechanical observe + one `offsetRect` marshal; diag `warning()` is fork noise | → onCel(rect, view, loop, cel, priority, owner, source=standalone) (L3); diag line: delete |
| P16 | paint16.cpp:543-554 | kernelGraphFrameBox hook: `uiPushFrameBox(g, color)` with `offsetRect` globalization | N | Mechanical observe + one `offsetRect` marshal | → onFrameBox(rect, pen) (L3) |
| P17 | paint16.cpp:571-573 | kernelGraphUpdateBox diag warning before the (unchanged) `bitsShow(rect)` | D | Pure fork diag; the box is already L2 via `bitsShow` | delete (already covered by onShow via bitsShow) |
| P18 | paint16.cpp:577-581 | kernelGraphRedrawBox hook: `onNativeEraseRect(rect)` on the already-global rect | N | Mechanical observe | → onErase(rect) (L2) |
| P19 | paint16.cpp:705-717 | kernelDisplay background-fill push: `uiPushText(rect, "", pen, back, ...)` (empty string = fill only; text captured per-line in Box), keyed by the save-under handle | N (+concern) | Mechanical observe, but overloading `uiPushText` with an empty string to mean "fill only" is a smell — a dedicated fill event would read better | → candidate onFill(rect, color, token) or fold into onText(source=textBox) (L3) — audit §6 decides on line budget |
| P20 | paint16.cpp:740-764 | kernelDisplay flush bracket: two `bitsShow(rect)` calls wrapped in `beginNativeDraw`/`endNativeDraw` (Box stays outside so its onNativeText is not depth-suppressed) | N | Mechanical brackets around pre-existing native shows; the CJK-fix logic itself is untouched | → beginSelfDraw()/endSelfDraw() (L2) |

<!-- coverage: hunks 1-17 (paint16.cpp) -> P1;P2,P3;P4;P5,P6,P7;(P7);P8;P9;P10;P11;P12;P13;P14;P15;P16;P17;P18;P19,P20  |  hunk 1 (paint16.h) -> P10.
  paint16.cpp hunk map (diff order): 1=P1(include); 2=P2+P3(drawPicture pre); 3=P4(drawPicture post); 4=P5+P6+P7-open(drawCelAndShow entry); 5=P7-close(endNativeDraw); 6=P8(drawHiresCel begin); 7=P9(drawHiresCel end); 8=P10(bitsShow sig); 9=P11(bitsShow body); 10=P12(bitsSave); 11=P13(bitsRestore); 12=P14(bitsFree); 13=P15(kernelDrawCel); 14=P16(kernelGraphFrameBox); 15=P17+P18(kGraphUpdateBox diag + kGraphRedrawBox erase); 16=P19(kernelDisplay bg-fill); 17=P20(kernelDisplay flush). paint16.h hunk 1 = P10. -->

### 3.2 engines/sci/engine/kgraphics.cpp (26 lines, 6 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| K1 | kgraphics.cpp:58 | `#include "sci/roger/roger_art_provider.h"` in the kernel graphics file | W (+concern) | Not mechanical: same `roger/`-path leak as P1 — a neutral observer header belongs here | → replace with `sci_gfx_observer.h`; observer-side |
| K2 | kgraphics.cpp:134-139 | kSetCursorSci0 hook: after `kernelSetShape`, emit `onCursorHidden(true)` if `cursorId < 0` else `onCursorShape(cursorId)` | N | Mechanical null-guarded observe alongside the native shape set | → claimCursor() + onCursorShape/onCursorHidden notifications (L4) |
| K3 | kgraphics.cpp:152-153 | kSetCursorSci11 hide case: `onCursorHidden(true)` after `kernelHide` | N | Mechanical observe | → claimCursor() + onCursorHidden (L4) |
| K4 | kgraphics.cpp:163-164 | kSetCursorSci11 show case: `onCursorHidden(false)` after `kernelShow` | N | Mechanical observe | → claimCursor() + onCursorHidden (L4) |
| K5 | kgraphics.cpp:210-212 | kSetCursorSci11 setView case: `onCursorView(view, loop, cel)` after `kernelSetView` | N | Mechanical observe | → claimCursor() + onCursorView (L4) |
| K6 | kgraphics.cpp:1274-1280 | kShakeScreen: when overlay visible, `onShake(count, dirs)` and early-return, skipping the native `kernelShakeScreen` | C | Native path skipped when provider active — a real claim, but it is inline `isOverlayVisible()`-gated fork logic, not a documented override | → claimShake(count, directions) -> bool (L4) |
| K7 | kgraphics.cpp:1286-1290 | kDisplay comment-only hunk explaining the overlay-not-hidden-for-text policy | D | Comment only, no code change; pure fork war-story | delete (comment relocates to FORK_AUDIT / observer impl) |

<!-- coverage: hunks 1-6 (kgraphics.cpp) -> K1;K2;K3,K4;K5;K6;K7.
  hunk map (diff order): 1=K1(include); 2=K2(kSetCursorSci0); 3=K3+K4(Sci11 hide/show); 4=K5(Sci11 setView); 5=K6(kShakeScreen); 6=K7(kDisplay comment). -->

### 3.3 engines/sci/graphics/cursor.cpp (3 lines, 2 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| CU1 | cursor.cpp:40 | `#include "sci/roger/roger_art_provider.h"` in the cursor file | W (+concern) | Not mechanical: same `roger/`-path leak as P1/K1 | → replace with `sci_gfx_observer.h`; observer-side |
| CU2 | cursor.cpp:84 | kernelShow `hidesNativeCursor()` veto: `CursorMan.showMouse(!(provider && provider->hidesNativeCursor()))` — the native hardware cursor is suppressed while the provider hides it (it draws ABOVE the OSystem overlay and leaks past the letterbox) | C | Native path altered while provider active — a genuine override, but a defensible one: the veto is the only way to keep the hw cursor off the overlay (commit 135ed9438a3) | → claimCursor() (L4), joining the K2–K5 cursor hooks under the same event |

<!-- coverage: hunks 1-2 (cursor.cpp) -> CU1;CU2.
  hunk map (diff order): 1=CU1(include); 2=CU2(kernelShow veto). -->

### 3.4 engines/sci/graphics/animate.cpp (74 lines, 14 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| A1 | animate.cpp:24 | `#include "common/system.h"` — pulls in `g_system` for the `getMillis()` cycle-telemetry timestamp | D | Include exists only to serve the ROGER-CYCLE dev-telemetry block (A14); no observer need | delete with A14 (fork-only) |
| A2 | animate.cpp:45,49-54 | `#include "sci/roger/roger_art_provider.h"` + `rogerOwnerToken(reg_t)` static helper packing segment/offset into a 32-bit owner token | W (+concern) for the include; N for the helper | Include is the same `roger/`-path leak as P1/K1; the helper is token marshalling (owner identity for `onCel`) that §4.3 moves observer-side | include → replace with `sci_gfx_observer.h`; helper → observer-side (token construction behind onCel source=initBake) |
| A3 | animate.cpp:427-431 | update() `kSignalAlwaysUpdate` branch: `onInitCel(view,loop,cel,rect,priority,owner)` during `_picNotValid` for an init-frame cast draw that bakes into the picture | N | Mechanical null-guarded observe; owner token disambiguates baked prop vs live actor | → onCel(rect, view, loop, cel, priority, owner, source=initBake) (L3) |
| A4 | animate.cpp:461-465 | update() `kSignalNoUpdate` branch: same `onInitCel` capture during `_picNotValid` | N | Mechanical observe, identical to A3 at the second cast-draw site | → onCel(..., source=initBake) (L3) |
| A5 | animate.cpp:491-495 | drawCels() branch: same `onInitCel` capture during `_picNotValid` | N | Mechanical observe, identical to A3 at the third cast-draw site | → onCel(..., source=initBake) (L3) |
| A6 | animate.cpp:512-516 | updateScreen() open: `beginNativeDraw()` re-entrancy bracket so the animate cels' native bitsShow is not double-captured by Feeder B (they are composited semantically) | N | Mechanical self-draw bracket, neutral | → beginSelfDraw() (L2) |
| A7 | animate.cpp:550-552 | updateScreen() close: `endNativeDraw()` bracket close | N | Mechanical bracket close | → endSelfDraw() (L2) |
| A8 | animate.cpp:585-589 | reAnimate() open: `beginNativeDraw()` bracket around the cel redraw (dialog-dismissal restore path) | N | Mechanical self-draw bracket | → beginSelfDraw() (L2) |
| A9 | animate.cpp:608-613 | reAnimate() close: `endNativeDraw()` bracket close + `renderFromAnimateList(_list)` re-composite after background restore | N | Mechanical bracket close + one frame-list observe; the animate-list emit is the L1 frame event | → endSelfDraw() (L2) + onAnimateFrame(list) (L1) |
| A10 | animate.cpp:652-654 | addToPicDrawCels(): `onAddToPicCel(view,loop,cel,rect,priority)` for a static cel baked into the pic (not in the animate list after) | N | Mechanical observe; one of the six `onCel` sources | → onCel(rect, view, loop, cel, priority, owner, source=addToPic) (L3) |
| A11 | animate.cpp:673-675 | addToPicDrawView(): same `onAddToPicCel` for a single addToPic view | N | Mechanical observe, identical to A10 for the single-view entry | → onCel(..., source=addToPic) (L3) |
| A12 | animate.cpp:699-701 | kernelAnimate() entry: `const uint32 rogerCycleT0 = g_system->getMillis()` unconditionally, feeding the A14 telemetry line | D | Fork dev-telemetry only (one unconditional getMillis); no observer need | delete with A14 (fork-only) |
| A13 | animate.cpp:756-765 | kernelAnimate(): `snapshotNativeBaseline()` after updateScreen (Feeder B diff baseline) + `renderFromAnimateList(_list)` after restoreAndDelete (composite the sorted cast) | N | Two mechanical observes at the cycle's frame boundaries | → onFrameEnd()/snapshot (L1) + onAnimateFrame(list) (L1) |
| A14 | animate.cpp:776-785 | kernelAnimate() tail: ROGER-CYCLE `warning()` telemetry gated on `cycleLogEnabled()`, using a **non-const function-static `s_prevCycleT0`** to hold the previous entry timestamp | D (+concern) | Dev-only telemetry AND an **upstream-forbidden reentrancy violation**: spec §portability bans non-const function statics (stale across return-to-launcher / in-process restart) — must not ship upstream in this form | delete (fork-only); if kept downstream, `s_prevCycleT0` must move to member state |

<!-- coverage: 14 hunks (animate.cpp), diff order:
  1=A1(common/system.h include); 2=A2(roger include + rogerOwnerToken helper);
  3=A3(update kSignalAlwaysUpdate onInitCel); 4=A4(update kSignalNoUpdate onInitCel);
  5=A5(drawCels onInitCel); 6=A6(updateScreen begin bracket); 7=A7(updateScreen end bracket);
  8=A8(reAnimate begin bracket); 9=A9(reAnimate end bracket + renderFromAnimateList);
  10=A10(addToPicDrawCels onAddToPicCel); 11=A11(addToPicDrawView onAddToPicCel);
  12=A12(kernelAnimate entry getMillis); 13=A13(snapshotNativeBaseline + renderFromAnimateList);
  14=A14(ROGER-CYCLE telemetry w/ static s_prevCycleT0). -->

### 3.5 engines/sci/graphics/text16.cpp (44 lines, 4 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| T1 | text16.cpp:39 | `#include "sci/roger/roger_art_provider.h"` in the text file | W (+concern) | Same `roger/`-path leak as P1/K1/A2 | → replace with `sci_gfx_observer.h`; observer-side |
| T2 | text16.cpp:570 | `int16 textWidth, maxTextWidth, textHeight = 0;` — adds `= 0` initializer to `textHeight` | G | Standalone upstreamable fix, no Roger needed: silences a real uninitialized-read path (`textHeight` is read into `hline` accumulation before assignment on a degenerate empty-box path) | standalone-PR (independent of observer work) |
| T3 | text16.cpp:597 | `int16 lineCount = 0;` local declared before the draw loop | N (+concern) | Support variable for the per-line capture (T4), incremented but only consumed by the fork path — dead outside Roger | → folds into onText per-line emit (L3); observer-side (drop if unused upstream) |
| T4 | text16.cpp:681-715,723-725 | Per-line capture block inside Box's draw loop: builds the exact placed line rect (offset+hline, measured textWidth/textHeight), `offsetRect` to global space, window-scoped token `0x60000000\|port->id`, emits `onNativeText(...)` per drawn line regardless of `show`; `lineCount++`; plus the post-loop comment noting the old whole-box hook was deleted | N | Mechanical observe (the load-bearing per-line-rect fix), but carries the token/offsetRect marshalling §4.3/§4.2 consolidate; the war-story comments relocate here | → onText(rect, text, font, pen, back, align, metrics, token, source=textBox) (L3); token+offsetRect helper observer-side; comments → FORK_AUDIT |

<!-- coverage: 4 hunks (text16.cpp), diff order:
  1=T1(roger include); 2=T2(textHeight = 0 init); 3=T3(lineCount decl);
  4=T4(per-line capture block + lineCount++ + post-loop comment). -->

### 3.6 engines/sci/graphics/transitions.cpp (27 lines, 2 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| TR1 | transitions.cpp:32 | `#include "sci/roger/roger_art_provider.h"` in the transitions file | W (+concern) | Same `roger/`-path leak as P1/K1/A2/T1 | → replace with `sci_gfx_observer.h`; observer-side |
| TR2 | transitions.cpp:184-208 | doit() claim block: when `enabled && isOverlayVisible()`, compute the blackout pre-type via `translateNumber`, `palVaryPrepareForTransition()`, emit `onTransition(_number, picRect, blackoutNumber)`, then finalize SCI instantly (`setNewScreen`/`setNewPalette`/`_picNotValid=0`) and **early-return**, skipping SCI's animated transition to avoid double-blocking | C | Native path skipped when provider active — a genuine, defensible override (avoids invisible double-blocking dead time under the opaque overlay); currently inline `isOverlayVisible()`-gated fork logic rather than a documented claim | → claimTransition(type, rect, blackoutType) -> bool (L4) |

<!-- coverage: 2 hunks (transitions.cpp), diff order:
  1=TR1(roger include); 2=TR2(doit claim early-return block). -->

<!-- Tasks 2-5 append per-file subsections here -->

## 4. Seam inventory

<!-- Tasks 6-7 append per-class subsections here -->

## 5. L2 completeness verification

<!-- Task 9 -->

## 6. Consolidation table and line budget

<!-- Task 8 -->

## 7. Gap list

<!-- Task 8 -->

## 8. Standalone upstream PR candidates (G bucket)

<!-- Task 5 seeds, Task 10 finalizes -->

## 9. Findings that contradict the design spec

<!-- any task may append here; Task 10 resolves -->
