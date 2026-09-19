"""Mark the weapons whose sight is built in and cannot be swapped.

A weapon with "optic_fixed": true has its optic as part of the gun. The BugBuster is the case that
prompted this: its red dot is its OWN geometry -- the tube is body mesh, cut through the bore by
Tools/make_lens_optic.py so it can be looked through, and only the glass disc is a separate optic
asset. There is nothing to unclip and nothing another sight could clamp onto, so the choice should
never be offered rather than offered and then silently ignored.

Reading a fixed optic is unchanged: it still has a kind, a zoom, a reticle and an eye point like
any other. Only CHANGING it is refused.

  Tools/ue_remote --file Tools/set_optic_fixed
  Tools/ue_remote --file Tools/set_optic_fixed -- Worlds/Wep_Sniper_02
"""
import json, io, os, sys


def _cat(root):
    """UI/Weapons.json moved under Content/GameData/ so a packaged build stages it. Try the new
    home first and fall back to the old one, so this tool cannot quietly write a file nobody
    reads -- and shout rather than inventing a path if neither is there."""
    import os as _os
    for p in (_os.path.join(root, 'Content', 'GameData', 'UI', 'Weapons.json'),
              _os.path.join(root, 'UI', 'Weapons.json')):
        if _os.path.exists(p):
            return p
    raise SystemExit('Weapons.json is at neither Content/GameData/UI nor UI, under ' + root)


try:
    import unreal
    CAT = _cat(unreal.Paths.project_dir())
except ImportError:
    CAT = _cat(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

FIXED = [a for a in sys.argv[1:] if not a.startswith('-')] or ['Worlds/Wep_Pistol_05']

cat = json.load(io.open(CAT, encoding='utf-8'))
weapons = cat.get('weapons', {})

missing = [k for k in FIXED if k not in weapons]
if missing:
    raise SystemExit('not in the catalogue: %s' % ', '.join(missing))
# A weapon with no optic fitted cannot have a fixed one -- that would freeze it at "no sight".
bare = [k for k in FIXED if not (weapons[k].get('optic') or '').strip()]
if bare:
    raise SystemExit('these have no optic to fix in place: %s' % ', '.join(bare))

plan = [k for k in FIXED if not weapons[k].get('optic_fixed')]
if plan:
    # Re-read immediately before writing; the running game rewrites this whole file through
    # SaveWeaponField, sixteen times per TUNE HANDS save.
    fresh = json.load(io.open(CAT, encoding='utf-8'))
    for k in plan:
        fresh['weapons'][k]['optic_fixed'] = True
    fresh.setdefault('_optic_fixed', 'true = the sight is built into the weapon and cannot be '
                                     'swapped; see Tools/set_optic_fixed.py. Reading it is normal, '
                                     'only changing it is refused.')
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(fresh, indent=1) + '\n')

now = [k for k, w in sorted(weapons.items()) if w.get('optic_fixed') or k in plan]
print('set on %d weapon(s)%s' % (len(plan), '' if plan else ' (already up to date)'))
print('weapons whose optic is fixed (%d):' % len(now))
for k in now:
    w = weapons[k]
    print('   %-26s %-30s optic %s' % (k, w.get('name'), w.get('optic')))
