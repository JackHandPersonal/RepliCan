"""Which of the packs' props could be IN A CHARACTER'S INVENTORY -- a soda, a power cell, a
keycard -- as against a cart, a wall segment or a vending machine.

    python Tools/ue_remote.py --file Tools/survey_items.py

Every static mesh under the packs' Props and Attachments folders is measured and sorted by two
things: what its NAME says it is (the words Synty uses: Drink, Ration, PowerCell, Keycard,
Helmet...) and how BIG it is (a hand-held thing is a few centimetres to about seventy on its
longest side and light in volume; a cart is not). Sub-parts of assemblies (_Lid, _Door, _Arm,
_Glass, _Trigger...) are not things. The result is RawArt/item_survey.json: every candidate with
its size, its category (armor / equipment / consumables / other) and the word that decided it,
plus everything rejected and why, so the rules can be argued with. Tools/build_item_catalog.py
turns the accepted ones into UI/Items.json.
"""
import unreal, io, json, os, re, traceback

OUT = os.path.join(unreal.Paths.project_dir(), 'RawArt', 'item_survey.json')
FOLDERS = ['/Game/PolygonSciFiSpace/Meshes/Props', '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/Attachments',
           '/Game/PolygonSciFiWorlds/Models/Props', '/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments',
           '/Game/PolygonCyberCity/Meshes/Props', '/Game/Synty/PolygonSciFiHorror/Meshes/Props',
           '/Game/PolygonMech/Models/Characters/Character_Attachments', '/Game/PolygonMech/Models/Props',
           '/Game/Synty/PolygonPoliceStation/Meshes/CharacterAttachments', '/Game/Synty/PolygonPoliceStation/Meshes/Props', '/Game/Synty/PolygonPoliceStation/Meshes/Weapons',
           '/Game/PolygonMilitary/Meshes/Characters/Attachments', '/Game/PolygonMilitary/Meshes/Items']
MAX_SIDE, MIN_SIDE, MAX_VOLUME_L = 90.0, 3.0, 150.0     # cm, cm, litres of bounding box (a power cell is 80 long)

# The words, in the order they are tried: the first category whose word appears wins.
CONSUMABLE = ('food', 'drink', 'soda', 'syncola', 'bottle', 'ration', 'snack', 'noodle', 'pill', 'med_kit', 'medkit', 'syringe',
              'stim', 'burger', 'coffee', 'mug', 'cup', 'packet', 'cigarette', 'beer', 'water_bottle', 'chips', 'sandwich', 'fruit',
              'apple', 'bread', 'meat', 'ham', 'rib', 'plate', 'tray', 'jar', 'can_', 'cans', 'bandage', 'injector', 'vial', 'ampoule')
ARMOR = ('helmet', 'armor', 'armour', 'vest', 'backpack', 'mask', 'goggle', 'glove', 'boot', 'shield', 'suit', 'pauldron', 'shoulder',
         'chest', 'belt', 'visor', 'hat', 'cap_', 'hood', 'bandana', 'kneepad', 'elbow', 'gauntlet', 'jetpack', 'harness')
EQUIPMENT = ('syringe_gun', 'grenade', 'propane', 'beaker', 'clipboard', 'microscope', 'cuff', 'laser', 'chef_tool', 'tool', 'drill', 'scanner', 'radio', 'phone', 'pda', 'tablet', 'keycard', 'swipecard', 'card', 'battery', 'powercell',
             'power_cell', 'cell', 'cartridge', 'torch', 'lantern', 'flashlight', 'recorder', 'camera', 'joystick', 'manual', 'book',
             'notes', 'handheld', 'datapad', 'pad', 'extinguisher', 'oxygen', 'mine', 'grenade', 'test_tube', 'testtube', 'surgical',
             'artifact', 'specimen', 'device', 'remote', 'detonator', 'beacon', 'sensor', 'binocular', 'compass', 'wrench', 'hammer',
             'screwdriver', 'saw', 'crowbar', 'multitool', 'welder', 'gadget', 'chip', 'drive', 'disk', 'key', 'lock', 'rope', 'grapple',
             'kit', 'canister', 'fuel', 'gascan', 'tape', 'glowstick', 'flare', 'walkie', 'headset', 'earpiece', 'microphone', 'stereo',
             'game_console', 'controller', 'badge', 'wallet', 'money', 'credit', 'coin', 'cash', 'chip_')
