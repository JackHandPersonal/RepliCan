# Demo_Corporation -- how this map is assembled

Scanned by `Tools/scan_pack_layout.py` from the map as the pack ships it. Every number here is
measured from Synty's own placement, not chosen by us.

- components: **3346**, distinct meshes: **529**, families: **310**
- commonest floor z: **-58**  -- **MULTI-STOREY**: floor pieces also at -3000, -2750, -2500, -2250, -1750, -750, so a single datum would be meaningless and heights below are measured against the nearest floor UNDER each prop
- building grid: **x none found** (0% of pieces on it), **y none found** (0%)

## Families, mounting height and rotation

`mount` is the median z above the floor plane: ~0 stands on the floor, a metre or two is
wall-mounted or waist-height, above that is ceiling or upper storey. `yaws` shows whether a
family is axis-aligned (only 0/90/180/270) or placed freely -- free rotation means the pack
does not expect it to tile.

| family | n | mount z | yaws | longest run | spacing |
|---|---|---|---|---|---|
| `SM_Env_Ground` | 155 | -1322 | free | 2 | 1979 |
| `SM_Bld_Platform_Large` | 150 | 58 | [0, 270] | 13 | 500 |
| `SM_Env_Plant_Balloon_Group` | 125 | -970 | free | 2 | 15142 |
| `SM_Env_Cliff_Rough` | 124 | -3102 | free | - | - |
| `SM_Bld_Platform_Base_Large` | 106 | 58 | [0, 90, 270] | 7 | 500 |
| `SM_Prop_Crate` | 102 | -1898 | free | 2 | 1726 |
| `SM_Env_Rock_Large` | 88 | -2038 | free | - | - |
| `SM_Env_Cliff_Flat` | 68 | -3034 | free | - | - |
| `SM_Env_Ground_Slope` | 65 | -2241 | free | 2 | 7587 |
| `SM_Env_Plant_Balloon_Tall` | 65 | -1115 | free | - | - |
| `SM_Prop_Monitor_02_Screen` | 65 | 1282 | free | 4 | 2 |
| `SM_Env_Plant_Shrub` | 59 | -799 | free | 2 | 2741 |
| `SM_Bld_Bridge_Rail` | 59 | -2730 | free | 19 | 1000 |
| `SM_Bld_Platform_Base_Small` | 56 | 189 | [0, 270] | 10 | 250 |
| `SM_Env_Plant_Fester` | 54 | -1277 | free | - | - |
| `SM_Env_Cliff_Curved` | 52 | -7682 | free | - | - |
| `SM_Env_Detail_Round` | 50 | -1579 | free | - | - |
| `SM_Prop_Monitor` | 44 | 828 | free | 2 | 785 |
| `SM_Env_Detail_Spikey_Group` | 44 | -2207 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe` | 42 | -2692 | [270] | 18 | 1000 |
| `SM_Bld_Corp_Wall` | 39 | 58 | free | 6 | 2000 |
| `SM_Bld_Platform_Small` | 36 | 58 | [0, 270] | 10 | 250 |
| `SM_Env_Road_Corner` | 34 | -905 | free | - | - |
| `SM_Env_Detail_Spikey` | 34 | -1858 | free | - | - |
| `SM_Env_Ground_Blob` | 33 | -1532 | free | - | - |
| `SM_Env_Snow_Pile` | 32 | 58 | free | - | - |
| `SM_Prop_Locker_01_Shelf` | 31 | 208 | [0, 90, 180, 270] | 5 | 85 |
| `SM_Prop_Locker_01_Door` | 31 | 165 | [0, 90, 180, 270] | 5 | 85 |
| `SM_Prop_Locker` | 31 | 56 | [0, 90, 180, 270] | 5 | 85 |
| `SM_Bld_Bridge` | 30 | -2730 | free | 19 | 1000 |
| `SM_Env_Plant_Coral` | 29 | -364 | free | - | - |
| `SM_Bld_Door_Double_02_Door` | 28 | 33 | [0, 270] | 2 | 167 |
| `SM_Prop_Crate_04_Lid` | 25 | -1837 | free | 2 | 123 |
| `SM_Env_Detail_Spire` | 24 | -1457 | free | - | - |
| `SM_Env_Plant_Balloon` | 24 | -852 | free | - | - |
| `SM_Env_Plant_Fern` | 24 | -2321 | free | 2 | 1879 |
| `SM_Bld_Door_Single_03_Door` | 24 | 294 | [90] | 4 | 1000 |
| `SM_Prop_Chair` | 22 | -1958 | free | 2 | 48 |
| `SM_Env_Road_Straight` | 19 | -1442 | free | - | - |
| `SM_Bld_Light` | 18 | -1559 | free | 3 | 475 |
| `SM_Prop_Crate_Lid` | 18 | -1871 | free | 2 | 956 |
| `SM_Bld_Greeble` | 17 | 160 | free | - | - |
| `SM_Env_Detail_Spire_Group` | 17 | -1811 | free | - | - |
| `SM_Generic_Small_Rocks` | 16 | -2728 | free | 2 | 8000 |
| `SM_Prop_Crate_01_Lid` | 16 | 254 | free | 2 | 65 |
| `SM_Prop_Turret_Large_02_Barrel_r` | 16 | 1958 | free | - | - |
| `SM_Prop_Turret_Large_02_Barrel_l` | 16 | 1958 | free | - | - |
| `SM_Bld_Column` | 15 | -465 | free | - | - |
| `SM_Env_Plant_Bulb` | 15 | -696 | free | - | - |
| `SM_Bld_Door_Single` | 14 | 61 | free | 4 | 1000 |
| `SM_Bld_Door_Double` | 13 | 48 | free | 2 | 486 |
| `SM_Prop_Crate_02_Lid` | 13 | -1794 | free | - | - |
| `SM_Prop_Monitor_03_Glass` | 13 | 467 | free | - | - |
| `SM_Env_Rock` | 13 | -1824 | free | - | - |
| `SM_Bld_Platform_Small_Corner` | 12 | 58 | [0, 90, 180, 270] | 2 | 2250 |
| `SM_Bld_Platform_Barrier_Pillar_Middle` | 12 | 1446 | [0, 90, 180] | 2 | 500 |
| `SM_Prop_Bed_Double_01_Blinds` | 12 | -1969 | free | 2 | 398 |
| `SM_Prop_Bed_Double` | 12 | -2050 | free | 2 | 520 |
| `SM_Prop_Container_01_Glass` | 12 | 156 | free | 2 | 385 |
| `SM_Prop_Container_01_Door` | 12 | 125 | free | 2 | 385 |
| `SM_Prop_Container` | 12 | -18 | free | 2 | 385 |
| `SM_Prop_Turret_Large` | 12 | 405 | free | - | - |
| `SM_Bld_Bridge_Rail_End` | 12 | -1415 | free | - | - |
| `SM_Bld_Bridge_Strut` | 12 | -1461 | free | - | - |
| `SM_Bld_Door_Single_03_Door_02_Glass` | 12 | 438 | [90] | 4 | 1000 |
| `SM_Bld_Platform_Barrier_Short` | 12 | 1446 | free | - | - |
| `SM_Bld_Platform_Barrier_Pillar` | 12 | 1446 | [0, 90, 180, 270] | 2 | 1550 |
| `SM_Env_Plant_Kelp` | 12 | -807 | free | - | - |
| `SM_Prop_Monitor_01_Arm` | 12 | 850 | [0, 90, 180, 270] | 3 | 62 |
| `SM_Env_Road_Slope_Down` | 12 | -1455 | free | - | - |
| `SM_Bld_Platform_Barrier_Long` | 11 | 1446 | [0, 90, 180] | 3 | 475 |
| `SM_Bld_Pod_Support` | 11 | -2186 | free | 2 | 744 |
| `SM_Env_Detail_Crater` | 10 | -926 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe_02_Pillar` | 10 | -2667 | [270] | 2 | 4012 |
| `SM_Prop_Scav_Scrap` | 10 | -7089 | free | - | - |
| `SM_Prop_Satellite` | 10 | 51 | free | - | - |
| `SM_Prop_Chair_02_Backrest` | 10 | -1893 | free | 2 | 87 |
| `SM_Prop_Chair_02_Pivot` | 10 | -1928 | free | 2 | 157 |
| `SM_Env_Detail_Round_Group` | 10 | -1510 | free | - | - |
| `SM_Bld_Pod_Support_02_Leg` | 9 | -2210 | free | 2 | 1252 |
| `SM_Prop_Drill_01_Hinge` | 8 | -217 | free | - | - |
| `SM_Prop_Turret_Large_03_Missiles` | 8 | 851 | free | 3 | 267 |
| `SM_Prop_PowerCell` | 8 | -2006 | free | 2 | 47 |
| `SM_Bld_Corp_Barracks` | 8 | 58 | [90, 270] | 4 | 1000 |
| `SM_Env_Rock_Spike` | 8 | -3945 | free | - | - |
| `SM_Prop_Crate_09_Lid` | 8 | 1030 | free | - | - |
| `SM_Prop_Antenna` | 8 | 480 | [0] | - | - |
| `SM_Env_Plant_Mushroom` | 8 | -2181 | free | - | - |
| `SM_Bld_Scav_Refinery_Pipe_02_Connector` | 8 | -3117 | [90] | 2 | 4012 |
| `SM_Prop_Turret_Large_02_Arm_r` | 8 | 1908 | free | - | - |
| `SM_Prop_Turret_Large_02_Arm_l` | 8 | 1908 | free | - | - |
| `SM_Prop_Turret_Large_02_Top` | 8 | 1758 | free | - | - |
| `SM_Bld_Corp_Platform_01_LowerLeg` | 8 | -1961 | [0] | 2 | 358 |
| `SM_Prop_Satellite_02_Arm` | 8 | 913 | free | - | - |
| `SM_Bld_Door_Double_02_Door_Glass` | 7 | 33 | [0, 270] | 2 | 1678 |
| `SM_Bld_Scav_Refinery_Pipe_01_Bend` | 7 | -2667 | [0, 90, 180, 270] | 2 | 8500 |
| `SM_Bld_Pod_Ramp` | 7 | 108 | [0, 180, 270] | 2 | 350 |
| `SM_Prop_Chair_01_Headrest` | 7 | 1284 | free | - | - |
| `SM_Prop_Chair_01_Back` | 7 | 1206 | free | - | - |
| `SM_Prop_Chair_01_Armrest` | 7 | 1206 | free | - | - |
| `SM_Prop_Chair_01_Pivot` | 7 | 1180 | free | - | - |
| `SM_Bld_Door_Double_01_Door` | 6 | -2028 | free | 3 | 180 |
| `SM_Prop_Crate_06_Lid` | 6 | -1791 | free | - | - |
| `SM_Env_Plant_Large_Spiral_Preset` | 6 | -2420 | free | - | - |
| `SM_Prop_PowerCellCharger` | 6 | 333 | [0, 180] | - | - |
| `SM_Env_Detail_Crater_Fill` | 6 | -194 | free | - | - |
| `SM_Prop_Drill_01_Drill_Head_01_Cone` | 6 | -1202 | free | - | - |
| `SM_Prop_Fuel_Cell_01_Lid` | 6 | 470 | free | - | - |
| `SM_Prop_Fuel_Cell` | 6 | 370 | free | - | - |
| `SM_Prop_Satellite_08_CrateArm` | 6 | 131 | [0] | 2 | 77 |
| `SM_Bld_Corp_Containment` | 6 | 46 | free | - | - |
| `SM_Prop_BatteryPack` | 6 | 110 | free | - | - |
| `SM_Bld_Pod_Silo` | 6 | 58 | free | - | - |
| `SM_Prop_Monitor_01_Screen` | 6 | 959 | [0, 90, 180, 270] | - | - |
| `SM_Prop_Monitor_01_Swivel` | 6 | 814 | [0, 90, 180, 270] | - | - |
| `SM_Prop_Cable` | 6 | 76 | free | - | - |
| `SM_Prop_Powerline` | 6 | -886 | free | - | - |
| `SM_Env_Artifact_AlienRuin` | 6 | -16122 | free | - | - |
| `SM_Prop_Crate_07_Lid` | 6 | -1895 | [0, 270] | - | - |
| `SM_Bld_Bridge_End` | 6 | -1415 | free | - | - |
| `SM_Env_Detail_Platform` | 6 | -2250 | free | - | - |
| `SM_Env_Plant_Succulent_Large` | 5 | -1599 | free | - | - |
| `SM_Prop_PowerGenerator` | 5 | 314 | free | - | - |
| `SM_Prop_Crate_08_Lid` | 5 | -1897 | free | - | - |
| `SM_Prop_Chair_04_Top` | 5 | -1915 | free | - | - |
| `SM_Prop_Turret_Medium_01_Barrell` | 5 | 729 | free | - | - |
| `SM_Prop_Turret_Medium_01_Gun` | 5 | 637 | free | - | - |
| `SM_Prop_Turret_Medium_01_Pivot` | 5 | 458 | free | - | - |
| `SM_Prop_Turret_Medium` | 5 | 408 | free | - | - |
| `SM_Prop_TrashBin_01_Lid` | 5 | -1859 | free | - | - |
| `SM_Prop_TrashBin` | 5 | -2012 | free | - | - |
| `SM_Prop_Canopy_Pole` | 4 | 59 | [0] | - | - |
| `SM_Prop_Cardboard_Box` | 4 | -1815 | free | - | - |
| `SM_Prop_Drill` | 4 | -1195 | free | - | - |
| `SM_Prop_Turret_Large_03_Launcher` | 4 | 737 | free | - | - |
| `SM_Prop_Turret_Large_03_Top` | 4 | 554 | free | - | - |
| `SM_Prop_Sofa` | 4 | -2016 | [0, 180] | - | - |
| `SM_Bld_Corp_Wall_Tower_Corner_01_Turret` | 4 | -1214 | [90, 270] | - | - |
| `SM_Prop_PowerGenerator_03_Gear` | 4 | 426 | free | - | - |
| `SM_Prop_Satellite_08_Lid` | 4 | 153 | [0, 180] | - | - |
| `SM_Bld_WindMill_Top` | 4 | 4544 | free | - | - |
| `SM_Bld_WindMill` | 4 | -992 | free | - | - |
| `SM_Prop_Projection_Table` | 4 | 1108 | free | - | - |
| `SM_Bld_Pod_Research` | 4 | -1804 | free | - | - |
| `SM_Bld_Corp_Wall_01_Half` | 4 | -2154 | [0, 90, 180] | 2 | 1000 |
| `SM_Bld_Corp_Containment_02_Glass` | 4 | 46 | [0] | - | - |
| `SM_Bld_Corp_Containment_02_Capsule` | 4 | 46 | [0] | - | - |
| `SM_Env_Road_End` | 4 | -2333 | free | - | - |
| `SM_Prop_Monitor_08_Screen` | 4 | -1596 | free | - | - |
| `SM_Env_Road_Slope_Up` | 4 | -1702 | free | - | - |
| `SM_Bld_Corp_Wall_Gate_01_Turret` | 4 | 999 | [0, 180] | - | - |
| `SM_Bld_Corp_Wall_Gate_01_Towers` | 4 | 58 | [0, 180] | - | - |
| `SM_Env_Plant_Flower` | 4 | -1374 | free | - | - |
| `SM_Env_Plant_Large_Spiral` | 4 | -2300 | [0] | - | - |
| `SM_Env_Plant_Grass_Flower` | 4 | -414 | free | - | - |
| `SM_Bld_Corp_Comms_Tower_01_Dish` | 4 | 2485 | free | - | - |
| `SM_Bld_WindMill_Blade` | 4 | 4544 | free | - | - |
| `SM_Prop_SolarPanel_02_Panel` | 3 | 146 | free | - | - |
| `SM_Prop_SolarPanel_02_Swivel` | 3 | 93 | free | - | - |
| `SM_Prop_SolarPanel` | 3 | 49 | free | - | - |
| `SM_Bld_Planetary_Cannon_01_Base` | 3 | 6936 | free | - | - |
| `SM_Bld_Planetary_Cannon` | 3 | 5398 | free | - | - |
| `SM_Bld_Corp_LandingPad` | 3 | 33 | [0, 270] | - | - |
| `SM_Bld_Platform_Large_Corner` | 3 | 58 | [90, 180] | - | - |
| `SM_Prop_Table_Medium` | 3 | -1960 | [0, 90] | - | - |
| `SM_Prop_Drill_02_Drill_Head_01_Cone` | 3 | -1151 | free | - | - |
| `SM_Prop_Drill_02_Extension` | 3 | -1091 | [0] | - | - |
| `SM_Prop_Drill_02_Leg_01_Foot` | 3 | -1176 | free | - | - |
| `SM_Prop_Drill_02_Leg` | 3 | -1116 | free | - | - |
| `SM_Prop_Banner_05_H` | 3 | 1586 | [0, 180, 270] | - | - |
| `SM_Bld_Core_Antenna_Greeble` | 3 | 1496 | [0, 180] | - | - |
| `SM_Prop_Crate_05_Lid` | 3 | -1778 | free | - | - |
| `SM_Prop_Satellite_06_Dish` | 3 | -1787 | free | - | - |
| `SM_Prop_Antenna_05_Leg` | 3 | -8389 | free | - | - |
| `SM_Bld_Door_Single_04_Door` | 3 | 61 | free | - | - |
| `SM_Prop_Drill_03_Extension` | 3 | -126 | free | 2 | 2 |
| `SM_Prop_Drill_03_Leg` | 3 | -54 | free | - | - |
| `SM_Prop_Drill_03_Leg_01_Foot` | 3 | -114 | free | - | - |
| `SM_Bld_Planetary_Cannon_01_Pivot` | 3 | 9462 | free | - | - |
| `SM_Bld_Planetary_Cannon_01_PowerUnit` | 3 | 8820 | free | - | - |

