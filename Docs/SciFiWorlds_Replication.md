# Replicating PolygonSciFiWorlds: how the pack's own artists build with it

Measured from all four demo maps on 2026-09-19 -- `Demo_Explorer` (2,789 components), `Demo_Corporation`
(3,346), `Demo_Scavenger` (2,746), `Demo_BlackMarket` (1,742) --
plus the multi-part recipes in `PolygonSciFiWorlds_DemoAssemblies.{md,json}`. Regenerate with:

    Tools/ue_remote.py --file Tools/scan_pack_layout.py     # open the map first; reads, never writes
    Tools/ue_remote.py --file Tools/demo_assemblies.py      # PACK = 'PolygonSciFiWorlds'

Everything below is a measurement of Synty's placement, not a preference of ours.

---

## 1. No WORLD grid -- but real modules, measured relative to each other

This section said "there is no grid, do not assume one" after scanning `Demo_Explorer` alone. That
was measured correctly and concluded wrongly, and scanning the other three demos showed how.

**Both things are true and they are not in conflict:**

- **Nothing sits on an absolute world lattice.** Measured in `Demo_BlackMarket`: of 105
  `Platform_Large` pieces, **0 land on a 500 lattice and 0 on a 250 lattice**. Same for all five
  platform families. No candidate modulus fits the map as a whole -- best was 125 cm over 38% of
  pieces with an offset allowed, i.e. noise.
- **But the pieces step from EACH OTHER in clean modules.** The commonest neighbour gaps are exactly
  500 and 250, and whole runs of them appear:

| system | module | seen in | longest run |
|---|---|---|---|
| `Platform_Large` / `Platform_Base_Large` | **500** | BlackMarket, Corporation | 13 |
| `Platform_Small` / `Platform_Base_Small` | **250** | BlackMarket | 6 |
| `Bridge` / `Bridge_Rail` | **1000** | Corporation | 19 |
| `Pod_Corridor_*` | **1000** on a z 200 deck | Explorer only | 3 |
| `Scav_Refinery_Pipe` | **~1000** | Corporation, Scavenger | 18 |
| `Railing` / `Railing_Pillar` | **288** | Scavenger | 5 |

**So the rule is: build each structure on its own module, and place the structure itself freely.**
A deck steps 500 from the deck beside it; where that deck sits in the world is arbitrary. Snapping
everything to a world grid is what fights the art -- not the module size.

The original advice ("do not carry the facility's 500 across") was wrong in a way worth naming: the
module genuinely IS 500 for platforms, the same number our facility uses. What does not carry across
is the world-aligned lattice, not the size. One demo was not enough to tell those apart.

## 2. Corridors: the pack's one tiling system

| property | measured |
|---|---|
| deck height | **z 200** — every tube, door and connector |
| module | **1000 cm** between segment centres |
| connector spacing | **962 cm** — collars sit ~38 off the segment centre |
| rotation | **multiples of 90 only** (the sole axis-aligned family) |
| legs | **z 0**, offset ~±345 in the run direction from their support |
| ramps | span z 0 → 200, the transition to ground |

Two tube families that do not interchange: `Corridor_Square_*` and `Corridor_Round_*`. Each has its
own `_Connector`, `_End`, `_Support`, `_Ramp`, `_Door_Single`, `_Door_Double` and `_Glass`.

A straight run reads, in order: `End` → `Connector` → `Square_NN` → `Connector` → `Square_NN` → …
Connectors are not optional decoration; the segments do not meet without them.

**Corridors are how this pack makes interiors.** It has almost no interior wall panels — matching
props against `SM_Bld*Wall*` produced an empty table. The tube *is* the room.

## 3. Glass is always a separate mesh, usually co-located

**Measured: 28 glass placements sit at exactly their solid piece's transform, 20 do not.** So the
rule is "separate mesh, check the offset", never "assume co-located".

The offsets are meaningful rather than sloppy:

- `Pod_Research_03_Glass_01` sits **442 above** its solid — it is the dome cap, not a window.
- `Pod_Research_05_Dome_01_Glass_01` is co-located with the frame; `_Glass_02` is **721 higher**.
  They are the lower band and the upper cap of one dome, and you need both.
- `Corridor_Square_05_Glass` is **+497 y, +198 z** off its tube — a skylight along the top.

Dropping a `_Dome` or `_Corridor` mesh alone gets you a frame with no glazing, and it will look
like a bug rather than an unfinished asset.

## 4. Multi-part props: 161 assemblies, and the big ones are big

`demo_assemblies.json` has the exact root-local transforms. The dome from the store art is the
worked example — **`SM_Bld_Pod_Research_05`, sixteen pieces**:

    Pod_Research_05                 z  609   orange base ring
      _Dome_01                      z 1577   white geodesic frame
      _Dome_01_Glass_01             z 1578   lower glazing, co-located with the frame
      _Dome_01_Glass_02             z 2299   upper cap
      _Leg_01 … _Leg_08             z  250   eight legs on a ~1730 radius ring
      _Light_01 … _Light_04         z  820   four fixtures at N/S/E/W

