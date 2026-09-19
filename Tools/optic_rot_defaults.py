"""Write the measured yaw into every optic that is authored across the aim.

Tools/optic_axes measures which optic meshes run along Y instead of X. This puts that finding into
the catalogue as the optic's own `rot`, so it mounts facing down the barrel instead of sideways.

The bounding box can tell ACROSS from ALONG, but it cannot tell front from back: a scope yawed the
wrong way round is still along the aim, just backwards. So this writes +90 and leaves the sign to be
flipped by eye if it is wrong -- two clicks in the hand tuning page, once those rows exist.

Nothing is overwritten: an optic that already carries a `rot` was set deliberately and is left alone.

  python Tools/optic_rot_defaults.py
"""
import io, json, collections

PATH = 'UI/Weapons.json'
ACROSS = ['SM_Wep_Scope_Alien_01', 'SM_Wep_Scope_Small_01', 'SM_Wep_Scope_Small_02',
          'SM_Wep_Scope_Small_03', 'SM_Wep_Scope_Sniper_01', 'SM_Wep_Scope_Sniper_02',
          'SM_Wep_Scope_Sniper_03']

d = json.load(io.open(PATH, encoding='utf-8'), object_pairs_hook=collections.OrderedDict)
optics = d.get('optics', {})
done = []
for key in ACROSS:
    o = optics.get(key)
    if o is None:
        print('   not in the catalogue, skipped: %s' % key)
        continue
    if 'rot' in o:
        print('   already set, left alone: %s -> %s' % (key, o['rot']))
        continue
    o['rot'] = [0, 90, 0]
    done.append(key)
io.open(PATH, 'w', encoding='utf-8', newline='\n').write(json.dumps(d, indent=1) + '\n')
print('optics turned onto the aim: %d' % len(done))
for k in done:
    print('   %s  rot [0, 90, 0]' % k)
