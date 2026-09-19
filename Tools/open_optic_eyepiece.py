"""Give the eyepiece cap to the glass, instead of cutting it out.

A Synty scope is a closed shape. Behind the glass disc there is a LID across the ocular end, in the
body's slot and wearing the pack's opaque atlas material, and at ADS it is the only thing the eye
can see: measured on SM_Wep_Sniper_02_Optic_01, a 4-triangle hexagon at x 2.78 of radius 4.04 cm,
sitting 0.7 cm nearer the eye than the 24-triangle glass disc at x 3.47 of radius 4.71 cm. That is
exactly the white ring around a black hexagon the sight showed.

Tools/make_lens_optic.py answered the same problem on the BugBuster by DELETING the lid. This does
not, because the lid is the better surface of the two:

  - it already faces the shooter (normal.x -1), where the glass disc faces downrange (+1) and is
    backface-culled unless its material is two-sided;
  - it is the nearest surface to the eye, so nothing can get in front of it;
  - M_ScopePiP samples by SCREEN position, so the lid's atlas UVs are irrelevant to it.

So the lid is moved into the glass's material slot and kept. Nothing is destroyed and the change is
reversible by running this with --revert.

The lid also gets the GLASS's UV layout, because it does not have one: measured, the disc spans a
full U 0..1 V 0..1 lens map while the lid's four triangles sit on a single atlas point (U 0.072,
V 0.989) -- a flat colour swatch. Dropped into the glass slot unchanged, a UV-sampling reticle would
render on it as one flat colour. The mapping is not assumed: it is FITTED by least squares from the
disc's own (y,z) -> (u,v) pairs, so the handedness and orientation come from the asset rather than
from a guess, and the fit's residual is printed and must be small.

  Tools/ue_remote --file Tools/open_optic_eyepiece
"""
import unreal, sys, os

Q = unreal.GeometryScript_MeshQueries
AU = unreal.GeometryScript_AssetUtils
MAT = unreal.GeometryScript_Materials
UV = unreal.GeometryScript_UVs

OPTICS = ['/Game/RepliCan/Weapons/Worlds/Parts/SM_Wep_Sniper_02_Optic_01']
GLASS_HINTS = ('Reticle', 'RedDot', 'ScopePiP')
MAX_UV_RESIDUAL = 0.02                # a worse fit than this means the disc is not a flat lens map
REVERT = '--revert' in sys.argv

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if ues.get_game_world() is not None:
    raise RuntimeError('the editor is in Play -- stop PIE before editing a mesh')


def load(path):
    a = unreal.load_asset(path)
    if not a:
        raise RuntimeError('no such mesh: %s' % path)
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(a, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                      unreal.GeometryScriptMeshReadLOD())
    return (r[0] if isinstance(r, tuple) else dyn), a


def tri_ids(dyn):
    tl = unreal.GeometryScript_List.convert_triangle_list_to_array(Q.get_all_triangle_indices(dyn, False)[1])
    return tl


def face_normal_x(dyn, ti):
    n = Q.get_triangle_face_normal(dyn, ti)
    n = n[0] if isinstance(n, tuple) else n
    return n.x


def covers(vs, cy, cz):
    """Seen down the bore, does this triangle contain the axis? A lid does; a rim does not."""
    p = [(v.y - cy, v.z - cz) for v in vs]
    s = []
    for i in range(3):
        a, b = p[i], p[(i + 1) % 3]
        s.append((b[0] - a[0]) * (0 - a[1]) - (0 - a[0]) * (b[1] - a[1]))
    return not (min(s) < 0 and max(s) > 0)


