"""Give every optic an explicit `kind`: red_dot, zoomed, or scope.

The three are different answers to "what does the player see when the weapon comes up", not points
on one scale:

  red_dot   no magnification, no overlay. A mark on glass you look PAST. The gun stays in view.
  zoomed    magnified, still looked past: no mask, no black surround, the weapon stays visible, the
            view simply narrows. Right at low power, where a tube occludes more than the
            magnification is worth, and it keeps the peripheral vision a scope gives up.
  scope     the tube stops being drawn to its own shooter, a mask with a circular opening covers
            the view, and the world narrows by the magnification. The opening IS the picture.

Until now the kind was inferred, and the inference had no way to express "zoomed": WeaponCatalog
defaulted `overlay` to `zoom > 1.01`, so ANY magnified optic became a scope automatically. That is
why the catalogue has eight scopes and no zoomed sights -- not a decision anyone made.

This writes the kind each optic ALREADY behaves as, so nothing changes in play; it only makes the
classification explicit and authorable. Re-running is a no-op. To reclassify, edit `kind` in
UI/Weapons.json by hand (or pass `zoomed=<key>` here) -- that IS the switch now, and the runtime
follows it.

  Tools/ue_remote --file Tools/set_optic_kinds
  Tools/ue_remote --file Tools/set_optic_kinds -- zoomed=Factory_Wep_SMG_03 zoomed=SM_Wep_Scope_Small_01
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
except ImportError:                                   # runs fine outside the editor too
    CAT = _cat(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

RECLASSIFY = {}
for a in sys.argv[1:]:
    if '=' in a:
        kind, key = a.split('=', 1)
        if kind in ('red_dot', 'zoomed', 'scope'):
            RECLASSIFY[key] = kind


def kind_of(o):
    """What this entry behaves as today, from whatever it happens to carry."""
    zoom = float(o.get('zoom') or 0.0)
    levels = o.get('zoom_levels') or []
    if levels:
        zoom = float(levels[0])
    if zoom <= 1.01:
        return 'red_dot'                              # no magnification: nothing else can apply
    overlay = o.get('overlay')
    if overlay is None:
        overlay = True                                # the old inference: magnified meant a tube
    return 'scope' if overlay else 'zoomed'


cat = json.load(io.open(CAT, encoding='utf-8'))
optics = cat.get('optics', {})
plan = {}
for key, o in sorted(optics.items()):
    if key.startswith('_'):
        continue
    want = RECLASSIFY.get(key) or kind_of(o)
    if o.get('kind') != want:
        plan[key] = want

if plan:
    # Re-read immediately before writing: the running game rewrites this whole file through
    # ABasePlayerController::SaveWeaponField (sixteen times per TUNE HANDS save), so never write
    # back a copy parsed earlier.
    fresh = json.load(io.open(CAT, encoding='utf-8'))
    for key, want in plan.items():
        entry = fresh.setdefault('optics', {}).get(key)
        if entry is None:
            raise RuntimeError('%s vanished from the catalogue while this ran' % key)
        entry['kind'] = want
        # `overlay` stays in step so anything still reading it agrees with the kind.
        entry['overlay'] = (want == 'scope')
    fresh.setdefault('_optics_kind', 'red_dot | zoomed | scope -- see Tools/set_optic_kinds.py. '
                                     'red_dot: 1x, no overlay. zoomed: magnified, no overlay, no '
                                     'black surround, weapon stays visible. scope: tube hidden from '
                                     'its owner, circular opening over a blacked-out view, world '
                                     'narrowed by the magnification.')
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(fresh, indent=1) + '\n')

by_kind = {}
for key, o in sorted(optics.items()):
    if key.startswith('_'):
        continue
    by_kind.setdefault(plan.get(key) or o.get('kind') or kind_of(o), []).append((key, o))

print('set on %d optic(s)%s' % (len(plan), '' if plan else ' (already up to date)'))
for kind in ('red_dot', 'zoomed', 'scope'):
    rows = by_kind.get(kind, [])
    print('\n%s (%d)' % (kind.upper(), len(rows)))
    for key, o in rows:
        z = (o.get('zoom_levels') or [o.get('zoom') or 1])[0]
        print('   %-26s zoom %-5s %s' % (key, z, '<- changed' if key in plan else ''))

low = [k for k, o in by_kind.get('scope', []) if float((o.get('zoom_levels') or [o.get('zoom') or 1])[0]) <= 2.01]
if low:
    print('\nScopes at 2x or under -- the usual candidates for `zoomed`, since a tube costs more '
          'peripheral vision than 2x of magnification is worth:')
    for k in low:
        print('   %s' % k)
