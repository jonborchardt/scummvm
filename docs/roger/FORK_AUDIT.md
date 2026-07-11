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
| T4 | text16.cpp:681-715,723-725 | Per-line capture block inside Box's draw loop: builds the exact placed line rect (offset+hline, measured textWidth/textHeight), `offsetRect` to global space, window-scoped token `0x60000000\|port->id`, emits `onNativeText(...)` per drawn line regardless of `show` (the align arg is hardcoded `SCI_TEXT16_ALIGNMENT_LEFT` at text16.cpp:711 — the per-line offset already encodes placement; relevant to the future `onText(align, …)` signature); `lineCount++`; plus the post-loop comment noting the old whole-box hook was deleted | N | Mechanical observe (the load-bearing per-line-rect fix), but carries the token/offsetRect marshalling §4.3/§4.2 consolidate; the war-story comments relocate here | → onText(rect, text, font, pen, back, align, metrics, token, source=textBox) (L3); token+offsetRect helper observer-side; comments → FORK_AUDIT |

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
| M1 | menu.h:25-28 | Adds `#include` of `common/{array,list,rect,str}.h` — the `array`/`rect`/`str` includes serve only the `RogerMenuRow` struct and the `Common::Array` state members; `list.h` is NOT Roger's (it serves the pre-existing `GuiMenuList`/`GuiMenuItemList` typedefs at menu.h:55/80) and stays regardless | S | Header-weight added purely to carry Roger state (M3) into `GfxMenu` — vanishes when the state is exiled | delete with M3 (observer-side keeps the state) |
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

### 3.14 base/main.cpp (post-audit addition, 2026-07-10)

Added on branch `jon-standalone-picker` (after the audit baseline at 14509d438c3).
Measured: **13 lines added, 0 deleted** (`git diff 4e23cc4859e..HEAD -- base/main.cpp`
filtered for true `+`/`-` lines, excluding diff headers).

