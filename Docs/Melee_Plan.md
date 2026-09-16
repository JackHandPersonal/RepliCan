# Melee Weapons: Plan

One- and two-handed melee, built on what the project already has. Written 2026-09-16.

## What exists today

- **Catalogue.** 46 weapons on the `Blade` stance (Sword 18, Dagger 9, Great Axe 7, Hand Axe 5, Hammer 2, Tool 2, Shuriken 3) and a dozen melee-looking kinds with no stance yet (Shock 3, Dagger 5, Hammer 4, Tool 4, Hand Axe 1, Sword 1). Every weapon already carries `damage`, `mass_kg`, `grip`, `muzzle` (the tip) and can carry `fore_grip`. Shields (7) sit on the ARMOR tab.
- **Stance.** `Blade` = the right arm from the clavicle, one-handed, no support hand. The stance system already does two-handed holds (`two_handed`, `fore_grip`, `carry` offsets) for guns; nothing stops a two-handed melee stance using the same machinery.
- **Animation.** The Synty Sword Combat set is imported (118 clips, retargeted to the Mannequin): light and heavy combos (A/B/C steps with `ReturnToIdle` and `RootMotion` twins), a stab, fencing and leaping attacks, block begin/loop/end, parry break, dodges, hit reactions (react, stagger, knockdown, stun), deaths, idles. Lyra adds a gun-butt melee clip per gun stance.
- **Code.** `ABaseCharacter::CombatAttack`, `CombatCombo`, `CombatDodge`, `PlayCombatSequence` and `IsInCombatAction` came over from the sandbox and still resolve clips by name from that set. The trigger path refuses a melee weapon today ("is not a firearm").
- **Damage.** Vitality, armour by body slot, head factor, per-limb injury and severing at the elbow and knee, ragdoll deaths, blood. All of it keys off a hit result and a damage number, so a swing only has to produce those.

## Design

### Weapon classes

| Class | Kinds | Stance | Hands | Attack set |
|---|---|---|---|---|
| Light blade | Dagger, Shock, Tool | `Blade` | 1 | LightFencing / stab; fast, short reach |
| Blade | Sword, Hand Axe | `Blade` | 1 | LightCombo, HeavyCombo, stab |
| Heavy | Great Axe, Hammer, two-handed swords | `Blade2H` (new) | 2 | HeavyCombo, HeavyStab; slow, long reach, knockdown |
| Thrown | Shuriken, Dagger (alt fire) | `Blade` | 1 | throw, later |

`Blade2H` = roots `spine_01` (the whole upper body swings), `two_handed: true`, its own `carry` offsets (the haft across the body at rest), and a `fore_grip` point on every heavy weapon derived by the point tool from the haft below the head. Shields are held in the off hand later and do not change the stance.

### Catalogue fields (new)

