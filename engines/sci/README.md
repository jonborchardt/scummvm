# SCI engine — fork notes: the Roger display enhancement

This fork's only additions to the SCI engine are the **`roger/` folder** and a
set of small, mechanical **observer hooks** in the engine proper. Everything
else is upstream ScummVM. This file explains the split; it is the entry point
for anyone landing in `engines/sci/` and wondering what is fork and what is
stock.

## What `roger/` is

Roger is a **display-layer enhancement provider inside the existing SCI
engine** — not a new engine. It has no engine class, no metaengine, no
detection tables; detection, game logic, save games, walkability, and
occlusion all stay native SCI. For SCI0 EGA games (SQ3, QFG1 EGA) it generates
high-resolution backgrounds and VIEW cels **in-engine from the original SCI
resources** (the omyac upscaler pipeline) and presents them through ScummVM's
OSystem overlay, composited above the untouched 320×200 game surface.

All Roger code lives in [`roger/`](roger/README.md) (see its README for the
source layout, build, and usage). User-facing docs: `docs/roger.md`.
Architecture invariants and the full hook table: the repo-root `CLAUDE.md`.
Diff inventory vs upstream: `docs/roger/FORK_AUDIT.md`.

## The generic engine alterations: the `SciGfxObserver` seam

The engine proper is modified in exactly one pattern. A neutral, engine-owned
observer interface — `SciGfxObserver` in
[`sci_gfx_observer.h`](sci_gfx_observer.h) — is called from SCI's graphics
chokepoints through the null-by-default POD global `g_sciGfxObserver`. Its
events are layered:

- **L1 — frame lifecycle:** `kernelAnimate` boundaries (`onFrameStart` /
  `onFrameEnd`, `onAnimateFrame`), UI batching, mouse.
- **L2 — pixel truth:** `bitsShow` / `bitsSave` / `bitsRestore` / `bitsFree`
  (`onShow` / `onSave` / `onRestore` / `onFree`), erase rects, self-draw
  brackets, palette changes.
- **L3 — semantic events:** text (`onText` from `GfxText16::Box`), cels
  (`onCel`), windows (`onWindowOpen` / `onWindowClose`), controls, pictures
  (`onPicture`), menus.
- **L4 — claims:** documented overrides where the observer may take over a
  native draw (`claimTransition`, `claimShake`, `claimCursor`) — returning
  false means the native path runs unchanged.

The contract, enforced on every hook site (full version in the
`sci_gfx_observer.h` header comment):

- **Null observer ⇒ byte-identical to stock.** Hook sites are mechanical
  null-guarded calls with minimal argument marshalling — no observer logic, no
  game-specific branches, and no concrete observer type is ever named in SCI
  code. Grep-enforced: `git grep -in "roger" -- engines/sci
  ':(exclude)engines/sci/roger'` must show only upstream game text and
  `module.mk` object paths.
- **New hooks are virtuals on `SciGfxObserver`**, never calls to a concrete
  provider, and never a `roger/` include outside `roger/`.
- **Per-cycle events must be O(1)** in the observer — SCI's game cycle is a
  single synchronous heartbeat, so a heavy per-cycle path slows game logic
  itself, not just rendering.

## Where the hooks live

| File | Hooks |
|------|-------|
| `graphics/paint16.cpp` | picture render (`onPicture`), `bitsShow`/`bitsSave`/`bitsRestore`/`bitsFree`, self-draw brackets, kDrawCel init bakes |
| `graphics/animate.cpp` | game-cycle bracket, cast draw (`onAnimateFrame`), addToPic + init-bake cel capture (`onCel`) |
| `graphics/text16.cpp` | all text out (`onText`, per line inside `GfxText16::Box`) |
| `graphics/ports.cpp` | window open/close (`onWindowOpen`/`onWindowClose` — THE UI lifetime signal) |
| `graphics/controls16.cpp` | dialog controls (`onControl`, list rows, text edits) |
| `graphics/menu.cpp` | status/menu bar, dropdown rows, highlight |
| `graphics/transitions.cpp` | scene transitions + shake (`claimTransition`/`claimShake`) |
| `graphics/palette16.cpp` | live palette funnel (`onPaletteChanged`) |
| `graphics/cursor.cpp` | cursor shape/view/hide + `claimCursor` |
| `engine/kgraphics.cpp` | kGraph primitives (redraw/frame box → `onErase`/`onFrameBox`) |
| `event.cpp` | `interceptEvent` (hotkeys, mouse remap) + the generic `.rin` scripted-input driver |
| `sci.cpp` | wiring only: `setSciGfxObserver(createSciGfxObserver(...))` after graphics init, then `onEngineStartup()`; slot cleared and observer deleted in the destructor. Fully neutral — no `roger/` include, no concrete type. |

The concrete observer (`FileRogerArtProvider`) is constructed observer-side by
the neutral factory `createSciGfxObserver()`, defined in
`roger/roger_register.cpp` — the one place the two worlds meet. (One fork-only
seam sits outside this engine entirely: a ~13-line guarded hook in
`base/main.cpp` runs the Roger game picker as the launcher round;
`roger_no_launcher` opts out.)

## Working on this code

- Adding a hook? First check whether an existing event already covers the need
  — shrinking the hook count is a goal. If not: add a virtual (default no-op)
  to `SciGfxObserver`, call it null-guarded from the chokepoint, implement it
  in `roger/`.
- Roger-off must stay byte-identical to stock SCI; run the grep gate above
  before committing anything under `engines/sci/` outside `roger/`.
- Read the "SCI0 rendering & UI invariants" section of the repo-root
  `CLAUDE.md` before touching any hook — most past bugs were one lesson
  (mirror both the draw *and* the lifetime of a native element; never fight
  the synchronous game cycle) wearing different clothes.