for path in OPTICS:
    dyn, asset = load(path)
    verts = unreal.GeometryScript_List.convert_vector_list_to_array(Q.get_all_vertex_positions(dyn, False)[1])
    tl = tri_ids(dyn)

    slots = []
    for i in range(asset.get_num_sections(0)):
        m = asset.get_material(i)
        slots.append(m.get_name() if m else '')
    glass = next((i for i, n in enumerate(slots) if any(h in n for h in GLASS_HINTS)), None)
    print('%s' % asset.get_name())
    for i, n in enumerate(slots):
        print('   slot %d: %-40s%s' % (i, n, '   <-- the glass' if i == glass else ''))
    if glass is None:
        print('   no glass slot found -- skipped'); continue
    # the bore axis, from the glass itself rather than assumed
    gv = [verts[i] for ti, t in enumerate(tl) if MAT.get_triangle_material_id(dyn, ti)[0] == glass
          for i in (t.x, t.y, t.z)]
    cy = (max(v.y for v in gv) + min(v.y for v in gv)) * 0.5
    cz = (max(v.z for v in gv) + min(v.z for v in gv)) * 0.5
    gx = sum(v.x for v in gv) / float(len(gv))
    print('   glass: %d tris, plane x %.2f, bore centre (y %.2f, z %.2f)' % (len(gv) // 3, gx, cy, cz))

    # BY PLANE, NOT BY TRIANGLE. A hexagonal lid is four triangles and only ONE of them contains the
    # axis; testing each on its own moves that one and leaves the other three sealing the sight. So
    # the flat plates in front of the glass are grouped by their x, the group is judged as a whole,
    # and a group that covers the axis goes across in full. Slot is ignored when grouping, so a
    # partly-moved lid from an earlier run is still recognised as one lid.
    planes = {}
    for ti, t in enumerate(tl):
        vs = [verts[t.x], verts[t.y], verts[t.z]]
        xs = [v.x for v in vs]
        if max(xs) - min(xs) > 0.25:                     # not a flat plate across the tube
            continue
        cx = sum(xs) / 3.0
        if cx >= gx - 0.01:                              # not in front of the glass
            continue
        planes.setdefault(round(cx, 1), []).append((ti, vs))

    moved, restored = [], []
    for px, group in sorted(planes.items()):
        lid = any(covers(vs, cy, cz) for _, vs in group)
        if not lid:
            print('   rim at x %.1f: %d triangle(s) left alone (nothing of it crosses the axis)'
                  % (px, len(group)))
            continue
        for ti, vs in group:
            mid = MAT.get_triangle_material_id(dyn, ti)[0]
            if REVERT:
                if mid == glass:
                    restored.append(ti)
                continue
            if mid == glass:                             # already moved by an earlier run
                continue
            if face_normal_x(dyn, ti) > -0.5:            # faces downrange: not what the eye meets
                continue
            moved.append(ti)
        print('   LID at x %.1f: %d triangle(s) in the plane, %d to %s'
              % (px, len(group), len(restored if REVERT else moved),
                 'return' if REVERT else 'move'))

    todo = restored if REVERT else moved
    if not todo:
        print('   nothing to %s' % ('revert' if REVERT else 'move')); continue

    if not REVERT:
        # FIT (y,z) -> (u,v) from the glass itself. Least squares on an affine map; no assumption
        # about which way U runs.
        rows = []
        for ti, t in enumerate(tl):
            if MAT.get_triangle_material_id(dyn, ti)[0] != glass:
                continue
            ids = UV.get_mesh_triangle_uv_element_i_ds(dyn, 0, ti)
            iv = next((x for x in ids if isinstance(x, unreal.IntVector)), None)
            if iv is None:
                continue
            for vid, eid in zip((t.x, t.y, t.z), (iv.x, iv.y, iv.z)):
                r2 = UV.get_mesh_uv_element_position(dyn, 0, eid)
                uv = next((x for x in r2 if isinstance(x, unreal.Vector2D)), None) if isinstance(r2, tuple) else r2
                if uv is None:
                    continue
                v = verts[vid]
                rows.append((v.y - cy, v.z - cz, uv.x, uv.y))
        if len(rows) < 3:
            print('   could not read the glass UVs -- SKIPPED'); continue

        def solve(col):
            # normal equations for [a b c] . [y z 1] = target
            import math
            A = [[0.0] * 3 for _ in range(3)]; B = [0.0] * 3
            for (y, z, u, vv) in rows:
                x = (y, z, 1.0); tgt = u if col == 0 else vv
                for i in range(3):
                    for j in range(3):
                        A[i][j] += x[i] * x[j]
                    B[i] += x[i] * tgt
            for i in range(3):                       # gaussian elimination
                pmax = max(range(i, 3), key=lambda r: abs(A[r][i]))
                A[i], A[pmax] = A[pmax], A[i]; B[i], B[pmax] = B[pmax], B[i]
                if abs(A[i][i]) < 1e-12:
                    return None
                for r in range(i + 1, 3):
                    f = A[r][i] / A[i][i]
                    for c2 in range(i, 3):
                        A[r][c2] -= f * A[i][c2]
                    B[r] -= f * B[i]
            out = [0.0] * 3
            for i in (2, 1, 0):
                out[i] = (B[i] - sum(A[i][j] * out[j] for j in range(i + 1, 3))) / A[i][i]
            return out

        fu, fv = solve(0), solve(1)
        if not fu or not fv:
            print('   glass UVs are degenerate -- SKIPPED'); continue
        res = 0.0
        for (y, z, u, vv) in rows:
            res = max(res, abs(fu[0]*y + fu[1]*z + fu[2] - u), abs(fv[0]*y + fv[1]*z + fv[2] - vv))
        print('   glass UV map fitted from %d points, worst residual %.4f' % (len(rows), res))
        if res > MAX_UV_RESIDUAL:
            print('   that is too loose to trust -- SKIPPED'); continue

    for ti in todo:
        MAT.set_triangle_material_id(dyn, ti, 0 if REVERT else glass, True)
        if REVERT:
            continue
        t = tl[ti]
        eids = []
        for vid in (t.x, t.y, t.z):
            v = verts[vid]
            y, z = v.y - cy, v.z - cz
            pos = unreal.Vector2D(fu[0]*y + fu[1]*z + fu[2], fv[0]*y + fv[1]*z + fv[2])
            r3 = UV.add_uv_element_to_mesh(dyn, 0, pos)
            eids.append(next((x for x in r3 if isinstance(x, int)), -1) if isinstance(r3, tuple) else r3)
        if all(e >= 0 for e in eids):
            UV.set_mesh_triangle_uv_element_i_ds(dyn, 0, ti, unreal.IntVector(eids[0], eids[1], eids[2]))
    print('   %s %d lid triangle(s) %s the glass slot%s'
          % ('returned' if REVERT else 'moved', len(todo), 'out of' if REVERT else 'into',
             '' if REVERT else ', with the glass UV layout'))

    to = unreal.GeometryScriptCopyMeshToAssetOptions()
    to.enable_recompute_normals = False
    to.enable_recompute_tangents = True
    to.replace_materials = False
    AU.copy_mesh_to_static_mesh(dyn, asset, to, unreal.GeometryScriptMeshWriteLOD())
    unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outermost()], False)
    p = os.path.join(unreal.Paths.project_content_dir(),
                     path.replace('/Game/', '').split('.')[0] + '.uasset')
    print('   saved: %s' % os.path.exists(p))