~23 m to the top of the cap. Siblings `Pod_Research_01`–`_06` are the coloured modules; `_07` adds
`_Dish` and `_Satellite` variants.

**The Overview map will mislead you on these.** It is a catalogue grid, so the base ring sat 7.3 m
from its own dome and the two glass caps were stacked on one spot. Take assembly from a *Demo* map.

## 5. Mounting heights, and what they tell you

`Docs/Demo_Explorer_Layout.md` has the full table. The z a family is placed at separates its role
without anyone labelling it:

- **~0** — floor-standing: `PowerGenerator` (−16), `TrashBin` (1), `Cable` (9)
- **~200** — the corridor deck, and everything dressed on it: `Sofa`, `Wall_Unit`, `BatteryPack`,
  `Vending_Machine` (197), `Bench_Seat` (206)
- **300–500** — waist and wall height: `Bed_Double` (399), `Wall_Unit_01_Door` (427),
  `Bed_Double_01_Blinds` (479)
- **700+** — mounted high or on structure: `Antenna_05_Leg` (744)

`z 200` doing double duty as both the corridor deck and the height most interior props sit at is the
single most useful number in this document.

## 6. Interior dressing: how far off the shell things sit

Median gap from each prop family to the nearest shell piece (wall, corridor tube or pod body).
Under ~300 reads as against the wall; 600+ is standing out in the room.

| prop | gap | z |
|---|---|---|
| `Bed_Double` | 87 | 399 |
| `Bed_Double_01_Blinds` | 162 | 479 |
| `Sofa` | 285 | 200 |
| `Vending_Machine` | 296 | 197 |
| `Wall_Unit` | 309 | 200 |
| `PowerGenerator` | 362 | −16 |
| `TrashBin` | 380 | 1 |
| `Bench_Seat` | 588 | 206 |
| `Crate_04_Lid` | 701 | 74 |

Beds hug the shell at under a metre; seating sits a couple of metres off it; crates are scattered
mid-floor. That ordering is the dressing grammar, and it is what makes a replicated room read as
this pack rather than ours.

---

## 7. The two demos build completely differently -- pick the right one to copy

Scanning `Demo_Corporation` as well turned the corridor findings from "how this pack works" into
"how one of its two idioms works". They share the pack and almost nothing else.

| | Explorer | Corporation | Scavenger | BlackMarket |
|---|---|---|---|---|
| components | 2,789 | 3,346 | 2,746 | 1,742 |
| storeys | one (z 0) | **multi-storey** (-3000..-750) | one (z 0) | one (z -242) |
| corridor pieces | **46** | 0 | 0 | 0 |
| its module | corridors 1000 @ z200 | bridges 1000, platforms 500 | railings 288, pipes 1000 | platforms 500 / 250 |
| world grid | none | none | none | none |
| median prop gap | 253 | 737 | 246 | 448 |
| monitors at | z 400 | z **1120-1347** | - | - |

**Explorer** is an outpost: single-storey pods linked by corridor tubes, tightly dressed, everything
within a few metres of a shell. **Corporation** is a compound: multi-storey buildings inside a walled
perimeter, rooms twice as deep, screens mounted high on walls rather than sat on desks.

So the corridor system in section 2 is *Explorer's* idiom, not the pack's. If the room being built is
a facility interior, Explorer is the model. If it is a site with buildings in it, Corporation is,
and the thing to copy is the 2000 wall module rather than the 1000 corridor one.

**All four maps: no WORLD grid, but each system has its own module** -- see section 1, which the
other three demos forced a rewrite of. Corridors are Explorer's alone (46 pieces there, 0 in the
other three); platforms at 500/250 appear in BlackMarket and Corporation; bridges at 1000 in
Corporation; railings at 288 in Scavenger.

### A measurement trap this exposed

The first Corporation scan reported a bed at "median z -1962" against a floor at -58. That is not a
mounting height, it is the gap between two storeys: the scan was measuring every prop against one
global floor plane, which only works on a single-storey map. Heights are now taken from the nearest
floor piece *under* each prop, and the tool says **MULTI-STOREY** in its header when it finds floors
in more than two bands. Corrected, the same beds sit at z 62 and the lockers at 50.

Worth remembering generally: a single datum is an assumption, and it is invisible until a map breaks
it by being taller than one room.

---

## Applying this to RepliCan

1. **Do not grid-snap Sci-Fi Worlds pieces.** Place them freely; only corridors tile, at 1000 on a
   z 200 deck with 90° yaws.
2. **Always place the `_Glass` sibling**, and take its offset from `demo_assemblies.json` rather
   than assuming zero.
3. **Take multi-part transforms from a Demo map, never Overview.**
4. **Dress to the measured gaps above**, not to eye — that is what our own rooms have tended to get
   wrong, and it is cheap to get right.

Related: `skill_synty_demo_assemblies` and `skill_synty_demo_map_conventions` in memory;
`Docs/Demo_Explorer_Layout.md` for the full family table.
