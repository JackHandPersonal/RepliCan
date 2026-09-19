"""Bring the SciFi Horror weapons into the game's own weapon library and catalogue.

    python Tools/ue_remote.py --file Tools/import_horror_weapons.py
    ... then normalise_weapons.py, reseat_grips.py, derive_weapon_points.py

Every weapon this project uses lives at /Game/RepliCan/Weapons/<Pack>/ as our own copy, baked
into HAC1 space (Docs/HeldAssetStandard.md). Pack meshes are never used directly: the bake is
destructive, and destroying an asset inside a vendor pack means the pack can never be updated
or re-imported.

MULTI-PART MESHES. Synty ships several of these weapons in pieces so a game can animate a
trigger or drop a magazine. The pieces are NOT authored consistently:

    SM_Wep_Drill_01_Teeth_01     origin (0, 44.7, -1.7)   -- already in the parent's space
    SM_Wep_Rifle_Mag_01          origin (0,  0.0,  0.0)   -- centred on itself

A part already sitting in the parent's space assembles at identity and is merged in. A part
centred on its own origin needs a placement that only exists in the pack's demo map, so it is
SKIPPED and reported rather than merged at the wrong place -- a magazine welded to the muzzle
is worse than no magazine, and at the distance these are seen a missing detachable part is
invisible while a misplaced one is not.
"""
import unreal, json, io, traceback, collections

SRC = '/Game/Synty/PolygonSciFiHorror/Meshes/Weapons/'
DST = '/Game/RepliCan/Weapons/Horror/'
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'

# name, kind, sound, ranged, description. The kinds are the ones the equipment slots already
# understand; nothing new is invented here.
WEAPONS = [
    ('SM_Wep_Rifle_01',        'Rifle',      'wep_rifle',  True,
     'A service rifle from a mining charter, not a military one. Worn where hands go, spotless everywhere else.'),
    ('SM_Wep_Flamethrower_01', 'Heavy Gun',  'wep_heavy',  True,
     'Rated for clearing fungal growth out of ducting. Nobody asked what else it had been used on.'),
    ('SM_Wep_Laser_Cutter_01', 'Laser',      'wep_laser',  True,
     'A hull cutter. Slow, silent, and it does not care what the hull is made of.'),
    ('SM_Wep_Mining_Laser_01', 'Laser',      'wep_laser',  True,
     'Industrial rock laser. The battery is the heavy part, and it is worn on the back.'),
    ('SM_Wep_Blow_Torch_01',   'Tool',       'wep_blunt',  False,
     'Cutting torch. Short reach, and it ruins whatever it touches.'),
    ('SM_Wep_Welder_01',       'Tool',       'wep_blunt',  False,
     'Arc welder. Repairs a bulkhead, or argues with something on the other side of it.'),
    ('SM_Wep_Drill_01',        'Tool',       'wep_blunt',  False,
     'Core drill. Two hands, and a wind-up long enough to regret.'),
    ('SM_Wep_Axe_01',          'Hand Axe',   'wep_blade',  False,
     'Emergency axe off a bulkhead mount. Every deck has one and nobody logs them.'),
    ('SM_Wep_Hammer_01',       'Hammer',     'wep_blunt',  False,
     'A mallet for seating deck plate. Unsubtle, and entirely effective.'),
    ('SM_Wep_Knife_01',        'Dagger',     'wep_blade',  False,
     'A working knife. Line, tape, rations, and whatever else the day turns up.'),
    ('SM_Wep_Wrench_01',       'Tool',       'wep_blunt',  False,
     'Pipe wrench, adjustable, heavy at the head. The oldest weapon on this list by several centuries.'),
    ('SM_Wep_Shock_Stick_01',  'Shock',      'wep_blunt',  False,
     'Livestock prod, repurposed. The charge light is the only part that still works reliably.'),
    ('SM_Wep_Surgical_Tool_01','Dagger',     'wep_blade',  False,
     'A surgical instrument, sterile and very sharp. Found a long way from any surgery.'),
    ('SM_Wep_Surgical_Tool_02','Dagger',     'wep_blade',  False,
     'The second instrument from the same tray. The tray is not aboard.'),
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
        """A part authored about its own origin cannot be placed without the demo map."""
        b = mesh.get_bounds()
        o = b.origin
        return (abs(o.x) + abs(o.y) + abs(o.z)) < 1.0

    for base, kind, sound, ranged, desc in WEAPONS:
        src = available.get(base)
        if not src:
            tally['missing'] += 1
            print('MISSING', base)
            continue
        dyn = unreal.DynamicMesh()
        opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            unreal.load_asset(src), dyn, opts, unreal.GeometryScriptMeshReadLOD())

        # Merge the parts that already sit in the parent's space.
        merged = []
        for name, path in sorted(available.items()):
            # Only a part of THIS weapon: "SM_Wep_Rifle_01_Trigger_01" or "SM_Wep_Rifle_Mag_01",
            # never a sibling weapon. Matching on the stem alone made Surgical_Tool_01 swallow
            # Surgical_Tool_02, because one name is a prefix of the other.
            stem = base[:-3] if base.endswith(('_01', '_02')) else base
            if name == base:
                continue
            if not (name.startswith(base + '_') or (name.startswith(stem) and not name[len(stem):].lstrip('_')[:2].isdigit())):
                continue
            part = unreal.load_asset(path)
            if not part:
                continue
            if bounds_centred(part):
                skipped_parts.append(name)
                continue
            tmp = unreal.DynamicMesh()
            tmp, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
                part, tmp, opts, unreal.GeometryScriptMeshReadLOD())
            unreal.GeometryScript_MeshEdits.append_mesh(dyn, tmp, unreal.Transform())
            merged.append(name)

        unreal.GeometryScript_Normals.set_per_face_normals(dyn)
        unreal.GeometryScript_Normals.recompute_normals(dyn, unreal.GeometryScriptCalculateNormalsOptions())

        full = DST + base
        mats = unreal.GeometryScript_AssetUtils.get_material_list_from_static_mesh(unreal.load_asset(src))
        asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
        to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        to_opts.enable_recompute_normals = True
        to_opts.enable_recompute_tangents = True
        to_opts.replace_materials = True
        to_opts.new_materials = list(mats[0]) if mats else []
        if asset:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
        else:
            create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            create.enable_recompute_normals = True
            create.enable_recompute_tangents = True
            asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
            if asset and mats:
                asset.set_editor_property('static_materials',
                    [unreal.StaticMaterial(material_interface=m) for m in mats[0]])
        if not asset:
            print('could not create', full); tally['failed'] += 1; continue
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)

        pretty = base.replace('SM_Wep_', '').replace('_01', '').replace('_02', ' 02').replace('_', ' ').strip()
        key = 'Horror/' + base.replace('SM_Wep_', 'Wep_')
        doc['weapons'][key] = {
            'name': pretty + ' 01' if not pretty.endswith('02') else pretty,
            'kind': kind,
            'pack': 'Horror',
            'description': desc,
            'icon': 'Horror_' + base.replace('SM_Wep_', ''),
            'mesh': full,
            'keep': False,
            'sound': sound,
            'ranged': ranged,
        }
        tally['imported'] += 1
        print('%-28s %-11s %d tris%s' % (base, kind, dyn.get_triangle_count(),
              ('  + ' + ', '.join(merged)) if merged else ''))

    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('')
    print('IMPORT', dict(tally), ' catalogue now %d weapons' % len(doc['weapons']))
    if skipped_parts:
        print('parts left out (centred on their own origin, need the pack demo map to place):')
        print('   ' + ', '.join(sorted(set(skipped_parts))))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