## Interior dressing

Props within 1200 cm of a SHELL piece -- a wall, a corridor tube or a pod body -- by how far off
it they sit. That gap is what decides whether a thing reads as against the wall or adrift in
the room, and it is the number a replication has to match.

Matching only `SM_Bld*Wall*` produced an empty table on an exterior demo, which is correct and
useless: this pack builds its interiors out of corridor tubes and pod shells, not wall panels.

| prop family | n | median gap to nearest wall | median z |
|---|---|---|---|
| `SM_Prop_Crate` | 55 | 580 | 143 |
| `SM_Prop_Monitor_02_Screen` | 27 | 888 | 1347 |
| `SM_Prop_Crate_04_Lid` | 17 | 681 | 309 |
| `SM_Prop_Monitor` | 15 | 872 | 1120 |
| `SM_Prop_Locker_01_Shelf` | 15 | 611 | 202 |
| `SM_Prop_Locker_01_Door` | 15 | 614 | 159 |
| `SM_Prop_Locker` | 15 | 612 | 50 |
| `SM_Prop_Chair` | 13 | 870 | 239 |
| `SM_Prop_Bed_Double_01_Blinds` | 12 | 892 | 143 |
| `SM_Prop_Bed_Double` | 11 | 737 | 62 |
| `SM_Prop_Crate_Lid` | 9 | 539 | 271 |
| `SM_Prop_PowerCell` | 8 | 693 | 234 |
| `SM_Prop_Chair_02_Backrest` | 8 | 887 | 305 |
| `SM_Prop_Chair_02_Pivot` | 8 | 870 | 270 |
| `SM_Prop_Turret_Large_02_Barrel_r` | 8 | 1047 | 1965 |
| `SM_Prop_Turret_Large_02_Barrel_l` | 8 | 871 | 1965 |
| `SM_Prop_Satellite_02_Arm` | 8 | 783 | 913 |
| `SM_Prop_Crate_02_Lid` | 7 | 440 | 173 |
| `SM_Prop_Turret_Large` | 6 | 978 | 1596 |
| `SM_Prop_Fuel_Cell_01_Lid` | 6 | 961 | 536 |
| `SM_Prop_Fuel_Cell` | 6 | 961 | 436 |
| `SM_Prop_Satellite` | 6 | 719 | 594 |
| `SM_Prop_Crate_06_Lid` | 5 | 912 | 187 |
| `SM_Prop_Monitor_03_Glass` | 5 | 822 | 1255 |
| `SM_Prop_TrashBin_01_Lid` | 5 | 446 | 187 |
| `SM_Prop_TrashBin` | 5 | 446 | 34 |
| `SM_Prop_PowerGenerator` | 4 | 1019 | 314 |
| `SM_Prop_Crate_08_Lid` | 4 | 972 | 215 |
| `SM_Prop_Crate_09_Lid` | 4 | 350 | 2060 |
| `SM_Prop_Monitor_01_Arm` | 4 | 1095 | 1148 |
| `SM_Prop_Crate_07_Lid` | 4 | 726 | 188 |
| `SM_Prop_Turret_Large_02_Arm_r` | 4 | 1076 | 1915 |
| `SM_Prop_Turret_Large_02_Arm_l` | 4 | 963 | 1915 |
| `SM_Prop_Turret_Large_02_Top` | 4 | 1004 | 1765 |
| `SM_Prop_Turret_Large_03_Missiles` | 4 | 966 | 1046 |
| `SM_Prop_PowerCellCharger` | 3 | 900 | 340 |
| `SM_Prop_Cardboard_Box` | 3 | 624 | 291 |
| `SM_Prop_Crate_01_Lid` | 3 | 521 | 160 |
| `SM_Prop_PowerGenerator_03_Gear` | 3 | 1017 | 426 |
| `SM_Prop_Chair_01_Headrest` | 3 | 669 | 1280 |