- `hands` 1 or 2 (drives the stance and the support-hand IK).
- `reach_cm` (defaults from the mesh: grip to tip along the blade).
- `swing_seconds` (defaults per class; overrides the clip's rate).
- `attack_set` (`light`, `blade`, `heavy`): which clip family the swings come from.
- `edge` point: where cutting starts along the blade (`grip` to `edge` is the haft, `edge` to `muzzle` the edge). Blunt weapons leave it empty and hit with the head.
- `blunt: true` for hammers and tools: no cuts, knockdown instead.

Backfilled by a tool (`Tools/backfill_melee.py`) from kind and mesh size, then tuned in the Reference like everything else.

### Input

- **Fire** on a melee weapon = a swing. Successive presses inside the recovery window chain the A/B/C combo steps; a press outside it starts over. Holding fire past 0.35 s charges a heavy swing (the heavy set, more damage, a knockdown on blunt weapons).
- **Aim** = guard: block begin, hold the loop, end on release. A press timed inside the first 0.2 s of an incoming blow is a parry (later, when anything swings at the player).
- **Dodge** stays on its key (`CombatDodge`), gated by stamina later.
- Wheel and number keys switch weapons as now; drawing a melee weapon plays the sheathed-to-idle clip.

### Swing and hit detection

No animation notifies. Each attack in the family table carries a hit window as a fraction of the clip (`[0.28, 0.55]` for a light swing, later for heavies). During the window, every tick sweeps the edge: the segment `edge`→`muzzle` in world space this frame against last frame's, traced as a fan of short line traces (five along the edge), on the Visibility channel, ignoring the wielder. The first hit per target per swing counts; a swing can still hit several targets.

Each hit goes through `ShotReactions::React` with the weapon's damage and a new `bMelee` flag:
- **Edged** weapons: the region rules as for bullets, blood, wound decal, and severing at the elbow or knee when the limb is destroyed. A sword takes an arm off in one or two hits by design (damage 40 to 60 against a 62 pool with the 50% limb rule).
- **Blunt** weapons: no blood decal, a velocity-change shove along the swing scaled by weapon mass, and a stagger or knockdown reaction instead of a flinch (the Sword Combat Hit set has both for extras; the player gets the same through the arm override).
- **Props**: the existing knock-about and burst rules, with a stronger shove.
- **Walls**: an impact effect at the point and a short recoil clip (the blade skips off); heavy weapons keep going.

A whoosh on every swing (from `RawAudio`, made with a tool like the reload sounds), a clang or thud per surface from Impacts.json's material table with two new rows (edge, blunt).

### Two-handed hold

The `Blade2H` stance carries the weapon in both hands: trigger hand at `grip`, support hand at `fore_grip` through the existing IK, with the `carry` offsets putting the haft across the body at rest, high in a guard while blocking. Swings come from the heavy set with `spine_01` roots, so the legs keep walking. In first person the same arms show (the FP rig is the same mesh), which is why the swing clips have to keep the weapon inside the view: the carry offsets and a small first-person pitch on the override handle that, as they do for guns.

### Reactions on the receiving end

Extras get the Sword Combat Hit set: react for light hits, stagger past a third of the pool in one blow, knockdown for a blunt heavy or a leg injury, stun on a head hit that does not kill. A knocked-down extra lies for two seconds on a partial ragdoll (legs only, the same LegFirst mechanism as the deaths) and gets up on the stand clip. Deaths use the death styles already built; an edged kill on a limb region severs as it does now.

### Shields, throws, later

- A shield in the off-hand slot: block on Aim without a stance change, the left hand IK'd to the shield's grip point; blocked damage reduced by its `armor_value`.
- Thrown kinds: alt fire throws the weapon as a physics prop along the aim with a spin, damage on impact through the same React path, pick it up again from the floor (Take).
- Stamina cost per swing and block once stamina exists on the sheet.

## Order of work

1. **Data and input.** Fields, backfill, `hands`, the Fire path switched to `MeleeSwing()` for non-firearms, one attack family (LightCombo) on the `Blade` stance, hit window and edge sweep, damage through React. Test on a junker with a sword: numbers in the fire note, blood, an arm off.
2. **Blunt and props.** `blunt`, the shove, stagger and knockdown clips on extras, walls and props.
3. **Two-handed.** `Blade2H` stance, fore-grip derivation for axes and hammers, heavy set, carry offsets tuned in play, first-person check.
4. **Guard.** Block and parry on Aim, block loop, blocked-damage rule, the shield variant.
5. **Combos, charge, dodge gating, sounds.** Combo chaining, the charged heavy, whoosh and impact sounds, stamina hooks.
6. **Throws.**

Each step is a build and a play test; nothing in it needs a Blueprint.

## Test checklist (per step)

- GIVE from the Reference: a Sword, a Dagger, a Great Axe, a Hammer, a Shuriken.
- Swing timing feels right at walk and run; the weapon stays in view in first person.
- A swing that misses costs a recovery; a hit lands once per target; two targets in an arc both take it.
- Fire note shows region, damage, armour, pool; the junker loses an arm to a sword and goes down to a hammer.
- Block holds, releases cleanly, does not stick when a page opens.
- No collision with the wielder's own capsule or parts; no hits through walls.
