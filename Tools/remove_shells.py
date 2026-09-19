"""Take whole SHELLS (connected pieces) off a baked weapon body, by where they sit.
    python Tools/ue_remote.py --file Tools/remove_shells.py

A Synty weapon is built from separate closed pieces, and the scope MOUNT on the Assault_01 is
two of them: a clamp body rising from the rail (x 12..20, z 16..27) and a lever beside it. A
box cut through the body took rifle geometry with it; taking shells whole takes only what is
a piece of its own. The body is split into components, every component whose bounds lie inside
one of the BOXES is dropped, the rest are joined back, the asset is saved, and the weapon's
optic_mount is set onto the receiver top that remains under the scope footprint.
"""
import unreal, json, io, traceback
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'
JOBS = {
    'Worlds/Wep_Assault_01': {'boxes': [((10.0, -6.0, 15.5), (22.0, 1.5, 28.0))], 'mount_x': 8.0},
}
MARGIN = 0.5


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


def inside(b, box):
    (x0, y0, z0), (x1, y1, z1) = box
    return b.min.x >= x0 - MARGIN and b.max.x <= x1 + MARGIN and b.min.y >= y0 - MARGIN and b.max.y <= y1 + MARGIN and b.min.z >= z0 - MARGIN and b.max.z <= z1 + MARGIN


try:
    doc = json.load(io.open(CAT, encoding='utf-8'))
    for key, job in JOBS.items():
        w = doc['weapons'][key]
        body = unreal.load_asset(w['body_mesh'].split('.')[0]); scope = unreal.load_asset(w['parts']['scope'].split('.')[0])
        sb = scope.get_bounding_box()
        dyn = unreal.DynamicMesh()
        r = AU.copy_mesh_from_static_mesh(body, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
        pool = unreal.DynamicMeshPool()
        r = unreal.GeometryScript_MeshDecomposition.split_mesh_by_components(dyn, pool)
        parts = [x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, (unreal.Array, list))][0]
        keep = []; dropped = []; top = 0.0
        for m in parts:
            b = first(MQ.get_mesh_bounding_box(m), unreal.Box)
            if any(inside(b, box) for box in job['boxes']): dropped.append((m.get_triangle_count(), b)); continue
            keep.append(m)
            # receiver top under the scope footprint: the highest shell top that overlaps the footprint below the scope
            if b.max.x > sb.min.x and b.min.x < sb.max.x and b.max.z < sb.min.z and b.max.z > top and (b.max.y - b.min.y) > 3.0: top = b.max.z
        out = unreal.DynamicMesh()
        for m in keep: unreal.GeometryScript_MeshEdits.append_mesh(out, m, unreal.Transform())
        ml = AU.get_material_list_from_static_mesh(body); mats = list(ml[0]) if ml else []
        to = unreal.GeometryScriptCopyMeshToAssetOptions(); to.enable_recompute_normals = True; to.enable_recompute_tangents = True; to.replace_materials = True; to.new_materials = mats
        AU.copy_mesh_to_static_mesh(out, body, to, unreal.GeometryScriptMeshWriteLOD())
        ok = unreal.EditorLoadingAndSavingUtils.save_packages([body.get_outermost()], False)
        old = w.get('optic_mount', [0, 0, 0]); w['optic_mount'] = [job.get('mount_x', old[0]), 0.0, round(top + 0.3, 2)]
        print('%s: dropped %s; body now %d tris in %d shells, saved %s; receiver top %.1f; optic_mount %s -> %s' % (key, ', '.join('%d tris at x %.1f..%.1f z %.1f..%.1f' % (n, b.min.x, b.max.x, b.min.z, b.max.z) for n, b in dropped), out.get_triangle_count(), len(keep), ok, top, old, w['optic_mount']))
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
