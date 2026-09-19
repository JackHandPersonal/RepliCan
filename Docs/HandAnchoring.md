# Hands on Weapons

*Why the hands drift, what the runtime does about it now, and what the pipeline should do next.
Companion to `Docs/HeldAssetStandard.md` (the mesh and socket conventions).*

## The honest answer to "why is this hard when shipped games manage it"

Shipped games do not get hands on weapons for free, and neither does Synty.

* **Every shipped weapon is authored against its animations.** A studio's animator poses the
  hands on THAT weapon for idle, aim, fire and reload, or the weapon carries hand sockets
  (`Grip_R`, `Grip_L`, per weapon, dragged into place in the mesh editor with the hand pose
  previewed) that the rig's IK reaches for. Either way it is per-weapon authored data, done once,
  visually. Systems like Lyra generalise it to a few weapon *classes*, and they still tune each
  weapon's sockets by hand.
* **Synty's weapon meshes carry none of that.** No sockets, no grip metadata, and pivots that
  follow five conventions across the library (`Docs/HeldAssetStandard.md` 1.2). Their animation
  packs are made against their own characters holding their own props in their own demo poses,
  and the Lyra clips this project uses were made for Lyra's rifle and pistol, not for a Synty
  wrench. So the parts are consistent *within* a pack and type, and not across packs, and never
  with someone else's animations. That is the nature of buying parts rather than a game.
* **First person is the hard case.** A shipped FPS draws separate view-model arms with
  animations authored per weapon relative to the camera; reach is never a question because the
  arms are drawn wherever the weapon is. This project uses the real body in first person (the
  Tarkov way), which is exactly the case that needs IK, reach limits and per-weapon hand data.

So: partly the assets (no grip data), partly the approach (true first person with generic
animations), and partly two defects in our own runtime that the numbers exposed. The
defects are fixed below; the asset gap is closed by making the per-weapon hand data a tunable,
saved value rather than a global.

## What was actually wrong (measured 2026-09-17)

1. **The sights put the weapon where the arm cannot go.** The carry table asks for a pistol's
   rear sight 56 to 60 cm in front of the EYE. The shoulder sits about 15 cm below and behind
   the eye and 18 cm out, so the hand target was 63 to 66 cm from the shoulder against a
   Mannequin arm of 57.5 cm. The IK clamps at 98.5 % of the arm, so the hand stopped 8 cm
   short and the weapon, driven by world transform, floated ahead of the fingers. The BugBuster
   (`Pistol1H`, 60 cm, sight at the grip) was the worst case; the two-handed pistols were
   borderline.
2. **The weapon and the hand were two systems.** Off the sights the weapon rode the hand
   socket; on them it was detached and placed from the eye with the hand IK chasing it. Every
   blend end swapped the two, and a wrench (never sight-aligned) and a pistol (always) behaved
   differently by construction.
3. **One wrist angle for every grip.** `TriggerHandRotation` (0, -20, 58) is a single global
   correction. A pistol's raked grip, a rifle's vertical grip and a wrench's handle all got the
   same wrist, and HAC1 says nothing about a grip's rake, so "a little off" on every gun that
   otherwise works is exactly this.
4. **The BugBuster's built-in scope is a closed tube.** The lens with the red dot sat at its
   rear opening, and behind the glass was the painted end cap of a solid Synty cylinder.

## What the runtime does now

* **The weapon never leaves the hand.** `PlaceWeapon` stores the pose the sights want
  (`SightSolvedWeaponWorld`); `TickHandIK` aims the trigger hand at the hand position that pose
  implies, with the IK weight following the sight blend; the weapon, a child of `WeaponGrip_R`,
  arrives with the hand. One system in every mode, melee included; no float, no hand-off.
* **Reach is a constraint on the solve.** `SolveWeaponPose` measures the right arm off its own
  bones, works out where the hand would have to be, and pulls the carry's forward component in
  until that point is within the arm's usable reach (the IK's own limit less a hair). The sight
  stays on the eye line and the barrel still converges on the point of aim; only the distance
  gives. The carry tables can therefore ask for arm's length and get "as far as this arm goes".
