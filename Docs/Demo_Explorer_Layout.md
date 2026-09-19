# Demo_Explorer -- how this map is assembled

Scanned by `Tools/scan_pack_layout.py` from the map as the pack ships it. Every number here is
measured from Synty's own placement, not chosen by us.

- components: **2789**, distinct meshes: **502**, families: **255**
- floor plane sits at z **0** (the commonest z among Floor/Ground pieces)
- building grid: **x none found** (0% of pieces on it), **y none found** (0%)

## Families, mounting height and rotation

`mount` is the median z above the floor plane: ~0 stands on the floor, a metre or two is
wall-mounted or waist-height, above that is ceiling or upper storey. `yaws` shows whether a
family is axis-aligned (only 0/90/180/270) or placed freely -- free rotation means the pack
does not expect it to tile.

| family | n | mount z | yaws | longest run | spacing |
|---|---|---|---|---|---|
| `SM_Prop_Crate` | 165 | 76 | free | 3 | 125 |
| `SM_Env_Rock` | 160 | 157 | free | 2 | 15403 |
| `SM_Env_Plant_Small` | 133 | 159 | free | 2 | 10986 |
| `SM_Env_Grass` | 124 | 20 | free | 2 | 6846 |
| `SM_Env_Plant_Coral` | 102 | 406 | free | 2 | 17650 |
| `SM_Bld_Greeble` | 93 | 350 | free | 3 | 75 |
| `SM_Env_Cliff_Curved_Large` | 84 | -227 | free | - | - |
| `SM_Env_Plant_Kelp` | 80 | 497 | free | 2 | 6422 |
| `SM_Env_Plant_Shrub` | 77 | 19 | free | 2 | 218 |
| `SM_Env_Ground` | 53 | 196 | free | - | - |
| `SM_Env_Plant_Fester` | 51 | 1199 | free | - | - |
| `SM_Env_Road_Corner` | 48 | 175 | free | - | - |
| `SM_Env_Plant_Large_Spiral_Preset` | 46 | 1243 | free | - | - |
| `SM_Prop_Crate_07_Lid` | 38 | 208 | free | 2 | 6850 |
| `SM_Bld_Pod_Support` | 36 | 150 | [0, 90, 180, 270] | 3 | 625 |
| `SM_Env_Ground_Slope` | 33 | 0 | free | 2 | 1396 |
| `SM_Env_Plant_Succulent_Large` | 30 | 548 | free | - | - |
| `SM_Prop_Crate_Lid` | 30 | 201 | free | 2 | 106 |
| `SM_Env_Plant_Flower_Large` | 29 | 842 | free | - | - |
| `SM_Prop_PowerCell` | 29 | 19 | free | 2 | 89 |
| `SM_Bld_Light` | 29 | 634 | [0, 90, 270] | 2 | 628 |
| `SM_Prop_Hydroponic_Plant` | 28 | 219 | free | 5 | 131 |
| `SM_Prop_Crate_08_Lid` | 27 | 171 | free | 3 | 125 |
| `SM_Prop_Satellite` | 24 | 200 | free | 2 | 1350 |
| `SM_Prop_Monitor` | 24 | 421 | free | 2 | 1554 |
| `SM_Prop_Cable` | 23 | 19 | free | 2 | 7 |
| `SM_Env_Plant_Flower` | 21 | 1298 | free | 2 | 190 |
| `SM_Prop_Antenna` | 20 | 693 | free | 2 | 1012 |
| `SM_Bld_Door_Single` | 20 | 200 | free | 2 | 1600 |
| `SM_Prop_Crate_10_Lid` | 19 | 94 | free | 3 | 542 |
| `SM_Env_Detail_Crater_Spike` | 18 | 1843 | free | - | - |
| `SM_Prop_Satellite_08_CrateArm` | 18 | 245 | free | 2 | 77 |
| `SM_Prop_SolarPanel_01_Panel` | 18 | 289 | free | - | - |
| `SM_Prop_Tether_Pole_Tall` | 17 | 0 | free | - | - |
| `SM_Prop_Hydroponic_Planter` | 17 | 197 | [0, 90] | 3 | 338 |
| `SM_Prop_Chair` | 16 | 199 | free | 2 | 219 |
| `SM_Bld_Pod_Support_03_Leg` | 16 | -6 | [0] | 3 | 625 |
| `SM_Env_Plant_Fern` | 16 | 1670 | free | - | - |
| `SM_Prop_Crate_01_Lid` | 16 | 402 | free | 2 | 107 |
| `SM_Prop_Satellite_07_DishArm` | 16 | 399 | free | 3 | 25 |
| `SM_Prop_Satellite_07_Leg` | 16 | 222 | free | 3 | 33 |
| `SM_Bld_Pod_Ramp` | 15 | 200 | [0, 90, 180, 270] | 2 | 1550 |
| `SM_Prop_Satellite_06_Dish` | 15 | 284 | free | - | - |
| `SM_Prop_Tether_Wire` | 14 | 267 | free | - | - |
| `SM_Prop_Crate_14_Lid` | 14 | 216 | free | - | - |
| `SM_Bld_Pod_Silo` | 14 | 31 | free | - | - |
| `SM_Env_Floating_Rocks` | 14 | 2850 | free | - | - |
| `SM_Env_Rock_Large` | 14 | 232 | free | - | - |
| `SM_Prop_PowerCellCharger` | 13 | 0 | free | - | - |
| `SM_Prop_PowerGenerator` | 13 | 0 | [0, 90, 180] | - | - |
| `SM_Bld_Door_Double` | 13 | 200 | [0, 90, 180, 270] | 2 | 2200 |
| `SM_Env_Crater_Edge` | 12 | 1671 | free | - | - |
| `SM_Bld_Pod_Support_01_Leg` | 12 | 12 | [0, 90, 180, 270] | - | - |
| `SM_Prop_Satellite_08_Lid` | 12 | 270 | free | - | - |
| `SM_Prop_Crate_04_Lid` | 12 | 265 | free | - | - |
| `SM_Env_Cliff_Curved` | 12 | 190 | free | 2 | 233 |
| `SM_Bld_Door_Single_04_Door` | 12 | 203 | [0, 90, 270] | 3 | 97 |
| `SM_Env_Cliff_Flat` | 12 | -643 | free | - | - |
| `SM_Bld_Pod_Corridor_Square_End` | 11 | 200 | free | 2 | 3000 |
| `SM_Bld_Door_Single_02_Door` | 10 | 367 | free | - | - |
| `SM_Bld_Pod_Base` | 10 | 89 | [0, 90, 180, 270] | 2 | 5500 |
| `SM_Prop_SolarPanel` | 10 | 0 | free | - | - |
| `SM_Bld_Pod_Research` | 9 | 200 | free | 2 | 4700 |
| `SM_Prop_Pod_Pipe_01_Connector` | 9 | 162 | [0] | 3 | 648 |
| `SM_Prop_Crate_02_Lid` | 9 | 102 | free | - | - |
| `SM_Env_Artifact_AlienRuin` | 9 | 1013 | free | - | - |
| `SM_Prop_Antenna_05_Leg` | 9 | 650 | free | - | - |
| `SM_Bld_Pod_Corridor_Square` | 9 | 200 | [0, 90, 180] | 2 | 1000 |
| `SM_Env_Plant_Bulb` | 9 | 566 | free | - | - |
| `SM_Prop_Drill_02_Extension` | 9 | 125 | free | - | - |
| `SM_Prop_Drill_02_Drill_Head_01_Cone` | 9 | 2 | free | 2 | 928 |
| `SM_Bld_Pod_Support_02_Leg` | 8 | 0 | [90, 270] | - | - |
| `SM_Prop_Crate_15_Lid` | 8 | 346 | free | 2 | 7372 |
| `SM_Prop_Hydroponic_Planter_02_Insert` | 8 | 196 | [90] | 3 | 522 |
| `SM_Bld_Pod_Corridor_Square_Connector` | 8 | 200 | [90, 180] | 3 | 962 |
| `SM_Env_Plant_Large_Spiral` | 8 | 1313 | free | - | - |
| `SM_Prop_Sofa` | 8 | 200 | [0, 90, 270] | - | - |
| `SM_Bld_Pod_Air_Lock` | 8 | 200 | [0, 90, 180, 270] | 2 | 2100 |
| `SM_Prop_BatteryPack` | 8 | 200 | free | - | - |
| `SM_Bld_Pod_Research_05_Leg` | 8 | -11 | [0] | - | - |
| `SM_Bld_Corp_Platform_01_LowerLeg` | 8 | -0 | [90] | 2 | 354 |
| `SM_Env_Floating_Rocks_Spiral` | 7 | 190 | free | - | - |
| `SM_Prop_TrashBin_01_Lid` | 7 | 154 | [0, 90, 270] | - | - |
| `SM_Prop_TrashBin` | 7 | 1 | [0, 90, 270] | - | - |
| `SM_Bld_Door_Single_01_Door` | 7 | 200 | [0, 90, 180, 270] | - | - |
| `SM_Prop_Crate_06_Lid` | 7 | 156 | free | - | - |
| `SM_Env_Road_Straight` | 7 | 123 | free | - | - |
| `SM_Prop_PowerGenerator_03_Gear` | 7 | 83 | [0, 90, 180] | - | - |
| `SM_Env_Plant_Tall` | 7 | 235 | free | - | - |
| `SM_Env_Road_End` | 6 | 190 | free | - | - |
| `SM_Bld_Pod_Corridor_Square_End_01_Glass` | 6 | 200 | [0, 180, 270] | - | - |
| `SM_Prop_Satellite_08_CrateDish` | 6 | 253 | free | - | - |
| `SM_Prop_Satellite_08_CrateSwivel` | 6 | 213 | free | - | - |
| `SM_Prop_Wall_Unit_01_Door` | 6 | 427 | free | 2 | 6964 |
| `SM_Prop_Wall_Unit` | 6 | 200 | free | 2 | 6969 |
| `SM_Prop_Crate_13_Lid` | 6 | 121 | free | - | - |
| `SM_Prop_Bed_Double_01_Blinds` | 6 | 479 | [0, 90, 180] | - | - |
| `SM_Prop_Bed_Double` | 6 | 399 | [0, 90, 180] | 2 | 579 |
| `SM_Env_Plant_Pinecone` | 6 | 193 | free | - | - |
| `SM_Bld_Door_Double_03_Door` | 6 | 200 | [0, 90] | - | - |
| `SM_Bld_Door_Double_01_Door` | 6 | 213 | [0, 270] | 3 | 180 |
| `SM_Prop_Drill_02_Leg` | 6 | 79 | free | - | - |
| `SM_Prop_Drill_02_Leg_01_Foot` | 6 | 17 | free | - | - |
| `SM_Prop_SolarPanel_01_Swivel` | 6 | 178 | free | - | - |
| `SM_Prop_Bench_Seat` | 5 | 206 | free | - | - |
| `SM_Prop_Vending_Machine` | 5 | 197 | free | - | - |
| `SM_Env_Artifact_Head` | 5 | 490 | free | - | - |
| `SM_Env_Ground_Blob` | 5 | 507 | free | - | - |
| `SM_Prop_Suit_Hanging` | 5 | 383 | [90, 180] | 2 | 115 |
| `SM_Bld_Pod_Hab_Small` | 5 | 200 | [90, 180, 270] | - | - |
| `SM_Env_Plant_Grass_Flower` | 5 | 208 | free | - | - |
| `SM_Bld_Pod_Corridor_Round` | 5 | 200 | [0, 90] | - | - |
| `SM_Env_Plant_Palm` | 5 | 301 | free | - | - |
| `SM_FX_Skydome_Planet` | 5 | 144181 | free | - | - |
| `SM_Prop_Pod_Pipe_01_End` | 5 | 162 | [0, 180, 270] | - | - |
| `SM_Prop_PowerGenerator_02_Chamber` | 5 | 185 | [0] | - | - |
| `SM_Prop_Chair_01_Headrest` | 5 | 316 | free | - | - |
| `SM_Prop_Chair_01_Back` | 5 | 238 | free | - | - |
| `SM_Prop_Chair_01_Armrest` | 5 | 238 | free | - | - |
| `SM_Prop_Chair_01_Pivot` | 5 | 212 | free | - | - |
| `SM_Prop_PowerGenerator_02_Glass` | 5 | 185 | [0] | - | - |
| `SM_Env_Plant_Spikey` | 4 | 1512 | free | - | - |
| `SM_Bld_Pod_Corridor_Round_Support_01_Leg` | 4 | 0 | [270] | 3 | 689 |
| `SM_Env_Artifact_Arm` | 4 | 1450 | free | - | - |
| `SM_Prop_Monitor_01_Arm` | 4 | 303 | [270] | - | - |
| `SM_Prop_Locker_01_Shelf` | 4 | 549 | free | - | - |
| `SM_Prop_Locker_01_Door` | 4 | 506 | free | - | - |
| `SM_Prop_Locker` | 4 | 396 | free | - | - |
| `SM_Prop_Crate_09_Lid` | 4 | 176 | free | - | - |
| `SM_Env_Plant_Balloon_Tall` | 4 | 3358 | [0] | - | - |
| `SM_Prop_Pod_Pipe` | 4 | 162 | [0] | - | - |
| `SM_Prop_Syncola_Can` | 4 | 207 | free | - | - |
| `SM_Prop_Satellite_07_Dish` | 4 | 369 | free | - | - |
| `SM_Prop_Vendor_Plate` | 4 | 298 | free | - | - |
| `SM_Prop_Monitor_05_Glass` | 4 | 445 | free | - | - |
| `SM_Prop_Satellite_07_Stand` | 4 | 350 | free | - | - |
| `SM_Prop_Cardboard_Box` | 4 | 231 | free | 2 | 9 |
| `SM_Bld_Door_Single_01_Door_03_Glass` | 4 | 307 | [0, 90, 270] | - | - |
| `SM_Bld_Bridge_End` | 4 | 50 | [90, 270] | - | - |
| `SM_Bld_Pod_Hab_Large_02_Leg` | 4 | -2 | [180] | - | - |
| `SM_Prop_Satellite_02_Arm` | 4 | 209 | free | - | - |
| `SM_Bld_Pod_Research_05_Light` | 4 | 840 | [0] | - | - |
| `SM_Prop_Table_Small` | 3 | 200 | free | - | - |
| `SM_Prop_Satellite_03_Dish` | 3 | 1261 | free | - | - |
| `SM_Prop_Satellite_03_Swivel` | 3 | 1000 | free | - | - |
| `SM_Env_Detail_Crater_Spiral` | 3 | -840 | free | - | - |
| `SM_Env_Road_Slope_Down` | 3 | 178 | free | - | - |
| `SM_Env_Plant_Mushroom` | 3 | 545 | free | - | - |
| `SM_Prop_Satellite_05_Sensor` | 3 | 131 | [0] | - | - |
| `SM_Env_Detail_Crater` | 3 | 1900 | [0] | - | - |
| `SM_Bld_Pod_Hub` | 3 | 200 | [0, 180] | - | - |
| `SM_Bld_Door_Double_03_Lock` | 3 | 351 | [0, 90] | - | - |
| `SM_Prop_Crate_05_Lid` | 3 | 126 | free | - | - |
| `SM_Prop_Monitor_03_Glass` | 3 | 335 | free | - | - |
| `SM_Bld_Pod_Corridor_Round_Connector` | 3 | 200 | [0, 90, 270] | - | - |
| `SM_Env_Plant_Flower_Large_Pod` | 3 | 1133 | free | - | - |
| `SM_Prop_Drill` | 3 | 0 | free | - | - |
| `SM_Prop_Drill_03_Extension` | 3 | 48 | free | - | - |
| `SM_Prop_Drill_03_Leg` | 3 | 121 | free | - | - |
| `SM_Prop_Drill_03_Leg_01_Foot` | 3 | 61 | free | - | - |
| `SM_Prop_Drill_02_Motor` | 3 | 341 | free | - | - |
| `SM_Prop_Drill_02_Drill_Head` | 3 | 46 | free | - | - |

