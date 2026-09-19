"""Melee fields for every non-ranged weapon in UI/Weapons.json (see Docs/Melee_Plan.md): hands,
attack_set, blunt, and a stance for the kinds that had none. Existing values are kept unless
FORCE. Shields are left alone: they are held, not swung, for now.
    python Tools/backfill_melee.py
"""
import json, io, collections
P = 'C:/Dev/Games/RepliCan/Content/GameData/UI/Weapons.json'
FORCE = False
d = json.load(io.open(P, encoding='utf-8'))
tally = collections.Counter()
for k, e in d['weapons'].items():
    if e.get('ranged') or e.get('kind') == 'Shield': continue
    kind = e.get('kind', ''); name = (e.get('name', '') + ' ' + k).lower()
    two = kind in ('Great Axe', 'Hammer') or 'great' in name or 'two_hand' in name or 'sledge' in name
    aset = 'light' if kind in ('Dagger', 'Shock', 'Tool', 'Shuriken') else 'heavy' if kind in ('Great Axe', 'Hammer') else 'blade'
    blunt = kind in ('Hammer', 'Tool', 'Shock')
    for key, val in (('hands', 2 if two else 1), ('attack_set', aset), ('blunt', blunt)):
        if FORCE or key not in e: e[key] = val; tally[key] += 1
    if two and e.get('hands') != 2: e['hands'] = 2; tally['hands'] += 1   # an earlier pass wrote 1 on everything
    if not e.get('stance'): e['stance'] = 'Blade'; tally['stance'] += 1
io.open(P, 'w', encoding='utf-8', newline='\n').write(json.dumps(d, indent=1, ensure_ascii=False))
print('backfilled', dict(tally))