* **A per-weapon hand turn, saved in data.** `hand_rot` (pitch, yaw, roll, degrees) on a
  catalogue entry is added to the global correction for that weapon. Tune it in play with
  `HandRotWeapon p y r` (it writes the catalogue and reloads it) or on the Reference page; the
  global `HandRot` stays for the skeleton-wide part. This is the per-weapon socket every shipped
  game has, done as numbers instead of a drag, and it is the one thing left that needs eyes.
* **Numbers on screen.** `HandDiag` toggles a line: reach used, the carry pull-in factor, the
  gap between the weapon on the hand and where the sights wanted it (after animation), and the
  sight's distance from the eye line (after the camera). "Reliable" means these read near zero
  for every weapon, in every stance, and stay there.
* **The BugBuster looks through its tube.** `Tools/make_lens_optic.py` now also writes a body
  mesh with the scope's end caps cut out, so the eye sees the deck through the glass and the dot.

## What to fix in the pipeline next (in order of payoff)

1. **Measure the grip's rake per weapon** instead of tuning it by eye: the handle is the
   cluster of vertices below the origin; its principal axis against HAC1's +Z is the rake, and
   its width tells the finger curl. `Tools/derive_weapon_points.py` is where it belongs; it
   would write `hand_rot` for every gun in one run and leave only the odd ones to the console.
2. **Hand pose assets per grip family** (pistol grip, rifle grip, tool handle, knife) blended on
   the finger bones, instead of one global finger curl. Cheap, and it is most of what makes a
   hold read as a hold.
3. **Carry offsets as fractions of arm length**, not centimetres, so a small character and a
   big one hold the same pistol the same way.
4. **A grip sheet with the hands drawn.** `Tools/render_weapon_grips.py` renders the weapon
   alone; rendering it in the hand with `hand_rot` applied turns tuning into a review of one
   contact sheet per pack, which is how the studios do it.
5. **The support hand on one-handed weapons.** It idles from the clip today; a per-stance
   "off-hand pose" (a fist, an open hand, a hand on the forearm) is a small addition once the
   hand pose assets exist.

## The support hand (found 2026-09-17, after the first pass)

The left hand was twisted round backwards under a rifle's handguard. Measured on
`SK_Chr_SpaceSoldier_Male_01` with the `WeaponGrip_L` socket as authored: the old wrap
(`SupportHandRotation` roll 90, a guess) sent the fingers back along the barrel (-0.83, 0.49,
0.27) with the palm facing down (-0.39, 0.14, -0.91). The socket itself was a naive mirror of the
right one (roll negated), which is not a mirror of a rotation.

The wrap is now solved from the hand bone's own finger and palm directions: fingers across the
guard (+Y), palm up into it (+Z), thumb forward (0.92, 0.38, 0.05). `SupportHandRotation` is
(-16.0, -120.6, -85.7). `HandRotL p y r` tunes it live; `fore_hand_rot` on a catalogue entry (set
with `HandRotLWeapon`) turns the support hand per weapon, the way `hand_rot` does the trigger hand.

**The Reference page's points have no direction.** `grip`, `fore_grip`, `sight`, `muzzle` and
`optic_mount` are positions only. The hand's orientation on each point comes from the socket
(per skeleton), the character's wrap (`HandRot` / `HandRotL`) and the weapon's own turn
(`hand_rot` / `fore_hand_rot`). Dragging a point moves where the hand closes, never how it is
turned.

## Should a Synty gun import with no intervention?

Honestly: after the setup cost, most of it does, and a review pass per weapon still does not go
to zero. What runs unattended for every weapon: the HAC1 bake (axes and grip origin by rule),
the derived sight and fore-grip points, muzzle, stance from kind, icon, parts welded from the
demo maps. What a bare Synty mesh cannot tell a tool: how raked the grip is (the wrist angle),
where the support hand goes on an odd shape, which modelled scope to strip, which loose part
belongs where. Shipped games pay that per weapon too; they just pay it in an animator's day
rather than a console command. The realistic target is: import, look at the grip sheet, spend a
minute with `HandDiag`, `HandRotWeapon` and `HandRotLWeapon` on the odd ones. Item 1 of the list
above (derive the grip rake from the handle geometry) is what would push the odd ones toward
zero.

