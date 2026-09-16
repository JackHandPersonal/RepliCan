"""Bring the POLYGON Police Station weapons into the game's own weapon library and catalogue.

    python Tools/ue_remote.py --file Tools/import_police_weapons.py
    ... then normalise_weapons.py, reseat_grips.py, derive_weapon_points.py

The same shape as import_horror_weapons.py, and for the same reasons (our own copy under
/Game/RepliCan/Weapons/<Pack>/, baked into HAC1 space, the pack untouched). One difference:
this pack ships each animated firearm twice -- in pieces for its Blueprint, and as one
SM_Wep_*_Combined mesh with every piece already in place. The Combined mesh is what is
copied, so no part is merged and none can land in the wrong place. The batons, the ram and
the contact stun gun are single meshes.

The riot shields, cuffs, grenades, ammunition and the mace can are not weapons in this
sense; Tools/survey_items.py lets those through from the same folder as items.
"""
import unreal, json, io, traceback, collections

SRC = '/Game/Synty/PolygonPoliceStation/Meshes/Weapons/'
DST = '/Game/RepliCan/Weapons/Police/'
CAT = r'C:\Dev\Games\RepliCan\UI\Weapons.json'

# mesh, kind, sound, ranged, description. Kinds and sounds are ones the catalogue already has.
WEAPONS = [
    ('SM_Wep_Pistol_01_Combined',       'Pistol',           'wep_pistol',   True,
     'Issue sidearm, polymer frame, fifteen rounds. Every one of these has a serial and most of them still match a record.'),
    ('SM_Wep_Revolver_01_Combined',     'Pistol',           'wep_pistol',   True,
     'Six-shot revolver. Older than the department that issued it and never once jammed.'),
    ('SM_Wep_Rifle_01_Combined',        'Assault Rifle',    'wep_rifle',    True,
     'Patrol carbine with a light and a reflex sight. Kept in the car, signed out by the shift.'),
    ('SM_Wep_Shotgun_01_Combined',      'Shotgun',          'wep_shotgun',  True,
     'Pump shotgun with a light under the barrel and shells on the stock. Doors, mostly.'),
    ('SM_Wep_DoubleBarrel_01_Combined', 'Shotgun',          'wep_shotgun',  True,
     'Side-by-side twelve gauge. Came in as evidence and stayed as something else.'),
    ('SM_Wep_Sniper_01_Combined',       'Marksman Rifle',   'wep_sniper',   True,
     'Bolt-action marksman rifle with a scope and a bipod. Signed for by name, always.'),
    ('SM_Wep_Launcher_01_Combined',     'Grenade Launcher', 'wep_launcher', True,
     'Forty-millimetre launcher, single shot. Fires gas, smoke and the occasional thing it should not.'),
    ('SM_Wep_Stungun_01_Combined',      'Shock',            'wep_laser',    True,
     'Dart taser. Two wires, one cartridge, and a long form to fill in afterwards.'),
    ('SM_Wep_Stungun_02',               'Shock',            'wep_blunt',    False,
     'Contact stun gun. Has to touch you, and then it does.'),
    ('SM_Wep_Stun_Baton_01',            'Shock',            'wep_blunt',    False,
     'Stun baton. A stick, and then a stick with a charge in it.'),
    ('SM_Wep_Baton_01',                 'Hammer',           'wep_blunt',    False,
     'Side-handle baton. Cheap, hard, and on every belt in the building.'),
    ('SM_Wep_Baton_02',                 'Hammer',           'wep_blunt',    False,
     'Collapsible baton. Fits in a pocket; does not feel like it when it opens.'),
    ('SM_Wep_Door_Ram_01',              'Hammer',           'wep_blunt',    False,
     'Breaching ram. Sixteen kilos of steel with handles, for doors that will not be talked to.'),
]

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before creating assets')

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    available = {}
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'StaticMesh'), True):
        p = str(a.package_name)
        if p.startswith(SRC):
            available[p.split('/')[-1]] = p

    doc = json.load(io.open(CAT, encoding='utf-8'))
    tally = collections.Counter()
    skipped_parts = []

    def bounds_centred(mesh):
        b = mesh.get_bounds(); o = b.origin
        return (abs(o.x) + abs(o.y) + abs(o.z)) < 1.0

    for base, kind, sound, ranged, desc in WEAPONS:
        src = available.get(base)
        if not src:
            tally['missing'] += 1; print('MISSING', base); continue
        dyn = unreal.DynamicMesh()
        opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(unreal.load_asset(src), dyn, opts, unreal.GeometryScriptMeshReadLOD())
        merged = []
        if not base.endswith('_Combined'):
            stem = base[:-3] if base.endswith(('_01', '_02')) else base
            for name, path in sorted(available.items()):
                if name == base: continue
                if not (name.startswith(base + '_') or (name.startswith(stem) and not name[len(stem):].lstrip('_')[:2].isdigit())): continue
                part = unreal.load_asset(path)
                if not part: continue
                if bounds_centred(part): skipped_parts.append(name); continue
                tmp = unreal.DynamicMesh()
                tmp, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(part, tmp, opts, unreal.GeometryScriptMeshReadLOD())
                unreal.GeometryScript_MeshEdits.append_mesh(dyn, tmp, unreal.Transform()); merged.append(name)
        unreal.GeometryScript_Normals.set_per_face_normals(dyn)
        unreal.GeometryScript_Normals.recompute_normals(dyn, unreal.GeometryScriptCalculateNormalsOptions())

        short = base.replace('_Combined', '')
        full = DST + short
        mats = unreal.GeometryScript_AssetUtils.get_material_list_from_static_mesh(unreal.load_asset(src))
        asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
        to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        to_opts.enable_recompute_normals = True; to_opts.enable_recompute_tangents = True
        to_opts.replace_materials = True; to_opts.new_materials = list(mats[0]) if mats else []
        if asset:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
        else:
            create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            create.enable_recompute_normals = True; create.enable_recompute_tangents = True
            asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
            if asset and mats:
                asset.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=m) for m in mats[0]])
        if not asset:
            print('could not create', full); tally['failed'] += 1; continue
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)

        pretty = short.replace('SM_Wep_', '').replace('_01', '').replace('_02', ' 02').replace('_', ' ').strip()
        key = 'Police/' + short.replace('SM_Wep_', 'Wep_')
        entry = doc['weapons'].get(key, {})
        entry.update({
            'name': pretty + ' 01' if not pretty.endswith('02') else pretty,
            'kind': kind, 'pack': 'Police', 'description': desc,
            'icon': 'Police_' + short.replace('SM_Wep_', ''), 'mesh': full, 'keep': entry.get('keep', False),
            'sound': sound, 'ranged': ranged,
        })
        doc['weapons'][key] = entry
        tally['imported'] += 1
        print('%-34s %-16s %d tris%s' % (base, kind, dyn.get_triangle_count(), ('  + ' + ', '.join(merged)) if merged else ''))

    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('')
    print('IMPORT', dict(tally), ' catalogue now %d weapons' % len(doc['weapons']))
    if skipped_parts:
        print('parts left out (centred on their own origin):', ', '.join(sorted(set(skipped_parts))))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