OTHER = ('photo', 'name_tag', 'nametag', 'ashtray', 'plushie', 'bobble', 'toy', 'board_game', 'dice', 'card_deck', 'trophy', 'figurine', 'statue_small', 'ornament', 'gift',
         'present', 'candle', 'skull', 'bone', 'gem', 'crystal', 'ore', 'scrap', 'junk_paper', 'paper', 'letter', 'note', 'sticky',
         'spoon', 'fork', 'knife', 'spatch', 'pen', 'pencil', 'brush', 'comb', 'mirror_hand', 'ball', 'rock', 'stone', 'shell', 'feather')
REJECT_WORDS = ('hair', 'beard', '_head_', 'head_blank', 'head_scaled', 'patch_', 'face', '_plant', 'drawers', 'wall', 'floor', 'door', 'pipe', 'vent', 'ladder', 'stair', 'table', 'chair', 'bed', 'sofa', 'couch', 'desk', 'cabinet',
                'locker', 'shelf', 'bench', 'crate', 'barrel', 'machine', 'generator', 'server', 'console_', 'monitor', 'screen',
                'billboard', 'sign', 'light', 'lamp', 'satellite', 'antenna', 'drone', 'turret', 'vehicle', 'cart', 'trolley', 'kiosk',
                'vending', 'fridge', 'microwave', 'sink', 'toilet', 'shower', 'mattress', 'pillow', 'rug', 'canopy', 'greeble', 'wires',
                'cable', 'hose', 'pod', 'panel', 'terminal', 'cardboard', 'detail', 'skeleton', 'body_part', 'body_bag', 'robot',
                'sweepo', 'dish', 'pole', 'street', 'traffic', 'manhole', 'puddle', 'slime', 'planter', 'plant_', 'hydroponic',
                'chamber', 'tank_', 'isolator', 'medical_', 'iv_', 'wheelchair', 'stove', 'oven', 'cooker', 'pans', 'pan_', 'stand',
                'rack', 'hologram', 'holo', 'projection', 'map', 'poster', 'frame', 'symbol', 'alien_letter', 'name_plate', 'stool',
                'bunk', 'tube_', 'centertube', 'spacewalk', 'solar', 'powerline', 'tether', 'turbine', 'missile', 'missle', 'laser_beam',
                'cage', 'cryo', 'suit_hanging', 'space_suit', 'mop', 'bin', 'trash', 'recycle', 'skip', 'lever', 'button', 'switch',
                'keyboard', 'mousepad', 'computer', 'supercomputer', 'cockpit', 'charge_station', 'charger', 'dock', 'hub', 'dispenser',
                'weapon_rack', 'work_bench', 'workbench', 'assembly', 'needle_arm', 'scav_', 'junky', 'chr_', 'gore', 'blood', 'web',
                'fence', 'gate', 'railing', 'pillar', 'column', 'beam', 'girder', 'strut', 'scaffold', 'container', 'cargo', 'pallet',
                'tyre', 'tire', 'wheel', 'engine', 'rocket', 'ship', 'fan', 'grate', 'grill', 'window', 'curtain', 'blind', 'flag',
                'banner', 'awning', 'tent', 'booth', 'cover', 'lid', 'tarp', 'blanket', 'cloth', 'towel', 'bag_', 'sack', 'box_')
