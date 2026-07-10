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

### 3.7 engines/sci/graphics/controls16.cpp (94 lines, 9 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| C1 | controls16.cpp:42 | `#include "sci/roger/roger_art_provider.h"` in the controls file | W (+concern) | Not mechanical: same `roger/`-path leak as P1/K1 — a neutral observer header belongs here | → replace with `sci_gfx_observer.h`; observer-side |
| C2 | controls16.cpp:117-134 | drawListControl per-row hook: for each list row, `offsetRect` to global space, window-scoped token `0x40000000\|port->id`, selected-row inversion (pen 15 / back 0), `StringWidth` metrics, `uiPushText(...)` | N | Mechanical observe: one `offsetRect` + `StringWidth` marshal + the `0x40000000` token — the boilerplate §4.3 folds behind a helper; the sel-branch pen/back is content the event already carries | → onText(rect, text, font, pen, back, align, metrics, token, source=listRow) (L3); token+offsetRect helper observer-side |
| C3 | controls16.cpp:295-303 | kernelTexteditChange width-cap relaxation: `if (!rogerActive && textWidth >= rect.width())` — the native "does it fit?" early-return is skipped while Roger is active, so typing is not capped at the native nsRect pixel width (buffer still bound by `maxChars`) | **C** | Behavior change — the NATIVE input path is altered when the provider is active (SCI would stop accepting keystrokes; Roger keeps taking them). A genuine, defensible claim: the hires field is rendered far wider so the native pixel cap is wrong, and `maxChars` still bounds the buffer. Currently an inline `enabled`-gated fork branch, not a documented override | → wantsUnclampedTextEdit() -> bool (L4) |
| C4 | controls16.cpp:321-334 | kernelTexteditChange live-typing push: after the buffer write-back, `offsetRect` + `0x40000000\|port->id` token + `editStyle` from the state selector + `StringWidth`, `uiPushTextEdit(...)` so the hires field tracks each keystroke (live typing redraws here, not via kernelDrawTextEdit) | N | Mechanical observe (separate concern from C3's claim): the marshalling boilerplate §4.3 consolidates; same token+rect as C8 so it replaces that element in place | → onControl(kind=textEdit, ...) (L3); token+offsetRect helper observer-side |
| C5 | controls16.cpp:356-364 | kernelDrawButton push: `offsetRect` + `0x40000000\|port->id` token + `StringWidth`, `uiPushButton(g, text, fontId, style, tok, ...)` | N | Mechanical observe; identical marshalling shape to C4 | → onControl(kind=button, ...) (L3); helper observer-side |
| C6 | controls16.cpp:444-452 | kernelDrawText push: `offsetRect` + `0x40000000\|port->id` token + `StringWidth`, `uiPushText(..., port penClr/backClr, align, ..., source=body)` | N | Mechanical observe; one of the five text paths §4.2 folds into `onText` | → onText(rect, text, font, pen, back, align, metrics, token, source=control) (L3); helper observer-side |
| C7 | controls16.cpp:465-478 | kernelDrawText SELECTED branch: after `frameRect(rect)`, when `style & SELECTED`, `offsetRect` the frame rect and `uiPushFrameBox(gSel, port penClr)` (gates internally on change so no per-cycle present storm) | N | Mechanical observe + one `offsetRect` marshal; the second of the two `uiPushFrameBox` call sites (the other is paint16 P16) | → onFrameBox(rect, pen) (L3) |
| C8 | controls16.cpp:503-511 | kernelDrawTextEdit push: `offsetRect` + `0x40000000\|port->id` token + `StringWidth`, `uiPushTextEdit(g, text, fontId, style, cursorPos, tok, ...)` | N | Mechanical observe; pairs with C4 (same token/rect, replace-in-place) — both fold to the textEdit `onControl` kind | → onControl(kind=textEdit, ...) (L3); helper observer-side |
| C9 | controls16.cpp:539-546 | kernelDrawIcon push: `offsetRect` + `0x40000000\|port->id` token, `uiPushIcon(g, viewId, loopNo, celNo, tok)` | N | Mechanical observe; the icon source of the consolidated `onCel` | → onCel(rect, view, loop, cel, priority, owner, source=icon) (L3); token+offsetRect helper observer-side |

<!-- coverage: 9 hunks (controls16.cpp), diff order:
  1=C1(roger include); 2=C2(drawListControl row push); 3=C3(kernelTexteditChange width-cap relaxation);
  4=C4(kernelTexteditChange live-typing push); 5=C5(kernelDrawButton push); 6=C6(kernelDrawText push);
  7=C7(kernelDrawText SELECTED frameBox); 8=C8(kernelDrawTextEdit push); 9=C9(kernelDrawIcon push). -->

### 3.8 engines/sci/graphics/menu.{cpp,h} (141+22 lines, 9+3 hunks)

The audit's **S-bucket centerpiece**. `GfxMenu` today stores captured menu state
(`RogerMenuRow` arrays, box, highlight) and hosts three `rogerPush*` methods that
emit overlay pushes at draw time. The redesign (spec §4.2 "menu state exile,
killing bucket S") deletes all of it: `GfxMenu` emits neutral L3 events at draw
time and the **observer** keeps whatever row state it needs. The M-rows below carry
the −100-line delta claim, so each state member's post-redesign home is stated
individually.

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| M1 | menu.h:25-28 | Adds `#include` of `common/{array,list,rect,str}.h` — needed only so the header can declare the `RogerMenuRow` struct and the `Common::Array` state members | S | Header-weight added purely to carry Roger state (M3) into `GfxMenu` — vanishes when the state is exiled | delete with M3 (observer-side keeps the state) |
| M2 | menu.h:107-114 | Declares `rogerPushMenuOverlay()`, `rogerClearMenuOverlay()`, `rogerPushBarOverlay()` (three private methods) with their doc comments | S | Roger method decls inside an SCI class — the S-bucket signature; the bodies (M6, M8, M10) live in `menu.cpp` | delete; the emit logic they contain becomes observer-side, reached via the L3 events at M6/M8/M10 |
| M3 | menu.h:144-151 | `struct RogerMenuRow { Common::Rect rect; Common::String text; uint16 id; }` + four state members: `_rogerMenuRows` (Array), `_rogerMenuBox` (Rect), `_rogerMenuHighlight` (uint16), `_rogerBarTitles` (Array) | S | The core state intrusion — Roger data living in `GfxMenu`. Per-member post-redesign home (spec §4.4, "the observer keeps whatever row state it needs"): **`_rogerMenuRows`** → reconstructed observer-side from the stream of `onText(source=menuRow)` emits (each row's rect/text/id arrives on the event) between the dropdown's `onWindowOpen(token=dropdown)` and `onWindowClose`; **`_rogerMenuBox`** → the `onWindowOpen(rect, ..., token=dropdown)` rect payload — the observer holds it under the dropdown token; **`_rogerMenuHighlight`** → carried by `onMenuHighlight(itemId)` and stored observer-side (the observer re-composites its retained rows on each highlight); **`_rogerBarTitles`** → reconstructed observer-side from the `onText(source=menuBar)` emits collected between `beginBatch`/`endBatch` at bar-draw time | delete (menu state exile); all four members live observer-side, keyed by onText(menuRow)/onText(menuBar)/onWindowOpen(dropdown)/onMenuHighlight |
| M4 | menu.cpp:39 | `#include "sci/roger/roger_art_provider.h"` in the menu file | W (+concern) | Same `roger/`-path leak as P1/K1 | → replace with `sci_gfx_observer.h`; observer-side |
| M5 | menu.cpp:372-405 | drawBar bar-title collection: `_rogerBarTitles.clear()` before the loop, and inside it builds a `RogerMenuRow` per title (start-x to pen-end rect over the bar-strip height) and `push_back`s it; calls `rogerPushBarOverlay()` after the loop | S/N hybrid | The per-title collection into the SCI-class array is S (state mutation in `GfxMenu`); the *content* it gathers (rect+text per bar title) is exactly what a neutral `onText(menuBar)` carries | → onText(rect, text, ..., source=menuBar) (L3) per title, inside beginBatch/endBatch; the array + push_back go observer-side (delete here) |
| M6 | menu.cpp:408-455 | `rogerTitleIsText()` static helper (ASCII-printable test) + `rogerPushBarOverlay()` body: `beginUiBatch` / `uiClearToken(0x10000000)` / `uiPushWindow` white bar + `uiPushWindow` black `_menuLine` underline / per-title `uiPushText(source=heading)` (skipping graphical-glyph titles) / `endUiBatch` | S/N hybrid | Method body living in the SCI class = S; the observe calls it makes are neutral L3/L1. `rogerTitleIsText` is an observer-side rendering-policy concern (which titles the TTF header font can render), not SCI's business | body → observer-side; its emits map to beginBatch/endBatch (L1), onWindowOpen (L3, the bar+underline strip), onText(source=menuBar) (L3); `rogerTitleIsText` → observer-side. NOTE the bar and banner share token `0x10000000` (mutual-exclusion) — token discipline §4.3 |
| M7 | menu.cpp:605-606 | kernelSelect: `rogerClearMenuOverlay()` after the menu closes (drops the composited dropdown) | S/N hybrid | The call is a mechanical dispose signal; it lands in an SCI method but carries no Roger state itself | → onWindowClose(token=dropdown) (L3) — the dropdown-close analogue |
| M8 | menu.cpp:739-744 | drawMenu dropdown-box capture: `_rogerMenuBox = _menuRect` (before the draw-time inset mutations) + `_rogerMenuRows.clear()` | S | State mutation in `GfxMenu` — stores the box rect and resets the row array | → onWindowOpen(rect=box, ..., token=dropdown) (L3) carries the box; the observer clears its rows on that open (delete the members here) |
| M9 | menu.cpp:773-779 | drawMenu per-row capture: builds a `RogerMenuRow` (global rect, text, item id) and `push_back`s it for each drawn row | S/N hybrid | Collection into the SCI-class array is S; the content (rect/text/id) is what `onText(menuRow)` carries | → onText(rect, text, ..., token=dropdown, source=menuRow) (L3) per row; array/push_back observer-side (delete here) |
| M10 | menu.cpp:811-845 | drawMenu tail (`_rogerMenuHighlight = 0` + `rogerPushMenuOverlay()`) + the `rogerPushMenuOverlay()` body (batch / `uiClearToken(0x20000000)` / white framed `uiPushWindow` box / per-row `uiPushText` with sel inversion / batch) + `rogerClearMenuOverlay()` body (`uiClearToken(0x20000000)` + `_rogerMenuRows.clear()`) | S/N hybrid | Method bodies in the SCI class = S (and they consume the exiled `_rogerMenuRows`/`_rogerMenuBox`/`_rogerMenuHighlight` state); the observe calls are neutral | bodies → observer-side; emits map to beginBatch/endBatch (L1), onWindowOpen(token=dropdown) (L3), onText(source=menuRow) (L3), onWindowClose(token=dropdown) (L3); the observer owns the re-composite loop |
| M11 | menu.cpp:850-857 | invertMenuSelection highlight re-push: when `itemId != 0 && itemId != _rogerMenuHighlight`, set `_rogerMenuHighlight = itemId` and `rogerPushMenuOverlay()` (skips the no-op old-row re-invert) | S/N hybrid | Reads/writes the exiled `_rogerMenuHighlight` (S), but the *signal* is a pure highlight-change notification — exactly `onMenuHighlight` | → onMenuHighlight(itemId) (L3); the observer holds the highlight and re-composites its retained rows (the `!= _rogerMenuHighlight` dedup moves observer-side) |
| M12 | menu.cpp:875-877 | interactiveStart comment-only hunk explaining the overlay stays up (menu bar shows through the strip, dropdown composited via rogerPushMenuOverlay) — no code change | D | Pure fork war-story comment; no behavior | delete (comment relocates to FORK_AUDIT / observer impl) |
| M13 | menu.cpp:1177-1184 | kernelDrawStatus banner push: `StringWidth` + `uiPushStatus(_menuBarRect, text, font, pen, back, 0x10000000, ...)` — renders the score/title banner into the overlay's top strip | N | Mechanical observe; one of the five text paths (the `status` source) that §4.2 folds into `onText` | → onText(rect, text, ..., token=0x10000000, source=status) (L3) |

<!-- coverage: 9 hunks (menu.cpp) + 3 hunks (menu.h), diff order.
  menu.cpp hunk map (verified @@ markers):
    hunk 1 (@@ -36)   = M4(roger include);
    hunk 2 (@@ -368)  = M5-partial(drawBar `_rogerBarTitles.clear()` prologue);
    hunk 3 (@@ -379)  = M5-rest(drawBar per-title collect + rogerPushBarOverlay() call) + M6(rogerTitleIsText static + rogerPushBarOverlay body) — one hunk;
    hunk 4 (@@ -534)  = M7(kernelSelect rogerClearMenuOverlay);
    hunk 5 (@@ -666)  = M8(drawMenu box capture);
    hunk 6 (@@ -693)  = M9(drawMenu per-row capture);
    hunk 7 (@@ -723)  = M10(drawMenu tail + rogerPushMenuOverlay/rogerClearMenuOverlay bodies) + M11(invertMenuSelection highlight re-push) — one hunk;
    hunk 8 (@@ -744)  = M12(interactiveStart comment);
    hunk 9 (@@ -1042) = M13(kernelDrawStatus uiPushStatus).
  (M5 spans hunks 2+3; every hunk maps to >=1 row.)
  menu.h hunk map: 1=M1(common/* includes); 2=M2(3 roger method decls); 3=M3(RogerMenuRow struct + 4 state members). -->

### 3.9 engines/sci/graphics/ports.cpp (54 lines, 2 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| PO1 | ports.cpp:37 | `#include "sci/roger/roger_art_provider.h"` in the ports file | W (+concern) | Same `roger/`-path leak as P1/K1 | → replace with `sci_gfx_observer.h`; observer-side |
| PO2 | ports.cpp:524-551 | drawWindow open hook: `0x40000000\|pWnd->id` token, `offsetRect(globalDims)`, `uiPushWindow(globalDims, back, pen, style, tok)`; when `STYLE_TITLE && !title.empty()`, a titlebar `uiPushText(source ~ windowTitle)` (centered white on grey/black); then the terminal `bitsShow(pWnd->dims, tok)` is tagged with the window token (drawWindow runs with `_wmgrPort` current, so `bitsShow` cannot derive the owner itself) | N | Mechanical observe: `offsetRect`/token/`StringWidth` marshalling §4.3 folds behind a helper; the tokened `bitsShow` is the same signature change audited at P10 (the default-arg keeps native callers byte-identical) | → onWindowOpen(rect, style, colors, title, token) (L3) — the title push folds into it (spec §4.2, `onText(source=windowTitle)` is subsumed by the window-open payload); tokened show → onShow(rect, owner) (L2), owner derived observer-side |
| PO3 | ports.cpp:557-585 | removeWindow close hook: two `uiClearToken` calls (`0x40000000\|id` controls, `0x60000000\|id` generic window text) + a diag `warning()`; then `hadNoSaveUnder` detection and, on the `!reanimate` path, an explicit `onNativeRestoreRect(0, restoreRect)` **reveal plant** for no-save-under windows (the subsequent `bitsShow(restoreRect)` is thereby covered and not re-stamped by Feeder B) | N (+D for the diag line) | Mechanical: the two clears are the window-dispose signal (`onWindowClose` subsumes both — spec §4.2 explicitly folds "the two `uiClearToken` calls in `removeWindow`"). The reveal plant is defensible and **load-bearing**: it is one of the **two documented duty-3 exceptions** in CLAUDE.md's invariants (no-save-under window disposals have no `bitsRestore` rect, so the cycle-diff net is blind — the manual reveal is the only same-present invalidation for that class). The diag `warning()` is fork noise | → onWindowClose(token) (L3) for both clears; the reveal plant → onRestore(token=0, rect) (L2) (documented duty-3 exception, retained); diag line: delete |

<!-- coverage: 2 hunks (ports.cpp), diff order:
  hunk 1 (@@ -34,6 +34,7) = PO1(roger include);
  hunk 2 (@@ -520,20 +521,67) = PO2(drawWindow open hook) + PO3(removeWindow close hook) — the two functions are adjacent, so the diff emits ONE hunk spanning both.
  So: hunk 1 -> PO1; hunk 2 -> PO2, PO3. -->

### 3.10 engines/sci/event.cpp (52 lines, 4 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| E1 | event.cpp:37 | `#include "sci/roger/roger_art_provider.h"` in the event file | W (+concern) | Not mechanical: same `roger/`-path leak as P1/K1 — a neutral observer header belongs here | → replace with `sci_gfx_observer.h`; observer-side |
| E2 | event.cpp:206-219 | `getScummVMEvent()` mouse-move re-present: tracks `sawMouseMove` through the mouse-move skip loop, then `onMouseMoved()` when set (Roger draws its cursor into the overlay, which does not tick `kernelAnimate` during blocking dialogs/menus, so the composited cursor would freeze) | N | Mechanical null-guarded notification at the one place SCI sees the discarded mouse-moves; genuinely useful to any consumer compositing its own cursor. Per spec §4.4 this may survive as an L1 notification if a generic-consumer story holds — the cursor-tracking case is exactly that | → onMouseMoved() candidate L1 notification (spec §4.4); observer-side otherwise |
| E3 | event.cpp:248-251 | side-by-side compare-mode mouse remap: `remapComparisonMouse(mousePos)` so a click in the left (enhanced) SBS panel drives the game (no-op in other display modes) | D | Display-mode plumbing for a Roger-only feature (SBS compare); listed under spec §4.4 as an observer-side/fork concern, not part of the generalized seam | observer-side (fork-only) |
| E4 | event.cpp:271-304 | Roger debug hotkeys + tune-panel mouse swallow: F10 `toggleOverlay()` / F11 `toggleDebugLog()` / F12 `toggleTunePanel()` consumed on keydown; then the mouse-button swallow that routes L/R button events to `tunePanelMouse()` while the tune panel is open, self-labeled **"TEMPORARY DEBUG TOOL ... Delete with the tune panel."** | D | Pure dev-tool input handling — F10–F12 hotkeys are the display-mode/diag/panel toggles (spec §4.4 observer-side), and the tune-panel mouse swallow is quoted as explicitly temporary in-source; neither belongs in a neutral observer | fork-only (delete the tune-panel swallow with the panel; F10/F11/F12 toggles stay observer-side) |

<!-- coverage: 4 hunks (event.cpp), diff order:
  1=E1(roger include);
  2=E2(getScummVMEvent mouse-move sawMouseMove + onMouseMoved);
  3=E3(remapComparisonMouse SBS remap);
  4=E4(F10-F12 hotkeys + TEMPORARY tune-panel mouse swallow) — one hunk spanning both blocks. -->

### 3.11 engines/sci/sci.cpp (67 lines, 4 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| S1 | sci.cpp:22-24,32 | `#define FORBIDDEN_SYMBOL_EXCEPTION_getenv` (with its explanatory comment) at the top of the file, plus the `#include "engines/metaengine.h"` that S3's `EngineMan.findTarget` needs | **D (+blocker concern)** | **Upstream-forbidden.** ScummVM's `common/forbidden.h` poisons `getenv`; the fork rules (CLAUDE.md portability + `common/forbidden.h`) say **never add a `FORBIDDEN_SYMBOL_EXCEPTION_*`**. The exception exists solely to read `ROGER_STUDIO`/`ROGER_EYETEST`/`ROGER_NO_LAUNCHER` dev env vars (S4, S5) — an upstream slice must replace those env gates with a non-`getenv` mechanism (ConfMan key / command-line arg). The `metaengine.h` include is legitimate wiring for the G-bucket caption fix (S3) | env `#define` → **must not ship upstream** (flag: upstream-forbidden); replace env gates before any upstream slice. The `metaengine.h` include rides with S3 (standalone-PR) |
| S2 | sci.cpp:75-78,229-231 | Provider lifecycle: the four `roger/` includes (`file_roger_art_provider.h`, `launcher/roger_launcher.h`, quarantined `utils/studio/roger_studio.h` + `utils/eyetest/roger_eyetest.h`) and the destructor `delete g_sciRogerProvider; g_sciRogerProvider = nullptr;` | W | Provider new/delete lifecycle wiring — becomes plugin self-registration via `setArtProvider()` (Stage 3). The concrete `roger/`-path includes leak the same way as P1 but here they are the actual instantiation site, not a hook | → provider registration API (`setArtProvider()`); includes move to the plugin's `module.mk` self-registration |
| S3 | sci.cpp:401-411 | `run()` window-caption fix: `EngineMan.findTarget(ConfMan.getActiveDomainName())` re-derives the game's full canonical title from the detection plugin's game table and `setWindowCaption()`s it (main.cpp's stored "description" key can be stale/series-level; leaves caption untouched on lookup failure) | **G** | Standalone upstreamable today, **no Roger dependency** — a general SCI caption-quality fix using only detection data and no hardcoded strings. Needs an upstream-facing justification independent of Roger (the stale-caption case) | standalone-PR (independent of observer work) |
| S4 | sci.cpp:413,415-433 | Provider instantiation + Studio/EyeTest env-gated blocks: `g_sciRogerProvider = new FileRogerArtProvider(...)`, then `if (getenv("ROGER_STUDIO"))` runs `RogerStudio` and returns, and `if (getenv("ROGER_EYETEST"))` runs `RogerEyeTest` and returns — each quarantined dev utility's ONLY engine reference | W (instantiation) + D (env-gated dev blocks) | The `new FileRogerArtProvider` is provider wiring (W, pairs with S2's delete). The two `getenv`-gated dev-utility launch blocks are downstream-only dev tools (D) and depend on the S1 forbidden-symbol exception | instantiation → provider registration (W); Studio/EyeTest blocks → fork-only (D), env gates replaced before upstream |
| S5 | sci.cpp:435-455 | launcher / `skipLauncher` block: `skipLauncher` from `roger_no_launcher` ini key OR `getenv("ROGER_NO_LAUNCHER")`; when not skipping, `RogerLauncher::run()` (returns false → engine restart on game-switch); else `precacheAll()` synchronous warm-up | D/W | The picker dialog + precache orchestration is Roger launcher plumbing — dev/fork lifecycle, not part of the neutral seam; the `getenv` half of the gate again depends on the S1 forbidden exception | fork-only (D/W); env gate replaced before upstream (the ini-key half is portable) |

<!-- coverage: 4 hunks (sci.cpp), diff order:
  hunk 1 (@@ -19..) = S1(FORBIDDEN_SYMBOL_EXCEPTION_getenv define + comment) + S1(metaengine.h include);
  hunk 2 (@@ -67/-72..) = S2-includes(4 roger includes);
  hunk 3 (@@ -218/-227..) = S2-delete(destructor delete g_sciRogerProvider);
  hunk 4 (@@ -387/-398..) = S3(caption findTarget) + S4(provider new + Studio/EyeTest env blocks) + S5(skipLauncher / launcher / precache) — one large run() hunk spanning all three.
  (S1 spans two edits in hunk 1; S2 spans hunks 2+3; hunk 4 -> S3,S4,S5. Every hunk maps to >=1 row.) -->

### 3.12 engines/sci/graphics/scifont.{cpp,h} (2+5 lines, 2+1 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| F1 | scifont.cpp:321-322 + :355 (was `#ifdef ENABLE_SCI32` / `#endif`) | Removes the `#ifdef ENABLE_SCI32` ... `#endif` guard around `GfxFontFromResource::drawToBuffer`, so the glyph-into-arbitrary-buffer renderer compiles for SCI0/SCI1 too (two hunks: the opening `-#ifdef` and the closing `-#endif`) | **G** | Standalone upstreamable today, **no Roger dependency** — `drawToBuffer` has no SCI32-specific code; the method is generally useful (SCI32 text path already used it, Roger's upscaled-native-font path now also does). Un-gating is a clean generalization | standalone-PR (independent of observer work) |
| F2 | scifont.h:65-67 | Matching header change: removes the `#ifdef ENABLE_SCI32` guard around the `drawToBuffer` override decl and replaces the `// SCI2/2.1 equivalent` comment with a neutral "no SCI32 dependency" doc comment | **G** | Same standalone fix as F1 (the header side of un-gating the method) — declaration must be un-guarded to match the definition | standalone-PR (pairs with F1 in the same PR) |

<!-- coverage: scifont.cpp 2 hunks + scifont.h 1 hunk, diff order:
  scifont.cpp hunk 1 (@@ -319..) = F1-open(remove `#ifdef ENABLE_SCI32` before drawToBuffer);
  scifont.cpp hunk 2 (@@ -355..) = F1-close(remove `#endif` after drawToBuffer);
  scifont.h hunk 1 (@@ -62..) = F2(remove `#ifdef`/`#endif` + comment rewrite on the decl).
  (F1 spans both scifont.cpp hunks — the open/close of one guard; F2 = the single scifont.h hunk.) -->

### 3.13 Wiring: engines/sci/module.mk + gui/EventRecorder.h + test/module.mk (36+6+6 lines, 1+1+1 hunks)

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| W1 | module.mk:106-141 | `MODULE_OBJS += \` block listing all ~33 `roger/*.o` object files (provider, gen pipeline, overlay, launcher, ui, quarantined utils/studio + utils/eyetest + utils/tunepanel, png_loader) | W | Pure build plumbing — the object list for the Roger sources. Stage 3 plan: `roger/*.o` move to the plugin's own `module.mk` when the provider becomes self-registering | → move to plugin `module.mk` (provider self-registration); build-only |
| W2 | gui/EventRecorder.h:197-201 | Removes the `#ifdef USE_IMGUI` / `#endif` guard around the `isImGuiRecorderEnabled() const` declaration and adds a comment: the method is defined unconditionally in `EventRecorder.cpp` (returns false when `USE_IMGUI` off) and called from unguarded paths, so gating the decl breaks event-recorder builds without the ImGui debugger | **G** | Standalone upstreamable today, **no Roger dependency** — a genuine compile-correctness fix: the declaration must match the unconditional definition and unguarded call sites. Entirely independent of the observer work | standalone-PR (independent of observer work) |
| W3 | test/module.mk:63-68 | Adds an `ifeq ($(ENABLE_SCI), STATIC_PLUGIN)` block: `TESTS += test/sci/roger/*.h`, `TEST_LIBS += engines/sci/libsci.a`, `TEST_CFLAGS += -DFIXTURE_DIR=...` — wires the Roger CxxTest suite into the make test build | W | Pure test-build plumbing. Stage 3 plan: the tests relink against a Roger static lib when the provider is exiled to a plugin | → relink tests against roger static lib (Stage 3); build-only |

<!-- coverage: module.mk 1 hunk + EventRecorder.h 1 hunk + test/module.mk 1 hunk, diff order:
  module.mk hunk 1 (@@ -103..) = W1(MODULE_OBJS += roger objects);
  EventRecorder.h hunk 1 (@@ -194..) = W2(un-#ifdef isImGuiRecorderEnabled decl);
  test/module.mk hunk 1 (@@ -60..) = W3(ENABLE_SCI STATIC_PLUGIN test block).
  One hunk per file, one row per hunk. -->

<!-- Section 3 coverage total (all subsections 3.1-3.13):
  3.1 paint16 18 (17 cpp + 1 h); 3.2 kgraphics 6; 3.3 cursor 2; 3.4 animate 14;
  3.5 text16 4; 3.6 transitions 2; 3.7 controls16 9; 3.8 menu 12 (9 cpp + 3 h);
  3.9 ports 2; 3.10 event 4; 3.11 sci.cpp 4; 3.12 scifont 3 (2 cpp + 1 h);
  3.13 wiring 3 (module.mk 1 + EventRecorder.h 1 + test/module.mk 1).
  Sum = 18+6+2+14+4+2+9+12+2+4+4+3+3 = 83 hunks across 18 files. Matches §2 baseline (830 lines / 83 hunks / 18 files). -->


## 4. Seam inventory

<!-- Tasks 6-7 append per-class subsections here -->

## 5. L2 completeness verification

<!-- Task 9 -->

## 6. Consolidation table and line budget

<!-- Task 8 -->

## 7. Gap list

<!-- Task 8 -->

## 8. Standalone upstream PR candidates (G bucket)

These are the standalone, immediately-submittable upstream fixes (bucket **G**) —
each is independent of the observer work and of Roger, per spec §8 ("cheap goodwill
before the big pitch"). One entry per G-bucket row across all of section 3; the
scifont `.cpp` and `.h` halves (F1 + F2) are one PR (both sides of un-gating the same
method). **Task 5 seeds; Task 10 finalizes.**

| Candidate | G rows | Change | Effort | Depends on Roger? |
|---|---|---|---|---|
| scifont drawToBuffer un-gating | F1 (scifont.cpp), F2 (scifont.h) | remove `#ifdef ENABLE_SCI32` around `GfxFontFromResource::drawToBuffer` (definition + override decl) | trivial (2 files, ~7 lines) | No |
| EventRecorder.h decl fix | W2 (gui/EventRecorder.h) | `isImGuiRecorderEnabled()` declared unconditionally (matches its unconditional definition + unguarded call sites) | trivial (1 file, ~6 lines) | No |
| Window caption from detection | S3 (sci.cpp) | `SciEngine::run` sets the OS window caption via `EngineMan.findTarget` (full canonical title, no hardcoded strings) | small (1 file, ~12 lines); needs an upstream-facing justification independent of Roger (stale/series-level caption) | No |
| text16 textHeight=0 init | T2 (text16.cpp, §3.5) | `textHeight = 0` initializer silences a real uninitialized-read path | trivial (1 line) | No |

## 9. Findings that contradict the design spec

<!-- any task may append here; Task 10 resolves -->
