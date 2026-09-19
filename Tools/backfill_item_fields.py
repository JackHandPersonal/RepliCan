"""Fills every catalogue entry's fields (Source/RepliCan/Items/ItemFields.h) with a best guess.

    python Tools/backfill_item_fields.py            # fills what is missing
    python Tools/backfill_item_fields.py --force    # recomputes every guessed field

Guesses come from what is already known: the name (Bottle is glass, Ration is paper, Keycard
activates a lock), the category, the measured size (mass is volume times a density for the
material, times how much of a bounding box a low-poly thing fills), and for weapons the kind
(a pistol and a sniper rifle do not share a magazine). Nothing here is authoritative: the
Reference page edits any of it, and a field a person has set is never overwritten without
--force. reviewdate stays empty until someone presses REVIEWED on the page.
"""
import io, json, os, re, sys, math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORCE = '--force' in sys.argv

DENSITY = {'metal': 1.0, 'plastic': 0.6, 'glass': 1.2, 'ceramic': 1.5, 'cloth': 0.3, 'paper': 0.4, 'organic': 0.8, 'rubber': 0.9, 'alien': 1.0}
FILL = 0.4   # how much of its bounding box a low-poly prop fills

def has(s, *words): return any(w in s for w in words)

def material_of(s, category):
    if category == 'weapons': return 'alien' if 'alien' in s else 'metal'
    if has(s, 'bottle', 'beaker', 'test_tube', 'testtube', 'vial', 'jar', 'glass'): return 'glass'
    if has(s, 'cup', 'mug', 'plate', 'bowl', 'ashtray'): return 'ceramic'
    if has(s, 'book', 'manual', 'notes', 'photo', 'sticky', 'paper', 'ration', 'packet', 'cigarette', 'clipboard', 'folder', 'board_game', 'name_tag', 'card'): return 'paper'
    if has(s, 'backpack', 'hat', 'hood', 'mask', 'pouch', 'cloth', 'plushie', 'bandana', 'glove', 'belt'): return 'cloth'
    if has(s, 'food', 'burger', 'donut', 'meat', 'ham', 'rib', 'noodle', 'snack', 'bug', 'vendor_product', 'sandwich'): return 'organic'
    if has(s, 'hose', 'tyre', 'tire', 'cable'): return 'rubber'
    if has(s, 'specimen', 'artifact', 'alien'): return 'alien'
    if has(s, 'helmet', 'tool', 'saw', 'wrench', 'torch', 'lantern', 'cell', 'battery', 'propane', 'tank', 'can', 'drill', 'cuff', 'grenade', 'mine', 'chestplate', 'armor', 'microscope', 'laser', 'extinguisher', 'oxygen'): return 'metal'
    return 'plastic'

