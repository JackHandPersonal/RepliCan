# The Held Asset Standard

*How a weapon, a hand and an animation agree with each other in RepliCan.*

## Why this document exists

There are 96 weapons in `UI/Weapons.json`, drawn from three Synty packs, and several
skeletons that can hold them. Naively that is 96 x N combinations, and every one of them is a
chance for the gun to end up floating beside the fist. The tempting fix is a lookup table —
"rifles rotate like this, swords rotate like that" — and then a second table for skeletons, and
then a special case for the one launcher that was modelled backwards. That is a switch
statement growing inside a renderer, and it never stops growing.

The alternative, and the rule here, is:

> **Make the assets agree on disk, so the runtime has nothing to decide.**

Every conversion is done once, by a tool, and baked into the `.uasset`. At play time a weapon
is attached to a socket with an identity transform and that is the whole of it. If a weapon
looks wrong in a hand, the bug is in an asset, not in a branch.

The same logic applies to animation. A weapon does not name an animation; it names a
**stance**, and a stance is a folder whose contents follow a fixed naming pattern. Asking for
"the reload for this weapon" is string composition, not a case statement.

---

## Part 1 — Weapon mesh space (HAC1)

### 1.1 The rule

Every weapon mesh under `/Game/RepliCan/Weapons/` is modelled in **grip space**:

| | |
|---|---|
| **Origin** | the point the right hand closes on — the pistol grip of a gun, the middle of a sword's handle, the centre of a shield's arm strap. |
| **+X** | forward: the direction a bullet leaves, a blade points, a shield faces. |
| **+Z** | up: the sight rail of a gun, the spine of a blade, the top of a shield. |
| **+Y** | the weapon's own right, following from X and Z in UE's left-handed frame. |
| **Scale** | 1.0. Centimetres. No mesh-level scale compensation. |

This is deliberately the same convention as a UE actor: `+X` is what
`GetActorForwardVector()` means everywhere else in the engine. A normalised weapon dropped in
the world at zero rotation points down the level's +X like everything else does, which makes
it possible to eyeball a whole rack of them in the editor and see immediately which one is
wrong.

### 1.2 What the assets actually were

Measured with `Tools/measure_weapon_axes.py` before any normalisation, across all 96:

```
  long axis  +X   1      guns run along Y, blades run along Z,
             +Y  37      and neither is universal
             +Z  51
             -Y   4
             -Z   3
```

Broken down, the packs were internally consistent per weapon *type* but not with each other:
every Sword ran along +Z, every Pistol along +Y, and then `Wep_Launcher_01` and `_03` ran
along **-Y** while `Wep_Launcher_02` ran along +Y — three meshes in one family, two
conventions. `Wep_BigAxe_07` is the one axe modelled upside down. `Shuriken_03` is the single
+X asset in the library.

That distribution is the argument for this document. Five conventions across one library is
five branches in any code that touches them, and the odd ones out stay invisible until
someone equips that exact launcher.

### 1.3 How normalisation is done

`Tools/normalise_weapons.py` bakes the correction into the mesh itself:

1. **Measure** the local bounding box.
2. **Pick forward** — the longest axis, signed toward whichever end reaches further from the
   origin. A weapon's business end is always the far end; that is what makes it the business
   end.
3. **Pick up** — of the two remaining axes, the one that is *not* the thin one. Guns are
   taller than they are wide (a rifle receiver is ~8 cm across and ~41 cm tall including grip
   and sights); blades are wider at the guard than they are thick. The thin axis is therefore
   the weapon's left-right, and the other is up.
4. **Resolve the up sign** per kind, from `axis_hints` in `UI/WeaponGrips.json`. This is the
   one thing geometry alone cannot settle — a bounding box does not know which way a gun's
   sights face — so it is declared once per kind rather than guessed 96 times.
5. **Translate** so the origin lands on the grip (see 1.4).
6. **Bake** the resulting transform into the vertex data with GeometryScript, save the asset,
   and stamp `"space": "hac1"` on the catalogue entry.

The stamp makes the tool idempotent: a mesh already stamped is skipped, so the script can be
re-run after adding new weapons without double-rotating the old ones. **If you ever need to
re-bake from scratch, restore the meshes from source control first** — the bake is destructive
by design, because the whole point is that the corrected state is what lives on disk.

### 1.4 Finding the grip

Axes can be measured. The grip cannot, quite — no bounding box knows where a thumb goes. The
standard uses a measured estimate plus a declared, per-weapon override:

* **Guns.** The hand is on the pistol grip: behind the receiver's midpoint and below the bore
  line. Estimated as a fraction of the rear extent along the barrel and of the drop below the
  centreline, declared per kind in `grip_rule`.
