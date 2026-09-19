"""MAKE and MODEL for every weapon and optic in UI/Weapons.json, from Docs/Arms_Manufacturers.md.

    python Tools/backfill_make_model.py

The model is the entry's existing display name (it stays the record's identity under "name");
the make is chosen by pack and kind, the way the manufacturers document maps them. An entry
that already has both is left alone, so hand edits in the Reference survive a re-run.
"""
import json, io, re
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'

# (pack, keyword in the key, make) -- first match wins; the pack's last row is its default
RULES = [
    ('Space',     'Pistol',       'Helion Standard Arms'),
    ('Space',     'Rifle',        'Helion Standard Arms'),
    ('Space',     'Shotgun',      'Dockside Foundry'),
    ('Space',     'Revolver',     'Dockside Foundry'),
    ('Space',     '',             'Helion Standard Arms'),
    ('Worlds',    'Alien',        'Unattributed'),
    ('Worlds',    'Unique',       'Kesh Provenance'),
    ('Worlds',    'Sniper',       'Cairn Outfitting'),
    ('Worlds',    'Shotgun',      'Tolliver Arms Cooperative'),
    ('Worlds',    'Revolver',     'Tolliver Arms Cooperative'),
    ('Worlds',    'Sword',        'Ochre & Sons'),
    ('Worlds',    'Dagger',       'Ochre & Sons'),
    ('Worlds',    'Axe',          'Ochre & Sons'),
    ('Worlds',    'Tool',         'Sunder Industrial'),
    ('Worlds',    'Laser',        'Coherent Light Company'),
    ('Worlds',    'Beam',         'Coherent Light Company'),
    ('Worlds',    'Plasma',       'Aurex Beamworks'),
    ('Worlds',    'Launcher',     'Kessler-Rand Armaments'),
    ('Worlds',    'Shield',       'Bastion Civil Systems'),
    ('Worlds',    '',             'Amazonis'),
    ('CyberCity', 'Shuriken',     'Harrow Street Cutlery'),
    ('CyberCity', 'Dagger',       'Ochre & Sons'),
    ('CyberCity', 'Sword',        'Ochre & Sons'),
    ('CyberCity', 'Laser',        'Coherent Light Company'),
    ('CyberCity', 'Paint',        "Paint Gun Pete's"),
    ('CyberCity', 'Rifle',        'Sixth Ward Machine Co.'),
    ('CyberCity', '',             'Kato Pattern Works'),
    ('Horror',    'Flamethrower', 'Kessler-Rand Armaments'),
    ('Horror',    'Mining',       'Vane Mining Systems'),
    ('Horror',    'Cutter',       'Vane Mining Systems'),
    ('Horror',    'Shock',        'Sunder Industrial'),
    ('Horror',    'Surgical',     'Sunder Industrial'),
    ('Horror',    '',             'Northlight Defense Group'),
    ('Police',    'Launcher',     'Kessler-Rand Armaments'),
    ('Police',    'Rifle',        'Bastion Civil Systems'),
    ('Police',    '',             'Meridian Public Order'),
    ('Military',  '',             'Amazonis'),
    ('SciFiCity', 'Plasma',       'Aurex Beamworks'),
    ('SciFiCity', 'Rifle',        'Northlight Defense Group'),
    ('SciFiCity', '',             'Sixth Ward Machine Co.'),
]
OPTIC_RULES = [('RedDot', 'Halcyon Optics'), ('Small', 'Meridian Sightworks'), ('Sniper', 'Vestergaard Precision'), ('Alien', 'Unattributed'), ('', 'Kesh Provenance')]

def make_for(pack, key):
    for p, kw, make in RULES:
        if p == pack and (not kw or kw.lower() in key.lower()): return make
    return 'Unattributed'

doc = json.load(io.open(CAT, encoding='utf-8'))
done = {'weapons': 0, 'optics': 0, 'kept': 0}
for key, e in doc['weapons'].items():
    if e.get('make') and e.get('model'): done['kept'] += 1; continue
    pack = key.split('/')[0]
    e['make'] = e.get('make') or make_for(pack, key)
    e['model'] = e.get('model') or e.get('name') or key.split('/')[-1]
    done['weapons'] += 1
for key, o in doc.get('optics', {}).items():
    if o.get('make') and o.get('model'): done['kept'] += 1; continue
    o['make'] = o.get('make') or next(m for kw, m in OPTIC_RULES if not kw or kw.lower() in key.lower())
    o['model'] = o.get('model') or o.get('name') or re.sub(r'^SM_Wep_', '', key).replace('_', ' ')
    done['optics'] += 1
io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
print('MAKE/MODEL', done)