## What works (2026-09-17, third pass) -- the rules, nailed down

Read this before touching the hold again.

1. **The weapon rides `WeaponGrip_R`, always.** Nothing sets its world transform. The sight solve
   produces a target; the right arm's IK goes to it; the weapon arrives with the hand. The reach
   rule pulls the carry in when the arm cannot get there.
2. **The trigger hand's turn on the grip is data, not code.** Skeleton-wide: `TriggerHandRotation`,
   probed live (the numbers and the method are in `BaseCharacter.h`). Per weapon: `hand_rot` and
   `grip` in the catalogue, tuned in play with `HandRotWeapon` and the Reference page. The fire
   axe was a `grip` problem, not a hand problem: the bake's rule put its origin just under the head,
   so the hand held it like a hammer; `grip` (-30, 0, 0) moves the hand down the handle.
3. **The support hand's turn comes from the clip.** This is Lyra's answer. Lyra animates
   `ik_hand_l` against `ik_hand_gun` so the left hand rides the gun exactly as posed; our clips
   lost those bones in the Blender clean (41 tracks, none `ik_`), but they still pose BOTH hands on
   the gun. So the anim proxy reads the animated left hand relative to the animated right hand,
   carries that turn along with the solved right hand, and moves only the PLACE to this weapon's
   `fore_grip`. There is no wrap constant to tune any more: `SupportHandRotation` and the left
   socket's mirror only matter for a carried object, never for a two-handed weapon.
4. **Points are places; turns are turns.** `grip`, `fore_grip`, `sight`, `muzzle` on the Reference
   page are positions. Nothing dragged there rotates a hand.
5. **Measure before tuning.** `HandDiag` shows reach, pull-in, the hand gap after animation and the
   sight's distance off the eye line. If those read near zero and it still looks wrong, the fault is
   in a turn (rule 2) or a grip position (rule 2), never in the solve.

What Lyra does that we do not, for the record: separate first-person arm meshes with
per-weapon-class animation layers; `ik_hand_gun` / `ik_hand_l` / `ik_hand_r` tracks in every clip;
weapon sockets authored on the weapon skeletal meshes. The first is a project decision (true
first person here). The second we could restore by re-importing the Lyra clips without stripping
the IK bones, which would make rule 3 exact rather than derived. The third is what `grip` +
`hand_rot` + `fore_grip` are in numbers.

## The hand-tuning page (2026-09-17, fourth pass)

The numbers above are tuned by eye now, in the game, not by editing Weapons.json and relaunching.
Reference page -> a weapon's detail -> `[ TUNE HANDS ]` (under GIVE, beside the render).

What the page is: `UHandTuneWidget` (Source/RepliCan/HandTuneWidget.*), driven by
`ABasePlayerController::ShowHandTune(name)`. It tunes a STAND-IN, not the player: the player's own
likeness (Characters/Player.json) spawned through `SpawnBoothCharacter`, the same call the
character sheet's mirror uses, into the same booth at (0, 0, -30000). Two things about that call
are the reason to use it rather than spawning a pawn and applying a config: it spawns DEFERRED and
sets `DefaultCharacterConfigName` before `FinishSpawning`, so `BeginPlay` builds the modular body
from the file (an ordinary spawn with `ApplyCharacterConfig` afterwards leaves the bare crew rig
standing there in its overalls -- that was a real bug), and it brings its own lit set. The stand-in
is possessed by an `AHandTuneController` (an AAIController whose `UpdateControlRotation` is a
no-op, so the aim the page sets stays put -- a pawn reads its view rotation off its controller),
and made to hold that weapon with `ApplyWeaponToPawn` (the same call `RefreshHeldWeapon` uses, so the
hold on the page IS the hold in the game: sight solve, reach rule, carry, IK, finger curls, hunch,
pull, all of it). The player and the world do not change until SAVE, which writes the catalogue,
reloads it and refreshes the player's held weapon.

