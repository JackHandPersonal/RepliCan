# Architecture review — RepliCan C++ module

Scanned 2026-09-19 at build `optic262`, commit `d87d216`. 172 files, 46,772 lines under
`Source/RepliCan/`, plus `Config/`. Every figure here was measured against the working tree, not
estimated. Findings are ranked by consequence, not by how easy they are to fix.

Nothing here is urgent — the build is green and the game runs. This is a map, not a fire alarm.

| Measured | |
|---|---|
| Lines / files | 46,772 / 172 |
| Mutual dependency cycles between lanes | **13** |
| `Core/BasePlayerController.cpp` | **7,046 lines**, 333 functions, 273 members |
| `Characters/BaseCharacter.{h,cpp}` | 7,604 lines, 280 functions, **550 header members** |
| Automated tests | **0** |
| Loose data dirs read at runtime / staged for packaging | **6 / 0** |
| Independent JSON loaders / with live `Reload()` | 13 / 5 |
| Hard-coded `/Game/…` path literals (distinct) | 222 (191) |
| `UFUNCTION(Exec)` console commands / shipping guards | 60 / **0** |

---

## 1. The game cannot currently be packaged — BLOCKING

Six directories that are **not** under `Content/` are read at runtime through
`FPaths::ProjectDir()`: `UI/`, `Conversations/`, `Characters/`, `Sequences/`, `Data/`, `RawAudio/`.
Between them they hold the weapon catalogue, the item catalogue, the character sheet spec, terminal
text, conversations, sequences, character configs, and the loose `.wav` files the runtime audio path
plays directly.

In a cooked build `ProjectDir()` resolves to the staged project folder, which contains only what
packaging was told to stage. Loose non-asset folders need `DirectoriesToAlwaysStageAsUFS` under
`[/Script/UnrealEd.ProjectPackagingSettings]`. **There is no such entry in any file in `Config/`**
(`DefaultEditor.ini`, `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`). So on the first
packaged run every catalogue comes back empty: no weapons, no items, no sheet, no dialogue.

This is invisible in the editor, which is why it has survived. It is not a design mistake — loose
JSON is exactly what makes the live-reload workflow good — it is a packaging step nobody has needed
yet.

**Fix:** add the six directories to `DefaultGame.ini` packaging settings, then do one cooked build
and launch it. Cheap, and it converts an unknown into a known.

## 2. There is no layering — every lane pair that touches is a cycle — STRUCTURAL

The directory split done earlier on 2026-09-19 gave the lanes clean *file* ownership and did what it
was meant to: it removed the collisions. What it did not do, and could not do on its own, is
establish a dependency direction.

    Lane         depends-on   depended-on-by
    Core              61            28
    UI                46            33
    Characters        16            24
    Weapons           14            24
    World              7            20
    Narrative          4            11
    Items              1             9      <- the only clean leaf

Thirteen pairs are mutual. The four heaviest:

| Pair | → | ← | What it is |
|---|---|---|---|
| `Core ↔ UI` | 29 | 21 | the controller owns the panels |
| `Core ↔ Characters` | 9 | 3 | pawn state read and written both ways |
| `Weapons ↔ Characters` | 5 | 6 | hand anchoring and hold state |
| `UI ↔ Characters` | 9 | 2 | sheet and appearance editing |

The practical cost: no lane can be reasoned about, tested or compiled alone, and a change in any of
them can surface anywhere. It is also why `Core/BasePlayerController.h` (1,112 lines) is included by
24 files — touch it and a quarter of the module rebuilds.

**`Items/` is the counter-example and the model.** It depends on one thing and nine depend on it.

**Direction:** aim at one rule — **`Core` must not include `UI`.** The 29 edges are almost all the
controller reaching into widgets to show, hide or refresh them, which is invertible with a small
interface or delegate the UI layer registers against.

## 3. Two god objects hold most of the game — STRUCTURAL

`BasePlayerController` is 8,158 lines across its pair: **333 member functions, 273 members**,
references to **21 distinct widget types**, 26 `#include "UI/…"` lines, and **57**
`Show*`/`Hide*`/`Is*`/`Toggle*` methods — a triple per panel across at least nine panels. That is a
UI window manager living inside the player controller, and it accounts for most of the `Core → UI`
coupling in finding 2.

`BaseCharacter` is the same problem with a worse header: **2,249 lines declaring 550 members**,
included by 15 files — a rebuild tax on every one.

`SESSION_LANES.md` already names this as the real coupling left after the repath. That assessment is
correct, and it is why several sessions can hold the same file uncompiled at once.

**First cut:** extract the panel show/hide/is surface into one `UScreenStack` / `UPanelRouter` the
controller owns and widgets talk to. Mechanical, removes the largest single block of the god object,
and serves finding 2 at the same time.

## 4. No automated tests, in a codebase whose bugs are all silent — RISK

Not one `IMPLEMENT_SIMPLE_AUTOMATION_TEST` or `DEFINE_SPEC` in 46,772 lines. Every defect is found by
a person looking at the screen, or by one session reading another's code.

