"""Strips the modelled scope from every weapon that has one baked out as a part, so an optic
can take its place.

    python Tools/ue_remote.py --file Tools/strip_scopes.py

A Synty rifle with a scope modelled on carries it as painted, opaque geometry: with an optic
fitted the eye looked through THAT, in front of the optic's glass, and saw a painted lens.
Tools/bake_weapon_parts.py already found each such scope as its own part with exact HAC1
geometry, so the body without it is one deletion: every triangle of the base mesh whose three
vertices coincide with vertices of the baked scope part (the part is that geometry, in the same
space, so the match is exact to float precision) goes. The count is checked against the part's
own triangle count and the body is written only when the two agree. (A bounding-box test was
tried first and refused most rifles: the rail the scope sits on puts a few triangles in the
scope's box.) Written to
<pack>/Body/<name>_Body with the base's materials and recorded in the catalogue as body_mesh,
which the equip uses whenever an optic is on (ABasePlayerController::RefreshHeldWeapon).
"""
import unreal, io, json, os, traceback

CAT = os.path.join(unreal.Paths.project_dir(), 'Content', 'GameData', 'UI', 'Weapons.json')
ONLY = []
MARGIN = 0.05
FORCE = True

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play')
    Q = unreal.GeometryScript_MeshQueries; SEL = unreal.GeometryScript_MeshSelection
    ED = unreal.GeometryScript_MeshEdits; AU = unreal.GeometryScript_AssetUtils
    doc = json.load(io.open(CAT, encoding='utf-8'))
    done, skipped = [], []
    for key, e in doc['weapons'].items():
        if ONLY and key not in ONLY:
            continue
        part = e.get('parts', {}).get('scope'); box = e.get('parts_bounds', {}).get('scope')
        if not part or not box:
            continue
        if e.get('body_mesh') and not FORCE and unreal.EditorAssetLibrary.does_asset_exist(e['body_mesh'].split('.')[0]):
            continue
        base = unreal.load_asset(e['mesh']); scope = unreal.load_asset(part)
        if not base or not scope:
            skipped.append((key, 'missing mesh')); continue
        dyn = unreal.DynamicMesh()
        dyn, _ = AU.copy_mesh_from_static_mesh(base, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        # The scope's vertices, on a coarse grid with their exact positions kept, so a base-mesh
        # vertex is "the scope's" when one of them lies within MARGIN of it.
        pd = unreal.DynamicMesh()
        pd, _ = AU.copy_mesh_from_static_mesh(scope, pd, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        grid = {}
        for i in range(pd.get_vertex_count()):
            p, ok = Q.get_vertex_position(pd, i)
            if ok: grid.setdefault((round(p.x), round(p.y), round(p.z)), []).append((p.x, p.y, p.z))
        pos = {}
        def inside(vi):
            if vi not in pos:
                p, ok = Q.get_vertex_position(dyn, vi)
                hit = False
                if ok:
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            for dz in (-1, 0, 1):
                                for q in grid.get((round(p.x) + dx, round(p.y) + dy, round(p.z) + dz), ()):
                                    if abs(q[0] - p.x) <= MARGIN and abs(q[1] - p.y) <= MARGIN and abs(q[2] - p.z) <= MARGIN: hit = True; break
                                if hit: break
                            if hit: break
                        if hit: break
                pos[vi] = hit
            return pos[vi]
        drop = []
        for t in range(Q.get_num_triangle_i_ds(dyn)):
            r = Q.get_triangle_indices(dyn, t); idx = r[0] if isinstance(r, tuple) else r
            if inside(int(idx.x)) and inside(int(idx.y)) and inside(int(idx.z)):
                drop.append(t)
        want = scope.get_num_triangles(0)
        if len(drop) != want:
            skipped.append((key, '%d triangles match the scope part, the part has %d' % (len(drop), want))); continue
        r = SEL.convert_index_array_to_mesh_selection(dyn, drop, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
        sel = [x for x in r if isinstance(x, unreal.GeometryScriptMeshSelection)][0] if isinstance(r, tuple) else r
        r = ED.delete_selected_triangles_from_mesh(dyn, sel); dyn = r[0] if isinstance(r, tuple) else dyn
        name = e['mesh'].rsplit('/', 1)[-1].split('.')[0] + '_Body'
        full = e['mesh'].rsplit('/', 1)[0] + '/Body/' + name
        target = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
        if target:
            to = unreal.GeometryScriptCopyMeshToAssetOptions()
            to.enable_recompute_normals = False; to.enable_recompute_tangents = True
            AU.copy_mesh_to_static_mesh(dyn, target, to, unreal.GeometryScriptMeshWriteLOD())
        else:
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.enable_recompute_normals = False; opts.enable_recompute_tangents = True
            res = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, opts)
            target = next((x for x in res if isinstance(x, unreal.StaticMesh)), None) if isinstance(res, tuple) else res
            if not target:
                target = unreal.load_asset(full)
        mats = [sm.material_interface for sm in base.get_editor_property('static_materials')]
        if target and mats:
            target.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=m) for m in mats])
        unreal.EditorAssetLibrary.save_asset(full)
        e['body_mesh'] = full + '.' + name
        done.append((key, len(drop)))
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('STRIPPED', done)
    print('SKIPPED', skipped)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc())