THE PAGE PAUSES THE GAME (the Reference page it opens from calls `SetPause(true)`, and hiding it
does not undo that). Everything the hold is made of runs on a tick: the carry blend and the sight
solve and the hand IK in `ABaseCharacter::Tick`, the weapon's final placement in the post-camera
tick function, the pose in the meshes' own ticks. A paused stand-in is therefore a statue with its
gun hanging by its leg, and the pictures never change even though they are being captured -- which
is exactly what the first two attempts looked like. `ABaseCharacter::SetTicksWhenPaused(true)`
unpauses that one character: the actor tick, `PostCameraTickFunction`, and every component (the
leader rig, the part followers, the weapon, the optic). Nothing else in the world is unpaused.
The same trick is what `ShowSheetMirrorFor` does to keep the sheet's likeness idling. Two orthographic `ASceneCapture2D`s see only
the stand-in (`PRM_UseShowOnlyList`), lit by three point lights of their own (the booth's are set
for a portrait), and are aimed and captured outright from the controller's tick (`TickHandTune`,
`CaptureScene()`; a player controller ticks while paused, so nothing is left to a capture's own
tick under a pause) at the trigger hand: the left picture from the
stand-in's right, the right picture from above.

Under the pictures, THE TABLE -- every number the hold is made of, one cell each:

| row (left column)      | cells                               | Weapons.json field |
|------------------------|-------------------------------------|--------------------|
| MAIN grip              | x y z (cm, HAC1 space)              | `grip`             |
| MAIN hand rot          | p y r (deg)                         | `hand_rot`         |
| MAIN fingers           | thumb index middle (deg per joint)  | `fingers_r`        |
| SUPPORT fore grip      | x y z (cm)                          | `fore_grip`        |
| SUPPORT fore hand rot  | p y r (deg)                         | `fore_hand_rot`    |
| SUPPORT fingers        | thumb index middle (deg per joint)  | `fingers_l`        |

| row (right column)     | cells                               | field              |
|------------------------|-------------------------------------|--------------------|
| SIGHTS hunch           | cm of shrug, AT THE SIGHTS ONLY     | `hunch`            |
| SIGHTS lean            | deg at the waist, AT THE SIGHTS ONLY| `lean`             |
| CARRY pull             | low / shldr / ads (cm ALONG THE BORE)| `pull`            |
| CARRY lateral          | low / shldr / ads (cm to the main side)| `lateral`       |
| CARRY low ready        | pitch, yaw (deg off the aim)        | `low_ready`        |
| ARMS elbow main        | low / shldr / ads (deg about the reach line)| `elbow_main` |
| ARMS elbow support     | the same for the other arm          | `elbow_support`    |
| EYELINE eye            | side, up, fwd (cm)                  | the CHARACTER file |

