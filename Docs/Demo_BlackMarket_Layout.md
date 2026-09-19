# Demo_BlackMarket -- how this map is assembled

Scanned by `Tools/scan_pack_layout.py` from the map as the pack ships it. Every number here is
measured from Synty's own placement, not chosen by us.

- components: **1742**, distinct meshes: **520**, families: **287**
- commonest floor z: **-242**
- building grid: **x none found** (0% of pieces on it), **y none found** (0%)

## Families, mounting height and rotation

`mount` is the median z above the floor plane: ~0 stands on the floor, a metre or two is
wall-mounted or waist-height, above that is ceiling or upper storey. `yaws` shows whether a
family is axis-aligned (only 0/90/180/270) or placed freely -- free rotation means the pack
does not expect it to tile.

| family | n | mount z | yaws | longest run | spacing |
|---|---|---|---|---|---|
| `SM_Bld_Platform_Large` | 103 | 117 | free | 8 | 500 |
| `SM_Bld_Platform_Base_Large` | 75 | 178 | free | 6 | 500 |
| `SM_Prop_Vendor_Bug` | 72 | 184 | free | 2 | 535 |
| `SM_Prop_Scav_Scrap` | 53 | 155 | free | - | - |
| `SM_Bld_Platform_Barrier_Pillar` | 50 | 231 | free | 3 | 250 |
| `SM_Prop_Crate` | 43 | 200 | free | 2 | 74 |
| `SM_Prop_Vendor_Product` | 39 | 216 | free | 2 | 17 |
| `SM_Bld_Platform_Small` | 36 | 217 | free | 6 | 250 |
| `SM_Bld_Platform_Barrier_Long` | 36 | 317 | free | 4 | 500 |
| `SM_Bld_Platform_Stairs` | 35 | 116 | free | - | - |
| `SM_Env_Asteroid` | 31 | -5187 | free | - | - |
| `SM_Bld_Crate_Building` | 30 | 478 | free | 2 | 144 |
| `SM_Bld_Crate_Building_Pillar` | 29 | 242 | free | 2 | 212 |
| `SM_Bld_Greeble` | 29 | 292 | free | 3 | 50 |
| `SM_Bld_Door_Single` | 27 | 292 | free | - | - |
| `SM_Bld_Platform_Base_Small` | 24 | 192 | free | 6 | 250 |
| `SM_Prop_Junk_Paper` | 22 | 117 | free | - | - |
| `SM_Prop_Billboard` | 20 | 1207 | free | - | - |
| `SM_Env_Ground` | 18 | 18 | free | - | - |
| `SM_Prop_Vendor_Tentacle` | 16 | 208 | free | 2 | 535 |
| `SM_Prop_Tether_Wire` | 16 | 869 | free | - | - |
| `SM_Prop_Junk_Cardboard` | 15 | 109 | free | - | - |
| `SM_Veh_Vendor_Tug_01_Attach_Roof` | 14 | 452 | free | - | - |
| `SM_Env_Snow_Pile` | 13 | 117 | free | - | - |
| `SM_Bld_Crate_Building_Ladder` | 13 | 242 | free | - | - |
| `SM_Bld_Vendor` | 13 | 117 | free | - | - |
| `SM_Prop_Monitor` | 13 | 417 | free | - | - |
| `SM_Veh_Vendor_Tug_01_Attach_Sign` | 13 | 452 | free | - | - |
| `SM_Prop_Syncola_Can` | 12 | 321 | free | - | - |
| `SM_Bld_Crate_Building_01_Shutter` | 12 | 605 | free | - | - |
| `SM_Bld_Door_Single_04_Door` | 12 | 292 | free | 3 | 97 |
| `SM_Prop_Cardboard_Box` | 11 | 117 | free | - | - |
| `SM_Env_Ground_Greeble` | 11 | 74 | free | - | - |
| `SM_Prop_Junky_Male` | 11 | 123 | free | 2 | 132 |
| `SM_Veh_Vendor_Tug_01_Attach_Barrels` | 11 | 452 | free | - | - |
| `SM_Bld_Billboard_01_Glass` | 11 | 946 | free | - | - |
| `SM_Bld_Platform_Barrier_Pillar_Middle` | 10 | 317 | [0, 90, 180, 270] | 2 | 6975 |
| `SM_Prop_Cable` | 10 | 292 | free | - | - |
| `SM_Env_Ground_Junk` | 10 | -20 | free | - | - |
| `SM_Prop_Junky_Female` | 10 | 118 | free | - | - |
| `SM_Prop_Vending_Machine` | 9 | 117 | free | - | - |
| `SM_Bld_Door_Single_05_Door_01_Glass` | 9 | 256 | free | - | - |
| `SM_Bld_Door_Single_05_Door` | 9 | 256 | free | - | - |
| `SM_Bld_Railing_Pillar` | 9 | 995 | free | - | - |
| `SM_Bld_Platform_Barrier_Short` | 9 | 167 | free | 3 | 250 |
| `SM_Bld_Light_13_Light` | 9 | 712 | [0] | - | - |
| `SM_Env_Plant_Balloon_Group` | 9 | 77 | free | 2 | 2224 |
| `SM_Veh_Vendor_Tug_01_Attach_Fin` | 9 | 2242 | free | - | - |
| `SM_Bld_Crate_Building_Balcony` | 8 | 675 | free | - | - |
| `SM_Bld_Door_Single_01_Door` | 8 | 632 | free | - | - |
| `SM_Prop_Buoy` | 8 | 501 | [0] | - | - |
| `SM_Prop_Vendor_Rib` | 8 | 218 | free | 2 | 61 |
| `SM_Env_Core_Strut_01_Leg` | 8 | -2729 | free | - | - |
| `SM_Prop_Powerline` | 8 | 106 | free | - | - |
| `SM_Prop_Crate_14_Lid` | 8 | 132 | free | - | - |
| `SM_Bld_Railing` | 8 | 982 | free | - | - |
| `SM_Prop_Crate_02_Lid` | 8 | 394 | free | - | - |
| `SM_Env_Core_Strut_02_Leg` | 8 | -3819 | free | - | - |
| `SM_Veh_Vendor_Tug_01_Attach_Tech` | 8 | 452 | free | - | - |
| `SM_Prop_Chair` | 7 | 499 | free | - | - |
| `SM_Prop_Syncola_Can_Stack` | 7 | 117 | free | - | - |
| `SM_Prop_Crate_Lid` | 7 | 274 | free | - | - |
| `SM_Prop_Vendor_Product_14_Plant` | 7 | 448 | free | 2 | 17 |
| `SM_Prop_Vendor_Product_14_Glass` | 7 | 448 | free | 2 | 17 |
| `SM_Decal_Graffiti` | 7 | 542 | free | - | - |
| `SM_Prop_Vendor_Pans_Hanging` | 6 | 456 | free | - | - |
| `SM_Bld_Crate_Building_Frame` | 6 | 524 | free | - | - |
| `SM_Bld_Vendor_02_Door` | 6 | 229 | [0, 90, 270] | - | - |
| `SM_Prop_Crate_05_Lid` | 6 | 243 | free | - | - |
| `SM_Prop_Antenna` | 6 | 67 | free | - | - |
| `SM_Wep_BigAxe` | 6 | 411 | free | 2 | 48 |
| `SM_Env_Core_Strut_01_Node` | 6 | 3278 | free | - | - |
| `SM_Prop_TrashBin_01_Lid` | 6 | 287 | [90, 270] | - | - |
| `SM_Prop_TrashBin` | 6 | 134 | [90, 270] | - | - |
| `SM_Prop_Crate_01_Lid` | 6 | 465 | free | 2 | 3443 |
| `SM_Env_Blob` | 6 | 117 | free | - | - |
| `SM_Prop_Vending_Machine_04_Arm` | 6 | 416 | free | - | - |
| `SM_Prop_Crate_07_Lid` | 6 | 156 | [0] | - | - |
| `SM_Veh_Vendor_Tug_01_Attach_Bow` | 6 | 525 | free | - | - |
| `SM_Veh_Vendor_Tug_01_Attach_Shelves` | 6 | 416 | free | - | - |
| `SM_Bld_Crate_Building_Balcony_01_Glass` | 5 | 514 | free | - | - |
| `SM_Prop_Canopy_Preset` | 5 | 123 | free | - | - |
| `SM_Bld_Light` | 5 | 702 | [0] | - | - |
| `SM_Prop_Hydroponic_Planter` | 5 | 267 | [0, 180, 270] | - | - |
| `SM_Bld_Watertank` | 5 | 867 | free | - | - |
| `SM_Prop_Monitor_02_Screen` | 5 | 1143 | free | 2 | 45 |
| `SM_Prop_Vending_Machine_02_Sign` | 5 | 1084 | free | - | - |
| `SM_Prop_Skeleton_Skull_Jaw` | 4 | 335 | free | 2 | 80 |
| `SM_Prop_Crate_08_Lid` | 4 | 450 | [0] | - | - |
| `SM_Prop_Vendor_Tool` | 4 | 224 | [180] | - | - |
| `SM_Prop_Tether_Wire_Ship` | 4 | 1766 | free | - | - |
| `SM_Env_Ground_Greeble_04_Metal` | 4 | 65 | free | - | - |
| `SM_Bld_Door_Double_04_Door` | 4 | 170 | [90] | - | - |
| `SM_Env_Core_Strut` | 4 | -2940 | free | - | - |
| `SM_Prop_Vendor_Arm` | 4 | 214 | free | - | - |
| `SM_Generic_Small_Rocks` | 4 | 138 | free | - | - |
| `SM_Prop_Powerline_Antenna` | 4 | 1167 | free | - | - |
| `SM_Bld_Core_Antenna_Greeble` | 4 | -869 | free | - | - |
| `SM_Prop_Drone_02_Claw` | 4 | 463 | free | - | - |
| `SM_Prop_Drone_02_Arm` | 4 | 465 | free | - | - |
| `SM_Prop_Drone` | 4 | 492 | free | - | - |
| `SM_Prop_PowerGenerator` | 4 | 761 | free | - | - |
| `SM_Prop_BatteryPack` | 4 | 992 | free | - | - |
| `SM_Bld_Door_Single_01_Door_03_Glass` | 4 | 270 | free | - | - |
| `SM_Prop_Satellite_02_Arm` | 4 | 454 | [0] | - | - |
| `SM_Prop_Satellite` | 4 | 592 | free | - | - |
| `SM_Bld_Pod_Ramp` | 4 | 267 | free | - | - |
| `SM_Bld_Door_Single_03_Door` | 4 | 673 | free | - | - |
| `SM_Env_Core_Strut_02_Node` | 4 | -212 | free | - | - |
| `SM_Prop_Skeleton_Skull` | 4 | 407 | free | 2 | 61 |
| `SM_Bld_Core_Vent_Blade` | 4 | -4136 | free | - | - |
| `SM_Prop_Drill_01_Hinge` | 4 | 280 | free | - | - |
| `SM_Bld_Billboard_08_Glass` | 4 | 1467 | free | - | - |
| `SM_Prop_Bed_Single` | 3 | 315 | free | - | - |
| `SM_Prop_Crate_04_Lid` | 3 | 183 | free | - | - |
| `SM_Prop_Vending_Machine_05_Sign` | 3 | 137 | free | - | - |
| `SM_Bld_Scav_Refinery_Silo` | 3 | 2 | [0] | - | - |
| `SM_Prop_Wall_Unit_01_Door` | 3 | 342 | [0, 180, 270] | - | - |
| `SM_Prop_Wall_Unit` | 3 | 115 | [0, 180, 270] | - | - |
| `SM_Prop_Vendor_Fork` | 3 | 321 | free | - | - |
| `SM_Bld_WindMill` | 3 | 95 | free | - | - |
| `SM_Prop_Skeleton_Spine` | 3 | 379 | free | 2 | 56 |
| `SM_Prop_Monitor_03_Glass` | 3 | 552 | free | - | - |
| `SM_Bld_Light_13_Core` | 3 | 1028 | [0] | - | - |
| `SM_Bld_Pod_Silo` | 3 | -699 | free | - | - |
| `SM_Prop_Vendor_Ham` | 3 | 366 | free | 2 | 66 |
| `SM_Prop_Vending_Machine_04_Topper` | 3 | 388 | free | - | - |
| `SM_Prop_Vending_Machine_04_Glass` | 3 | 167 | free | - | - |
| `SM_Prop_PowerGenerator_03_Gear` | 3 | 899 | free | - | - |
| `SM_Env_Ground_Slope` | 3 | 40 | free | - | - |
| `SM_Bld_Bridge_Strut` | 3 | -18 | [180] | - | - |
| `SM_Prop_SolarPanel_03_Panel` | 3 | 475 | [0] | - | - |
| `SM_Prop_SolarPanel` | 3 | 442 | [0] | - | - |
| `SM_Prop_Satellite_06_Dish` | 3 | 366 | [0] | - | - |
| `SM_Env_Ground_Greeble_02_Metal` | 3 | 74 | free | - | - |
| `SM_Prop_Vendor_Block` | 3 | 210 | free | - | - |
| `SM_Prop_Vendor_Pan` | 3 | 257 | free | - | - |
| `SM_Bld_Greeble_28_Tank` | 3 | 152 | [0] | - | - |
| `SM_Bld_WindMill_Top` | 3 | 1342 | free | - | - |
| `SM_Prop_Drill_01_Drill_Head_01_Cone` | 3 | -325 | free | - | - |
| `SM_Bld_WindMill_Blade` | 3 | 1342 | free | - | - |

