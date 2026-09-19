"""Bring the POLYGON Sci-Fi City weapons (the 4.25 pack, Content/PolygonScifi) into the game's
own weapon library and catalogue.

    python Tools/ue_remote.py --file Tools/import_scificity_weapons.py
    ... then normalise_weapons.py, reseat_grips.py, derive_weapon_points.py

Same rules as the Horror and Police importers (our own copy under /Game/RepliCan/Weapons/
SciFiCity/, the pack untouched). This pack's guns are RIGGED -- SK_Wep_* on a SKEL_ each, so a
magazine or a bipod can animate -- and the reference pose holds every part where it belongs,
so the skeletal mesh is flattened straight into a static one; the loose SM_Wep_*_Mag_* pieces
are the same magazines centred on their own origin and are not merged. The blades and the
shiv are static meshes already. Ammunition, magazines, sights, silencers and crosshairs are
not weapons and are left where they are.
"""
import unreal, json, io, traceback, collections

SRC = '/Game/PolygonScifi/Meshes/Weapons/'
DST = '/Game/RepliCan/Weapons/SciFiCity/'
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'

# mesh, kind, sound, ranged, description. Kinds and sounds are ones the catalogue already has.
WEAPONS = [
    ('SK_Wep_Pistol_01',             'Pistol',          'wep_pistol',   True,  'City-issue sidearm. Polymer, boxy, and everywhere.'),
    ('SK_Wep_Revolver_01',           'Pistol',          'wep_pistol',   True,  'Heavy revolver with a long barrel. Makes a point, then a hole.'),
    ('SK_Wep_MachinePistol_Gen1_01', 'SMG',             'wep_smg',      True,  'First-generation machine pistol. Loud, cheap, and it climbs.'),
    ('SK_Wep_MachinePistol_Gen2_01', 'SMG',             'wep_smg',      True,  'Second-generation machine pistol. The recoil went into a spring; the price went up.'),
    ('SK_Wep_SMG_01',                'SMG',             'wep_smg',      True,  'Compact sub-machine gun with a folding stock. The security firms buy them by the crate.'),
    ('SK_Wep_Rifle_Small_01',        'Assault Rifle',   'wep_rifle',    True,  'Short carbine. Fits in a car door and out of it in one movement.'),
    ('SK_Wep_Rifle_Base_01',         'Assault Rifle',   'wep_rifle',    True,  'The base rifle the city builds everything else on. Rails top and bottom.'),
    ('SK_Wep_Rifle_Laser_01',        'Laser',           'wep_laser',    True,  'Laser rifle with a battery in the stock. Silent, hot, and it leaves a line in the smoke.'),
    ('SK_Wep_Rifle_Plasma_01',       'Heavy Gun',       'wep_heavy',    True,  'Plasma rifle, first pattern. The coils glow before it fires and for a while after.'),
    ('SK_Wep_Rifle_Plasma_02',       'Heavy Gun',       'wep_heavy',    True,  'Plasma rifle, second pattern. Cooler running; the same argument.'),
    ('SK_Wep_Rifle_Plasma_03',       'Heavy Gun',       'wep_heavy',    True,  'Plasma rifle, third pattern, with a drum feed. Corporate security only, in theory.'),
    ('SK_Wep_Shotgun_Plasma_01',     'Shotgun',         'wep_shotgun',  True,  'Plasma shotgun. A wide cone of something that used to be gas.'),
    ('SK_Wep_Sniper_01',             'Marksman Rifle',  'wep_sniper',   True,  'Bolt-action marksman rifle with a long scope. Rooftops.'),
    ('SK_Wep_Sniper_Plasma_02',      'Marksman Rifle',  'wep_sniper',   True,  'Plasma marksman rifle. One bolt, a long way, and a flash on the far end.'),
    ('SK_Wep_Syringe_Gun_01',        'Pistol',          'wep_pistol',   True,  'Syringe gun off a medical cart. What it loads is up to whoever holds it.'),
    ('SM_Wep_Knife_01',              'Dagger',          'wep_blade',    False, 'Combat knife. Clean edge, taped grip.'),
    ('SM_Wep_Shiv_01',               'Dagger',          'wep_blade',    False, 'A shiv. Something that was not a knife until somebody needed one.'),
    ('SM_Wep_Sword_01',              'Sword',           'wep_blade',    False, 'A straight sword with a powered edge. Ornamental, according to the licence.'),
]

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before creating assets')

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    available = {}
    for a in ar.get_assets_by_path(SRC, True):
        cls = str(a.asset_class_path.asset_name)
        if cls in ('StaticMesh', 'SkeletalMesh'):
            available[str(a.asset_name)] = (str(a.package_name), cls)

    doc = json.load(io.open(CAT, encoding='utf-8'))
    tally = collections.Counter()
    for base, kind, sound, ranged, desc in WEAPONS:
        found = available.get(base)
        if not found:
            tally['missing'] += 1; print('MISSING', base); continue
        path, cls = found
        src = unreal.load_asset(path)
        dyn = unreal.DynamicMesh()
        opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
        if cls == 'SkeletalMesh':
            dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(src, dyn, opts, unreal.GeometryScriptMeshReadLOD())
            mats = [m.material_interface for m in src.materials]
        else:
            dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(src, dyn, opts, unreal.GeometryScriptMeshReadLOD())
            ml = unreal.GeometryScript_AssetUtils.get_material_list_from_static_mesh(src)
            mats = list(ml[0]) if ml else []
        unreal.GeometryScript_Normals.set_per_face_normals(dyn)
        unreal.GeometryScript_Normals.recompute_normals(dyn, unreal.GeometryScriptCalculateNormalsOptions())

        short = 'SM_' + base[3:]
        full = DST + short
        asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
        to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        to_opts.enable_recompute_normals = True; to_opts.enable_recompute_tangents = True
        to_opts.replace_materials = True; to_opts.new_materials = mats
        if asset:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
        else:
            create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            create.enable_recompute_normals = True; create.enable_recompute_tangents = True
            asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
            if asset and mats:
                asset.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=m) for m in mats])
        if not asset:
            print('could not create', full); tally['failed'] += 1; continue
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)

        pretty = short.replace('SM_Wep_', '').replace('_01', '').replace('_02', ' 02').replace('_03', ' 03').replace('_', ' ').strip()
        key = 'SciFiCity/' + short.replace('SM_Wep_', 'Wep_')
        entry = doc['weapons'].get(key, {})
        entry.update({
            'name': pretty + ' 01' if not pretty[-2:].isdigit() else pretty,
            'kind': kind, 'pack': 'SciFiCity', 'description': desc,
            'icon': 'SciFiCity_' + short.replace('SM_Wep_', ''), 'mesh': full, 'keep': entry.get('keep', False),
            'sound': sound, 'ranged': ranged,
        })
        doc['weapons'][key] = entry
        tally['imported'] += 1
        print('%-32s %-16s %5d tris  (%s)' % (base, kind, dyn.get_triangle_count(), cls))

    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('')
    print('IMPORT', dict(tally), ' catalogue now %d weapons' % len(doc['weapons']))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