What makes this worth raising is the *character* of the bugs this project produces. On 2026-09-19
alone:

- `M_ScopeMask`'s distance term was never connected, so ADS was a black screen;
- `GiveStartingWeapons` returned the first slot accepting a kind regardless of occupancy, so the
  second weapon was silently dropped — for every session that had ever run;
- `SheetSpec`'s slot defaults granted kinds (`Primary`, `Sidearm`) that no weapon has ever had;
- `RecordAssistRay` ignored only `GhostActor`, so its `ECC_Pawn` trace hit the player's own capsule
  at distance 0 and recorded a payload that looked like a deliberate click on a character;
- a guard compared `FField::Key`, a `const TCHAR*`, with `==` — which would have compiled into
  something that never matched.

**None of those throw. All of them pass a build.** Several are pure functions over data and are
exactly what a cheap spec catches.

**Start narrow:** one spec over the catalogue layer — every weapon's `kind` resolves to at least one
enabled slot; every optic's `eye` lies on its own glass; no `FField::Key` compared with `==`. Three
assertions would have caught three of that day's bugs before a human saw them.

## 5. Thirteen independent JSON loaders — DUPLICATION

Thirteen files implement load / deserialize / cache independently, across 23 read sites;
`UI/ReferenceWidget.cpp` alone has six. No shared helper, so each re-decides how it handles a missing
file, a parse failure, a stale cache and a reload.

Two consequences are already visible:

- **Live reload works for only 5 of the 13** (`BasePlayerController`, `ImpactEffects`, `ItemCatalog`,
  `ReferenceWidget`, `WeaponCatalog`). Editing the others needs a restart, and which is which is not
  discoverable without reading the code.
- The read-modify-write hazard — a tool parsing a file, working, then writing the whole thing back
  over what the game saved meanwhile — is a property of this duplication. There is no single place
  to fix it once.

**Consolidate** behind one `FJsonDataFile` owning path resolution, load, parse-failure logging, cache
and `Reload()`. Do it *after* finding 1, since path resolution is exactly what packaging changes.

## 6. 191 hard-coded asset paths, with no compile-time safety — FRAGILITY

222 `"/Game/…"` literals, 191 distinct, concentrated in `Characters/CharacterAnimInstance.cpp` (46),
`Characters/FaceController.cpp` (38) and `Characters/CharacterConfig.cpp` (24). Renaming or moving an
asset in the editor cannot break the build; it breaks at runtime.

To be fair to the code: **the loads themselves are disciplined.** 77 of 97 check their result, usually
on the very next line with a warning, and most of the 20 that don't are engine primitives
(`/Engine/BasicShapes/…`) that always resolve. A broken path degrades and logs rather than crashing —
right behaviour, and also how a missing asset sits unnoticed in a log nobody reads.

**Cheap mitigation:** not a wholesale conversion to soft references. One commandlet or Python pass
resolving every literal in the source and reporting misses turns all 191 into a check you can run.

## 7. Sixty console commands ship unguarded — DECISION

60 `UFUNCTION(Exec)` commands — tuning consoles, diagnostics, edit-mode tooling — compile into every
configuration; there is not one `UE_BUILD_SHIPPING` guard in the module (5 files use `WITH_EDITOR`).

Listed as a decision rather than a defect: plenty of shipped games leave their console in, and for a
project this deep in iteration the tooling is worth more than the hygiene. Recorded so it stays a
choice.

---

## What is already sound

A review that lists only faults misrepresents the codebase. Two of these were expected to be findings
and came back clean when measured properly.

- **Save game** carries `SaveVersion = 1` plus a readable JSON mirror alongside the binary. Migration
  is possible.
- **Asset loads**: 77 of 97 guarded and logged. The first pass at this used a same-line test and was
  simply wrong — worth re-measuring rather than repeating.
- **Tick**: only 14 files implement it, all actors or components with genuine per-frame work. No
  incidental per-frame cost in the controller or widgets.
- **The data-driven design is the project's real strength.** Weapons, items, sheet layout,
  conversations and impacts are all authored as JSON and reloadable. Findings 1 and 5 are about
  plumbing it properly, not replacing it.
- **`Items/`** has exactly the dependency shape the other six lanes should move toward.

## Recommended sequence

Ordered so each step lands in one build, and the cheap certainty comes before the expensive refactor.

1. **Package once, and see what breaks.** Until this is done, everything else rests on an untested
   assumption about how the game ships.
2. **Write three catalogue assertions.** Cheapest possible first test, aimed at the class of bug this
   project actually produces.
3. **Extract the panel router out of the player controller.** Mechanical; removes the largest single
   block of the god object and cuts `Core → UI` more than anything else available.
4. **Consolidate the thirteen JSON loaders.** After step 1, so path resolution is written once with
   packaging understood.
5. **Then split `BaseCharacter`.** The 2,249-line header is the rebuild tax, but it is also the
   riskiest change here and should follow the tests, not precede them.

---

Findings 2 and 3 restate, with numbers attached, a coupling `SESSION_LANES.md` had already
identified. Lane owners: nothing here is a request — it is a map for whoever picks a piece up.