| # | Site (file:line) | What it does | Bucket | Defensibility verdict | Disposition |
|---|---|---|---|---|---|
| B1 | main.cpp: include block | `#if PLUGIN_ENABLED_STATIC(SCI)` guard + `#include "sci/roger/launcher/roger_standalone.h"` — pulls the standalone-launcher declaration into the main translation unit | **D/W** | Fork plumbing: a `roger/`-path include inside a core ScummVM file, guarded so it compiles away when SCI is not a static plugin. The guard keeps it out of dynamic-plugin builds but the include still leaks the concrete path into `base/` | fork-only; the include rides with the call (B2); not part of any upstream slice |
| B2 | main.cpp: launcherDialog() do-while body | `#if PLUGIN_ENABLED_STATIC(SCI)` guard + `if (Sci::Roger::rogerStandaloneLauncher()) { status = true; continue; }` — runs the Roger picker as the launcher round; if it returns true (picker handled the round), skips the stock GUI::LauncherDialog | **D/W** | Fork-only behavior change: the stock launcher is replaced by the Roger picker for each no-target launcher round, unless `roger_no_launcher` / `ROGER_NO_LAUNCHER` opts out. Never upstreamable as-is (hardcodes a concrete engine's picker in the shared `base/` bootstrap) | fork-only |

**Line-budget note:** the §2 baseline (830 lines / 18 files) predates this file.
The measured addition is **+13 lines** in `base/main.cpp` (1 new file in the
inventory). These lines fall entirely in bucket D/W with disposition fork-only, so
they do not affect the §6 neutral-seam projection (fork-only rows are carved out of
the seam measurement per the R20 rule); the §2 raw baseline grows from 830 to **843**.

<!-- coverage: 2 hunks (main.cpp), diff order:
  hunk 1 (@@ -34) = B1(PLUGIN_ENABLED_STATIC(SCI) include block);
  hunk 2 (@@ -116) = B2(launcherDialog do-while call block). -->

## 4. Seam inventory

Convention (all 4.x subsections): the constructor and destructor are collapsed
into a **single** excluded `ctor / dtor` row; the `(NN public methods)` count in
each header includes that one row (so `row count == NN == the header's public
method rows, ctor+dtor counted once`). Overloaded methods each get their own row.
Public **member variables** (e.g. `GfxText16::_font`; `GfxPorts::_wmgrPort`,
`_picWind`, `_menuPort`, `_menuBarRect`, `_menuRect`, `_menuLine`, `_curPort`) are
data, not methods — ignored per the audit method. `#if 0`-compiled-out decls
(`GfxText16::ClearChar`, `ShowString`) are not part of the public surface and are
not counted. Hooked rows cite section-3 row ids; a method that reaches a hook only
by delegating to a hooked leaf is marked hooked with a "(via …)" note.

### 4.1 GfxPaint16 (36 public methods)

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| init | excluded | wires `_animate`/`_text16` pointers; no draw effect |
| debugSetEGAdrawingVisualize | excluded | sets `_EGAdrawingVisualize` flag; no draw effect |
| drawPicture | hooked | paint16.cpp:91-108 pre-hook + :135-149 post-hook (rows P2, P3, P4) — the room-background replace/prefetch seam |
| drawCelAndShow | hooked | paint16.cpp:158-176 (`beginNativeDraw`, `onInitCel`) + :189-190 close (rows P5, P6, P7) |
| drawCel (celRect, GuiResourceId) | derivable-from-L2 | thin wrapper → the GfxView overload; draws into the 320×200 buffers, no bitsShow; caller issues a separate show onShow covers |
| drawCel (celRect, GfxView\*) | derivable-from-L2 | "This version of drawCel is not supposed to call bitsShow()!" (paint16.cpp:199) — mutates visual/priority/control buffers only; screen reached via a later bitsShow the caller issues (L2 onShow). No text/owner semantics lost |
| drawHiresCelAndShow | hooked | paint16.cpp:227-230 begin + :250-251 close (rows P8, P9) — KQ6 hires-cel path composited via onDrawCel |
| redrawHiresCels | derivable-from-L2 | replays stored hires draws through `drawHiresCelAndShow` (itself hooked at P8/P9); no distinct screen-reaching path |
| clearScreen | derivable-from-L2 | → `fillRect` of the whole port; buffer fill only, reaches screen via a caller show |
| invertRect | derivable-from-L2 | → `fillRect` in pen-mode 2; buffer-only invert (textedit caret / inversion), no own show — L2 onShow covers |
| invertRectViaXOR | derivable-from-L2 | SCI0early-only; XORs visual bytes directly (`_screen->putPixel`), no bitsShow; reaches screen via caller show |
| eraseRect | derivable-from-L2 | → `fillRect` with backClr; buffer fill only |
| paintRect | derivable-from-L2 | → `fillRect` with penClr; buffer fill only |
| fillRect | derivable-from-L2 | the shared buffer-fill primitive (`_screen->putPixel` loops); no show — every visible fill reaches screen via a separate bitsShow/updateBox (L2 onShow). No text/owner semantics lost |
| frameRect | derivable-from-L2 | four `paintRect` edge fills into the buffer; no own show (the semantic selection-frame is captured separately at controls16 C7 / paint16 P16, not here) |
| bitsShow | hooked | paint16.cpp:369 sig + :398-410 Feeder-B body (rows P10, P11) — the L2 pixel-truth funnel `onNativeShowRect` |
| bitsSave | hooked | paint16.cpp:428-433 journal checkpoint `onNativeSaveRect` (row P12) |
| bitsGetRect | excluded | pure accessor — writes the saved hunk's rect into `*destRect`; no draw effect |
| bitsRestore | hooked | paint16.cpp:452-460 journal rollback `onNativeRestoreRect` (row P13) |
| bitsFree | hooked | paint16.cpp:471-475 journal drop `onNativeFreeSave` (row P14) |
| kernelDrawPicture | hooked | (via drawPicture) delegates to `drawPicture` (paint16.cpp:485/497), captured at P2/P4 |
| kernelDrawCel | hooked | paint16.cpp:507-520 standalone-cel `onDrawCel` hook (row P15); also delegates to drawCelAndShow (P5-P7) / drawHiresCelAndShow (P8-P9) |
| kernelGraphFillBoxForeground | derivable-from-L2 | → `paintRect` buffer fill; no own show |
| kernelGraphFillBoxBackground | derivable-from-L2 | → `eraseRect` buffer fill; no own show |
| kernelGraphFillBox | derivable-from-L2 | → `fillRect`; buffer fill only, reaches screen via a caller updateBox/bitsShow (L2 onShow) |
| kernelGraphFrameBox | hooked | paint16.cpp:543-554 `uiPushFrameBox` after the native `frameRect` (row P16) |
| kernelGraphDrawLine | derivable-from-L2 | clips/offsets then `_screen->drawLine` into the buffer; no bitsShow — reaches screen only via a bitsShow/kernelGraphUpdateBox the caller issues (L2 onShow, the §3.2 "line never hooked" note). No text/owner semantics lost |
| kernelGraphSaveBox | hooked | (via bitsSave) → `bitsSave` (paint16.cpp:564), captured at P12 |
| kernelGraphRestoreBox | hooked | (via bitsRestore) → `bitsRestore` (paint16.cpp:568), captured at P13 |
| kernelGraphUpdateBox | derivable-from-L2 | paint16.cpp:571-574 → `bitsShow(rect)`; the show itself is L2 onShow via P10/P11. (The P17 diag line here is a to-be-deleted dev warning, not a distinct hook — coverage is the bitsShow funnel, not P17) |
| kernelGraphRedrawBox | hooked | paint16.cpp:577-581 `onNativeEraseRect` on the global rect (row P18) |
| kernelDisplay | hooked | paint16.cpp:705-717 background-fill push + :740-764 flush brackets (rows P19, P20); per-line text captured downstream at GfxText16::Box (T4) |
| kernelPortraitLoad | excluded | no-op stub (returns NULL_REG; body commented out); no draw effect |
| kernelPortraitShow | derivable-from-L2 | KQ6CD speech-sync portrait animation — draws + shows natively via `Portrait::doit` (its own bitsShow crosses L2 onShow); not on any Roger-supported EGA game. No text/owner semantics beyond the pixels |
| kernelPortraitUnload | excluded | empty body; no draw effect |

### 4.2 GfxPorts (43 public methods)

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| init | excluded | wires paint16/text16 pointers + gfx-function mode; no draw effect |
| reset | excluded | resets port state; no draw effect |
| kernelSetActive | derivable-from-L2 | selects the active port (state); may `bitsShow` the menu port on restore — that show is L2 onShow (P10/P11). No text/owner semantics of its own |
| kernelGetPicWindow | excluded | pure accessor — returns the pic window rect / top-left; no draw effect |
| kernelSetPicWindow | excluded | sets pic-window geometry + priority-band flag (state); no draw effect |
| kernelGetActive | excluded | pure accessor — returns the active port's reg_t id; no draw effect |
| kernelNewWindow | hooked | (via drawWindow) → `addWindow` + `drawWindow` (ports.cpp:238), window-open captured at PO2 |
| kernelDisposeWindow | hooked | (via removeWindow) → `removeWindow` (ports.cpp:247), window-close captured at PO3 |
| setActiveWindowHasEditText | excluded | records the edit-text port id (state); no draw effect |
| isFrontWindow | excluded | pure predicate over the window list; no draw effect |
| beginUpdate | derivable-from-L2 | walks front windows issuing `bitsSave`/`bitsRestore` (paint16, hooked P12/P13); no distinct un-covered screen path |
| endUpdate | derivable-from-L2 | mirror of beginUpdate; restores via `bitsRestore` (L2 onRestore, P13) |
| addWindow | hooked | (via drawWindow) may call `drawWindow` when `draw` set (else deferred to kernelNewWindow's explicit draw) — window-open captured at PO2 |
| drawWindow | hooked | ports.cpp:524-551 `uiPushWindow` + titlebar `uiPushText` + tokened `bitsShow` (row PO2) |
| removeWindow | hooked | ports.cpp:557-585 two `uiClearToken` clears + no-save-under reveal plant (row PO3) |
| freeWindow | excluded | frees the disposed window struct/hunks; no draw effect (dispose signal already fired at removeWindow/PO3) |
| updateWindow | derivable-from-L2 | re-snapshots save-unders via `bitsSave`/`bitsRestore` (hooked P12/P13); effect crosses L2 save/restore |
| getPortById | excluded | pure accessor — index into `_windowsById`; no draw effect |
| setPort | excluded | swaps the current port pointer (state); no draw effect |
| getPort | excluded | pure accessor — returns current port; no draw effect |
| setOrigin | excluded | sets port origin (state); no draw effect |
| moveTo | excluded | sets port cursor position (state); no draw effect |
| move | excluded | relative cursor move (state); no draw effect |
| openPort | excluded | initializes a port's fields (state); no draw effect |
| penColor | excluded | sets pen colour (state); no draw effect |
| backColor | excluded | sets back colour (state); no draw effect |
| penMode | excluded | sets pen mode (state); no draw effect |
| textGreyedOutput | excluded | sets greyed-output flag (state); no draw effect |
| getPointSize | excluded | pure accessor — current font height; no draw effect |
| offsetRect | excluded | pure computation — port-local → global rect (the marshalling helper hooks reuse); no draw effect |
| offsetLine | excluded | pure computation — port-local → global line endpoints; no draw effect |
| clipLine | excluded | pure computation — clips a line to the port; no draw effect |
| priorityBandsInit (bandCount, top, bottom) | excluded | fills the `_priorityBands` LUT; no draw effect (priority stays native per Stage 1) |
| priorityBandsInit (SciSpan) | excluded | LUT init from data; no draw effect |
| priorityBandsInitSci11 | excluded | SCI1.1 LUT init from data; no draw effect |
| kernelInitPriorityBands | excluded | recomputes the band LUT; no draw effect |
| kernelGraphAdjustPriority | excluded | sets priority top/bottom + reinit LUT; no draw effect |
| kernelCoordinateToPriority | excluded | pure computation — y → priority band; no draw effect |
| kernelPriorityToCoordinate | excluded | pure computation — priority band → y; no draw effect |
| processEngineHunkList | excluded | walks ports adding save-under hunks to the GC worklist; no draw effect |
| printWindowList | excluded | debug console dump of the window list; no draw effect |
| saveLoadWithSerializer | excluded | savegame (de)serialization of port state; no draw effect |

### 4.3 GfxText16 (22 public methods)

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| GetFontId | excluded | pure accessor — current font id; no draw effect |
| GetFont | excluded | loads/returns the current `GfxFont`; no draw effect |
| SetFont | excluded | sets the active font (state); no draw effect |
| CodeProcessing | excluded | parses inline `|c|`/`|f|` control codes, mutates font/colour state (or measures); no glyphs drawn here |
| GetLongest | excluded | pure computation — longest line that fits `maxWidth`; no draw effect |
| Width | excluded | pure computation — measures text width/height; no draw effect |
| StringWidth | excluded | pure computation — → `Width`; no draw effect |
| DrawString (str, font, pen) | derivable-from-L2 | → `Draw` (glyphs into the buffer via `_font->draw`), no own bitsShow; reaches screen via a caller show (L2 onShow). Semantic text captured at the higher Box/control/status seams, not this low-level path |
| Size | excluded | pure computation — measures wrapped-block rect (width×height); no draw effect |
| Draw | derivable-from-L2 | draws each glyph directly into the buffer (`_font->draw`), no bitsShow (text16.cpp:505-555); the §3.2 "direct-draw path outside Box" — Roger deliberately captures text at Box (T4) not here. Reaches screen via a caller show (L2 onShow) |
| Show | derivable-from-L2 | `Draw` then `_paint16->bitsShow(rect)` (text16.cpp:557-566) — its show IS the L2 onShow funnel (P10/P11); semantics captured at Box (T4) |
| Box (languageSplitter, 6-arg) | hooked | text16.cpp:681-725 per-line capture `onNativeText` inside the draw loop (row T4) — THE text chokepoint |
| Box (5-arg overload) | hooked | (via 6-arg Box) inline forwarder → `Box(text, 0, show, …)` (text16.h:72-74), captured at T4 |
| DrawString (str) | derivable-from-L2 | → `Draw` (glyphs into buffer, RTL-aware), no own show; reaches screen via a caller show |
| DrawStatus | derivable-from-L2 | draws status glyphs directly into the buffer (`_font->draw`, text16.cpp:745-774), no bitsShow; the status-banner semantics are captured at the menu seam (kernelDrawStatus → `uiPushStatus`, row M13), not here |
| allocAndFillReferenceRectArray | excluded | pure computation — returns the collected code-ref rects as a reg_t array; no draw effect |
| kernelTextSize | excluded | pure computation — → `Size`, writes width/height out params; no draw effect |
| kernelTextFonts | excluded | loads the inline code-font table (state); no draw effect |
| kernelTextColors | excluded | loads the inline code-colour table (state); no draw effect |
| macTextSize | excluded | pure computation — Mac hires text measurement; no draw effect |
| macDraw | excluded | Mac SCI hires (640×400) text draw via the Mac font manager; never reached on a Roger-supported EGA game (Mac-hires-only path, analogous to the SCI32-only exclusion) |

### 4.4 GfxControls16 (7 public methods)

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| kernelDrawButton | hooked | controls16.cpp:356-364 `uiPushButton` (row C5) |
| kernelDrawText | hooked | controls16.cpp:444-452 `uiPushText(source=control)` + :465-478 SELECTED `uiPushFrameBox` (rows C6, C7) |
| kernelDrawTextEdit | hooked | controls16.cpp:503-511 `uiPushTextEdit` (row C8) |
| kernelDrawIcon | hooked | controls16.cpp:539-546 `uiPushIcon` (row C9) |
| kernelDrawList | hooked | (via drawListControl) → private `drawListControl` per-row hook (controls16.cpp:117-134, row C2) |
| kernelTexteditChange | hooked | controls16.cpp:295-303 width-cap relaxation (row C3, claim) + :321-334 live-typing `uiPushTextEdit` (row C4) |

### 4.5 GfxMenu (9 public methods)

Same convention as 4.1: ctor+dtor collapse to one excluded row; public
member variables are ignored; the `(9)` count == the row count == the public
methods (ctor+dtor counted once). GfxMenu's hooked draw logic lives in the
**private** helpers `drawMenu` / `invertMenuSelection` / the three `rogerPush*`
methods (rows M5–M11) — those are not part of the public surface, so their
public entry points below are marked hooked "(via …)". The bar/status/select
public methods carry their own hooks directly.

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| reset | excluded | clears the menu/item lists + `_curMenuId`/`_curItemId` state (menu.cpp); no draw effect |
| kernelAddEntry | excluded | parses a menu definition string into `GuiMenuEntry`/`GuiMenuItemEntry` state; no draw effect |
| kernelSetAttribute | excluded | mutates an item entry's text/key/enabled/tag fields (state); no draw effect |
| kernelGetAttribute | excluded | pure accessor — reads an item entry's attribute into a reg_t; no draw effect |
| drawBar | hooked | menu.cpp:372-405 per-title collection + `rogerPushBarOverlay()` (rows M5, M6) — the menu-bar `onText(menuBar)` seam |
| kernelSelect | hooked | menu.cpp:605-606 `rogerClearMenuOverlay()` at close (row M7); drives the interactive loop that reaches the hooked private `drawMenu` (M8-M10) / `invertMenuSelection` (M11) dropdown captures |
| kernelDrawStatus | hooked | menu.cpp:1177-1184 `uiPushStatus` score/title banner into the overlay top strip (row M13) |
| kernelDrawMenuBar | hooked | (via drawBar) `clear`→erases the bar, else → `drawBar()` (menu.cpp), whose per-title push is captured at M5/M6 |

### 4.6 GfxAnimate (18 public methods)

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| isFastCastEnabled | excluded | pure accessor — returns `_fastCastEnabled`; no draw effect |
| disposeLastCast | excluded | clears `_lastCastData` (state); no draw effect |
| invoke | derivable-from-L2 | runs each cast object's `doit` (game-logic script calls); any drawing happens later in the frame via the hooked update/drawCels/updateScreen path, not here |
| makeSortedList | excluded | pure computation — builds the priority-sorted `_list` from the kAnimate list; no draw effect |
| applyGlobalScaling | excluded | pure computation — sets an entry's `scaleX`/`scaleY` from global scale (state); no draw effect |
| fill | derivable-from-L2 | resolves cel geometry/nsRect and paints hidden cels into the buffers; screen reached via the frame's hooked updateScreen/onAnimateFrame path (A6-A9, A13). No text/owner semantics lost |
| update | hooked | animate.cpp:427-431 + :461-465 `onInitCel` during `_picNotValid` at both cast-draw branches (rows A3, A4) |
| drawCels | hooked | animate.cpp:491-495 `onInitCel` during `_picNotValid` (row A5) |
| updateScreen | hooked | animate.cpp:512-516 begin + :550-552 end `beginNativeDraw`/`endNativeDraw` self-draw bracket (rows A6, A7) |
| restoreAndDelete | derivable-from-L2 | restores each cast object's save-under via `bitsRestore` (hooked P13/onRestore) then deletes disposed entries; the composite that follows is `renderFromAnimateList` at A13 (onAnimateFrame). No un-covered screen path |
| reAnimate | hooked | animate.cpp:585-589 begin + :608-613 end bracket + `renderFromAnimateList` (rows A8, A9) — dialog-dismissal cel redraw |
| addToPicDrawCels | hooked | animate.cpp:652-654 `onAddToPicCel` static-baked cel capture (row A10) |
| addToPicDrawView | hooked | animate.cpp:673-675 `onAddToPicCel` single addToPic view (row A11) |
| printAnimateList | excluded | debug console dump of `_list`; no draw effect |
| kernelAnimate | hooked | animate.cpp:756-765 `snapshotNativeBaseline` + `renderFromAnimateList` at the cycle frame boundaries (row A13); entry telemetry A12/A14 is fork-only dev |
| kernelAddToPicList | hooked | (via addToPicDrawCels) → `addToPicDrawCels` after building the list, captured at A10 |
| kernelAddToPicView | hooked | (via addToPicDrawView) → `addToPicDrawView`, captured at A11 |

### 4.7 GfxTransitions (3 public methods)

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| setup | excluded | stores `_number`/`_blackoutFlag` only (transitions.cpp:117-127); the actual effect runs in `doit`, so nothing draws here |
| doit | hooked | transitions.cpp:184-208 claim block: `onTransition(...)` then instant SCI finalize + early-return, skipping the native animated transition (row TR2) |

### 4.8 GfxPalette (41 public methods)

Class `GfxPalette` in `engines/sci/graphics/palette16.h` — the SCI16 (SCI0-SCI1.1)
palette (the SCI32 variant `GfxPalette32` in `palette32.h` is out of scope,
SCI32-only). No hook exists in `palette16.cpp` (grep for `g_sciRogerProvider`
returns nothing) — the fork's `roger_palette_live` behavior is compositor-side
(re-applies the live `_sysPalette` to the RGBA plate each frame). *(Shipped
2026-07-10, commit 2f14bb179c5: the §7 kept gap has since landed —
`onPaletteChanged` now hooks the `copySysPaletteToScreen` funnel; this
subsection's no-hook statement describes the audited baseline.)* Public member
`_sysPalette` is data, ignored per convention. The palette methods change the
color LUT, not the framebuffer bytes; on the EGA path `setOnScreen` →
`copySysPaletteToScreen` → `_screen->setPalette` (an OSystem *palette* upload,
not a `copyRectToScreen`), so palette changes emit **no L2 pixel event** — the
basis for the per-tick-vary real-gap verdict below.

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| isMerging | excluded | pure accessor — returns `_useMerging`; no draw effect |
| isUsing16bitColorMatch | excluded | pure accessor — returns `_use16bitColorMatch`; no draw effect |
| setDefault | excluded | loads the default palette into `_sysPalette` state (EGA/Amiga/resource dispatch); LUT only, no pixel event |
| createFromData | excluded | pure computation — parses palette resource bytes into a `Palette` out-param; no draw effect |
| setAmiga | excluded | loads the Amiga palette into state; LUT only, no pixel event |
| modifyAmigaPalette | excluded | rewrites Amiga palette entries (state); LUT only, no pixel event |
| setEGA | excluded | installs the 16+mix EGA palette into `_sysPalette` (state); LUT only, no pixel event |
| set | excluded | merges/installs a palette into `_sysPalette` and may `setOnScreen`; a palette (LUT) upload, no `copyRectToScreen` — not an L2 pixel event |
| insert | excluded | pure computation — merges `newPalette` into `destPalette`, returns changed flag; no draw effect |
| merge | excluded | merges a source palette into `_sysPalette` (state); LUT only |
| matchColor | excluded | pure computation — nearest-palette-index search; no draw effect |
| setOnScreen | derivable-from-L2 | → `copySysPaletteToScreen` (palette16.cpp:478); an OSystem palette upload, not a pixel show. The *steady-state* re-apply is already covered compositor-side by `roger_palette_live` reading `_sysPalette` per frame — no new semantics at this one-shot call. (Smooth per-tick vary is the distinct real-gap row below) |
| copySysPaletteToScreen | derivable-from-L2 | palette16.cpp:486 — builds `bpal[3*256]` and `_screen->setPalette`; a LUT upload with no pixel event, mirrored compositor-side by the live re-apply |
| drewPicture | excluded | bumps the palette timestamp + reloads a palVary target on picture change (state); no draw effect |
| kernelSetFromResource | excluded | loads a palette resource into `_sysPalette` via `set` (state/LUT); no pixel event |
| kernelSetFlag | excluded | sets per-color flag bits in `_sysPalette` (state); no draw effect |
| kernelUnsetFlag | excluded | clears per-color flag bits (state); no draw effect |
| kernelSetIntensity | excluded | scales `_sysPalette.intensity[]` and may `setOnScreen` (LUT upload); no pixel event |
| kernelFindColor | excluded | pure computation — → `matchColor`; no draw effect |
| kernelAnimate | real-gap | palette16.cpp:552 — color-cycling: rotates palette entries on a schedule and `setPalette`s the LUT with **no** `copyRectToScreen`. Same real-gap class as per-tick vary (see below): the cycled intermediate LUT states are not recoverable from L2 pixels (no pixel event fires), and the consumer story is any display-layer enhancer wanting smooth cycling on the RGBA plate rather than a binary re-apply. **Conditions:** (1) not-in-L2 — a cycle step emits no `onShow`; the per-step rotated palette is invisible to a pixel-only consumer; (2) non-Roger consumer story — a TTS/streaming/enhancer overlay reproducing SCI color-cycling (water/fire effects) needs the LUT deltas |
| kernelAnimateSet | real-gap | palette16.cpp:607 — commits the cycled palette via `setOnScreen` (LUT upload, no pixel event); pairs with `kernelAnimate` above under the same `onPaletteChanged` proposal. Content (the committed cycle LUT) not in L2; same enhancer consumer story |
| kernelSave | excluded | serializes `_sysPalette` colors into a hunk for kSave/kRestore (state); no draw effect |
| kernelRestore | excluded | restores `_sysPalette` from a saved hunk (state); no pixel event beyond a later LUT upload |
| kernelAssertPalette | excluded | ensures a resource palette is merged into `_sysPalette` (state); no pixel event |
| kernelSyncScreenPalette | excluded | grabs the backend palette back into `_sysPalette` (state read-back); no draw effect |
| kernelPalVaryInit | excluded | sets up a palVary (origin/target/step/ticks state) + installs the tick timer; the per-tick effect is `palVaryProcess`, not this call |
| kernelPalVaryReverse | excluded | reverses the running palVary direction/target (state); the per-tick effect is `palVaryProcess` |
| kernelPalVaryGetCurrentStep | excluded | pure accessor — returns `_palVaryStep`; no draw effect |
| kernelPalVaryChangeTarget | excluded | swaps the palVary target palette (state); per-tick effect is `palVaryProcess` |
| kernelPalVaryChangeTicks | excluded | changes the palVary tick interval (state); no draw effect |
| kernelPalVaryPause | excluded | pauses/resumes the palVary timer (state); no draw effect |
| kernelPalVaryDeinit | excluded | tears down the running palVary (state + timer removal); no draw effect |
| palVaryUpdate | real-gap | palette16.cpp:841 — the per-tick vary path: on `_palVarySignal`, → `palVaryProcess(sig, true)` which computes the 64-step inbetween palette and `setOnScreen`s it. **THE highest-value underused signal** (CLAUDE.md). Gap-fill **condition (1) — content not in L2:** a vary tick changes only the color LUT; `copySysPaletteToScreen` uploads the palette with **no** `copyRectToScreen`/`bitsShow`, so an L2-pixel-only consumer sees *zero* events across a whole smooth fade — the intermediate `_palVaryStep` state (the 64 inbetween Colors) and even the fact a change occurred are unrecoverable from pixels. The overlay plate being RGBA does not help: nothing re-stamps it on a vary tick, so the live re-apply is a *binary* snap, not the smooth curve. **condition (2) — non-Roger consumer story:** any display-layer enhancer needing smooth fades/cycling (Roger's own smooth-fade want; a streaming overlay mirroring a fade-to-black; a TTS/accessibility layer signalling scene dimming) consumes a proposed `onPaletteChanged(palette, step, total)` L-event. Both conditions hold → **real-gap** |
| palVaryPrepareForTransition | derivable-from-L2 | palette16.cpp:876 — one-shot `palVaryProcess(0, false)` that resolves the palVary state WITHOUT `setOnScreen` (setPalette=false) ahead of a transition; the visible result is finalized by the hooked `doit`/`onTransition` (TR2). No independent pixel or LUT event of its own |
| palVaryProcess | real-gap | palette16.cpp:884 — the shared per-tick worker `palVaryUpdate` calls; computes the inbetween palette and conditionally `setOnScreen`s (LUT only, no pixel event). Same `onPaletteChanged(step,total)` gap as `palVaryUpdate`: the intermediate LUT is not in L2 and the smooth-fade enhancer consumer story holds. (Listed for completeness — `palVaryUpdate` is the public tick entry; both fold into one proposed event) |
| delayForPalVaryWorkaround | excluded | busy-waits up to 4 ticks at kAnimate start so a zero-tick palVary can fire (timing workaround); no draw effect |
| saveLoadWithSerializer | excluded | savegame (de)serialization of palette + palVary state; no draw effect |
| palVarySaveLoadPalette | excluded | (de)serializes one `Palette` struct for the serializer; no draw effect |
| findMacIconBarColor | excluded | pure computation — nearest Mac-CLUT color for the icon bar; no draw effect (Mac-only) |
| colorIsFromMacClut | excluded | pure predicate — is index a non-black Mac-CLUT color; no draw effect (Mac-only) |

### 4.9 GfxCursor (16 public methods)

The Roger cursor is composited into the overlay from `onMouseMoved` (event.cpp
E2) plus the shape/view/hidden notifications at the kSetCursor kernel wrappers
(kgraphics.cpp rows K2-K5) and the `hidesNativeCursor()` veto in `kernelShow`
(cursor.cpp:84, row CU2). The kSetCursor* hooks live at the **callers** in
`kgraphics.cpp`, not in GfxCursor itself — hooked-at-caller is a legitimate
"hooked" citation, noted per row. The position/zone methods drive the OSystem
hardware cursor (`CursorMan` / `gfxDriver()->replaceCursor` / `warpMouse`) and
issue no game-surface draw, so they are excluded (Roger's cursor tracks the
pointer via onMouseMoved, not these).

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| kernelShow | hooked | cursor.cpp:84 `hidesNativeCursor()` veto on `CursorMan.showMouse` (row CU2); also notified at the Sci11 show caller (kgraphics.cpp:163-164, row K4, `onCursorHidden(false)`) |
| kernelHide | hooked | (hooked-at-caller) kgraphics.cpp:152-153 `onCursorHidden(true)` after the Sci11 hide case (row K3); the method itself just `CursorMan.showMouse(false)` |
| isVisible | excluded | pure accessor — returns `_isVisible`; no draw effect |
| kernelSetShape | hooked | (hooked-at-caller) kgraphics.cpp:134-139 `onCursorShape(cursorId)`/`onCursorHidden` after kSetCursorSci0 (row K2); the method also delegates to `kernelShow` (CU2) at cursor.cpp:181 |
| kernelSetView | hooked | (hooked-at-caller) kgraphics.cpp:210-212 `onCursorView(view,loop,cel)` after the kSetCursorSci11 view case (row K5) |
| kernelSetMacCursor | excluded | Mac-platform cursor set (`kernelSetMacCursor` branch in kSetCursorSci11) — no Roger notification is emitted for the Mac path; never reached on a Roger-supported EGA game (Mac-only) |
| setPosition | excluded | warps the OSystem hardware cursor (`gfxDriver()->setMousePos`/`warpMouse`) + touch-input position workarounds; no game-surface draw. Roger's composited cursor follows onMouseMoved (E2), not this |
| getPosition | excluded | pure accessor — reads the backend mouse position (upscale-adjusted); no draw effect |
| refreshPosition | excluded | clips to the move zone + redraws the zoom-zone hardware cursor via `gfxDriver()->replaceCursor` (backend cursor only); no game-surface pixel event |
| kernelResetMoveZone | excluded | clears `_moveZoneActive` (state); no draw effect |
| kernelSetMoveZone | excluded | sets `_moveZone`/`_moveZoneActive` (state); no draw effect |
| kernelSetZoomZone | excluded | configures the LB2/Freddy Pharkas zoom-cursor state (views/multiplier/zone); the actual zoom draw is in `refreshPosition` (backend cursor); no game-surface draw here |
| kernelClearZoomZone | excluded | clears `_zoomZoneActive` + frees the zoom cursor surface (state); no draw effect |
| kernelSetPos | excluded | → `setPosition` (backend warp); no game-surface draw |
| kernelMoveCursor | excluded | refreshes then → `setPosition` (backend warp + move-zone clamp); no game-surface draw |

### 4.10 GfxCoordAdjuster16 (7 public methods)

Class `GfxCoordAdjuster16` in `engines/sci/graphics/coordadjuster.h` — the SCI16
coordinate adjuster. **All-excluded**: every method is pure coordinate math
(port-offset add / rect clip / display-area geometry) that returns or mutates a
Point/Rect — none draws, shows, or reaches the screen. Verified against
`coordadjuster.cpp` (each body is a few add/CLIP/offsetRect lines).

| Method | Classification | Evidence / justification |
|---|---|---|
| ctor / dtor | excluded | construction/teardown; no draw effect |
| kernelGlobalToLocal | excluded | pure computation — subtracts the current port's left/top from x/y (coordadjuster.cpp:41); no draw effect |
| kernelLocalToGlobal | excluded | pure computation — adds the current port's left/top to x/y (coordadjuster.cpp:47); no draw effect |
| onControl | excluded | pure computation — clips a rect to the pic window and offsets it to global (coordadjuster.cpp:53); no draw effect |
| setCursorPos | excluded | pure computation — offsets a Point by the current port origin (coordadjuster.cpp:63); no draw effect (backend warp is GfxCursor's job) |
| moveCursor | excluded | pure computation — offsets + clamps a Point to the pic-window rect (coordadjuster.cpp:68); no draw effect |
| pictureGetDisplayArea | excluded | pure computation — returns the current port's display-area rect (coordadjuster.cpp:76); no draw effect |

## 5. L2 completeness verification

**Claim (spec §3.3 / success-criterion 3):** every pixel that reaches the screen
in **SCI16** paths crosses the L2 `onShow` seam (`GfxPaint16::bitsShow` →
`GfxScreen::copyRectToScreen`), is inside a documented begin/endNativeDraw
**self-draw bracket** (§3 rows), or is a **documented L4 claim** the observer
overrides. Per §3.3 this is *checked by enumeration, not asserted*: every
`copyRectToScreen`/`copyToScreen` caller in the SCI graphics stack is listed and
classified.

This §5 enumeration was run at commit **14509d438c3** and must be re-run if the branch
advances before the reshaping work consumes it (the line/hit counts and classifications
below are pinned to that commit).

**Reproducible enumeration (run at 14509d438c3 on branch jon-update-core-audit):**

```
git grep -n "copyRectToScreen\|copyToScreen\|copyDisplayRectToScreen" -- engines/sci   # 92 hits
git grep -n "bitsShow(" -- engines/sci ':(exclude)engines/sci/roger'                    # 43 hits
```

The `copyDisplayRectToScreen` token appears in no hit (SCI has no such method; it
was carried from the plan's grep for completeness). **92** copy-to-screen hits;
**0** are under `engines/sci/roger/` (footnote below), so the table has **92**
rows — one per hit, none excluded as observer-side.

### 5.1 The two-layer funnel (why most hits are "below L2, not a bypass")

`copyRectToScreen` names **two structurally distinct layers**:

- **`GfxScreen::copyRectToScreen` / `copyToScreen`** — the SCI16 funnel. This is
  the level L2 observes: `GfxPaint16::bitsShow` calls `GfxScreen::copyRectToScreen`
  (paint16.cpp:396, the funnel body — §3.1 P10/P11) and emits `onNativeShowRect`
  right after. Every game-visible SCI16 pixel show flows through `bitsShow` (43
  hits from the grep above; classified per-hit below) → `GfxScreen::copyRectToScreen`.

  **`bitsShow(` hit classification (43 hits):**

  **(a) Non-executing — definition, declaration, and comments (5 hits):**

  | File:line | Kind |
  |---|---|
  | paint16.cpp:194 | comment: "This version of drawCel is not supposed to call bitsShow()!" |
  | paint16.cpp:199 | comment: same |
  | paint16.cpp:369 | **definition** — `void GfxPaint16::bitsShow(const Common::Rect &rect, uint32 rogerOwner)` |
  | paint16.cpp:819 | comment in KQ6 hires-portrait workaround block |
  | paint16.h:63 | **declaration** — `void bitsShow(const Common::Rect &r, uint32 rogerOwner = 0)` |

  **(b) True call sites (38 hits) — verdict: funnel; each reaches `GfxScreen::copyRectToScreen` via `bitsShow`'s body (paint16.cpp:396):**

  | File | Lines | Notes |
  |---|---|---|
  | animate.cpp | 533, 542, 598, 605 | cast draw / lsRect / workerRect shows |
  | controls16.cpp | 161, 170, 315, 338, 392, 412, 437, 489, 498, 534, 554, 563, 575 | text-edit caret, control redraws |
  | menu.cpp | 596, 602, 666, 702, 809, 867, 964, 965, 1100, 1144, 1168, 1175, 1194 | menu bar, menu items |
  | paint16.cpp | 182, 186, 574, 750, 761 | drawCelAndShow inner calls, bitsGetView, kGraphUpdateBox |
  | ports.cpp | 551, 583 | window open / bitsRestore show |
  | text16.cpp | 565 | text Box show |

  5 + 38 = 43. All 38 call sites feed `bitsShow`'s body; the hook at P11 fires on every one.
- **`GfxDriver::copyRectToScreen`** (the `drivers/*` implementations + the
  `g_system->copyRectToScreen` inside them) — the **backend transport beneath**
  `GfxScreen`. `GfxScreen::displayRect` (screen.cpp:219) → `_gfxDrv->copyRectToScreen`
  → `g_system->copyRectToScreen`. This is how the funnel reaches the OSystem
  surface; it is *downstream* of L2, never a path that bypasses it. Classifying a
  driver-layer call "uncovered" would be a category error — no SCI16 pixel reaches
  the driver except through `GfxScreen`, which the funnel already covers.

So the verdicts below use these values (the four brief categories, plus two
sub-labels that are honestly *covered*, not gaps — each justified inline):

- **funnel** — the `GfxScreen` funnel itself (defs, decls, the `bitsShow` body) or
  the driver/backend transport beneath it (reached only through `GfxScreen`).
- **bracketed** — inside a begin/endNativeDraw self-draw bracket a §3 row documents.
- **claimed (L4)** — a native path the observer overrides via a documented L4 claim
  (transitions early-return, §3.6 TR2 → `claimTransition`). The claim's early-return
  prevents the ANIMATED transition paths (rows 63–89) from running; rows 60–62
  (`setNewScreen`, transitions.cpp:204/306–313) DO run under the claim but write only the
  native surface beneath the opaque overlay, with the observer notified via
  `claimTransition`/`onTransition` — so no unobserved pixel escapes. Covered, not a gap.
- **SCI32-only** — excluded; compiled only under `ifdef ENABLE_SCI32` (module.mk:143).
- **debug/console** — excluded developer instrumentation (SCI debugger console,
  `#ifdef DEBUG_*` / `#if 0` visualizers, `kDebugLevelAvoidPath` channel); not part
  of the observed game render loop, the same way SCI32 is out of scope. Gating cited
  per row.
- **UNCOVERED** — a *game-reachable* SCI16 screen write outside funnel, bracket, and
  claim. Any such hit is a real finding (recorded in §9). Result: **one** — the Mac
  icon bar (Mac-platform-only SCI16 UI; see §5.3 and §9).

### 5.2 Classification table (92 rows)

| # | Caller (file:line) | Path | Verdict |
|---|---|---|---|
| 1 | console.cpp:2050 copyToScreen | `cmdDrawPic` debugger command | debug/console |
| 2 | console.cpp:2079 copyRectToScreen | `cmdDrawCel` debugger command | debug/console |
| 3 | console.cpp:2390 copyRectToScreen | `cmdPaintSetSize`/paint-rect debugger command | debug/console |
| 4 | console.cpp:2394 copyRectToScreen | same debugger command, restore path | debug/console |
| 5 | kpathing.cpp:1605 copyToScreen | AvoidPath input viz, gated `isDebugChannelEnabled(kDebugLevelAvoidPath)` | debug/console |
| 6 | kpathing.cpp:1861 copyToScreen | AvoidPath intersections viz, same debug channel | debug/console |
| 7 | kpathing.cpp:2409 copyToScreen | `draw_line`-based path viz, `#ifdef DEBUG_MERGEPOLY` region | debug/console |
| 8 | kpathing.cpp:2491 copyToScreen | merge-poly viz, `#ifdef DEBUG_MERGEPOLY` | debug/console |
| 9 | animate.cpp:549 `// _screen->copyToScreen();` | commented-out debug line — not a call site | debug/console (comment) |
| 10 | cursor32.cpp:135 `g_system->copyRectToScreen` | GfxCursor32 (SCI32 cursor) | SCI32-only |
| 11 | drivers/cga.cpp:34 decl | `SCI0_CGADriver::copyRectToScreen` override decl (driver layer) | funnel (backend decl) |
| 12 | drivers/cga.cpp:146 def | CGA driver `copyRectToScreen` def (below GfxScreen) | funnel (backend) |
| 13 | drivers/cga.cpp:164 `g_system->copyRectToScreen` | CGA driver → OSystem transport | funnel (backend) |
| 14 | drivers/cgabw.cpp:34 decl | CGA-BW driver override decl | funnel (backend decl) |
| 15 | drivers/cgabw.cpp:67 def | CGA-BW driver def | funnel (backend) |
| 16 | drivers/cgabw.cpp:89 `g_system->copyRectToScreen` | CGA-BW → OSystem transport | funnel (backend) |
| 17 | drivers/default.cpp:183 copyRectToScreen | GfxDefaultDriver internal full-bitmap re-push | funnel (backend) |
| 18 | drivers/default.cpp:191 def | GfxDefaultDriver `copyRectToScreen` def | funnel (backend) |
| 19 | drivers/default.cpp:205 `g_system->copyRectToScreen` | default driver → OSystem transport | funnel (backend) |
| 20 | drivers/ega.cpp:138 copyRectToScreen | SCI1_EGADriver internal full re-push | funnel (backend) |
| 21 | drivers/ega.cpp:141 def | SCI1_EGADriver `copyRectToScreen` def | funnel (backend) |
| 22 | drivers/ega.cpp:156 `g_system->copyRectToScreen` | EGA driver → OSystem transport | funnel (backend) |
| 23 | drivers/gfxdriver.h:50 pure-virtual decl | `GfxDriver::copyRectToScreen` interface (driver layer) | funnel (backend decl) |
| 24 | drivers/gfxdriver_intern.h:37 decl | GfxDefaultDriver override decl | funnel (backend decl) |
| 25 | drivers/gfxdriver_intern.h:96 decl | UpscaledGfxDriver override decl | funnel (backend decl) |
| 26 | drivers/gfxdriver_intern.h:132 decl | further driver override decl | funnel (backend decl) |
| 27 | drivers/hercules.cpp:34 decl | Hercules driver override decl | funnel (backend decl) |
| 28 | drivers/hercules.cpp:70 def | Hercules driver `copyRectToScreen` def | funnel (backend) |
| 29 | drivers/hercules.cpp:97 `g_system->copyRectToScreen` | Hercules → OSystem transport | funnel (backend) |
| 30 | drivers/pc98_8col_sci1.cpp:36 decl | PC98 8-color driver override decl | funnel (backend decl) |
| 31 | drivers/pc98_8col_sci1.cpp:164 def | PC98 8-color driver def | funnel (backend) |
| 32 | drivers/upscaled.cpp:91 def | UpscaledGfxDriver `copyRectToScreen` def | funnel (backend) |
| 33 | drivers/upscaled.cpp:168 `g_system->copyRectToScreen` | upscaled driver → OSystem transport | funnel (backend) |
| 34 | drivers/win256col.cpp:34 decl | Windows-256 driver override decl | funnel (backend decl) |
| 35 | drivers/win256col.cpp:156 def | Windows-256 driver def | funnel (backend) |
| 36 | drivers/win256col.cpp:161 `UpscaledGfxDriver::copyRectToScreen` | delegates to base driver | funnel (backend) |
| 37 | frameout.cpp:654 `g_system->copyRectToScreen` | GfxFrameout (SCI32 render loop) | SCI32-only |
| 38 | frameout.cpp:1105 comment | comment mentioning `OSystem::copyRectToScreen` — not a call | SCI32-only (comment) |
| 39 | frameout.cpp:1117 `g_system->copyRectToScreen` | GfxFrameout show-rect | SCI32-only |
| 40 | frameout.cpp:1121 `g_system->copyRectToScreen` | GfxFrameout show-rect (partial) | SCI32-only |
| 41 | maciconbar.cpp:203 `gfxDriver()->copyRectToScreen` | Mac icon bar, upscaled draw — direct to driver, no bitsShow | **UNCOVERED** (Mac-only; §9) |
| 42 | maciconbar.cpp:209 `gfxDriver()->copyRectToScreen` | Mac icon bar, disabled-icon draw — direct to driver | **UNCOVERED** (Mac-only; §9) |
| 43 | maciconbar.cpp:211 `gfxDriver()->copyRectToScreen` | Mac icon bar, enabled-icon draw — direct to driver | **UNCOVERED** (Mac-only; §9) |
| 44 | paint16.cpp:396 `_screen->copyRectToScreen(workerRect)` | **the funnel body**: `bitsShow` → `GfxScreen::copyRectToScreen`, then `onNativeShowRect` (§3.1 P10/P11) | **funnel** |
| 45 | picture.cpp:450 copyToScreen | `#ifdef DEBUG_PICTURE_DRAW` op-trace | debug/console |
| 46 | picture.cpp:727 copyToScreen | `_EGAdrawingVisualize` debug-visualize flag | debug/console |
| 47 | picture.cpp:878 copyToScreen | `#if 0` floodfill debug | debug/console |
| 48 | screen.cpp:219 `_gfxDrv->copyRectToScreen` | `GfxScreen::displayRect` → driver (funnel→backend seam) | funnel |
| 49 | screen.cpp:233 copyToScreen | `clearForRestoreGame` full re-push (funnel self-call) | funnel |
| 50 | screen.cpp:236 def | `GfxScreen::copyToScreen` def (funnel) | funnel |
| 51 | screen.cpp:242 `_gfxDrv->copyRectToScreen` | `copyToScreen` → driver transport | funnel |
| 52 | screen.cpp:249 def | `GfxScreen::copyRectToScreen(rect)` def (funnel, bitsShow target) | funnel |
| 53 | screen.cpp:275 `_gfxDrv->copyRectToScreen` | `copyHiResRectToScreen` → driver (hires upscaled-mode transport) | funnel |
| 54 | screen.cpp:279 def | `GfxScreen::copyRectToScreen(rect,x,y)` overload def (funnel) | funnel |
| 55 | screen.cpp:792 copyToScreen | `debugShowMap` (console `debug_showmap` command) | debug/console |
| 56 | screen.cpp:927 `_gfxDrv->copyRectToScreen` | `bakCopyRectToScreen` → driver; sole caller is transitions scroll (claimed) | funnel |
| 57 | screen.h:85 decl | `GfxScreen::copyToScreen` decl | funnel (decl) |
| 58 | screen.h:87 decl | `GfxScreen::copyRectToScreen(rect)` decl | funnel (decl) |
| 59 | screen.h:89 decl | `GfxScreen::copyRectToScreen(rect,x,y)` decl | funnel (decl) |
| 60 | transitions.cpp:306 `_screen->copyRectToScreen(_picRect)` | `setNewScreen`; reached only via `doit()` (claimed early-return, §3.6 TR2) | claimed (L4) |
| 61 | transitions.cpp:311 def | `GfxTransitions::copyRectToScreen` helper def | claimed (L4) |
| 62 | transitions.cpp:313 `_screen->copyRectToScreen(rect)` | transitions helper body → funnel | claimed (L4) |
| 63 | transitions.cpp:383 copyRectToScreen | `pixelation()` | claimed (L4) |
| 64 | transitions.cpp:409 copyRectToScreen | `blocks()` | claimed (L4) |
| 65 | transitions.cpp:431 copyRectToScreen | `straight()` from-right | claimed (L4) |
| 66 | transitions.cpp:446 copyRectToScreen | `straight()` from-left | claimed (L4) |
| 67 | transitions.cpp:461 copyRectToScreen | `straight()` from-bottom | claimed (L4) |
| 68 | transitions.cpp:474 copyRectToScreen | `straight()` from-top | claimed (L4) |
| 69 | transitions.cpp:522 `_screen->copyRectToScreen(...)` | `scroll()` | claimed (L4) |
| 70 | transitions.cpp:540 `_screen->copyRectToScreen(...)` | `scroll()` | claimed (L4) |
| 71 | transitions.cpp:559 `_screen->copyRectToScreen(...)` | `scroll()` | claimed (L4) |
| 72 | transitions.cpp:575 `_screen->copyRectToScreen(...)` | `scroll()` final | claimed (L4) |
| 73 | transitions.cpp:588 `_screen->copyRectToScreen(newScreenRect)` | `scrollCopyOldToScreen`/scroll finalize | claimed (L4) |
| 74 | transitions.cpp:603 copyRectToScreen | `verticalRollFromCenter()` left | claimed (L4) |
| 75 | transitions.cpp:604 copyRectToScreen | `verticalRollFromCenter()` right | claimed (L4) |
| 76 | transitions.cpp:620 copyRectToScreen | `verticalRollToCenter()` left | claimed (L4) |
| 77 | transitions.cpp:621 copyRectToScreen | `verticalRollToCenter()` right | claimed (L4) |
| 78 | transitions.cpp:641 copyRectToScreen | `horizontalRollFromCenter()` upper | claimed (L4) |
| 79 | transitions.cpp:642 copyRectToScreen | `horizontalRollFromCenter()` lower | claimed (L4) |
| 80 | transitions.cpp:658 copyRectToScreen | `horizontalRollToCenter()` upper | claimed (L4) |
| 81 | transitions.cpp:659 copyRectToScreen | `horizontalRollToCenter()` lower | claimed (L4) |
| 82 | transitions.cpp:690 copyRectToScreen | `diagonalRollFromCenter()` upper | claimed (L4) |
| 83 | transitions.cpp:691 copyRectToScreen | `diagonalRollFromCenter()` lower | claimed (L4) |
| 84 | transitions.cpp:692 copyRectToScreen | `diagonalRollFromCenter()` left | claimed (L4) |
| 85 | transitions.cpp:693 copyRectToScreen | `diagonalRollFromCenter()` right | claimed (L4) |
| 86 | transitions.cpp:711 copyRectToScreen | `diagonalRollToCenter()` upper | claimed (L4) |
| 87 | transitions.cpp:712 copyRectToScreen | `diagonalRollToCenter()` lower | claimed (L4) |
| 88 | transitions.cpp:713 copyRectToScreen | `diagonalRollToCenter()` left | claimed (L4) |
| 89 | transitions.cpp:714 copyRectToScreen | `diagonalRollToCenter()` right | claimed (L4) |
| 90 | transitions.h:76 decl | `GfxTransitions::copyRectToScreen` helper decl | claimed (L4) |
| 91 | video32.cpp:269 `g_system->copyRectToScreen` | SCI32 video playback (Robot/VMD) | SCI32-only |
| 92 | video32.cpp:1269 `g_system->copyRectToScreen` | SCI32 video playback | SCI32-only |

### 5.3 Result

**Result: 92 callers — 89 covered rows + 3 uncovered rows (one finding). Verdict tally: funnel 38, claimed-L4 31, SCI32-only 7, debug/console 13, UNCOVERED 3.**

- **funnel: 38** (rows 11–36, 44, 48–54, 56–59 — the `GfxScreen` funnel + the
  driver/backend transport beneath it; the funnel body at row 44 emits `onNativeShowRect`).
- **claimed (L4): 31** (rows 60–90 — all reached only through `GfxTransitions::doit`,
  which early-returns to `onTransition`/`claimTransition` while the overlay is visible;
  §3.6 TR2).
- **SCI32-only: 7** (rows 10, 37–40, 91–92 — `ifdef ENABLE_SCI32`, module.mk:143; row 38
  is a comment).
- **debug/console: 13** (rows 1–8 + row 9 commented-out line, rows 45–47, 55 — SCI
  debugger console, `DEBUG_*`/`#if 0` visualizers, `kDebugLevelAvoidPath` channel).
  Developer instrumentation, out of the observed render loop (excluded on the same
  footing as SCI32).
- **UNCOVERED: 3** (rows 41–43 — the single Mac-icon-bar draw path; three call sites,
  **one finding**, so "89 covered + 3 uncovered = the one Mac-icon-bar gap").

**The one uncovered path — the Mac icon bar** (`GfxMacIconBar::drawImage`,
maciconbar.cpp:203/209/211). It writes directly to `_screen->gfxDriver()->copyRectToScreen`,
bypassing `GfxPaint16::bitsShow` and any self-draw bracket, and it is **SCI16**
(compiled unconditionally; module.mk:54), not SCI32. It is gated on
`hasMacIconBar()` — the Macintosh-only persistent icon strip (Mac SCI game versions).
It is therefore a real screen write the literal L2-completeness claim ("every pixel
in SCI16 paths crosses onShow or a bracket") does **not** account for. In practice it
is out of Roger's shipping scope (Roger targets SCI0/SCI1 **EGA DOS** games — SQ3,
QFG1 EGA — which have no Mac icon bar; the overlay is never active for a Mac target),
so it is not a live bug, but the *stated* claim is over-broad. Recorded as a §9
finding: the completeness claim must be **scoped to non-Mac SCI16** (or the Mac icon
bar must emit an L2 `onShow`, a one-line hook at `drawImage`) before the design can
assert it unqualified. *(Resolved 2026-07-10: the spec now carries the non-Mac
scoping — §9 resolution 3.)*

### 5.4 Footnote — roger/ exclusions

Both greps were re-run against `engines/sci/roger/`:
`git grep -n "copyRectToScreen\|copyToScreen\|copyDisplayRectToScreen" -- engines/sci/roger`
and the `bitsShow(` grep with `':(exclude)engines/sci/roger'` inverted — **both return
zero hits**. The Roger overlay never calls `copyRectToScreen`/`copyToScreen` (it
presents through the OSystem *overlay* via `g_system->copyRectToOverlay`, a different
surface) and never calls `bitsShow` (it *observes* it). So there are **no
observer-side hits to exclude** from the 92-row enumeration; every row is SCI-engine
code, and the table row count equals the grep hit count exactly.

## 6. Consolidation table and line budget

**Provider surface:** `engines/sci/roger/roger_art_provider.h` declares **49 virtuals**
(measured 2026-07-10: `(Select-String -Pattern '^\s*virtual ').Count` = 49, matching the
plan's expected count; the virtual destructor is one of the 49). Every one of them
appears in exactly one consolidation row below (spec success-criterion 5); the compact
mapping is the coverage comment at the end of this section.

**Baseline note:** the plan's literal "825" (measured 2026-07-09) is superseded by the
2026-07-10 re-measure at 14509d438c3: **830 lines / 18 files / 83 hunks** (§2). The
budget target is therefore ≤ 830; the 1000 hard cap is unchanged.

**Measurement rule.** The projection measures the reshaped **neutral-seam** diff outside
`engines/sci/roger/` — what the observer-based fork (and any upstream slice) carries in
SCI engine code. Per the section-3 dispositions: `→ event` rows become mechanical
null-guarded observer calls; `delete` rows vanish; `standalone-PR` rows leave via §8
(merged upstream, they are no longer fork diff); `observer-side` marshalling moves
behind the new header's helpers or into `roger/` (uncounted); `fork-only` rows (bucket
D) are carried as downstream-only patches **outside** the seam measurement, the same way
§2's baseline scope already excludes `build_*.ps1`/`CLAUDE.md`/`docs/` (spec §2). The
new observer header is counted because it lives outside `roger/` (spec §2). Row R20
makes the fork-only carve-out explicit so the arithmetic stays transparent.

**Estimation method** (stated per the plan, applied to every row): Δ = (current lines at
the affected sites, apportioned from the §2 per-file measurements across the §3 rows) −
(reshaped lines: one mechanical observer call per site + §4.3 helper reuse) −
(war-story comment lines relocated into this audit / the observer implementation).
Per-row apportionment within a file is estimated; the per-file sums are measured (§2),
and the reconciliation comment below ties every row's "current" lines back to the 830
total exactly. Every Δ is rounded to the nearest 5 and tagged high/med/low confidence —
no invented precision.

| Row | Old (virtuals + hook sites) | New (spec §4.2 event, layer) | §3 rows covered | Sites touched | Cur → reshaped | Est. Δ | Conf. | Risk notes |
|---|---|---|---|---|---|---|---|---|
| R1 | ten `#include "sci/roger/roger_art_provider.h"` sites | one neutral engine-owned `sci_gfx_observer.h` include per file | P1, K1, CU1, A2(inc), T1, TR1, C1, M4, PO1, E1 | 10 files | 10 → 10 | 0 | high | SCI code never names a `roger/` path again (Stage 3 rule); 1:1 swap |
| R2 | `prefetch`, `hasBackground`, `pushHiresBackground`, `pushHiresBackgroundAddTo`, `onNativePicture`; `loadBuffers` = explicit deletion (documented obsolete no-op) | `onPicture(picId, addToFlag)` + `onPictureAbsent()` (L3) | P2, P4 | paint16.cpp ×2 | 34 → ~10 | −25 | med | replace/absent/prefetch decisions move observer-side (the observer knows its own art inventory); the pre-hook gate disappears — SCI emits unconditionally after the native render; the addTo trap (CLAUDE.md) is observer logic, not seam logic |
| R3 | `onInitCel` (3 sites), `onAddToPicCel` (2 sites), `onDrawCel`, `uiPushIcon`, + `rogerOwnerToken` helper | `onCel(rect, view, loop, cel, priority, owner, source)`, source ∈ {animate, addToPic, initBake, standalone, icon} (L3) | P6, P15, A2(helper), A3, A4, A5, A10, A11, C9 | paint16 ×2, animate ×5 (+helper), controls16 ×1 | 54 → ~24 | −30 | med | owner-token promotion rule is load-bearing (CLAUDE.md `_picNotValid` trap) — `owner` must stay on the event; token construction → §4.3 helper |
| R4 | `onNativeText`, `uiPushText`, `uiPushStatus` (+ kDisplay bg-fill push) | `onText(rect, text, font, pen, back, align, metrics, token, source)`, source ∈ {textBox, control, status, listRow, fill} (L3) | T3, T4, C2, C6, M13, P19 | text16, controls16 ×2, menu ×1, paint16 ×1 | 92 → ~37 | −55 | med | per-line rects are the DRAWN extent (text16 invariant) — the placement math stays at the Box site. **P19 decision (spec §6 asked the audit):** the `onFill` candidate is REJECTED on line-budget grounds; the kDisplay background fill folds into `onText` as `source=fill` (retires the empty-string overload smell, no new virtual). menuBar/menuRow sources are emitted from menu.cpp and budgeted in R5; windowTitle folds into `onWindowOpen` (R6, per spec §4.2) |
| R5 | menu.h `RogerMenuRow` struct + 4 state members + 3 `rogerPush*` decls; menu.cpp collection code, `rogerTitleIsText`, `rogerPushBarOverlay`/`rogerPushMenuOverlay`/`rogerClearMenuOverlay` bodies, M12 comment | draw-time events: `onText(menuBar)`/`onText(menuRow)` inside `beginBatch`/`endBatch`, `onWindowOpen/Close(token=dropdown)`, `onMenuHighlight(itemId)` — the **observer** keeps all row state (menu state exile, kills bucket S) | M1, M2, M3, M5, M6, M7, M8, M9, M10, M11, M12 | menu.{cpp,h} | 154 → ~30 | −125 | med | grounded in §2's measured 163 menu lines: 163 − 1 (include, R1) − 8 (M13, R4) = 154 current; reshaped ≈ 30 mechanical emits (drawBar ~8, kernelSelect ~2, drawMenu ~12, invertMenuSelection ~4, + guards) → −125, LARGER than spec §4.2's −100 guess. Risks: invalidation verified only by interactive soak, not the gate; the bar/banner token mutual-exclusion (`0x10000000`) must survive in the §4.3 token scheme |
| R6 | `uiPushWindow`, `uiClearToken`, `uiClearAll` | `onWindowOpen(rect, style, colors, title, token)` / `onWindowClose(token)` (L3) | PO2, PO3 | ports.cpp ×2 | 53 → ~23 | −30 | med | titlebar text folds into the open payload; the two removeWindow clears become ONE `onWindowClose`; `uiClearAll` has NO out-of-roger caller (verified by grep) → deleted from the seam (observer-internal). PO3's no-save-under **reveal plant is a retained duty-3 exception** — survives as `onRestore(0, rect)` and must never be "simplified" away; menu's three `uiClearToken` calls go observer-side with R5 |
| R7 | `uiPushButton`, `uiPushTextEdit` | `onControl(kind ∈ {button, textEdit}, ...)` (L3) | C4, C5, C8 | controls16 ×3 | 32 → ~17 | −15 | high | C4/C8 same-token replace-in-place discipline moves observer-side (journal `opSupersedes`); offsetRect/StringWidth/token boilerplate → §4.3 helpers |
| R8 | `uiPushFrameBox` | `onFrameBox(rect, pen)` (L3) | P16, C7 | paint16, controls16 | 26 → ~11 | −15 | med | C7's change-gating (present-storm guard) moves observer-side; duty-3 exception #2 (frame-box vacated mark) is observer behavior, unaffected at the seam |
| R9 | `onNativeShowRect` + the `bitsShow(rect, rogerOwner)` signature change | `onShow(rect, owner)` (L2); the `rogerOwner` param is dropped from the public SCI signature, owner derived observer-side | P10, P11 | paint16.{cpp,h} | 19 → ~9 | −10 | med | risk: PO2's tokened show (drawWindow runs under `_wmgrPort`, so bitsShow cannot self-derive the owner) must stay attributable — event ordering (`onWindowOpen` precedes the show) is the replacement mechanism; verify at implementation time |
| R10 | `onNativeSaveRect`, `onNativeRestoreRect`, `onNativeFreeSave`, `onNativeEraseRect` | `onSave(token, rect)` / `onRestore(token, rect)` / `onFree(token)` / `onErase(rect)` (L2) | P12, P13, P14, P18 | paint16 ×4 | 24 → ~14 | −10 | high | already the consolidated journal path — near-verbatim renames; token = save-handle identity (§4.3) |
| R11 | `beginNativeDraw` / `endNativeDraw` | `beginSelfDraw()` / `endSelfDraw()` (L2) | P3, P5, P7, P8, P9, P20, A6, A7, A8, A9(bracket) | paint16 ×6, animate ×4 | 63 → ~33 | −30 | med | brackets stay mechanical; the shrink is war-story comment relocation (esp. P20's kDisplay-flush rationale) — the bracket PLACEMENTS are load-bearing and must not move |
| R12 | `renderFromAnimateList`, `snapshotNativeBaseline` | `onAnimateFrame(list)` + `onFrameEnd()` (L1) | A9(render), A13 | animate ×2 | 14 → ~9 | −5 | high | snapshot is SBS-panel-only since the diff-backstop removal — `onFrameEnd` is the honest generalized name |
| R13 | `beginUiBatch` / `endUiBatch` | `beginBatch()` / `endBatch()` (L1), generalized to any frozen-loop re-push | (sites live inside the exiled menu bodies — lines budgeted in R5) | menu.cpp (reshaped drawBar/drawMenu) | 0 → 0 | 0 | high | the present-storm guard (menu mouse-crawl fix) must survive the exile — the reshaped drawBar/drawMenu emits stay bracketed |
| R14 | `onTransition`, `isOverlayVisible` (gated TR2; also gated K6/R15) | `claimTransition(type, rect, blackoutType) -> bool` (L4) | TR2 | transitions.cpp | 26 → ~11 | −15 | med | claim-false → native runs; the instant-finalize block stays inside the claim-true branch (double-blocking dead-time contract documented at the claim); `isOverlayVisible` disappears from the seam — the observer returns false from claims while its overlay is hidden |
| R15 | `onShake` | `claimShake(count, directions) -> bool` (L4) | K6 | kgraphics.cpp | 7 → ~4 | −5 | high | same claim contract; the inline `isOverlayVisible()` gate becomes the claim return |
| R16 | `onCursorShape`, `onCursorHidden`, `onCursorView`, `hidesNativeCursor` | `claimCursor() -> bool` + `onCursorShape/View/Hidden` notifications (L4) | K2, K3, K4, K5, CU2 | kgraphics ×4, cursor.cpp | 15 → ~10 | −5 | high | `hidesNativeCursor` ≡ `claimCursor` (the plan's mapping for 135ed9438a3 — no vocabulary extension); CU2's kernelShow veto is the claim's enforcement point; the edge-leak rationale relocates to the claim doc |
| R17 | (no virtual today — inline `enabled`-gated branch) | `wantsUnclampedTextEdit() -> bool` (L4) | C3 | controls16 | 9 → ~4 | −5 | med | documents exactly what native behavior is skipped (pixel-width keystroke cap) and what the observer guarantees in exchange (`maxChars` still bounds the buffer) |
| R18 | `onMouseMoved` | `onMouseMoved()` — survives as an L1 notification (spec §4.4: the generic composited-cursor consumer story holds) | E2 | event.cpp | 14 → ~9 | −5 | med | the `sawMouseMove` tracking through the skip loop stays (detection logic, not marshalling) |
| R19 | `diagEnabled`, `cycleLogEnabled` + the ROGER-DIAG/ROGER-CYCLE instrumentation | deleted from the seam (diag/telemetry stay `roger/`-internal facilities) | P17, K7, A1, A12, A14 (diag lines inside P2/P5/P15/PO3 are netted in their host rows) | paint16, kgraphics, animate ×3 | 24 → 0 | −25 | high | deleting A14 also retires its **upstream-forbidden** non-const function-static `s_prevCycleT0` |
| R20 | `precacheAll`, `precacheOnePic`, `precacheOneView`, `toggleOverlay`, `toggleDebugLog`, `toggleTunePanel`, `tunePanelMouse`, `remapComparisonMouse` + hotkeys / env gates / launcher block | none — fork-only carve-out: carried as downstream-only patches outside the neutral seam (measurement rule above) | E3, E4, S1, S4(dev blocks), S5 | event.cpp, sci.cpp | 84 → 0 (in-seam) | −85 | med | the fork still carries ~84 lines downstream; they leave the seam measurement the way Stage 3 already excludes dev tooling. Regardless of measurement: S1's `FORBIDDEN_SYMBOL_EXCEPTION_getenv` must be replaced (ConfMan/CLI) before ANY upstream slice |
| R21 | provider new/delete + concrete includes in sci.cpp; module.mk object list; test/module.mk block | `setArtProvider()` registration (~6 lines); `roger/*.o` exiled to a `roger/`-owned module.mk (uncounted); tests relink against a roger static lib | S2, S4(instantiation), W1, W3 | sci.cpp, module.mk, test/module.mk | 51 → ~15 | −35 | low | Stage 3 registration mechanism not yet designed — the estimate assumes the object list moves under `roger/`; if the build cannot include a nested module.mk, this Δ shrinks toward 0 (see robustness bound below) |
| R22 | T2 `textHeight = 0` init; S3 caption fix; F1/F2 scifont un-gating; W2 EventRecorder decl fix | standalone upstream PRs (§8) — once merged upstream they leave the fork diff entirely | T2, S3, F1, F2, W2 | text16, sci.cpp, scifont.{cpp,h}, gui/EventRecorder.h | 25 → 0 | −25 | high | cheap goodwill before the observer pitch (spec §8); the Δ realizes only when the PRs are merged |
| R23 | **NEW: observer header outside roger/** (the `~RogerArtProvider` virtual dtor maps here — the new class owns its own) | `sci_gfx_observer.h`: `SciGfxObserver` (≈30 virtuals incl. dtor: 6 L1, 7 L2, 9 L3, 7 L4, `onMouseMoved`) + §4.3 token-scheme enum & constructors + offsetRect/StringWidth/token marshalling helpers | — | 1 new file | 0 → 200 | +200 | med | spec §2's +200 sanity-checked against the event count this table actually needs: ~30 declarations × ~4 lines (decl + doc comment) ≈ 130, token scheme ~30, source/kind enums ~15, GPL header + boilerplate ~25 ≈ 200 — plausible as budgeted, no adjustment needed |
| **Total** | | | | | **830 → 475** | **ΣΔ = −355** | | **projected = 830 − 355 = 475** |

**Arithmetic** (per-row Δs, summed by hand): negatives 25+30+55+125+30+15+15+10+10+30+5
+15+5+5+5+5+25+85+35+25 = 555; positives +200; ΣΔ = 200 − 555 = **−355**; projected =
830 − 355 = **475** (consolidation only; the kept §7 gap adds +10 → **485** final). The
total row derives from the rounded per-row Δs (the authoritative sum); the "Cur →
reshaped" column's own reshaped estimates sum to ≈480, the ~5-line rounding slack — the
830 − ΣΔ = 475 figure is the one to quote.

**Budget verdict: 485 (475 consolidation + 10 kept palette gap, §7) ≤ 830 target (cap 1000) — PASS.**

No shrink list is required (the gate passes with a 345-line margin), and the 1000 hard
cap is nowhere near threatened — no §9 cap contradiction. **Robustness bound:** flipping
every contested estimate to its conservative value at once — menu exile only −100
(spec's own guess), wiring Δ 0 (nested module.mk impossible), the fork-only carve-out
disallowed and counted back in (+85), text and bracket rows −15 shallower each — lands
at 475 + 25 + 35 + 85 + 30 = **650 consolidation-only / 660 including the kept +10 gap —
still PASS vs 830**. The projection does land well below
spec §2's "~780 ± 100" — logged as a §9 finding for Task 10 (the spec's projection
under-counted the D/W/G departures its own dispositions imply). *(Resolved
2026-07-10: spec §2's projection updated to the audited numbers — §9 resolution 2.)*

<!-- per-file "current" reconciliation (apportioned within measured §2 per-file totals; sums EXACTLY 830):
  kgraphics 26 = R1 1 + R15 7 (K6) + R16 13 (K2-K5) + R19 5 (K7)
  event 52 = R1 1 + R18 14 (E2) + R20 37 (E3 4 + E4 33)
  animate 74 = R1 1 + R3 27 (A2h 6, A3-A5 15, A10/A11 6) + R11 16 (A6-A9 brackets) + R12 14 (A9-render 4 + A13 10) + R19 16 (A1 1 + A12 3 + A14 12)
  controls16 94 = R1 1 + R3 8 (C9) + R4 29 (C2 20 + C6 9) + R7 32 (C4 14 + C5 9 + C8 9) + R8 15 (C7) + R17 9 (C3)
  cursor 3 = R1 1 + R16 2 (CU2)
  menu 163 = R1 1 + R4 8 (M13) + R5 154
  paint16 171 = R1 1 + R2 34 (P2 18 + P4 16) + R3 19 (P6 6 + P15 13) + R4 13 (P19) + R8 11 (P16) + R9 19 (P10 5 + P11 14) + R10 24 (P12 6 + P13 8 + P14 5 + P18 5) + R11 47 (P3 4 + P5 7 + P7 2 + P8 4 + P9 2 + P20 28) + R19 3 (P17)
  ports 54 = R1 1 + R6 53 (PO2 28 + PO3 25)
  scifont 7 = R22 7 (F1+F2)
  text16 44 = R1 1 + R4 42 (T3 1 + T4 41) + R22 1 (T2)
  transitions 27 = R1 1 + R14 26 (TR2)
  module.mk 36 = R21 36 (W1)
  sci.cpp 67 = R20 47 (S1 6 + S4dev 20 + S5 21) + R21 9 (S2 7 + S4inst 2) + R22 11 (S3)
  EventRecorder.h 6 = R22 6 (W2)
  test/module.mk 6 = R21 6 (W3)
  Row totals: R1 10, R2 34, R3 54, R4 92, R5 154, R6 53, R7 32, R8 26, R9 19, R10 24,
  R11 63, R12 14, R13 0, R14 26, R15 7, R16 15, R17 9, R18 14, R19 24, R20 84, R21 51, R22 25.
  Sum = 10+34+54+92+154+53+32+26+19+24+63+14+0+26+7+15+9+14+24+84+51+25 = 830. -->

<!-- virtual coverage: 49/49 —
  ~RogerArtProvider→R23; prefetch→R2; precacheAll→R20; precacheOnePic→R20; precacheOneView→R20;
  hasBackground→R2; loadBuffers→R2(explicit deletion); pushHiresBackground→R2; pushHiresBackgroundAddTo→R2;
  renderFromAnimateList→R12; onNativePicture→R2; onMouseMoved→R18; diagEnabled→R19; cycleLogEnabled→R19;
  onDrawCel→R3; onInitCel→R3; onAddToPicCel→R3; beginNativeDraw→R11; endNativeDraw→R11;
  onNativeShowRect→R9; onNativeText→R4; onNativeEraseRect→R10; onNativeSaveRect→R10; onNativeFreeSave→R10;
  onNativeRestoreRect→R10; snapshotNativeBaseline→R12; onTransition→R14; onShake→R15;
  onCursorShape→R16; onCursorHidden→R16; onCursorView→R16; hidesNativeCursor→R16;
  uiPushWindow→R6; uiPushText→R4; uiPushButton→R7; uiPushTextEdit→R7; uiPushIcon→R3; uiPushStatus→R4;
  uiClearToken→R6; uiClearAll→R6(interface deletion, no out-of-roger caller); beginUiBatch→R13; endUiBatch→R13;
  uiPushFrameBox→R8; toggleOverlay→R20; toggleDebugLog→R20; remapComparisonMouse→R20;
  toggleTunePanel→R20; tunePanelMouse→R20; isOverlayVisible→R14(subsumed by claim returns; also gated K6/R15).
  Count: R2 6 + R3 4 + R4 3 + R6 3 + R7 2 + R8 1 + R9 1 + R10 4 + R11 2 + R12 2 + R13 2 + R14 2
  + R15 1 + R16 4 + R18 1 + R19 2 + R20 8 + R23 1 = 49. -->

<!-- disposition-family check (every `→ on…`/claim…/other disposition in §3 appears in a row):
  onPicture/onPictureAbsent→R2; onCel→R3; onText→R4 (menuBar/menuRow emits budgeted R5); onFill candidate→decided in R4;
  onWindowOpen/onWindowClose→R5/R6; onControl→R7; onFrameBox→R8; onShow→R9; onSave/onRestore/onFree/onErase→R10
  (PO3 reveal plant retained in R6); beginSelfDraw/endSelfDraw→R11; onAnimateFrame/onFrameEnd→R12;
  beginBatch/endBatch→R13; onMenuHighlight→R5; claimTransition→R14; claimShake→R15; claimCursor+onCursor*→R16;
  wantsUnclampedTextEdit→R17; onMouseMoved→R18; sci_gfx_observer.h include swap→R1; delete (diag/telemetry/comments)→R19
  (+M12 in R5, K7 in R19); fork-only→R20; provider registration/build wiring→R21; standalone-PR→R22. -->

## 7. Gap list

One entry per real-gap from section 4. The only real-gap cluster the seam inventory
found is §4.8's palette-vary/cycle family (4 real-gap rows: `kernelAnimate`,
`kernelAnimateSet`, `palVaryUpdate`, `palVaryProcess`); everything else was hooked,
derivable-from-L2, or excluded. Gaps ADD lines, so each kept entry must fit inside the
post-gate budget — shown added into the projection below.

| Gap (§4 rows) | Proposed event | Layer | Consumer story | Sites | Est. Δ | Keep/defer |
|---|---|---|---|---|---|---|
| GfxPalette per-tick vary + color-cycling (§4.8: `kernelAnimate`, `kernelAnimateSet`, `palVaryUpdate`, `palVaryProcess`) | `onPaletteChanged(palette, step, total)` — an explicit **extension** of the spec §4.2 vocabulary (no palette event exists today; the §9 entry from Task 7 already flags it; `step`/`total` come from `_palVaryStep`/`_palVaryStepStop`, 0/0 when no vary is active) | **L2** (LUT truth — the palette sibling of `onShow`: every visible state change is either pixels crossing `onShow` or a LUT change crossing `onPaletteChanged`; this resolves the "place it in the layer model" question left open in §9) | Any display-layer enhancer needing smooth fades/cycling on an RGBA plate: Roger's `roger_palette_live` becomes the smooth 64-step curve instead of a binary re-apply; a streaming overlay mirrors a fade-to-black; an accessibility layer detects scene dimming. Content is NOT recoverable from L2 pixels (a vary tick emits no pixel event at all — §4.8) — both gap-fill conditions hold | **One** hook at the shared funnel `GfxPalette::copySysPaletteToScreen` (palette16.cpp:486) — all four real-gap rows plus every other `setOnScreen` path flow through it: 1 include + ~5-line null-guarded call + ~5 header lines | **+10** (high conf.) | **KEEP** — both §5 conditions hold, CLAUDE.md lists palette-vary-per-tick as the highest-value underused signal, and the cost is a single hook site at an existing funnel |

**Budget fit:** 475 (§6 consolidation projection) + 10 (kept gap) = **485 ≤ 830** — the
gap fits with no shrink-list consequences. No other gap entries: the list is complete
with one kept entry (an empty/all-deferred list was a valid outcome; the analysis
supports keeping this one).

**Shipped (2026-07-10, commit 2f14bb179c5):** the kept entry landed exactly as
proposed — one null-guarded `onPaletteChanged(_sysPalette, _palVaryStep,
_palVaryStepStop)` hook at the `GfxPalette::copySysPaletteToScreen` funnel
(+12 lines in palette16.cpp, included in the post-reshape re-measures below).

## 8. Standalone upstream PR candidates (G bucket)

These are the standalone, immediately-submittable upstream fixes (bucket **G**) —
each is independent of the observer work and of Roger, per spec §8 ("cheap goodwill
before the big pitch"). One entry per G-bucket row across all of section 3; the
scifont `.cpp` and `.h` halves (F1 + F2) are one PR (both sides of un-gating the same
method). **Finalized 2026-07-10 (Task 10):** the four candidates cover all five §3
G-bucket rows (T2, S3, F1, F2, W2 — re-checked against §3.5/§3.11/§3.12/§3.13; no
G row uncovered, no non-G row leaked in). Audit notes carried into the entries:
the S3 caption PR must be pitched on its Roger-independent merit (the
stale/series-level-caption case) and carries S1's `engines/metaengine.h` include
with it (§3.11 — that include is S3's legitimate wiring, not part of the
forbidden-`getenv` block); per spec's commit rules, the Claude-attribution footer
is stripped from anything submitted upstream.

| Candidate | G rows | Change | Effort | Depends on Roger? |
|---|---|---|---|---|
| scifont drawToBuffer un-gating | F1 (scifont.cpp), F2 (scifont.h) | remove `#ifdef ENABLE_SCI32` around `GfxFontFromResource::drawToBuffer` (definition + override decl) | trivial (2 files, ~7 lines) | No |
| EventRecorder.h decl fix | W2 (gui/EventRecorder.h) | `isImGuiRecorderEnabled()` declared unconditionally (matches its unconditional definition + unguarded call sites) | trivial (1 file, ~6 lines) | No |
| Window caption from detection | S3 (sci.cpp) | `SciEngine::run` sets the OS window caption via `EngineMan.findTarget` (full canonical title, no hardcoded strings); carries the `engines/metaengine.h` include from S1 | small (1 file, ~12 lines); needs an upstream-facing justification independent of Roger (stale/series-level caption) | No |
| text16 textHeight=0 init | T2 (text16.cpp, §3.5) | `textHeight = 0` initializer silences a real uninitialized-read path | trivial (1 line) | No |

## 9. Findings that contradict the design spec

All four findings **resolved 2026-07-10** (Task 10). Every resolution below is a
spec amendment (none refuted); each spec edit is tagged "(audit 2026-07-10)"
inline for traceability.

- Task 7 (§4.8): the spec's L1-L4 event vocabulary (§4.2) has **no palette event**, yet the seam inventory finds a real-gap for the per-tick palette-vary/cycle path (`palVaryUpdate`/`palVaryProcess`, `kernelAnimate`/`kernelAnimateSet`) — a smooth fade/cycle emits no L2 pixel event and its intermediate LUT is unrecoverable from pixels, and CLAUDE.md already lists palette-vary-per-tick as the highest-value underused signal. The design needs a new event (proposed `onPaletteChanged(palette, step, total)`, likely L1/L2-adjacent) to cover it; per the §2 budget it should be one event folding all four palette-vary/cycle call sites. Task 8/10 to place it in the layer model.
  **Resolution: spec amended** — `onPaletteChanged(palette, step, total)` added to spec §4.2's **L2** table (placed per §7's layer argument: the palette sibling of `onShow` — every visible state change is either pixels crossing `onShow` or a LUT change crossing `onPaletteChanged`), with the one-hook/+10-line rationale citing this audit's §4.8/§7; the L2 funnel paragraph now names it as the second half of the pixel/LUT truth pair.
- Task 8 (§6): the projected reshaped footprint — **475** consolidation-only, **485** with the kept palette gap — lands **well below** spec §2's "~780 ± 100" projection. Not a budget violation (success-criterion 4 passes with a 345-line margin), but §2's projection paragraph under-counts three departures the audit's own dispositions make explicit: the fork-only D-bucket carve-out (−85: E3/E4/S1/S4-dev/S5, carried downstream outside the neutral seam), wiring → registration + plugin module.mk (−35), and G-bucket standalone-PR departures (−25); it also under-estimates the menu exile (−125 grounded in the measured 163 menu lines vs the −100 guess). Task 10 should update spec §2's projection, or state explicitly which measurement rule (with vs without the fork-only carve-out) its number assumes — even with the carve-out counted back in, the conservative bound is 650 consolidation-only / 660 including the kept +10 gap — still PASS vs 830.
  **Resolution: spec amended** — spec §2's projection paragraph replaced with the audited numbers (ΣΔ = −355, projected 475 / 485 with the kept gap, robustness bound 650, citing §6/§7 here) and it now states the measurement rule explicitly (fork-only D-bucket rows carried downstream outside the neutral-seam measurement); §2's 822 baseline carries a supersession note (re-measured **830 / 18 files / 83 hunks** at 14509d438c3, per §2 here); §4.2's menu-exile "~-100" guess tagged with the measured −125; spec §9 criteria 4/5 tagged with the measured 830 baseline and 49-virtual count.
- Task 9 (§5): the L2-completeness claim (spec §3.3 / success-criterion 3) as stated — "every pixel reaching the screen in **SCI16** paths crosses `onShow` or a self-draw bracket" — is **falsified by one enumerated path**: the Macintosh icon bar (`GfxMacIconBar::drawImage`, maciconbar.cpp:203/209/211) writes directly to `_screen->gfxDriver()->copyRectToScreen`, bypassing `bitsShow` and any bracket, and is SCI16 (compiled unconditionally, module.mk:54), not SCI32. It is gated on `hasMacIconBar()` (Mac SCI game versions only), so it is outside Roger's shipping EGA-DOS scope and not a live bug — but the claim is over-broad. Resolution options (Task 10): (a) **scope the claim to non-Mac SCI16** — the honest, zero-code fix, matching Roger's actual EGA-DOS target; or (b) emit an L2 `onShow` from `GfxMacIconBar::drawImage` (one hook), making the claim literally true. The 91 other copy-to-screen callers are all funnel / claimed-L4 / SCI32 / debug-console — the funnel itself is complete for the game render loop; this is the lone platform-UI gap.
  **Resolution: spec amended** — option **(a)** taken (this is a docs-only effort; option (b) is a code change and the spec's own structure supports scoping): spec §3.3's verification bullet and §4.2's L2 funnel paragraph now scope the claim to **non-Mac SCI16**, name `GfxMacIconBar::drawImage` as the sole game-reachable exception (citing §5.3 here), and record the one-line `onShow` hook as the noted future fix if Mac SCI ever enters scope.
- Task 8 (§6, row R4): spec §6's `onFill(rect, color, token)` candidate is **rejected** on the line-budget grounds §6 delegated to the audit — the kDisplay background fill (P19) folds into `onText` as a `source=fill` enum value instead (retires the empty-string-overload smell without adding a virtual). Task 10: update spec §6's candidate list and §4.2's `onText` source enum accordingly.
  **Resolution: spec amended** — spec §6's candidate bullet now records the decision (onFill rejected, fold into `onText(source=fill)`, citing row R4 here) and spec §4.2's `onText` source enum gains `fill`.

## 10. Success criteria (spec §9)

Run 2026-07-10 after the §9 resolutions. Spec §9's own six criteria match the
plan's six one-for-one; two of the spec's literals were superseded by re-measure
and are reconciled in the evidence cells (criterion 4's "~800" baseline →
measured 830; criterion 5's "~47" virtuals → measured 49 — both now also tagged
in the spec itself).

| # | Criterion | Result | Evidence |
|---|---|---|---|
| 1 | every hunk classified | PASS | 83/83 hunks mapped across 18 files (per-subsection coverage comments at §3.1–3.13; the §3 coverage-total comment sums 18+6+2+14+4+2+9+12+2+4+4+3+3 = 83, matching §2's measured 83) |
| 2 | every public entry point inventoried | PASS | §4.1–4.10: per-class method count == table row count for all ten classes (36/43/22/7/9/18/3/41/16/7; ctor+dtor collapsed to one row per the §4 convention) |
| 3 | L2 completeness verified by enumeration | PASS | §5: 92 copy-to-screen callers + 43 `bitsShow(` hits enumerated and classified (funnel 38, claimed-L4 31, SCI32-only 7, debug/console 13, UNCOVERED 3 — the one Mac-icon-bar path). The claim holds **scoped to non-Mac SCI16**; the spec now carries that scoping (§9 resolution 3) |
| 4 | projected diff ≤ 830 (cap 1000) | PASS | §6 + §7: 475 consolidation + 10 kept palette gap = **485 ≤ 830** (345-line margin; conservative robustness bound 650 consolidation-only / 660 including the kept +10 gap — still PASS vs 830; the 1000 cap nowhere near threatened). Baseline evidence note: the 830 target is the 2026-07-10 re-measure (§2) superseding the plan's literal 825 and spec §9's "~800" — both supersessions recorded (§2 note here; spec §2/§9 amendments). **Post-implementation measured (2026-07-10, Task 14 finale):** same-scope `git diff --stat` vs merge-base = 20 files, 1079(+)/15(−) = **1094 raw** (baseline methodology reproduces exactly: 830 at 14509d438c3). Raw exceeds the 485 projection because the raw figure counts mass the measurement rule excludes or that landed on a bounded fallback: observer header+impl 388+35 = 423 (R23 budgeted 200 — the relocated war-story/doc comments live there now), R20 fork-only carve-out ~90 lines still carried in event.cpp/sci.cpp (excluded from the neutral-seam target by the measurement rule), R22 standalone-PR rows +25 still in-fork pending upstream merge, and R21 landed on its pre-authorized fallback (roger object list kept inline in module.mk, +43 — the "wiring Δ 0" arm of the robustness bound; create_project's createModuleList does not follow GNU-make `include`). Apples-to-apples: non-header measured mass ≈513 vs ≈285 projected non-header (485 − 200 header allowance) — hook sites came in richer than projected (contract comments at the seams), roughly +215; the header itself landed at 423 vs 200 budgeted (+223; R23 assumed ~3 doc lines/event, but load-bearing contracts — rollback semantics, owner promotion, coordinate spaces, batching — legitimately average ~6). Rule-adjusted headline: ≈979 (1094 − 90 − 25) vs robustness bound 650 — OVER the bound, UNDER the 1000 hard cap; criterion 4's ≤830 target is met on the rule-adjusted figure, so the 485 projection was optimistic about comment mass, not about structure. Disposition: accept-and-document — ~55–60 lines of relocatable war-story anecdotes in the header could move to roger/-side docs during the pre-upstream manufactured-branch pass; slimming cannot rescue the projection and the header contracts are load-bearing |
| 5 | all 49 virtuals mapped, no orphans | PASS | §6: the virtual-coverage comment maps all 49 `roger_art_provider.h` virtuals (measured count, incl. the dtor) to rows R2–R23, per-row counts summing to exactly 49; every §3 disposition family also lands in a row (disposition-family check comment) |
| 6 | G-bucket PR candidates with effort | PASS | §8 (finalized): four PR candidates covering all five G rows (F1+F2 as one PR, W2, S3, T2), each with an effort estimate and an explicit "Depends on Roger? No" |

All six PASS — no loop-back required. With §9 fully resolved, the audit + amended
spec pair is the go signal for the future observer-reshaping work (Stage 3).

Measurement note (2026-07-10, genericization follow-up): the `createSciGfxObserver()`
factory + `onEngineStartup()`/`interceptEvent()` follow-up removed the last named-type
references outside `engines/sci/roger/` (the R20 fork-only carve-out blocks in
sci.cpp/event.cpp are gone — zero Roger references remain outside `roger/` except
module.mk object paths and upstream game text); same-scope `git diff --stat` vs
merge-base re-measured at 20 files, 1043(+)/15(−) = **1058 raw** (down from Task 14's
1094; the ~90-line R20 exclusion no longer applies, so raw ≈ rule-adjusted − the R22
standalone-PR rows: ≈1033).

Measurement note (2026-07-11, jon-save-gui at 61a79ff5bc2): the §3.14 base/main.cpp
picker hook landed after the note above, so the zero-reference statement carries one
further documented exception — base/main.cpp's `PLUGIN_ENABLED_STATIC(SCI)`-guarded
`sci/roger/` include + call (§3.14 B1/B2, fork-only; the CLAUDE.md grep gate is scoped
to `engines/sci` and is unaffected — re-verified at this commit). Same-scope
`git diff --stat` vs merge-base re-measured at 21 files, 1060(+)/15(−) = **1075 raw**:
the delta over 1058 is base/main.cpp (+13, §3.14's own 13-line measurement still
reproduces exactly), its module.mk object path (+1), and a doc-comment expansion on
`interceptEvent` in sci_gfx_observer.h.

Measurement note (2026-07-11, jon-first-pass-prompt0 at b69fffeb828): style-conformance
pass (Prompt 0, plan `docs/future-prompts/0-conform-code-to-scummvm-guidelines.md`) ran
on this branch — 10 pass commits, 00dc60541d9..b69fffeb828
(`git log --oneline 00dc60541d9..b69fffeb828 | wc -l` = 10). Changes outside
`engines/sci/roger/`: ASCII-ification of fork hunk comments (5 comment rewrites in the
observer seam files); no code logic changed. Reproducible measurement command and output:

    git diff --stat origin/master...b69fffeb828 -- engines/sci base gui \
        ":(exclude)engines/sci/roger" ":(exclude)engines/sci/README.md"
    # 20 files changed, 1047 insertions(+), 15 deletions(-)  → 1062 raw

Same command at the pre-pass head (00dc60541d9):

    git diff --stat origin/master...00dc60541d9 -- engines/sci base gui \
        ":(exclude)engines/sci/roger" ":(exclude)engines/sci/README.md"
    # 20 files changed, 1054 insertions(+), 15 deletions(-)  → 1069 raw

The pass removed 7 net insertion lines (comment compression in the seam files).
The previous note's figures (21 files / 1060(+) / 1075 raw) do not reproduce with
this command (off by one file and ~6 lines — measurement-convention drift); future
notes should cite this exact command.
The pass continued past b69fffeb828 with three follow-up commits (docs corrections,
test-comment refs, final style fixes — 00dc60541d9..HEAD on jon-first-pass-prompt0)
that do not affect the out-of-roger measurement except this file.

## 11. Prompt 1 verification pass (2026-07-11)

Run at HEAD `a425b352960` (branch `jon-first-pass-prompt-1`). This is a
documentation-only verification pass — no code was changed. All findings below
are flags for future work; nothing was fixed inline.

### 11.1 Re-measurement

Re-run at `a425b352960` using the established audit command:

    git diff --stat origin/master...HEAD -- engines/sci base gui \
        ":(exclude)engines/sci/roger" ":(exclude)engines/sci/README.md"
    # 20 files changed, 1047 insertions(+), 15 deletions(-)  → 1062 raw

**Result: exact match.** The last recorded note (2026-07-11, `b69fffeb828`) predicted
the three follow-up commits (`c5488b32885`, `bd6d1dd44ca`, `a425b352960` — docs and
test-only) would not affect the out-of-roger measurement. Confirmed: figures
are unchanged at 1062 raw. No stale figures in §2 or the trailing notes.

### 11.2 §3 classification coverage at the current head

The §2 baseline enumerates 18 files (measured 2026-07-10 at `14509d438c3`). The
current 20-file diff adds three post-baseline files that lack §3 subsections. They
are correctly documented in later sections but are catalogued here for the
manufactured-branch pass:

| File | Lines | Where classified | §3 subsection needed? |
|---|---|---|---|
| `engines/sci/sci_gfx_observer.h` | +419 | §6 R23 (new observer header); §10 post-impl note | No — postdates §3; §6/§7/§10 cover it |
| `engines/sci/sci_gfx_observer.cpp` | +35 | §6 R23 (companion impl); §10 post-impl note | No — same class as above |
| `engines/sci/graphics/palette16.cpp` | +12 | §4.8 real-gap; §7 "Shipped" note | No — §4.8/§7 fully document the one hook |

All three are properly covered; no §3 subsection gap is a finding.

### 11.3 Bucket table (full diff)

Every file in the diff mapped to the seven plan buckets from the
Prompt 1 preamble. File groups are used where the per-file disposition is uniform.

| File / group | Lines (+/-) | Plan bucket | Disposition |
|---|---|---|---|
| `engines/sci/roger/**` (83 files) | +20788 | Roger provider code | Wholesale move to plugin when self-registration lands (Stage 3) |
| `test/sci/roger/**` (67 files) | +7338 | Documentation / downstream-only dev tooling | Test infrastructure for the Roger provider; relinks against roger static lib at Stage 3; never part of an upstream PR |
| `engines/sci/sci_gfx_observer.h` | +419 | Required SCI hook sites / observer seam | Engine-owned neutral seam; upstreamable as the core of the observer PR |
| `engines/sci/sci_gfx_observer.cpp` | +35 | Required SCI hook sites / observer seam | Companion impl; ships with sci_gfx_observer.h |
| `engines/sci/graphics/paint16.{cpp,h}` | +107/−2 | Required SCI hook sites | Mechanical N-bucket hooks (P1–P20); reshape to §6 R1–R23 events before upstream PR |
| `engines/sci/graphics/animate.cpp` | +62 | Required SCI hook sites | Mechanical N-bucket hooks (A1–A14); A1/A12/A14 fork-only telemetry to delete |
| `engines/sci/graphics/controls16.cpp` | +101/−2 | Required SCI hook sites | Mechanical hooks (C1–C9); C3 claim → R17; offsetRect/token helpers → observer-side |
| `engines/sci/graphics/menu.{cpp,h}` | +68/+2 | Roger-specific leak outside roger/ (S bucket) | Menu state exile (§3.8, R5) still needed before upstream; the largest S-bucket item |
| `engines/sci/graphics/text16.cpp` | +41/−1 | Required SCI hook sites | T1–T4; T2 is G-bucket standalone PR (§8) |
| `engines/sci/graphics/ports.cpp` | +30/−2 | Required SCI hook sites | PO1–PO3; duty-3 reveal plant retained (documented exception) |
| `engines/sci/graphics/transitions.cpp` | +23 | Required SCI hook sites | TR1–TR2; TR2 is a documented L4 claim |
| `engines/sci/engine/kgraphics.cpp` | +19 | Required SCI hook sites | K1–K7; K7 diag comment to delete (D); K6 claim → R15 |
| `engines/sci/graphics/cursor.cpp` | +5/−1 | Required SCI hook sites | CU1–CU2; CU2 is a documented L4 claim |
| `engines/sci/graphics/palette16.cpp` | +12 | Required SCI hook sites | One-hook gap fill (§7, shipped); palVaryUpdate/etc. real-gap |
| `engines/sci/graphics/scifont.{cpp,h}` | +2/−5 | Generic reusable ScummVM change (G bucket) | G-bucket standalone PR — un-gate drawToBuffer; no Roger dependency (§8 F1+F2) |
| `engines/sci/module.mk` | +45 | Required SCI hook sites / build wiring | W1 object list; moves to plugin module.mk at Stage 3 |
| `engines/sci/sci.cpp` | +32 | Roger-specific leak outside roger/ + build wiring | S1 FORBIDDEN_SYMBOL_EXCEPTION_getenv must go before any upstream slice; S3 G-bucket |
| `engines/sci/event.cpp` | +29 | Roger-specific leak outside roger/ (D/W) | E3/E4 fork-only; E2 N-bucket onMouseMoved |
| `base/main.cpp` | +13 | Temporary hacks / downstream-only dev tooling | B1/B2 fork-only launcher seam; never upstreamable (§3.14) |
| `gui/EventRecorder.h` | +4/−2 | Generic reusable ScummVM change (G bucket) | W2 G-bucket standalone PR; no Roger dependency (§8) |
| `test/module.mk` | +6 | Required SCI hook sites / build wiring | W3 test wiring; relinks at Stage 3 |
| `engines/sci/README.md` | +94 | Documentation / downstream-only tooling | Fork orientation doc; excluded from measurement; never upstream as-is |
| `docs/roger/**`, `docs/roger.md` | +1484 | Documentation / downstream-only tooling | User-facing docs and audit; never part of upstream PR |
| `CLAUDE.md`, `.claude/**`, `build_and_run.ps1`, `build_tests.ps1`, `.gitignore` | various | Documentation / downstream-only dev tooling | Dev harness; excluded from all measurements; never upstream |

### 11.4 Quarantine sweep (Step 2 verification)

**Sweep A — `engines/sci` outside `roger/`:**

    git grep -in "roger" -- engines/sci ':(exclude)engines/sci/roger'

Hits: upstream game text only (Roger Wilco strings in `script_patches.cpp`,
`workarounds.cpp`, `celobj32.cpp`, `picture.cpp`; detection-table strings in SQ/AGI)
plus `module.mk` object PATHS and `engines/sci/README.md` (docs file, excluded from
measurement). Zero unexpected code references. **PASS.**

**Sweep B — outside `engines/sci` entirely:**

    git grep -il "roger" -- . ':(exclude)engines/sci' ':(exclude)docs' \
        ':(exclude)CLAUDE.md' ':(exclude)test/sci/roger'

Hits: `base/main.cpp` (§3.14 B1/B2 — documented fork-only launcher seam),
`.gitignore`, `build_and_run.ps1`, `build_tests.ps1` (downstream-only dev tooling),
`.claude/skills/roger-loop/SKILL.md` (skill, never committed upstream), plus
`devtools/`, `dists/`, `engines/supernova/` and other upstream files matching
"Roger Wilco" or character names — all false-positive name matches. No unexpected
code leaks outside the two documented fork-only sites. **PASS.**

### 11.5 Asset and binary check (Step 4 verification)

    git diff --numstat origin/master...HEAD | grep "^-"

Four binary files listed (all under `test/sci/roger/fixtures/`):
`4x4_gradient.png` (86 bytes), `4x4_p5.png` (73 bytes),
`plate_8x8.png` (75 bytes), `rgba_2x2.png` (76 bytes).
All are tiny synthetic PNG fixtures (< 100 bytes each), consistent with the
known adjudicated state ("current PNG fixtures are ~75 bytes each" — CLAUDE.md).
No game assets, no generated cache files, no large binaries in the diff.
**PASS.**

### 11.6 GPL header check (Step 6 verification)

    git grep -rL "GNU General Public License" -- engines/sci/roger/ | grep -E "\.(h|cpp)$"

No output — all 76 tracked `.h`/`.cpp` files under `engines/sci/roger/` have GPL
headers. Production set is 100%. Test headers under `test/sci/roger/` remain at
17/40 deliberately (upstream `test/` has none; adjudicated at Prompt 0 and not
re-examined here). **PASS.**

### 11.7 Conformance drift spot-check (Step 6 preamble)

    git grep -inE "claude|anthropic|ai agent|copilot" -- \
        engines/sci docs/roger docs/roger.md engines/sci/roger

Hits: `docs/roger/FORK_AUDIT.md` only — plain-text references to "CLAUDE.md"
(a filename) and one occurrence of "Claude-attribution footer" in the §8 PR-prep
note. No hits in any `.cpp`/`.h` source file under `engines/sci/` or
`engines/sci/roger/`. Zero hits in `docs/roger.md` (user-facing doc). **PASS.**

No obvious style drift observed during the grep-first scan.

**Correction (2026-07-11, later the same pass):** the "FORK_AUDIT.md only" claim
above was wrong — re-running the same grep also hits `CLAUDE.md` citations in
four README (non-source) files that already existed at the time:
`engines/sci/README.md:21,92`, `engines/sci/roger/README.md:91,122`,
`engines/sci/roger/gen/README.md:30`, `engines/sci/roger/overlay/README.md:25`.
Source files (`.cpp`/`.h`) and `docs/roger.md` remain clean, so the PASS verdict
for code stands, but the README citations are **flagged drift**: they reference
a downstream-only orientation file from documentation that would accompany an
upstream submission, and must be stripped or replaced with neutral pointers
(e.g. `docs/roger/` docs) during the manufactured-branch pass. Later hits in
`docs/roger/PR_PLAN.md` / `UPSTREAMING_PLAN.md` are legitimate (they state the
no-AI-references rule and the downstream-only exclusion list).

### 11.8 Summary

| Sweep | Result |
|---|---|
| Re-measurement at `a425b352960` | PASS — 1062 raw, exact match to last note |
| §3 coverage of new files | PASS — 3 post-baseline files all documented in §6/§7/§10 |
| Bucket table | COMPLETE — 24 rows covering every file group |
| Quarantine grep (SCI + external) | PASS — no unexpected Roger references in code |
| Asset/binary check | PASS — 4 tiny fixtures (<100 bytes each), no game data |
| GPL header check | PASS — 76/76 production sources have headers |
| AI-ref drift | PASS — zero hits in source files or user-facing docs |

No blockers. Known open flags carried forward from earlier passes (S1
FORBIDDEN_SYMBOL_EXCEPTION_getenv, menu S-bucket state exile, A14 non-const static
`s_prevCycleT0`) are documented in §3 and unchanged.

### 11.9 Build and test verification (2026-07-11)

Branch head: `a78571c8dc6` (`jon-first-pass-prompt-1`, documentation-only pass 1–5).

| Gate | Command / check | Result |
|---|---|---|
| Unit tests | `.\build_tests.ps1` | PASS — 351/351 |
| Game build | MSBuild (Release\|x64) | PASS — scummvm.exe produced |
| Smoke (enhanced) | `-Game qfg1 -SaveSlot 1 -Script qfg1-smoke.rin` | PASS — exit 0 |
| Smoke (original) | `-Mode original -Game qfg1 -SaveSlot 1 -Script qfg1-smoke.rin` | PASS — exit 0 |
| Regression gate | `test\sci\roger\run-regression.ps1` | 41/42 PASS (1 known-stale FAIL) |
| Other-engines diff | `git diff --stat origin/master...HEAD -- engines :(exclude)engines/sci` | PASS — empty (no other engine touched) |
| Quarantine grep | `git grep -in "roger" -- engines/sci :(exclude)engines/sci/roger` | PASS — upstream game text + module.mk object paths only |

Regression gate detail: the single failure is `qfg1-menu-cycle presence:m-after`
(0 differing px < 6000 threshold). This is the documented KNOWN-STALE manifest
entry — the window-height fraction regions in the manifest are stale since the
2026-07-05 stretch-mode change (dcaa7e2625a); the top-4.5% band is now pure
letterbox in both captures, but the menu opens correctly (verified standalone).
Not a code regression; requires manifest re-derivation, not a code fix.

Walk-perf entries: both PASS on this run (qfg1 median=83/p90=84/busy=10,
sq3 median=83/p90=84/busy=4); no flakiness adjudication required.

All SCI hook sites outside `engines/sci/roger/` are null-guarded `g_sciGfxObserver`
events against the neutral `SciGfxObserver` interface; the concrete provider type
appears nowhere outside `engines/sci/roger/`. This is enforced by the quarantine
grep above and documented in §3 and §10. Roger-off (null observer) is byte-identical
to stock SCI; original-mode smoke confirms the provider-present native path also exits
cleanly.