## Interior dressing

Props within 1200 cm of a SHELL piece -- a wall, a corridor tube or a pod body -- by how far off
it they sit. That gap is what decides whether a thing reads as against the wall or adrift in
the room, and it is the number a replication has to match.

Matching only `SM_Bld*Wall*` produced an empty table on an exterior demo, which is correct and
useless: this pack builds its interiors out of corridor tubes and pod shells, not wall panels.

| prop family | n | median gap to nearest wall | median z |
|---|---|---|---|
| `SM_Prop_Vendor_Product` | 34 | 1003 | 246 |
| `SM_Prop_Vendor_Bug` | 20 | 1055 | 163 |
| `SM_Prop_Scav_Scrap` | 14 | 820 | 164 |
| `SM_Prop_Crate` | 13 | 815 | 322 |
| `SM_Prop_Billboard` | 9 | 589 | 1247 |
| `SM_Prop_Junk_Paper` | 7 | 996 | 195 |
| `SM_Prop_Vendor_Tentacle` | 7 | 462 | 417 |
| `SM_Prop_Junky_Male` | 7 | 312 | 172 |
| `SM_Prop_Tether_Wire` | 6 | 1105 | 900 |
| `SM_Prop_Vendor_Product_14_Plant` | 6 | 475 | 498 |
| `SM_Prop_Vendor_Product_14_Glass` | 6 | 475 | 498 |
| `SM_Prop_Junk_Cardboard` | 6 | 753 | 167 |
| `SM_Prop_Hydroponic_Planter` | 5 | 357 | 317 |
| `SM_Prop_Skeleton_Skull_Jaw` | 4 | 363 | 385 |
| `SM_Prop_Vendor_Tool` | 4 | 1154 | 258 |
| `SM_Prop_Monitor` | 4 | 1117 | 960 |
| `SM_Prop_Vendor_Arm` | 4 | 1171 | 264 |
| `SM_Prop_Cable` | 4 | 672 | 174 |
| `SM_Prop_Crate_02_Lid` | 4 | 815 | 132 |
| `SM_Prop_Skeleton_Skull` | 4 | 332 | 457 |
| `SM_Prop_Chair` | 3 | 249 | 426 |
| `SM_Prop_Crate_08_Lid` | 3 | 866 | 493 |
| `SM_Prop_Syncola_Can_Stack` | 3 | 874 | 167 |
| `SM_Prop_Syncola_Can` | 3 | 1142 | 1071 |
| `SM_Prop_Vendor_Rib` | 3 | 461 | 503 |
| `SM_Prop_Wall_Unit_01_Door` | 3 | 808 | 420 |
| `SM_Prop_Wall_Unit` | 3 | 823 | 193 |
| `SM_Prop_Skeleton_Spine` | 3 | 335 | 429 |
| `SM_Prop_BatteryPack` | 3 | 433 | 1012 |
| `SM_Prop_Canopy_Preset` | 3 | 706 | 274 |
| `SM_Prop_Crate_01_Lid` | 3 | 715 | 1001 |
| `SM_Prop_Vendor_Ham` | 3 | 454 | 416 |
| `SM_Prop_Vending_Machine` | 3 | 807 | 163 |
| `SM_Prop_Junky_Female` | 3 | 623 | 165 |
| `SM_Prop_Monitor_02_Screen` | 3 | 1123 | 1144 |
| `SM_Prop_Vending_Machine_02_Sign` | 3 | 916 | 1608 |
| `SM_Prop_Chair_01_Headrest` | 2 | 1192 | 1095 |
| `SM_Prop_Chair_04_Top` | 2 | 268 | 469 |
| `SM_Prop_Vendor_Tool_01_Trigger` | 2 | 1154 | 274 |
| `SM_Prop_Vendor_Tool_02_Trigger` | 2 | 1169 | 259 |