A cell the carry in view does not use is greyed and inert: the hunch away from the sights, low
ready's angles anywhere else, the pull or elbow column belonging to another carry. The buttons and
the CARRY / AIM choices stand in the strip beside the second picture; the page has no CLOSE of its
own (the header's X does that) and no note line (the cells already say the numbers).

Click a cell to pick it; roll the mouse wheel over a cell (or anywhere, for the picked one) to
change it: a notch is 0.5 cm, 2 deg, or 1 deg on a finger; shift is a fifth of that, ctrl five
times. The pawn takes every notch the same frame (`HandTuneAdjust` -> `HandTuneApply` -> the
character's setters, which re-seat the weapon on the socket at once). Moving the trigger hand's
grip moves the HAND along the weapon, not the weapon: the weapon is what the sight solve pins,
and `grip` says where on it the hand closes. LOW READY / SHOULDERED / ADS pins the carry
(`ABaseCharacter::CarryOverride`, -1 = the game decides); AIM HIGH / MIDDLE / LOW sets the
control pitch (+28 / 0 / -42) so the reach rule and the support hand are checked where they
drift. RESET is the catalogue's numbers; SAVE writes all six fields through `SaveWeaponField`
(the generalised `SaveHeldWeaponRotField`) and reloads the catalogue; CLOSE (or Escape)
restores the equipped weapon, the zoom and the Reference page.

`fore_hand_rot` finally does something: the anim proxy conjugates it through the weapon's
frame on the solved right hand (`HandIKForeDeltaWeapon` / `WeaponInHandR`) and applies it on top
of the clip-derived hold from rule 3. Zero means rule 3 untouched.

FINGER CURLS (`fingers_r` / `fingers_l`) are new data: degrees each phalanx closes on top of
the clip's hand, + closes, - opens, per finger. They are applied in the anim proxy after the IK
(`ApplyHandIK`), on the hands as just solved. The curl axis is read off the pose itself: the palm
normal is the plane of hand, index_01 and pinky_01 (mirrored for the left hand), the bone
direction comes from the reference skeleton's child offset in the bone's own frame, and the axis
is their cross product -- so there is no per-bone constant and the left hand's mirrored bone
axes do not matter. Each phalanx is set parent-first without its children ever being evaluated,
so the children follow (FCSPose keeps their local transforms). The thumb curls towards the palm
too, which reads as flexion; negative opens it out.

HUNCH (`hunch`, degrees) is head down, shoulders up, by the weapon: applied in the anim proxy
before the hands are read or solved (so the arms are solved from the hunched shoulders):
spine_03 pitches 0.2 H, neck_01 0.4 H, head 0.6 H (nose down), the clavicles roll 0.5 H each
(left +, right -: measured, roll +90 sends +Z to +Y, so + raises the left). Each turn is stated
in the ACTOR's frame and conjugated into component space through the mesh transform; each bone
is turned about its own origin, children following. Negative opens up: chin up, shoulders down.
This is separate from the GAIT hunch (GaitAdjustments.HunchDegrees, a stoop of the whole spine
used for sitting); the two compose.

LENGTH OF PULL (`pull`, cm) is the weapon along the aim: `SolveWeaponPose` adds it to the carry
offset's forward component, before the reach rule, so - brings the weapon in towards the
shoulder and + pushes it out (and the reach rule still refuses to push it past the arm).

FINGERS ARE THUMB, INDEX AND MIDDLE. Probed on the live pawn 2026-09-17: SK_Chr_Crew_Male_01 on
UE4_Mannequin_Skeleton has no ring and no pinky. The first curl asked for `pinky_01_r` to work out
which way the palm faced, never found it, and returned before curling anything -- the finger cells
did nothing at all and it looked like the feature was not wired up. The palm plane is now taken
from the outermost knuckle the hand HAS (pinky, else ring, else middle), and the table shows three
fingers. A fuller hand would work unchanged.

WHICH EYE. Measured on the stand-in with the rifle shouldered: the weapon's sight, muzzle and butt
all sit +3.20 cm to the character's right of the head bone, with the weapon's own yaw, pitch and
roll at zero -- so the gun is NOT canted across the face, and the eye offset does what it says.
3.2 cm is half a human interpupillary distance; on a stylised head, across a 150 cm capture frame,
that is about ten pixels and reads as dead centre, which looks like sighting down the nose. Hence
`eye_side` as a tuned number per weapon rather than a constant. The page draws a thin red rod from
the aiming eye straight down the aim (a component of the stand-in, which is what puts it inside the
captures' show-only list): line the sight up on the rod and the weld is right.

THE CAMERAS ORBIT THE WEAPON. A drag turns the camera's offset about the weapon's bounds centre in
THE PICTURE'S OWN FRAME -- sideways about the picture's up, vertically about the picture's right.
Each picture brings its own two axes: crossing the offset with the world's up (the first attempt)
works for the side view and gives the top view a ZERO axis, because it looks straight down, so the
top picture could not be dragged at all. Forty degrees each way, snapping back on release.

THE AIM PITCH ONLY EVER REACHED THE PLAYER (found by measurement, 2026-09-17). `AimRotationSteady`
reads the aim from the player camera manager and, failing that, fell straight through to the
ACTOR's rotation -- which is level by definition. So for anything not player-controlled, the
stand-in and every armed NPC alike, `AimPitchDegrees` was zero: the torso never leaned into the
aim and the head never followed it. Any pawn with a controller now reports its control rotation.
Measured on the stand-in before and after, as head tilt against aim pitch:

| aim pitch | before | after |
|-----------|--------|-------|
| 0         | 24.9   | 25.2  |
| +28       | 26.0   | 52.6  |
| -42       | 26.1   | -16.4 |

The head ends up following the aim very nearly one for one, because it inherits the spine's share
(`SpineLeanFraction`, 0.35) on top of its own (the remaining 0.65, split neck and head).

FINGER CURLS CLOSE THE WAY THEY ARE MEASURED TO, not the way a sign constant says. The first pass
took the palm's normal and flipped it for the left hand, which inverted some fingers -- they bent
backwards. Now each finger on each hand rotates a test vector ten degrees about its hinge and keeps
whichever direction brings the fingertip NEARER THE WRIST, which is what closing means. A value is
degrees per joint ON TOP OF THE CLIP, so a pointed index finger is a NEGATIVE number undoing the
clip's own curl (about -25 to -35 on a rifle), and zero is the clip's hand, not a straight hand.
Named shapes live in UI/Weapons.json under "finger_presets" and appear as buttons on the page.

THE ELBOW SITS ON A CIRCLE. Two-bone IK fixes the hand; the elbow is free anywhere on a circle
about the shoulder-to-hand line, and this solver deliberately takes that position from the clip
rather than inventing a pole (inventing one is how elbows flip through the torso). The cost is that
the elbow keeps the clip's flared plane even when the hand comes back and needs room, so the
weapon's "elbow" pair (trigger, support) twists the pole about that line: negative takes the
trigger elbow back behind the ribs.

Two things the page does not do, on purpose: it does not move the sight point (that is the
Reference page's SIGHT dot, a fact about the weapon, not the hand) and it does not estimate
anything -- every number shown is the number in the file after SAVE.


## First person: the view without a view model (2026-09-17)

Asked whether this wants a "virtual" first person -- a separate view model, as shipped shooters
use. The answer for now is no, and the two reasons people reach for one were dealt with directly.

CLIPPING. The ADS carry puts the sight sixteen centimetres from the eye and nothing overrides the
near clip plane, so it is the engine default of ten: the back of the optic and the receiver behind
it were inside the near plane and sliced open. UE 5.5 added a first-person rendering path and it is
exactly the separate pass one would otherwise hand-roll -- `SetFirstPersonPrimitiveType(FirstPerson)`
on a primitive, with `bEnableFirstPersonFieldOfView` / `bEnableFirstPersonScale` on the camera, draws
it with its own field of view and a COMPRESSED DEPTH RANGE so it cannot clip the world.
`ABaseCharacter::ApplyFirstPersonRendering` marks the weapon, its optic and the arms while the view
is first person and clears the mark when it is not.

JITTER. In first person the weapon rides the hand, so every twitch in an arm clip arrives at the eye
at full size. `PostCameraTick` now takes the weapon's pose into THE EYE'S OWN FRAME, follows it with
a frame-rate-independent damped spring (`ViewDampRate`), and puts it back. The eye's frame is the
point: a turn of the head is not motion there, so nothing lags when you look around, and only the
animation's own noise is removed. Bounded by `ViewDampMaxCm` / `ViewDampMaxDeg` so the weapon can
never leave the hand however bad a clip is; leaving the sights clears it and `ApplyWeapon` puts the
component back on its socket.

WHAT A VIEW MODEL WOULD STILL BUY, and when it becomes worth it: authored first-person animations --
reloads, inspects, malfunctions -- which exist only for a view model. The cost against it here is
a second arm rig and a second animation set per weapon, and arms assembled per character from the
cut library, so first-person arms would have to track appearance too. Nothing above blocks it later.

OPTICS ON EVERYTHING. There were eleven optics defined and three weapons carrying one, because an
optic needs an `optic_mount` and only those three had one. `Tools/find_optic_mounts.py` measures it
off the mesh: the highest point over the RECEIVER (between the grip and the half-way mark to the
muzzle, so a barrel or a foregrip cannot be mistaken for a rail) and near the centreline (so a side
rail cannot either). The optic is chosen by class -- red dot for rifles, SMGs and pistols, scope for
snipers, none for launchers or anything already tuned -- and it reports what it could not measure
rather than guessing.
