# Demo_Scavenger -- how this map is assembled

Scanned by `Tools/scan_pack_layout.py` from the map as the pack ships it. Every number here is
measured from Synty's own placement, not chosen by us.

- components: **2746**, distinct meshes: **435**, families: **218**
- commonest floor z: **0**
- building grid: **x none found** (0% of pieces on it), **y none found** (0%)

## Families, mounting height and rotation

`mount` is the median z above the floor plane: ~0 stands on the floor, a metre or two is
wall-mounted or waist-height, above that is ceiling or upper storey. `yaws` shows whether a
family is axis-aligned (only 0/90/180/270) or placed freely -- free rotation means the pack
does not expect it to tile.

| family | n | mount z | yaws | longest run | spacing |
|---|---|---|---|---|---|
| `SM_Prop_Scav_Scrap` | 422 | 299 | free | 2 | 14723 |
| `SM_Env_Grass` | 248 | 5 | free | 2 | 2823 |
| `SM_Prop_Crate` | 110 | 4 | free | 3 | 245 |
| `SM_Env_Plant_Balloon_Group` | 102 | 13 | free | 2 | 7930 |
| `SM_Env_Ground` | 78 | 0 | free | - | - |
| `SM_Env_Plant_Grass_Flower` | 70 | 5 | free | - | - |
| `SM_Env_Road_Corner` | 62 | -38 | free | 2 | 5654 |
| `SM_Env_Plant_Fern` | 60 | 17 | free | - | - |
| `SM_Prop_Scav_Scrap_Pile` | 57 | -58 | free | - | - |
| `SM_Env_Ground_Junk` | 54 | -27 | free | - | - |
| `SM_Bld_Railing_Pillar` | 52 | 1460 | free | 4 | 288 |
| `SM_Env_Plant_Small` | 52 | 4 | free | - | - |
| `SM_Bld_Railing` | 47 | 1460 | [90, 180, 270] | 5 | 288 |
| `SM_Env_Plant_Palm` | 43 | -5 | free | - | - |
| `SM_Env_Cliff_Flat` | 32 | -230 | free | - | - |
| `SM_Env_Plant_Spikey` | 31 | -5 | free | - | - |
| `SM_Env_Plant_Balloon` | 30 | 8 | free | 2 | 88 |
| `SM_Bld_Light` | 28 | 1520 | [0, 90, 180] | 2 | 1722 |
| `SM_Env_Plant_Tall` | 28 | 5 | free | - | - |
| `SM_Bld_Scav_Refinery_Silo` | 26 | 0 | [0, 90, 180] | 2 | 419 |
| `SM_Env_Rock` | 26 | -175 | free | - | - |
| `SM_Bld_Greeble` | 25 | 1271 | [0, 90, 180, 270] | 2 | 136 |
| `SM_Bld_Scav_Refinery_Pipe` | 24 | 588 | [0, 90, 180] | 2 | 50 |
| `SM_Env_Detail_Crater_Fill` | 24 | -82 | free | - | - |
| `SM_Env_Road_Straight` | 21 | -40 | free | - | - |
| `SM_Env_Detail_Crater` | 21 | 30 | free | - | - |
| `SM_Bld_Scav_Refinery_Smoke_Stack` | 21 | 0 | free | 2 | 657 |
| `SM_Prop_Crate_08_Lid` | 21 | 336 | free | 3 | 245 |
| `SM_Prop_Drill_03_Extension` | 21 | 11 | free | - | - |
| `SM_Prop_Drill_03_Leg` | 21 | 84 | free | - | - |
| `SM_Prop_Drill_03_Leg_01_Foot` | 21 | 24 | free | - | - |
| `SM_Prop_Tether_Wire` | 20 | 428 | free | - | - |
| `SM_Prop_Drill_01_Hinge` | 20 | 1199 | free | - | - |
| `SM_Prop_Crate_07_Lid` | 20 | 365 | free | 2 | 255 |
| `SM_Prop_Drill` | 19 | 5 | free | - | - |
| `SM_Env_LakeEdge_Corner` | 18 | 33 | free | - | - |
| `SM_Prop_Drill_02_Drill_Head_01_Cone` | 18 | 49 | free | 2 | 3 |
| `SM_Prop_Drill_02_Extension` | 18 | 151 | free | - | - |
| `SM_Prop_Drill_02_Leg_01_Foot` | 18 | 24 | free | - | - |
| `SM_Prop_Drill_02_Leg` | 18 | 84 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe_02_Connector` | 17 | 588 | [0, 90, 270] | 4 | 1012 |
| `SM_Bld_Scav_Refinery_Pipe_01_Bend` | 17 | 150 | [0, 90, 180, 270] | 3 | 250 |
| `SM_Prop_Crate_02_Lid` | 17 | 236 | free | 3 | 212 |
| `SM_Prop_Chair` | 16 | 1061 | free | - | - |
| `SM_Prop_Tether_Pole_Tall` | 16 | 0 | free | - | - |
| `SM_Prop_Drill_01_Drill_Head_01_Cone` | 15 | 214 | free | - | - |
| `SM_Prop_Monitor` | 15 | 1623 | free | 2 | 412 |
| `SM_Env_Road_End` | 14 | -38 | free | - | - |
| `SM_Prop_Crate_11_Lid` | 14 | 122 | free | 2 | 84 |
| `SM_Env_Plant_Shrub` | 14 | 5 | free | - | - |
| `SM_Env_Plant_Hallowed` | 13 | 6 | free | - | - |
| `SM_Prop_Vendor_Product` | 12 | 1151 | free | 2 | 40 |
| `SM_Prop_Crate_09_Lid` | 11 | 180 | free | 2 | 96 |
| `SM_Env_Plant_Fester` | 10 | 13 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe_01_Pillar` | 10 | 203 | [0] | 2 | 1200 |
| `SM_Prop_Monitor_02_Screen` | 10 | 1676 | free | 2 | 41 |
| `SM_Prop_Crate_Lid` | 9 | 241 | free | - | - |
| `SM_Prop_Locker_01_Shelf` | 9 | 216 | [0, 270] | 2 | 73 |
| `SM_Prop_Locker_01_Door` | 9 | 173 | free | 2 | 73 |
| `SM_Prop_Locker` | 9 | 64 | [0, 270] | 2 | 73 |
| `SM_Bld_Scav_Refinery_Pipe_02_Half` | 9 | 588 | [0, 90, 270] | - | - |
| `SM_Env_Detail_Platform` | 8 | 0 | free | - | - |
| `SM_Prop_Chair_04_Top` | 8 | 234 | free | - | - |
| `SM_Bld_Greeble_22_Fan` | 8 | 1408 | [0, 90, 180] | - | - |
| `SM_Env_Cracked_Edge` | 8 | 41 | free | - | - |
| `SM_Prop_Crate_14_Lid` | 8 | 271 | free | - | - |
| `SM_Bld_Scav_Refinery` | 7 | 0 | [0, 180] | - | - |
| `SM_Bld_Scav_Wreckage` | 7 | -190 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe_01_Connector` | 7 | 150 | [0, 90, 270] | - | - |
| `SM_Prop_Crate_12_Lid` | 7 | 120 | free | - | - |
| `SM_Prop_Canopy_Preset` | 7 | 5 | free | - | - |
| `SM_Prop_Tether_Pole` | 7 | 13 | free | - | - |
| `SM_Prop_Drill_03_Motor` | 7 | 267 | free | - | - |
| `SM_Prop_Drill_03_Drill_Head` | 7 | -11 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe_01_Half` | 6 | 150 | [0] | - | - |
| `SM_Prop_Drill_02_Drill_Head` | 6 | 88 | free | - | - |
| `SM_Prop_Drill_02_Motor` | 6 | 342 | free | - | - |
| `SM_Prop_Tether_Wire_Tilt` | 6 | 15 | free | - | - |
| `SM_Prop_Cable` | 6 | 903 | free | 2 | 404 |
| `SM_Env_Plant_Cactus` | 6 | 168 | free | - | - |
| `SM_Prop_Table_Medium` | 6 | 1561 | free | - | - |
| `SM_Prop_Crate_05_Lid` | 6 | 190 | free | - | - |
| `SM_Prop_Monitor_04_Arm` | 6 | 736 | free | - | - |
| `SM_Prop_Drill_01_Drill_Head` | 5 | 346 | free | - | - |
| `SM_Prop_Drill_01_Hinge_04_Leg_01_Foot` | 5 | 246 | free | - | - |
| `SM_Prop_Drill_01_Hinge_04_Leg` | 5 | 914 | free | - | - |
| `SM_Prop_Drill_01_Hinge_03_Leg_01_Foot` | 5 | 246 | free | - | - |
| `SM_Prop_Drill_01_Hinge_03_Leg` | 5 | 914 | free | - | - |
| `SM_Prop_Drill_01_Hinge_02_Leg_01_Foot` | 5 | 246 | free | - | - |
| `SM_Prop_Drill_01_Hinge_02_Leg` | 5 | 914 | free | - | - |
| `SM_Prop_Drill_01_Hinge_01_Leg_01_Foot` | 5 | 246 | free | - | - |
| `SM_Prop_Drill_01_Hinge_01_Leg` | 5 | 914 | free | - | - |
| `SM_Prop_Drill_01_Extension` | 5 | 678 | free | - | - |
| `SM_Prop_Crate_10_Lid` | 5 | 276 | [0] | 2 | 794 |
| `SM_Bld_Scav_Refinery_Pipe_02_Bend` | 5 | 588 | [0, 90, 180] | - | - |
| `SM_Prop_Crate_06_Lid` | 5 | 159 | free | - | - |
| `SM_Prop_Cardboard_Box` | 5 | 1621 | free | - | - |
| `SM_Prop_Drill_01_Motor` | 5 | 1432 | free | - | - |
| `SM_Prop_PowerCell` | 4 | 241 | free | 2 | 47 |
| `SM_Prop_Crate_13_Lid` | 4 | 125 | [0] | - | - |
| `SM_Bld_Door_Double` | 4 | 67 | [0, 90] | - | - |
| `SM_Bld_Door_Double_05_Door` | 4 | 363 | [0] | - | - |
| `SM_Prop_Crate_04_Lid` | 4 | 908 | [0, 180] | - | - |
| `SM_Bld_Scav_Refinery_Pipe_02_Pillar` | 4 | 590 | [0, 270] | - | - |
| `SM_Prop_Vending_Machine_04_Arm` | 4 | 1304 | [180, 270] | - | - |
| `SM_Env_Plant_Pinecone` | 4 | 23 | [0] | - | - |
| `SM_Bld_Door_Garage` | 4 | 65 | [0] | - | - |
| `SM_Bld_Scav_Refinery_Pipe_Coupling` | 3 | 325 | [0, 180] | - | - |
| `SM_Bld_Elevator_Hover_Single` | 3 | 169 | [0] | - | - |
| `SM_Prop_Vending_Machine` | 3 | 187 | [180, 270] | - | - |
| `SM_Bld_Door_Single` | 3 | 153 | [0, 90] | - | - |
| `SM_Prop_Wall_Unit_01_Door` | 3 | 414 | [0, 270] | - | - |
| `SM_Prop_Wall_Unit` | 3 | 187 | [0, 270] | - | - |
| `SM_Prop_Vendor_Pan` | 3 | 263 | free | - | - |
| `SM_Env_Blob` | 3 | 4 | free | - | - |
| `SM_Bld_Greeble_27_Crusher` | 3 | 234 | [180, 270] | - | - |
| `SM_Prop_TrashBin_01_Lid` | 3 | 340 | [0] | - | - |
| `SM_Prop_TrashBin` | 3 | 187 | [0] | - | - |
| `SM_Env_Plant_Coral` | 3 | 7 | free | - | - |
| `SM_FX_Skydome_Planet` | 3 | 342813 | free | - | - |
| `SM_Bld_Scav_Dish_Background_01_Top` | 3 | 5665 | free | - | - |
| `SM_Bld_Scav_Dish_Background` | 3 | 560 | free | - | - |
| `SM_Bld_Door_Garage_01_Door` | 3 | -352 | [0] | - | - |

## Interior dressing

Props within 1200 cm of a SHELL piece -- a wall, a corridor tube or a pod body -- by how far off
it they sit. That gap is what decides whether a thing reads as against the wall or adrift in
the room, and it is the number a replication has to match.

Matching only `SM_Bld*Wall*` produced an empty table on an exterior demo, which is correct and
useless: this pack builds its interiors out of corridor tubes and pod shells, not wall panels.

| prop family | n | median gap to nearest wall | median z |
|---|---|---|---|
