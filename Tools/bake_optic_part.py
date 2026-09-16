"""Turn a pack's red-dot PART into one of our optics, with the parallax reticle glass in its window.
    python Tools/ue_remote.py --file Tools/bake_optic_part.py

The part (Sci-Fi Worlds' SM_Wep_Assault_02_RedDot_01, the user's pick) is authored where it
sits on its own rifle, long along +Y. Here it is copied, turned to HAC1 (+X downrange), set
with its base on z 0 and centred, and its WINDOW is found by firing rays down the length
through a grid over the cross-section: the rays that pass clean through are the window, their
centroid is where the eye looks, and a pane carrying M_RedDot (the collimated reticle from
Tools/make_optics.py) is set into it on its own material slot. The result goes to
/Game/RepliCan/Optics/<NAME> and into UI/Weapons.json's "optics" as <KEY> with eye and sit;
the weapon named in ASSIGN_TO gets it as its optic.
"""
import unreal, io, json, traceback
SRC = '/Game/PolygonSciFiWorlds/Models/Weapons/Parts/SM_Wep_Assault_02_RedDot_01'
NAME = 'SM_Optic_RedDot_02'; KEY = 'RedDot_02'; PRETTY = 'RDS-2 reflex sight'
ASSIGN_TO = 'Worlds/Wep_Assault_01'
PKG = '/Game/RepliCan/Optics'; GLASS = '/Game/RepliCan/Materials/M_RedDot'
CAT = r'C:\Dev\Games\RepliCan\UI\Weapons.json'
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils; SP = unreal.GeometryScript_MeshSpatial
XF = unreal.GeometryScript_MeshTransforms; P = unreal.GeometryScript_Primitives


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


try:
    src = unreal.load_asset(SRC)
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(src, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
    # +Y downrange -> +X: yaw -90; then base to z 0 and centred in x and y
    r = XF.rotate_mesh(dyn, unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), unreal.Vector(0, 0, 0)); dyn = r[0] if isinstance(r, tuple) else dyn
    b = first(MQ.get_mesh_bounding_box(dyn), unreal.Box)
    shift = unreal.Vector(-(b.min.x + b.max.x) * 0.5, -(b.min.y + b.max.y) * 0.5, -b.min.z)
    r = XF.translate_mesh(dyn, shift); dyn = r[0] if isinstance(r, tuple) else dyn
    b = first(MQ.get_mesh_bounding_box(dyn), unreal.Box)
    L, W, H = b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z
    print('part in HAC1: %.1f long x %.1f wide x %.1f tall, base at z 0' % (L, W, H))
    # the window: rays along +X through a fine grid over the cross-section
    r = SP.build_bvh_for_mesh(dyn); bvh = first(r, unreal.GeometryScriptDynamicMeshBVH)
    clear = []
    n = 24
    for i in range(n):
        for j in range(n):
            y = b.min.y + W * (i + 0.5) / n; z = b.min.z + H * (j + 0.5) / n
            r = SP.find_nearest_ray_intersection_with_mesh(dyn, bvh, unreal.Vector(b.min.x - 5.0, y, z), unreal.Vector(1, 0, 0), unreal.GeometryScriptSpatialQueryOptions())
            hit = first(r, unreal.GeometryScriptRayHitResult)
            if not (hit and hit.hit): clear.append((y, z))
    # the window is the clear cells in the UPPER part that are ringed by frame: drop the rows that
    # are clear across the whole width (nothing there at all, above or beside the sight)
    rows = {}
    for y, z in clear: rows.setdefault(round(z, 3), []).append(y)
    win = [(y, z) for y, z in clear if len(rows[round(z, 3)]) < n - 1 and z > H * 0.3]
    if not win: raise RuntimeError('no window found in the part')
    yc = sum(y for y, z in win) / len(win); zc = sum(z for y, z in win) / len(win)
    ys = [y for y, z in win]; zs = [z for y, z in win]
    win_w = (max(ys) - min(ys)) + W / n; win_h = (max(zs) - min(zs)) + H / n
    print('window: %d of %d rays clear; centre y %.2f z %.2f; %.1f wide x %.1f tall' % (len(win), n * n, yc, zc, win_w, win_h))
    # the frame's x extent at the window height: where along the sight the ring is, for the pane
    xs = []
    for t in range(dyn.get_triangle_count()):
        pts = [q for q in MQ.get_triangle_positions(dyn, t) if isinstance(q, unreal.Vector)] if isinstance(MQ.get_triangle_positions(dyn, t), tuple) else []
        for q in pts:
            if abs(q.z - zc) < win_h * 0.6 and abs(q.y - yc) < win_w * 0.8: xs.append(q.x)
    lens_x = (min(xs) + max(xs)) * 0.5 if xs else 0.0
    print('ring runs x %.2f..%.2f; pane at x %.2f' % (min(xs) if xs else 0, max(xs) if xs else 0, lens_x))
    # the pane, on its own slot
    lens = unreal.DynamicMesh(); opts = unreal.GeometryScriptPrimitiveOptions()
    xf = unreal.Transform(unreal.Vector(lens_x, yc, zc), unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0), unreal.Vector(1, 1, 1))   # pitch 90: the pane's normal from +Z to -X, facing the eye
    P.append_rectangle_xy(lens, opts, xf, win_h * 1.08, win_w * 1.08, 1, 1)
    unreal.GeometryScript_Normals.set_per_face_normals(lens)
    ml = AU.get_material_list_from_static_mesh(src); body_mats = list(ml[0]) if ml else []
    glass_mat = unreal.load_asset(GLASS)
    dyn, combined = unreal.GeometryScript_MeshEdits.append_mesh_with_materials(dyn, body_mats, lens, [glass_mat], unreal.Transform())
    unreal.GeometryScript_Normals.set_per_face_normals(dyn)
    unreal.GeometryScript_Normals.recompute_normals(dyn, unreal.GeometryScriptCalculateNormalsOptions())
    full = PKG + '/' + NAME
    asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
    to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    to_opts.enable_recompute_normals = True; to_opts.enable_recompute_tangents = True
    to_opts.replace_materials = True; to_opts.new_materials = list(combined) if combined else body_mats + [glass_mat]
    if asset:
        AU.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
    else:
        create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions(); create.enable_recompute_normals = True; create.enable_recompute_tangents = True
        asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
        if asset: asset.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=m) for m in (combined if combined else body_mats + [glass_mat])])
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    doc = json.load(io.open(CAT, encoding='utf-8'))
    doc.setdefault('optics', {})[KEY] = {'mesh': full, 'eye': [round(lens_x, 2), round(yc, 2), round(zc, 2)], 'sit': round(zc, 2), 'name': PRETTY, 'source': SRC}
    if ASSIGN_TO in doc['weapons']:
        doc['weapons'][ASSIGN_TO]['optic'] = KEY
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('OPTIC %s: eye (%.2f, %.2f, %.2f), sit %.2f; %s now mounts %s' % (full, lens_x, yc, zc, zc, ASSIGN_TO, KEY))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