def kind_of(s, category, e):
    if category == 'weapons': return e.get('kind', 'Weapon')
    if category == 'armor':
        for w, k in (('helmet', 'Helmet'), ('hat', 'Hat'), ('mask', 'Mask'), ('goggle', 'Goggles'), ('chest', 'Chestplate'), ('vest', 'Vest'), ('backpack', 'Backpack'), ('glove', 'Gloves'), ('boot', 'Boots'), ('belt', 'Belt'), ('pouch', 'Pouch'), ('headset', 'Headset'), ('suit', 'Suit')):
            if w in s: return k
        return 'Armor'
    if category == 'consumables':
        for w, k in (('med_kit', 'Medkit'), ('medkit', 'Medkit'), ('pill', 'Pills'), ('syringe', 'Syringe'), ('stim', 'Stim'), ('drink', 'Drink'), ('syncola', 'Drink'), ('bottle', 'Drink'), ('cup', 'Drink'), ('can', 'Drink'), ('food', 'Food'), ('ration', 'Ration'), ('snack', 'Food'), ('noodle', 'Food'), ('burger', 'Food'), ('donut', 'Food'), ('cigarette', 'Cigarette'), ('plate', 'Food'), ('tray', 'Food')):
            if w in s: return k
        return 'Consumable'
    if category == 'equipment':
        for w, k in (('keycard', 'Keycard'), ('swipecard', 'Keycard'), ('card', 'Keycard'), ('powercell', 'PowerCell'), ('cell', 'PowerCell'), ('battery', 'Battery'), ('grenade', 'Grenade'), ('mine', 'Mine'), ('torch', 'Flashlight'), ('lantern', 'Lantern'), ('radio', 'Radio'), ('phone', 'Phone'), ('pda', 'Datapad'), ('pad', 'Datapad'), ('scanner', 'Scanner'), ('camera', 'Camera'), ('tool', 'Tool'), ('saw', 'Tool'), ('drill', 'Tool'), ('book', 'Book'), ('manual', 'Book'), ('notes', 'Notes'), ('recorder', 'Recorder'), ('specimen', 'Specimen'), ('artifact', 'Artifact'), ('test_tube', 'Sample'), ('testtube', 'Sample'), ('beaker', 'Sample'), ('cuff', 'Restraint'), ('propane', 'FuelTank'), ('oxygen', 'AirTank'), ('microscope', 'Instrument'), ('laser', 'Instrument'), ('joystick', 'Controller'), ('game_console', 'Console'), ('microphone', 'Microphone'), ('handheld', 'Device'), ('surgical', 'Tool')):
            if w in s: return k
        return 'Equipment'
    for w, k in (('plushie', 'Toy'), ('bobble', 'Toy'), ('dice', 'Game'), ('board_game', 'Game'), ('sticky', 'Note'), ('paper', 'Paper'), ('photo', 'Photo'), ('name_tag', 'Badge'), ('junk', 'Junk'), ('spoon', 'Cutlery'), ('fork', 'Cutlery'), ('knife', 'Cutlery'), ('bowl', 'Dish'), ('pan', 'Pan'), ('vendor', 'Produce'), ('ashtray', 'Ashtray')):
        if w in s: return k
    return 'Curio'

WEAPON_STATS = {   # damage, fire_rate, magazine, reload_s, spread_hip, spread_aim, recoil, range_m, ammo, mass
    'pistol': (22, 5, 12, 1.6, 3.5, 0.8, 1.2, 40, 'light', 1.0), 'sidearm': (22, 5, 12, 1.6, 3.5, 0.8, 1.2, 40, 'light', 1.0),
    'smg': (14, 12, 30, 2.0, 4.0, 1.2, 1.0, 35, 'light', 3.0), 'rifle': (28, 9, 30, 2.4, 3.0, 0.6, 1.5, 80, 'medium', 3.8),
    'assault': (28, 9, 30, 2.4, 3.0, 0.6, 1.5, 80, 'medium', 3.8), 'marksman': (55, 3, 10, 2.6, 2.0, 0.3, 2.5, 150, 'medium', 4.5),
    'shotgun': (70, 1.2, 6, 3.0, 8.0, 4.0, 3.0, 20, 'shell', 3.5), 'sniper': (90, 0.8, 5, 3.2, 6.0, 0.2, 4.0, 200, 'heavy', 5.5),
    'heavy': (18, 15, 100, 5.0, 6.0, 2.0, 2.0, 60, 'heavy', 9.0), 'launcher': (120, 0.7, 1, 3.5, 5.0, 2.0, 5.0, 100, 'rocket', 7.0),
    'grenade': (90, 1.0, 4, 3.0, 5.0, 2.0, 3.0, 60, 'rocket', 5.0), 'laser': (20, 10, 50, 2.5, 1.0, 0.3, 0.4, 120, 'cell', 3.0),
    'alien': (35, 6, 20, 2.2, 3.0, 0.8, 2.0, 70, 'cell', 4.0), 'paint': (5, 8, 40, 2.0, 5.0, 2.0, 0.5, 20, 'light', 1.5),
    'shock': (25, 1.5, 0, 0, 0, 0, 0.5, 1.5, 'none', 1.5), 'blade': (30, 1.5, 0, 0, 0, 0, 0, 1.5, 'none', 1.2), 'sword': (35, 1.2, 0, 0, 0, 0, 0, 1.8, 'none', 1.6),
    'dagger': (15, 2.5, 0, 0, 0, 0, 0, 1.0, 'none', 0.5), 'axe': (40, 1.0, 0, 0, 0, 0, 0, 1.6, 'none', 2.5), 'hammer': (45, 0.8, 0, 0, 0, 0, 0, 1.6, 'none', 4.0),
    'shuriken': (12, 3, 0, 0, 2.0, 1.0, 0, 15, 'none', 0.2), 'tool': (8, 1.0, 0, 0, 0, 0, 0, 1.2, 'none', 1.5),
}

