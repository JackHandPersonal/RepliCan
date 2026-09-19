"""Put an optic's `eye` back on its own glass.

`eye` is the point the ADS solve brings to the eye line, so it has to BE the thing you look
through: the centre of the rear lens, which is also exactly where the reticle material draws its
dot (M_RedDot centres the dot on its LensCentre parameter, in the optic's own space). When the two
disagree, the weapon is placed so that a point in mid-air lands on your eye line and the sight sits
wherever it sits -- you are not looking through it at all, and no amount of tuning the carry, the
hands or the mask downstream will fix that.

The factory scopes inherited the WEAPON's derived rear-sight point rather than their own eyepiece
(Tools/promote_factory_scopes.py), which is right only when a weapon's iron rear sight happens to
sit where its scope's glass does. Measured 2026-09-19:

    Factory_Wep_SMG_03     eye [-34.16, 0, 22.43]   glass [ 6.44, 0, 26.08]   40.6 cm out
    Factory_Wep_Heavy_01   eye [ 25.57, 0, 22.48]   glass [36.38, 0, 38.49]   14.7 cm out

This writes the glass point into the named optics only, and REPORTS every other optic's disagreement
without touching it -- an `eye` may have been tuned by hand, and a tool that quietly reclaims all of
them would undo that. Name more keys on the command line to convert them.

Each write is checked before it lands: the new point must lie within the optic's own bounds and at
its REARWARD end (weapon +X is downrange, so the eyepiece is the low-X end once the optic's `rot`
has been undone). A lens centre that came out at the objective end would aim the player through the
wrong end of the scope, so it is refused rather than written.

  Tools/ue_remote --file Tools/fix_optic_eye_points
  Tools/ue_remote --file Tools/fix_optic_eye_points -- Factory_Wep_Sniper_02 Factory_Wep_Alien_04
"""
import unreal, json, io, os, sys

CAT = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
MEL = unreal.MaterialEditingLibrary

# The ones this run is allowed to change. Everything else is reported only.
FIX = [a for a in sys.argv[1:] if not a.startswith('-')] or ['Factory_Wep_SMG_03', 'Factory_Wep_Heavy_01']


def rotated(v, rot):
    """The optic's own space into the weapon's, the way the catalogue stores `eye`."""
    if not rot or not any(rot):
        return list(v)
    # NAMED, NOT POSITIONAL: unreal.Rotator(a, b, c) fills (roll, pitch, yaw), while the catalogue
    # stores (pitch, yaw, roll) as FRotator does.
    R = unreal.Rotator(pitch=float(rot[0]), yaw=float(rot[1]), roll=float(rot[2]))
    p = unreal.MathLibrary.quat_rotate_vector(R.quaternion(), unreal.Vector(v[0], v[1], v[2]))
    return [p.x, p.y, p.z]


def glass_of(o):
    """The centre of this optic's rear lens, in the optic's own space, or None."""
    asset = unreal.load_asset((o.get('mesh') or '').split('.')[0])
    if not asset:
        return None, None
    box = asset.get_bounding_box()
    for sm in asset.get_editor_property('static_materials'):
        mi = sm.material_interface
        if mi and 'Reticle' in mi.get_name():
            c = MEL.get_material_instance_vector_parameter_value(mi, 'LensCentre')
            return [c.r, c.g, c.b], box
    return None, box


cat = json.load(io.open(CAT, encoding='utf-8'))
optics = cat.get('optics', {})
weapons = cat.get('weapons', {})
fits = {}
for wk, w in weapons.items():
    if w.get('optic'):
        fits.setdefault(w['optic'], []).append(wk)

changed, reported, refused = [], [], []
for okey, o in sorted(optics.items()):
    glass, box = glass_of(o)
    if glass is None:
        continue
    want = [round(v, 3) for v in rotated(glass, o.get('rot'))]
    have = [float(v) for v in (o.get('eye') or [0, 0, 0])]
    gap = sum((want[i] - have[i]) ** 2 for i in range(3)) ** 0.5
    if gap < 0.5:
        continue                                   # already on the glass
    if okey not in FIX:
        reported.append((okey, have, want, gap, fits.get(okey, [])))
        continue

    # THE CHECKS. The point has to be on the optic, and at the end you put your eye to.
    lo, hi = box.min, box.max
    pad = 1.0
    inside = (lo.x - pad <= glass[0] <= hi.x + pad and lo.y - pad <= glass[1] <= hi.y + pad
              and lo.z - pad <= glass[2] <= hi.z + pad)
    long_axis = max((hi.x - lo.x, 'x'), (hi.y - lo.y, 'y'), (hi.z - lo.z, 'z'))[1]
    span = {'x': (lo.x, hi.x, glass[0]), 'y': (lo.y, hi.y, glass[1]), 'z': (lo.z, hi.z, glass[2])}[long_axis]
    # Rearward means the low end of the long axis once `rot` maps that axis onto the weapon's -X.
    at_rear = (span[2] - span[0]) <= (span[1] - span[0]) * 0.35
    if not inside or not at_rear:
        refused.append((okey, want, 'not on the optic' if not inside else
                        'at the objective end, not the eyepiece (long axis %s)' % long_axis))
        continue
    o['eye'] = want
    changed.append((okey, have, want, gap, fits.get(okey, [])))

if changed:
    # RE-READ, THEN APPLY, THEN WRITE -- never write back the copy parsed at the top of the script.
    # The game writes this same file: ABasePlayerController::SaveWeaponField is a full
    # load-from-disk / set one field / reserialise the WHOLE root / save, and one click of SAVE on
    # the TUNE HANDS page runs it sixteen times in a row (HandTuneSave). It only ever edits the
    # `weapons` object, but it rewrites the root, so anything written to `optics` between its load
    # and its save is gone.
    #
    # Remote-exec Python runs ON the game thread, so the game cannot tick -- and therefore cannot
    # save -- while this script is between its read and its write, which makes the window here very
    # nearly zero. Very nearly is not zero: asset loads in the loop above can flush and pump. This
    # costs three lines, and it is the right shape for any tool sharing a file with the running
    # game, so it should not depend on a reader knowing the threading argument.
    fresh = json.load(io.open(CAT, encoding='utf-8'))
    for okey, _have, want, _gap, _ws in changed:
        entry = fresh.setdefault('optics', {}).get(okey)
        if entry is None:
            raise RuntimeError('%s vanished from the catalogue while this ran' % okey)
        entry['eye'] = want
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(fresh, indent=1) + '\n')

print('written (%d):' % len(changed))
for k, have, want, gap, ws in changed:
    print('   %-24s %-24s -> %-24s  %5.1f cm   fitted on %s'
          % (k, [round(v, 2) for v in have], [round(v, 2) for v in want], gap, ', '.join(ws) or '-'))
if refused:
    print('refused (%d):' % len(refused))
    for k, want, why in refused:
        print('   %-24s %-24s  %s' % (k, [round(v, 2) for v in want], why))
print('off their glass but left alone (%d) -- name one to convert it:' % len(reported))
for k, have, want, gap, ws in sorted(reported, key=lambda r: -r[3]):
    print('   %-24s %-24s would be %-22s %5.1f cm   fitted on %s'
          % (k, [round(v, 2) for v in have], str([round(v, 2) for v in want]), gap, ', '.join(ws) or '-'))
