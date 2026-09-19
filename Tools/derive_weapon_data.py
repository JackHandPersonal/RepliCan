"""Fills in the per-weapon data the firing code needs, derived from the meshes themselves.

    python Tools/ue_remote.py --file Tools/derive_weapon_data.py

Adds two fields to every entry in UI/Weapons.json:

  ranged  whether it shoots. Taken from the sound family, which already groups the catalogue
          into pistol/smg/rifle/sniper/shotgun/heavy/launcher/laser/alien and blade/blunt.
  muzzle  where the shot leaves the weapon, in the mesh's own local space.

The muzzle is DERIVED rather than authored, because authoring it a hundred times by hand is how
this kind of thing never gets finished. Synty weapons are modelled running along their local Y
with the business end at +Y -- the same assumption the reference booth already makes when it
places its muzzle flash -- so the muzzle is the far +Y face of the bounds, centred across the
other two axes. Anything that comes out looking wrong can be corrected by hand afterwards: the
script only fills a muzzle in when the entry does not already have one, so hand edits survive.
"""
import unreal, json, io, collections

CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'
GUN_FAMILIES = {'wep_pistol', 'wep_smg', 'wep_rifle', 'wep_sniper', 'wep_shotgun',
                'wep_heavy', 'wep_launcher', 'wep_laser', 'wep_alien'}

doc = json.load(io.open(CAT, encoding='utf-8'))
weapons = doc['weapons']
stats = collections.Counter()
missing = []

for key, e in sorted(weapons.items()):
    family = (e.get('sound') or '').replace('.wav', '')
    e['ranged'] = family in GUN_FAMILIES
    stats['ranged' if e['ranged'] else 'melee'] += 1

    if 'muzzle' in e:
        stats['muzzle kept'] += 1
        continue
    mesh = unreal.load_asset(e['mesh'])
    if not mesh:
        missing.append(e['mesh']); continue
    b = mesh.get_bounding_box()
    centre = (b.min + b.max) * 0.5
    # Far +Y face, centred across X and Z. For a melee weapon this lands on the tip, which is
    # the right answer there too: it is where a trace for a swing should start from.
    e['muzzle'] = [round(centre.x, 2), round(b.max.y, 2), round(centre.z, 2)]
    stats['muzzle derived'] += 1

io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
print('WEAPONS', len(weapons), dict(stats))
if missing:
    print('MESHES MISSING', len(missing))
    for m in missing[:5]: print('   ', m)
