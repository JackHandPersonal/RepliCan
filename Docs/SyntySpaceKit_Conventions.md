# Synty POLYGON Sci-Fi Space: how the kit is meant to be assembled

Measured from `Demonstration_Interior.umap` (1376 static-mesh actors) and from the
mesh bounds. Full actor dump: `RawArt/demo_interior_actors.json`.
All numbers are centimetres, Unreal axes, pivot at the actor location.

## The grid

* Everything sits on a **500 cm grid**, at **scale 1.0** (no stretched walls anywhere
  in the demo), at **z = 0** for anything that stands on the floor.
* A floor tile (`SM_Bld_Floor_xx`) has its pivot at one corner and covers
  x 0..500, y 0..500 (top at z 0, slab down to −18).
* A ceiling tile (`SM_Bld_Ceiling_xx`) shares the floor pivot and covers z 400..449.
  The interior clear height is therefore **400**.
* Walls are **one-sided**: pivot at the left end, body runs **x 0..500, y 0..89**
  (Wall_01; 80..86 for Wall_02/05/06), z −100..400. The detailed (room-facing)
  side is +y; Wall_01's main face is at local y ≈ 75 with recess panels down to y 29.
  The 100 cm below the floor is a skirt for stacked decks; it is buried on a
  single-storey floor.
* A room is built with wall pivots **on the grid line**, bodies **inside** the
  floor tile they stand on. Two rooms that share a grid line get two walls
  back to back, one on each side of the line (total 178 thick).
* Corner pillars (`Wall_Corner_Pillar_01`, ~80 square; `_Wide_01`, ~150 square)
  have their pivot on the grid corner and grow into +x +y; they cap the joint
  where two walls meet and hide the 89 cm end of the wall body.

## Doors

The demo uses `SM_Bld_Wall_Doorframe_02` 15 times (13 with `Doorframe_Door_02`);
`Doorframe_03/04/06` are special (wide bay door, hangar-style, thin).

* A doorframe is a **wall-length piece centred on the grid line**: Doorframe_02
  runs x 0..500 and **y −43.6..+43.6**, z −100..400. It replaces a wall segment
  but, unlike a wall, straddles the line.
* The frame's face therefore sits ~31 cm **behind** the flanking walls' main
  face (75 − 43.6) on the room side. That recess is intended; with a back-to-back
  wall on the far side the frame is recessed the same amount from both rooms.
  **One frame serves both rooms.** The demo never stacks two frames.
* The leaf `Doorframe_Door_02` (one mesh, both halves, 261 wide × 297 tall × 19
  thick, centred on its pivot) goes at **frame pivot + (250, 0, 0), same yaw**.
  Every instance in the demo follows this.
* Flanking pieces at ±500 along the wall are ordinary walls at the same yaw,
  or a corner pillar / the perpendicular wall of the next room at the frame's
  pivot (rel (0,0), yaw ±90/180).
* Frames rotated 180° are just the same piece seen from the other room; nothing
  moves.

Door pieces and what they are for:

| Mesh | Footprint (x, y, z) | Notes |
|---|---|---|
| Doorframe_01 + Door_01 | 500, ±36, −100..400 | thin frame; single narrow leaf (169 wide) at x 3..172, pivot-hinged style |
| Doorframe_02 + Door_02 | 500, ±44, −100..400 | the standard interior door; one-piece double leaf, 261 × 297 |
| Doorframe_03 + Door_03 | 900 (x ±450), ±47 | wide bay door; leaf 711 × 391, placed at +450 z (demo) |
| Doorframe_04 + Door_04 | 500, y −58..302 | deep hangar-style; leaf offset (444, 70, 250) yaw −110 in the demo (hinged open) |
| Doorframe_05 + Door_L_05 / Door_R_05 | 500, ±62, −100..400 | the only frame with **split leaves**, each 187 wide × 244 tall, centred on its own pivot: made for a sliding animation |
| Doorframe_06 + Wall_Door_06 | 500, ±22.5 | thin frame; small hatch leaf 102 × 185 at x 199..301 |
| Doorframe_Outer_01 / _02 | 540 / 1040 wide, ±71 | a proud trim collar around a frame (single / double width) |
| Lift_Wall_Door_01/02 + Lift_Door_01/02 | leaf 98 wide × 235 | lift doors, two leaves, x 160..258 |
| Corridor_Single_Arch_01/02/03, Double_Arch_01/02 | 500 / 1000, ±61 | open arches centred on a grid line, used as corridor thresholds and junctions |

## Corridors and halls in the demo

* A hall is simply a run of floor tiles one or two cells wide with walls on both
  sides, on the same grid; there is no dedicated corridor floor.
* `Corridor_Single_Arch_01` is dropped on a grid line across the hall every few
  cells and at junctions, and often one cell in front of a doorframe
  (rel (−450, 0) or (0, 50) in the dump). It has no wall body, only the arch.
* Wall variety comes from mixing Wall_01/01_Alt/02/05/06 at the same pivots,
  and `Wall_Glass_04` / `Wall_Exterior_Window_02` where a window is wanted.