def weapon_stats(e):
    s = (e.get('kind', '') + ' ' + e.get('name', '') + ' ' + e.get('stance', '')).lower()
    for k, v in WEAPON_STATS.items():
        if k in s: return v
    return (20, 4, 10, 2.0, 3.0, 1.0, 1.0, 40, 'medium', 2.5)

def fill(e, key, category, is_weapon):
    """Every guessed field, for one entry; returns the dict of fields that were set."""
    s = key.lower() + ' ' + e.get('name', '').lower()
    size = e.get('size') or [10.0, 10.0, 10.0]
    vol = max(0.01, size[0] * size[1] * size[2] / 1000.0)
    out = {}
    def put(k, v):
        if FORCE or k not in e: out[k] = v
    kind = kind_of(s, category, e); put('kind', kind)
    tags = []
    for w, t in (('med', 'medical'), ('pill', 'medical'), ('syringe', 'medical'), ('alien', 'alien'), ('specimen', 'science'), ('test_tube', 'science'), ('beaker', 'science'), ('keycard', 'access'), ('card', 'access'), ('food', 'food'), ('drink', 'drink'), ('syncola', 'drink'), ('cigarette', 'vice'), ('junk', 'junk'), ('tool', 'tool'), ('saw', 'tool'), ('grenade', 'explosive'), ('mine', 'explosive'), ('cell', 'power'), ('battery', 'power'), ('chef', 'kitchen'), ('vendor', 'kitchen')):
        if w in s and t not in tags: tags.append(t)
    put('tags', tags)
    mat = material_of(s, category); put('material', mat)
    put('volume_l', round(vol, 2))
    if is_weapon:
        dmg, rate, mag, reload, sh, sa, rec, rng, ammo, mass = weapon_stats(e)
        put('mass_kg', mass); put('hands', 2 if e.get('stance') in ('Rifle', 'Shotgun') else 1); put('hold', 'Weapon')
        put('stack_max', 1); put('bulk', 4 if e.get('stance') in ('Rifle', 'Shotgun') else 2)
        put('equip_slot', 'Slot1' if e.get('stance') in ('Rifle', 'Shotgun') else 'Slot2')
        put('value', int(dmg * rate * 3 + rng)); put('rarity', 'rare' if 'alien' in s else 'common')
        put('use', 'equip'); put('use_time_s', 0.6); put('charges', 0); put('cooldown_s', 0.0); put('effects', [])
        put('durability_max', 0)
        put('damage', dmg); put('fire_rate', rate); put('ammo_kind', ammo); put('magazine', mag); put('reload_s', reload)
        put('spread_hip', sh); put('spread_aim', sa); put('recoil', rec); put('range_m', rng)
    else:
        mass = round(max(0.05, vol * DENSITY[mat] * FILL), 2); put('mass_kg', mass)
        put('hands', 2 if max(size) > 45 or mass > 6 else 1)
        put('hold', 'Worn' if category == 'armor' else ('Bottle' if has(s, 'drink', 'bottle', 'can', 'cup', 'syncola') else ('Pad' if has(s, 'pad', 'pda', 'phone', 'tablet') else ('Tool' if has(s, 'tool', 'saw', 'drill', 'torch', 'wrench') else 'Handheld'))))
        put('stack_max', 1 if category in ('armor', 'equipment') else (10 if has(s, 'grenade', 'cell', 'battery') else (5 if category == 'consumables' else 3)))
        put('bulk', int(min(6, max(1, math.ceil(vol / 2.0)))))
        slot = ''
        if category == 'armor':
            slot = 'Head' if has(s, 'helmet', 'hat', 'mask', 'goggle', 'headset', 'hood') else ('Chest' if has(s, 'chest', 'vest', 'backpack', 'suit', 'tank') else ('Hands' if 'glove' in s else ('Feet' if 'boot' in s else 'Acc')))
        elif category == 'consumables' and has(s, 'med', 'pill', 'syringe', 'stim'): slot = 'Healing'
        elif category == 'equipment' and has(s, 'tool', 'saw', 'drill', 'torch', 'scanner', 'handheld', 'laser'): slot = 'Slot2'
        put('equip_slot', slot)
        put('value', {'armor': 150, 'equipment': 60, 'consumables': 8, 'other': 3}[category] + int(mass * 5))
        put('rarity', 'rare' if has(s, 'alien', 'artifact') else 'common')
        use = 'none'
        if category == 'consumables': use = 'consume'
        elif category == 'armor': use = 'equip'
        elif has(s, 'keycard', 'swipecard', 'card', 'radio', 'phone', 'scanner', 'torch', 'lantern', 'recorder', 'pda', 'pad', 'handheld', 'laser', 'camera'): use = 'activate'
        elif has(s, 'book', 'manual', 'notes', 'photo', 'clipboard', 'folder'): use = 'read'
        elif has(s, 'grenade', 'mine'): use = 'throw'
        put('use', use); put('use_time_s', {'consume': 1.5, 'equip': 1.0, 'read': 2.0, 'activate': 0.5, 'throw': 0.8, 'none': 0.0}[use])
        put('charges', 1 if use in ('consume', 'throw') else 0); put('cooldown_s', 0.0)
        eff = []
        if has(s, 'med_kit', 'medkit'): eff = ['heal 40']
        elif has(s, 'pill'): eff = ['heal 10']
        elif has(s, 'syringe', 'stim'): eff = ['stamina 50']
        elif has(s, 'drink', 'syncola', 'bottle', 'cup', 'can'): eff = ['thirst -25']
        elif has(s, 'food', 'ration', 'snack', 'noodle', 'burger', 'donut', 'plate', 'tray', 'packet', 'vendor'): eff = ['hunger -20']
        elif has(s, 'keycard', 'swipecard', 'card'): eff = ['unlock KEY_' + re.sub(r'[^A-Za-z0-9]+', '_', key.split('/')[-1]).upper()]
        elif has(s, 'grenade'): eff = ['explode 60']
        elif has(s, 'mine'): eff = ['explode 90']
        elif has(s, 'cigarette'): eff = ['calm 10', 'health -1']
        put('effects', eff)
        put('durability_max', 100 if category == 'armor' or has(s, 'tool', 'saw', 'drill') else 0)
        if category == 'armor':
            put('armor_value', 15 if 'helmet' in s else (30 if has(s, 'chest', 'vest') else (5 if 'mask' in s else 2)))
            put('body_slot', slot if slot in ('Head', 'Chest', 'Hands', 'Feet') else 'Back')
            put('hides_hair', bool(has(s, 'helmet', 'hood', 'hat')))
    put('physics_on_drop', True); put('bounce', {'metal': 0.2, 'plastic': 0.4, 'glass': 0.1, 'ceramic': 0.1, 'cloth': 0.05, 'paper': 0.05, 'organic': 0.1, 'rubber': 0.6, 'alien': 0.3}[mat])
    put('droppable', True); put('sellable', True); put('quest', False); put('unique', False)
    put('sound_drop', 'drop_' + mat); put('sound_pickup', 'pickup_soft' if mat in ('cloth', 'paper', 'organic') else 'pickup_hard')
    put('sound_use', {'consume': ('drink' if has(s, 'drink', 'syncola', 'bottle', 'cup') else 'eat'), 'activate': 'beep', 'read': 'page', 'throw': 'whoosh', 'equip': 'equip', 'none': ''}[e.get('use', out.get('use', 'none'))])
    put('glow', bool(has(s, 'holo', 'neon', 'laser', 'glow', 'cell', 'crystal')))
    put('hidden', False); put('reviewdate', '')
    e.update(out)
    return out

def run(path, root_key, is_weapon):
    doc = json.load(io.open(path, encoding='utf-8'))
    n = 0; touched = 0
    for key, e in doc[root_key].items():
        cat = 'weapons' if is_weapon else e.get('category', 'other')
        out = fill(e, key, cat, is_weapon)
        n += 1; touched += 1 if out else 0
    io.open(path, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('%s: %d entries, %d filled' % (os.path.basename(path), n, touched))

run(os.path.join(ROOT, 'Content', 'GameData', 'UI', 'Weapons.json'), 'weapons', True)
run(os.path.join(ROOT, 'Content', 'GameData', 'UI', 'Items.json'), 'items', False)
