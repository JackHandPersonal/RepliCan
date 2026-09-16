# RepliCan

An Unreal Engine 5.8 first/third-person game built on Synty POLYGON content. C++ project, no
Blueprint logic; the level is built and maintained by scripts.

## What is in the repo

- `Source/RepliCan/` – all game code (character, weapons, doors, lift, UI pages, catalogues).
- `Content/RepliCan/` – the project's own assets: baked weapons (HAC1 grip space), the cut
  character library, optics, icons, materials, the maps.
- `Content/Characters/` – shared character materials, animation retargets, the nose prop.
- `Tools/` – editor-side Python (run through `Tools/ue_remote.py --file <script>`): the facility
  layout, surveys, importers, icon renders, sound synthesis. `Tools/LevelManifest/` records what
  the layout script placed and what was deleted by hand, so a re-run never resurrects a deletion.
- `UI/` – the data the game reads: `Weapons.json`, `Items.json`, `Impacts.json`, the character
  sheet spec.
- `RawAudio/` – the loose WAVs the game plays (synthesised by `Tools/make_*_sounds.py`, plus
  CC0 gunshot samples from BigSoundBank converted by `Tools/convert_samples.py`).
- `Docs/` – the held-asset standard and other notes.

## What is NOT in the repo

The Synty POLYGON packs (Sci-Fi Space, Sci-Fi Worlds, Cyber City, Sci-Fi Horror, Mech,
Military, Police Station, Sci-Fi City, Goblin War Camp, Modular Fantasy Hero) are licensed and
must not be redistributed. Install them from the pack zips into `Content/` at the paths the
`.gitignore` lists; the importers in `Tools/` show which folders each pack needs. `Binaries/`,
`Intermediate/`, `Saved/`, `DerivedDataCache/` and `RawArt/` renders are regenerated.

## Building

Generate project files from `RepliCan.uproject`, then build `RepliCanEditor Win64 Development`
(or `Engine/Build/BatchFiles/Build.bat RepliCanEditor Win64 Development -project=<path>`).