* Pillar props (`Prop_Pillar_Detail_01`, `Prop_Detail_Pipe_Pillar_01`) and
  `Wall_Pillar_02/05` are set on grid lines to break long runs.

## What our facility layout does differently (Tools/facility_layout.py)

* Walls are placed **outside** the room's floor area (pivot at −89) and the
  outer walls are stretched (scale 1.001 / 1.12) to close corner gaps. Synty
  keeps everything unstretched and lets the corner pillar hide the joint.
* Between the bay and the foyer we place **two** Doorframe_02 back to back
  (`Bay_DoorFrame_N` at y = 1544, `Foyer_DoorFrame_S` at y = 1634) each with
  its own leaf. Synty would put **one** frame on the shared line (y = 1589)
  with one leaf. The doubled frame is what makes the door read wrong against
  the wall: each frame stands 14 cm proud of its own room's wall face instead
  of 31 cm behind it.
* The foyer's exit door is a single frame set 44 into a single wall, so it
  stands proud there as well.

Recommendation for a working door: one `Doorframe_05` on the shared grid line
with `Door_L_05` / `Door_R_05` as separate actors (leaves slide apart along
the frame's x). See the measurements section below for the leaf rest positions.

## Measurements for placing a working door

Vertex-measured in the editor (`scratchpad/frame05.py`), frame pivot at (0,0,0), yaw 0.

| | Doorframe_02 | Doorframe_05 |
|---|---|---|
| Clear opening (x) | 144..356 (212 wide) | 126..374 (248 wide) |
| Lintel underside (z) | ~297 (leaf height) | ~237..243 |
| Frame thickness (y) | ±43.6 | ±62.1 |
| Leaf | Door_02, one mesh 261 × 297, centred on its pivot | Door_L_05 / Door_R_05, 187 × 244 each, centred on their own pivots (x ±93.6 / ±92.1, y ±21.1, z 0..244) |
| Closed leaf position | frame + (250, 0, 0) | L at frame + (156.4, 0, 0), R at frame + (342.1, 0, 0); they meet at x 250 and overlap each jamb by ~63 behind the frame |
| Open leaf position | (not made to open) | L slides to x ≈ −30 (rel −187), R to x ≈ 529 (rel +187): each leaf disappears into the flanking wall body |

For the bay/foyer wall the single frame goes on the shared line y = 1589
(`H + WALL_T`), x 250..750 (pivot x 250, yaw 0); leaves at (406.4, 1589, 0) and
(592.1, 1589, 0). The current `Bay_DoorFrame_N`, `Bay_DoorLeaves_N`,
`Foyer_DoorFrame_S`, `Foyer_DoorLeaves_S` would be retired through the
manifest's REMOVE set.

## The crew wing and the cafeteria (built 2026-09-14)

Everything east and west of the foyer follows the demo's convention exactly
(see `Tools/facility_layout.py`, the `tile` / `line_piece` / `wall_in` /
`pillar_in` helpers):

* Unstretched 500 tiles on two grids that start at the foyer's wall lines
  (x −89 west for the crew hall, x 1089 east for the cafeteria; the foyer's
  stretched floor and ceiling reach those lines, so the floors are continuous).
  Directions are the player's: coming north (+Y) out of the bay, Unreal's
  left-handed frame puts the RIGHT hand on −X, so "right" = west.
* Wall bodies **inside** the tile (pivot on the line, face 75 in), so floors
  and ceilings never need thresholds. Door frames and the crew modules
  straddle the line; where two such pieces meet at a corner they clip into
  each other's solid ends and need no pillar. `Corner_Pillar_01` only where
  two plain walls meet.
* The crew hall is 7 tiles (x −3589..−89, y 2178..2678); each long side is
  `[Crew_Blank 250] + 4 × [Doorframe_01 hatch door 500][Crew_Blank 250]`,
  i.e. cabins on a 750 pitch with a 250 dead band between them. Hall
  ceilings are yawed −90 so the light strip runs along the hall (y 2450.5);
  the fixtures are yawed 90 so their long axis follows the hall.
* A cabin is one tile: `Crew_Beds_01` on the far line, `Crew_Shower_01` +
  `Crew_Toilet_01` on the west line, `Crew_Blank_02` + `Crew_Desk_01` on the
  east line (the desk top is at z 89, 66 proud of the line), a locker, a
  footlocker, a stool and a small screen. Only cabin S1 ("room four": out
  of the foyer door, right, second door on the right = the south side,
  x −1589..−1089) is built; the other seven doors are locked
  `ASlidingDoorActor`s of kind Hatch.
* Hatch doors: `Doorframe_01` (opening x 169..352, 183 wide, ~290 tall) with
  the hinged `Doorframe_Door_01` leaf at frame + (169, 0, 128); it swings
  105° into the cabin (`SwingSign`).
* The cafeteria is 3 × 3 tiles (x 1089..2589, y 1678..3178) with
  `Crew_Kitchen_01` (pivot at its right-hand end, x −528..−28) in the middle
  of the north line, Work_Bench counters, Space tables and stools, Worlds and
  Cyber City vending machines / fridge / shelf (those keep their own
  materials: `mat=False`).

### Doorframe_01 and its leaf do not match

`SM_Bld_Wall_Doorframe_Door_01` is 169 × 256 but the frame's portal (vertex
measured at every depth) is 183 wide × 291 tall, so placed at the hinge the
leaf leaves a 14 cm slot beside it and 35 cm above. The demo never places
this leaf at all (its four `Doorframe_06` frames carry `Wall_Door_01`, the
102 × 185 hatch, at +200). `ASlidingDoorActor` (kind Hatch) therefore scales
the leaf by 183/169 × 291/256 to fill the portal.

