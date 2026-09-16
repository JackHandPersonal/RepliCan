"""Take the scope MOUNT off a weapon body that already had its scope stripped.
    python Tools/ue_remote.py --file Tools/strip_mount.py

strip_scopes.py removed the scope by matching the baked scope part's own triangles, which
left the rail block the scope sat on: it is body geometry, not scope geometry. Measured on the
Assault_01 body, that block is the cluster of body triangles inside the scope's x/y footprint
from 5 cm under the scope's bottom edge upward (z 16..27 on a receiver whose top is at 15).
This deletes exactly that -- footprint from the scope part, height from its bottom minus
MOUNT_HEIGHT -- saves the body, and sets the weapon's optic_mount down onto the receiver top
that is left, so a fitted optic sits on metal rather than floating where the mount was.
"""
import unreal, json, io, traceback, collections
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils
CAT = r'C:\Dev\Games\RepliCan\UI\Weapons.json'
ONLY = ['Worlds/Wep_Assault_01']
MOUNT_HEIGHT = 5.0
MARGIN = 1.0


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


try:
    doc = json.load(io.open(CAT, encoding='utf-8'))
    for key in ONLY:
        w = doc['weapons'][key]
        scope_path = (w.get('parts') or {}).get('scope'); body_path = w.get('body_mesh')
        if not scope_path or not body_path: print(key, 'needs parts.scope and body_mesh'); continue
        scope = unreal.load_asset(scope_path.split('.')[0]); body = unreal.load_asset(body_path.split('.')[0])
        sb = scope.get_bounding_box()
        x0, x1, y0, y1, zcut = sb.min.x - MARGIN, sb.max.x + MARGIN, sb.min.y - MARGIN, sb.max.y + MARGIN, sb.min.z - MOUNT_HEIGHT
        dyn = unreal.DynamicMesh()
        r = AU.copy_mesh_from_static_mesh(body, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
        drop = []; keep_top = 0.0
        for t in range(dyn.get_triangle_count()):
            r = MQ.get_triangle_positions(dyn, t); pts = [q for q in r if isinstance(q, unreal.Vector)]
            if len(pts) != 3: continue
            cx = sum(p.x for p in pts) / 3; cy = sum(p.y for p in pts) / 3; cz = sum(p.z for p in pts) / 3
            if x0 <= cx <= x1 and y0 <= cy <= y1:
                if cz >= zcut: drop.append(t)
                else: keep_top = max(keep_top, max(p.z for p in pts))
        sel = unreal.GeometryScript_MeshSelection.convert_index_array_to_mesh_selection(dyn, drop, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
        sel = first(sel, unreal.GeometryScriptMeshSelection)
        r = unreal.GeometryScript_MeshEdits.delete_selected_triangles_from_mesh(dyn, sel); dyn = r[0] if isinstance(r, tuple) else dyn
        r = unreal.GeometryScript_MeshRepair.compact_mesh(dyn); dyn = r[0] if isinstance(r, tuple) else dyn
        ml = AU.get_material_list_from_static_mesh(body); mats = list(ml[0]) if ml else []
        to_opts = unreal.GeometryScriptCopyMeshToAssetOptions(); to_opts.enable_recompute_normals = True; to_opts.enable_recompute_tangents = True
        to_opts.replace_materials = True; to_opts.new_materials = mats
        AU.copy_mesh_to_static_mesh(dyn, body, to_opts, unreal.GeometryScriptMeshWriteLOD())
        ok = unreal.EditorLoadingAndSavingUtils.save_packages([body.get_outermost()], False)
        old = w.get('optic_mount', [0, 0, 0]); new_z = round(keep_top + 0.2, 2)
        w['optic_mount'] = [old[0], old[1], new_z]
        print('%s: mount = %d triangles in x %.1f..%.1f y %.1f..%.1f above z %.1f -> removed; body now %d tris, saved %s; receiver top %.1f, optic_mount z %.2f -> %.2f' % (key, len(drop), x0, x1, y0, y1, zcut, dyn.get_triangle_count(), ok, keep_top, old[2], new_z))
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