## Interior dressing

Props within 1200 cm of a SHELL piece -- a wall, a corridor tube or a pod body -- by how far off
it they sit. That gap is what decides whether a thing reads as against the wall or adrift in
the room, and it is the number a replication has to match.

Matching only `SM_Bld*Wall*` produced an empty table on an exterior demo, which is correct and
useless: this pack builds its interiors out of corridor tubes and pod shells, not wall panels.

| prop family | n | median gap to nearest wall | median z |
|---|---|---|---|
| `SM_Prop_Crate` | 143 | 445 | 50 |
| `SM_Prop_Crate_07_Lid` | 32 | 420 | 208 |
| `SM_Prop_Hydroponic_Plant` | 28 | 560 | 219 |
| `SM_Prop_Crate_Lid` | 26 | 487 | 196 |
| `SM_Prop_Crate_08_Lid` | 26 | 561 | 171 |
| `SM_Prop_Monitor` | 23 | 271 | 400 |
| `SM_Prop_Satellite` | 19 | 340 | 1 |
| `SM_Prop_Hydroponic_Planter` | 17 | 403 | 197 |
| `SM_Prop_Antenna` | 17 | 331 | 695 |
| `SM_Prop_Chair` | 16 | 175 | 199 |
| `SM_Prop_Crate_10_Lid` | 16 | 499 | 94 |
| `SM_Prop_Crate_01_Lid` | 16 | 198 | 402 |
| `SM_Prop_Satellite_08_CrateArm` | 15 | 401 | 52 |
| `SM_Prop_Satellite_06_Dish` | 15 | 660 | 284 |
| `SM_Prop_Tether_Pole_Tall` | 14 | 300 | 0 |
| `SM_Prop_PowerCell` | 14 | 699 | 36 |
| `SM_Prop_Crate_14_Lid` | 14 | 364 | 216 |
| `SM_Prop_Tether_Wire` | 12 | 300 | 267 |
| `SM_Prop_Satellite_07_DishArm` | 12 | 238 | 290 |
| `SM_Prop_Satellite_07_Leg` | 12 | 239 | 222 |
| `SM_Prop_Satellite_08_Lid` | 10 | 429 | 71 |
| `SM_Prop_Crate_04_Lid` | 9 | 701 | 74 |
| `SM_Prop_Cable` | 9 | 684 | 9 |
| `SM_Prop_PowerGenerator` | 9 | 362 | -16 |
| `SM_Prop_Crate_15_Lid` | 8 | 384 | 346 |
| `SM_Prop_Hydroponic_Planter_02_Insert` | 8 | 395 | 196 |
| `SM_Prop_Sofa` | 8 | 285 | 200 |
| `SM_Prop_TrashBin_01_Lid` | 7 | 338 | 154 |
| `SM_Prop_TrashBin` | 7 | 380 | 1 |
| `SM_Prop_BatteryPack` | 7 | 375 | 200 |
| `SM_Prop_Crate_02_Lid` | 6 | 412 | 302 |
| `SM_Prop_PowerCellCharger` | 6 | 375 | 191 |
| `SM_Prop_Wall_Unit_01_Door` | 6 | 306 | 427 |
| `SM_Prop_Wall_Unit` | 6 | 309 | 200 |
| `SM_Prop_Crate_13_Lid` | 6 | 576 | 121 |
| `SM_Prop_Antenna_05_Leg` | 6 | 388 | 744 |
| `SM_Prop_Bed_Double_01_Blinds` | 6 | 162 | 479 |
| `SM_Prop_Bed_Double` | 6 | 87 | 399 |
| `SM_Prop_Bench_Seat` | 5 | 588 | 206 |
| `SM_Prop_Vending_Machine` | 5 | 296 | 197 |