* **Blades, axes, hammers.** Synty already puts the pivot at the butt of the handle
  (`butt_offset` measures 12–27 cm on swords, which is exactly a handle's length). The grip is
  most of a handle-length up from the pivot.
* **Shields.** The arm strap is the centre of the plate, and forward is the face normal rather
  than the long axis, so shields are the one kind whose forward is declared, not measured.

Any weapon that comes out wrong gets an entry in the `overrides` block of
`UI/WeaponGrips.json` and is re-baked. **Overrides are expected**; what is not acceptable is a
special case in C++.

---

## Part 2 — The hand socket

### 2.1 The rule

Every skeleton that can hold something carries a socket named exactly:

```
WeaponGrip_R        on hand_r
WeaponGrip_L        on hand_l      (shields, off-hand, dual wield)
```

The socket's transform is authored **once per skeleton**, and its job is to cancel out
whatever that skeleton's hand bone frame happens to be, so that:

> a HAC1-normalised weapon attached to `WeaponGrip_R` with an **identity** relative transform
> is held correctly.

This replaces `ABaseCharacter::ComputeBoneFrameConversion(TEXT("hand_r"))` for weapons. That
function computed the same correction at runtime by comparing reference poses — correct, but
it meant the answer lived in code and could only be checked by playing the game. A socket can
be dragged in the Skeleton editor and seen immediately.

`Tools/add_grip_sockets.py` adds the sockets, to the mesh and (via add_socket's promote flag)
to the skeleton behind it, so one edit covers every mesh on that rig. It carries the measured
offsets from the old runtime path, so the first pass is not a guess; from then on the socket is
the source of truth and can be nudged in the Skeleton editor.

Three things about UE's Python socket API that the script has to work around, recorded here
because none of them fail loudly:

* `USkeletalMeshSocket::SocketName` and `BoneName` are both `VisibleAnywhere`, so
  `set_editor_property` refuses them. The name is set by adding the socket under an
  auto-generated one ("Socket", "Socket_0") and calling `rename_socket`; the bone is set with
  `set_socket_parent(mesh, bone)`, which validates against that mesh's skeleton.
* `USkeleton::Sockets` is not exposed at all -- no read, no add. The only route to a skeleton
  socket is `SkeletalMesh.add_socket(socket, add_to_skeleton=True)`.
* `add_socket` copies the socket, and the copy can arrive parented to `root` instead of the
  bone it was given. The script reads the socket back and repairs it, then verifies; a socket
  silently on `root` puts the weapon at the character's feet.

Current state: 250 character meshes carry `WeaponGrip_R` on `hand_r`. Four head-and-helmet
meshes could not take it (they share the skeleton but have no arm geometry) and are harmless --
a head is never the mesh a weapon hangs off.

### 2.2 What the runtime does

All of it:

```cpp
WeaponMeshComponent->AttachToComponent(
    GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("WeaponGrip_R"));
```

No JSON read, no per-kind rotation, no per-skeleton conversion. If that line is ever followed
by a `SetRelativeRotation`, the standard has been broken and the fix belongs in the asset.

---

## Part 3 — Animation stances

### 3.1 The rule

A weapon does not name animations. It names a **stance**, in `UI/Weapons.json`:

```json
"stance": "Rifle"
```

A stance is a folder of clips following one naming pattern, so every lookup is composed:

```
/Game/Characters/Animations/Lyra/<Stance>/<Prefix>_<Stance>_<Clip>
```

`MM_` is the masculine set and `MF_` the feminine one; the character's existing locomotion-set
choice picks the prefix, which is the same choice it already makes for unarmed movement. When
a stance only ships one prefix for a clip, the loader falls back to `MM_`.

### 3.2 The clip vocabulary

These names are the contract. A stance folder that provides a clip must call it this:

| Clip | When it plays |
|---|---|
| `Idle_Hipfire` | held, not aiming — the base pose the whole upper body layers over |
| `Idle_ADS` | held, aiming (right mouse) |
| `Fire` | one shot, from the hip |
| `DryFire` | trigger pull on an empty magazine |
| `Reload` | the full reload |
| `Equip` | drawing it, on a weapon swap |
| `Melee` | the bash, for a gun used as a club |
| `Spawn` | the one-off flourish when the character first receives it |
| `IdleBreak_Scan` | ambient fidget while idle |
| `Jump_Start` / `Jump_Apex` / `Jump_Fall_Loop` / `Jump_Fall_Land` | airborne, holding it |

and locomotion, which is the same vocabulary the unarmed sets already use:

```
<Prefix>_<Stance>_<Gait>_<Dir>[_Start|_Stop|_Pivot]     Gait: Walk|Jog   Dir: Fwd|Bwd|Left|Right
<Prefix>_<Stance>_Crouch_<...>
<Prefix>_<Stance>_TurnLeft_90 | _180 | TurnRight_90 | _180
```

Several clips also ship an `_Additive` twin (`MM_Rifle_Reload_Additive`). Additives are the
preferred form for anything that should play *while* the legs keep moving — firing, reloading
and melee all read better layered onto locomotion than replacing it.

### 3.3 What exists today

| Stance | Clips | State |
|---|---|---|
| `Rifle` | 173 | complete: locomotion, crouch, jump, aim offsets, Fire, Reload, Equip, Melee, DryFire, idle breaks |
| `Pistol` | 150 | complete, same coverage |
| `Shotgun` | 9 | **partial**: `Fire`, `Reload`, `Melee`, `Idle_Hipfire`, `Idle_ADS` only. No locomotion, no `Equip`, no aim offsets |
| `Blade` | 0 | **missing entirely**. Melee weapons fall back to the unarmed Synty set |

A partial stance declares what to fall back to, in data, not code:

```json
"stances": { "Shotgun": { "fallback": "Rifle" } }
```

so `Shotgun/Equip` resolves to `Rifle/Equip` while `Shotgun/Reload` uses its own. The fallback
chain always terminates at `Unarmed`.

### 3.4 Mapping kinds to stances

Declared once, in `UI/Weapons.json` under `stance_by_kind`, and stamped onto each weapon so an
individual weapon can still override it:

```
Rifle, Assault Rifle, Marksman Rifle, SMG,
Heavy Gun, Launcher, Grenade Launcher, Laser, Alien Weapon   -> Rifle
Pistol, Paint, Shock                                          -> Pistol
Shotgun                                                       -> Shotgun
Sword, Dagger, Great Axe, Hand Axe, Hammer, Tool, Shuriken     -> Blade
Shield                                                        -> (off-hand; no stance)
```

Two-handed versus one-handed is a *stance* distinction, not a weapon one, which is why a
grenade launcher and an assault rifle share `Rifle`: the body holds them the same way.

---

## Part 4 — Working with the standard

### Adding a weapon

1. Put the mesh under `/Game/RepliCan/Weapons/<Pack>/`.
2. Add an entry to `UI/Weapons.json` with `name`, `kind`, `pack`, `mesh`, `description`.
3. Run `Tools/derive_weapon_data.py` — fills `ranged`, `muzzle`, `stance`.
4. Run `Tools/normalise_weapons.py` — bakes it to HAC1, stamps `"space": "hac1"`.
5. Run `Tools/render_weapon_grips.py` then `Tools/grip_sheet.ps1`, and look at the tile.

No code changes. If step 5 looks wrong, add an override in `UI/WeaponGrips.json` and repeat
step 4 — never patch it at runtime.

### Adding a skeleton

Run `Tools/add_grip_sockets.py`. Check the socket in the Skeleton editor with a normalised
rifle previewed on it. Nudge the socket, not the code.

### Adding a stance

1. Import the clips to `/Game/Characters/Animations/Lyra/<Stance>/`, named per 3.2.
2. Add the stance to `stances` in `UI/Weapons.json` with its fallback.
3. Point the relevant kinds at it in `stance_by_kind`.

### The smell test

If you are about to write `if (Kind == ...)` or `switch (Stance)` in C++, stop. The thing you
are branching on belongs in `Weapons.json`, `WeaponGrips.json`, a socket, or a baked mesh. The
runtime's job is to compose strings and attach transforms it was handed.

---

## Appendix — files

| File | Role |
|---|---|
| `UI/Weapons.json` | the catalogue: name, kind, pack, mesh, muzzle, ranged, stance, space stamp |
| `UI/WeaponGrips.json` | normalisation inputs: `axis_hints`, `grip_rule` per kind, per-weapon `overrides` |
| `Tools/measure_weapon_axes.py` | reports what the meshes currently are; writes `Tools/weapon_axes.json` |
| `Tools/normalise_weapons.py` | bakes meshes to HAC1 |
| `Tools/add_grip_sockets.py` | authors `WeaponGrip_R` / `_L` on every character skeleton |
| `Tools/derive_weapon_data.py` | fills muzzle, ranged and stance from the meshes and the kind map |
| `Tools/render_weapon_grips.py` | renders every weapon held, for review |
| `Source/RepliCan/WeaponCatalog.*` | reads the catalogue; exposes stance and clip-path composition |