## Floors, window walls, door-top bulkheads, cameras (2026-09-14)

- Floor tiles: the kit has no plain tile. 01-03/08 carry hazard bands, 04-06/011 cable strips,
  07 a hatch. Rooms use `SM_Bld_Floor_09` (octagonal plate) at yaw 0 everywhere so edges meet;
  the hall uses `SM_Bld_Floor_011` at yaw 90 (pivot at the tile's +x corner) so its strips run
  along the hall. `ROOM_FLOOR` / `HALL_FLOOR` in Tools/facility_layout.py.
- Window wall: `SM_Bld_Wall_04` + `SM_Bld_Wall_Glass_04` (mat=False, translucent) placed on
  both sides of a shared line with mirrored yaw (foyer side 90, cafeteria side -90) line up
  exactly (glass local x 79..416, z 80..252). Later wall loops must skip the label or they
  override it (the cafeteria's `Caf_Wall_W%d` loop now skips j 0 and 1).
- Door-top gap: wide doors stand 400 tall, `SM_Bld_Ceiling_01` spans z 400..449 with raised
  channels (underside 420) crossing the door line. `Door_Fill_*`: an engine cube 500 x 80 x 53
  at z 422.5 in `MI_Bulkhead` (flat grey) fills it. `M_FlatColor` is an empty material; flat
  colours come from `M_Flat` (Color / Roughness params): MI_Bulkhead, MI_BlankBoard, MI_DrabTable.
- Backboards: `backboard(label, fx, fy, nx, ny)` = a black engine-cube slab 235 x 145 x 4 (M_Black)
  against a traced flat wall face + a 215 x 133 blank plane 5 cm proud (MI_BlankBoard); foyer faces:
  west x -37.2, north y 2715.2, south y 1640.8. (The Greeble_Panel plate reads as a hazard sign
  and stood 21 cm proud on a flat wall; it only works sunk in a recess, as in the bay.)
- Security cameras: Cyber City's four-part CCTV set assembled exactly as the pack's own demo
  maps do it (see Docs/PolygonCyberCity_DemoAssemblies.md, generated by Tools/demo_assemblies.py):
  plate on the wall face at z 345 (stub along the wall normal), Arm_01 (down link) at plate-local
  (-1.4, 10.7, -1.1), Arm_02 (straight link) at (-1.4, 34.7, -21.8) rolled 19, the pod at
  (-1.4, 67.1, -26.7) rolled 30 nose-down and yawed phi into the room. `cctv(label, fx, fy, psi, phi)`.
  All parts in MI_CCTV_Dark (Cyber City palette 03_B, navy with an orange lens, EmissiveMultiply
  0.25). The foyer's NE corner pillar stands 43 proud, so that camera sits 150 in. Guessing the
  rig from bounds and ortho renders failed three times; read the demo map.
- Cafeteria palette: `place()` swaps labels starting `Caf_` to `M_PolygonSciFiSpace_01_A`, the
  same atlas as M_Facility_Grime without its dirt overlay (grime = 01_A + T_PolygonSciFiSpace_Dirt_01).
- Signs (ASignActor): face plane MakeFromXZ((0,-1,0), (1,0,0)) so U runs left-to-right for a
  viewer, rows drawn top-down; 41 characters = 246 LED columns x 7 rows across the 370 face.
  Static text only (no marquee yet). Build.bat with UBA compiles a one-file change in ~4 s and
  still prints a fresh DLL timestamp, so a "3 second build" is a real build.
- Layout REVISE: new label families must be added to the REVISE prefix set or a re-run keeps the
  first-spawned transforms (CCTV_, Foyer_Board_, Door_Fill_ were caught this way); revise() now
  also re-applies scale and the material override.
- Line traces from editor python (`SystemLibrary.line_trace_single`, ECC_VISIBILITY) return the
  start point when they begin inside geometry; probe from open air.
- Inspectable polygons on kit geometry: cut them out (Tools/cut_feature.py, cube-seeded,
  connectivity-grown) into /Game/RepliCan/Cut/SM_<Wall>_<Feature> plus a *_No<Feature> wall
  copy; the piece is placed at the wall's transform with name:/desc:/action: tags and gets
  complex-as-simple collision (the duplicated asset otherwise keeps the whole wall's convex hull
  and the entire wall answers the inspect trace). First one: Cabin_S1_Blank_Button, the left
  button box of the cabin's east Crew_Blank_02.