PART_SUFFIX = re.compile(r'_(lid|door|glass|arm|arms|screen|handle|insert|trigger|blade|leg|foot|wheel|wheels|swivel|pivot|top|base|'
                         r'drawer|cone|head|claw|finger|fingers|wing|dish|sensor|stand|extension|light|hinge|fabric|pole|frame|picture|'
                         r'phone|antenna|needle|orb|eye|gear|chamber|cell|jaw|end|connector|valve|sign|topper|stick|drum|barrel|barrell|lens|bottom|camera|'
                         r'arm_[lr]|barrel_[lr]|launcher|missiles|brush|curtain|shelf|hologram|row|alt|preset|group|stack|pile|broken|'
                         r'damaged|curved|detailed|hanging|bare|small|large|medium|tall|half|full|bundle|adapter)$')

def classify(short):
    s = short.lower()
    for word in REJECT_WORDS:
        if word in s: return None, word
    for cat, words in (('consumables', CONSUMABLE), ('armor', ARMOR), ('equipment', EQUIPMENT), ('other', OTHER)):
        for w in words:
            if w in s: return cat, w
    return 'other', '(size only)'

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None: raise RuntimeError('the editor is in Play')
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    accepted, rejected = [], []
    for folder in FOLDERS:
        for ad in ar.get_assets_by_path(folder, recursive=True):
            if str(ad.asset_class_path.asset_name) != 'StaticMesh': continue
            name = str(ad.asset_name)
            short = re.sub(r'^SM_(Prop|Item|Att|Chr_Attach|Attach)_', '', name)
            base = re.sub(r'_\d+[a-z]?$', '', short)
            pack = folder.split('/')[2] if folder.startswith('/Game/Polygon') else folder.split('/')[3]
            if 'Character_Attachments' in folder: folder_kind = 'Attachments'
            path = str(ad.package_name)
            # A weapons folder is mostly triggers, slides and magazines: only the loose things come in.
            if folder.endswith('/Weapons') and not re.search(r'(Ammo_|Grenade_|Mase_Can|Shield_Riot_\d\d$|Cuffs)', name):
                rejected.append({'path': path, 'why': 'weapon or weapon part'}); continue
            # sub-parts of an assembly are not things
            tail = re.sub(r'_\d+[a-z]?$', '', short)
            if PART_SUFFIX.search(tail.lower()) and 'Attachments' not in folder and 'Character_Attachments' not in folder:
                rejected.append({'path': path, 'why': 'assembly part'}); continue
            m = unreal.load_asset(path)
            if not m: rejected.append({'path': path, 'why': 'no load'}); continue
            b = m.get_bounds(); e = b.box_extent
            size = [round(2 * e.x, 1), round(2 * e.y, 1), round(2 * e.z, 1)]
            side = max(size); litres = (size[0] * size[1] * size[2]) / 1000.0
            cat, word = classify(base)
            # A character attachment is worn -- armour -- unless its name says it is something carried
            # (a canteen, a grenade, a chef's tool) or a part of the body (rejected above).
            if ('Attachments' in folder or 'Character_Attachments' in folder) and cat is not None and cat not in ('consumables', 'equipment'): cat, word = ('armor', word if cat == 'armor' else 'attachment')
            rec = {'path': path, 'name': name, 'pack': pack, 'size': size, 'litres': round(litres, 1), 'category': cat, 'word': word}
            if cat is None: rec['why'] = 'name: ' + word; rejected.append(rec); continue
            if side > MAX_SIDE or side < MIN_SIDE or litres > MAX_VOLUME_L:
                rec['why'] = 'size %.0f x %.0f x %.0f (%.0f l)' % (size[0], size[1], size[2], litres); rejected.append(rec); continue
            accepted.append(rec)
    accepted.sort(key=lambda r: (r['category'], r['pack'], r['name']))
    io.open(OUT, 'w', encoding='utf-8', newline='\n').write(json.dumps({'accepted': accepted, 'rejected': rejected}, indent=1))
    import collections
    print('accepted %d, rejected %d' % (len(accepted), len(rejected)))
    print('by category', dict(collections.Counter(r['category'] for r in accepted)))
    print('by pack', dict(collections.Counter(r['pack'] for r in accepted)))
    print('reject reasons', collections.Counter(r['why'].split(':')[0].split(' ')[0] for r in rejected).most_common(6))
except Exception:
    print('ERROR', traceback.format_exc())
