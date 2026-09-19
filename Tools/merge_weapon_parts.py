"""Weld a weapon's loose parts into its one baked mesh.

    python Tools/ue_remote.py --file Tools/merge_weapon_parts.py

Synty ships some weapons in pieces so a game can animate them, and a piece modelled about its
own origin says nothing about where it goes: Tools/import_horror_weapons.py leaves those out
rather than weld them somewhere wrong. Where a piece belongs is written down in exactly one
place, the pack's demo maps, where its own artists put the pieces together. MERGES below
records each part's seat in its parent's PACK-SPACE frame, read off such a map. (The Horror
pack's Overview map stands SM_Wep_Wrench_01 at rotation zero with its clamp 1.93 behind and
49.81 up the handle; the two tilted copies in its Demo map agree to the hundredth. The maps
were read as loaded World objects, never opened: scratch wrench_demo_scan, 2026-09-17.)

Our copy of the weapon has since been rotated and shifted into HAC1 space by
Tools/normalise_weapons.py and Tools/reseat_grips.py, neither of which wrote its transform
down; it is recovered the way Tools/bake_weapon_parts.py recovers it, from the pack mesh and
the baked mesh directly (one of the 24 axis swaps plus a voted translation, checked against
every vertex), then applied to the seated part before the part is appended to the body. One
static mesh comes out, so the weapon spawns, renders, drops, is held and has its icon drawn as
one actor everywhere -- nothing in the game needs to know it was ever two.

Idempotent: a catalogue entry stamped "merged_parts" is skipped. The muzzle (the far +X face,
in HAC1) is re-read from the saved mesh, since a welded jaw can lengthen a tool. Re-render the
icon afterwards: Tools/warm_icon_backdrop.py, then Tools/render_weapon_icons.py with ONLY set,
then Tools/import_icons.py.
"""
import unreal, io, json, os, traceback

CAT = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
# catalogue key -> (pack mesh folder, [(part asset, (x, y, z) seat, (roll, pitch, yaw)) in the parent's pack frame])
MERGES = {
    'Horror/Wep_Wrench_01': ('/Game/Synty/PolygonSciFiHorror/Meshes/Weapons/', [
        ('SM_Wep_Wrench_01_Clamp_01', (0.0, -1.93, 49.81), (0.0, 0.0, 0.0)),   # the hook jaw: Maps/Overview, wrench at (2983, 669, 0), clamp at (2983, 667.07, 49.81), both unrotated
    ]),
}
MAX_UNMATCHED = 0.02   # share of body vertices the recovered transform may fail to land on the baked body
DRY_RUN = False

V = unreal.Vector
ML = unreal.MathLibrary


# ---- the solver, as in Tools/bake_weapon_parts.py --------------------------------------------
def verts(dyn):
    Q = unreal.GeometryScript_MeshQueries
    out = []
    for i in range(dyn.get_vertex_count()):
        p, ok = Q.get_vertex_position(dyn, i)
        out.append((p.x, p.y, p.z) if ok else None)
    return out


def mesh_to_dyn(asset):
    dyn = unreal.DynamicMesh()
    dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        asset, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    return dyn


def axis_rotations():
    """The 24 proper rotations that map axes to axes, as 3x3 row matrices."""
    import itertools
    out = []
    for perm in itertools.permutations(range(3)):
        for signs in itertools.product((1, -1), repeat=3):
            R = [[0, 0, 0] for _ in range(3)]
            for r in range(3):
                R[r][perm[r]] = signs[r]
            det = (R[0][0] * (R[1][1] * R[2][2] - R[1][2] * R[2][1])
                   - R[0][1] * (R[1][0] * R[2][2] - R[1][2] * R[2][0])
                   + R[0][2] * (R[1][0] * R[2][1] - R[1][1] * R[2][0]))
            if det > 0:
                out.append(R)
    return out


def solve_rigid(src, dst):
    """(R, t, apply_R, misses): dst ~ R*src + t. The rotation is one of the 24 axis swaps; the
    translation is voted for by a sample of source vertices against every baked vertex, then
    refined from one agreeing pair and checked against every source vertex. Extra geometry in
    the baked mesh costs nothing: the source only has to be a subset of it."""
    src = [p for p in src if p is not None]; dst = [p for p in dst if p is not None]
    if len(src) < 4 or len(dst) < 4:
        return None
    lut = set((round(p[0], 1), round(p[1], 1), round(p[2], 1)) for p in dst)
    near = [(dx, dy, dz) for dx in (-0.1, 0.0, 0.1) for dy in (-0.1, 0.0, 0.1) for dz in (-0.1, 0.0, 0.1)]
    step = max(1, len(src) // 40)
    sample = src[::step][:40]
    best = None
    for R in axis_rotations():
        def apply_R(p, R=R): return tuple(sum(R[r][k] * p[k] for k in range(3)) for r in range(3))
        votes = {}
        for p in sample:
            rp = apply_R(p)
            for q in dst:
                key = (round((q[0] - rp[0]) * 2) / 2.0, round((q[1] - rp[1]) * 2) / 2.0, round((q[2] - rp[2]) * 2) / 2.0)
                votes[key] = votes.get(key, 0) + 1
        if not votes:
            continue
        t_coarse = max(votes, key=votes.get)
        if votes[t_coarse] < max(3, len(sample) // 3):
            continue
        t = t_coarse
        for p in sample:
            rp = apply_R(p)
            for q in dst:
                d = (q[0] - rp[0], q[1] - rp[1], q[2] - rp[2])
                if abs(d[0] - t_coarse[0]) <= 0.3 and abs(d[1] - t_coarse[1]) <= 0.3 and abs(d[2] - t_coarse[2]) <= 0.3:
                    t = d; break
            else:
                continue
            break
        misses = 0
        for p in src:
            q = apply_R(p); q = (q[0] + t[0], q[1] + t[1], q[2] + t[2])
            if not any((round(q[0] + n[0], 1), round(q[1] + n[1], 1), round(q[2] + n[2], 1)) in lut for n in near):
                misses += 1
                if best is not None and misses > best[3]:
                    break
        if best is None or misses < best[3]:
            best = (R, t, apply_R, misses)
    return best


def rot_to_unreal(R):
    """A 3x3 rows matrix as an unreal.Rotator, via the axes it sends X and Z to."""
    fx = V(R[0][0], R[1][0], R[2][0])   # image of local X
    fz = V(R[0][2], R[1][2], R[2][2])   # image of local Z
    return ML.make_rot_from_xz(fx, fz)


def box(pts):
    pts = [p for p in pts if p]
    return ([round(min(p[i] for p in pts), 2) for i in range(3)], [round(max(p[i] for p in pts), 2) for i in range(3)])


def write_catalogue(doc):
    """Back in the file's own style: the catalogue is tab-indented with CRLF line ends."""
    raw = io.open(CAT, encoding='utf-8', newline='').read()
    tabs = '\n\t' in raw
    text = json.dumps(doc, indent=('\t' if tabs else 1), ensure_ascii=False)
    if '\r\n' in raw:
        text = text.replace('\n', '\r\n')
    io.open(CAT, 'w', encoding='utf-8', newline='').write(text)


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before merging')
    doc = json.load(io.open(CAT, encoding='utf-8'))
    cat = doc['weapons']
    changed = False
    for key, (src_dir, parts) in MERGES.items():
        e = cat.get(key)
        if not e:
            print('MISSING catalogue entry', key); continue
        already = set(e.get('merged_parts', []))
        todo = [p for p in parts if p[0] not in already]
        if not todo:
            print('%-24s already carries %s' % (key, ', '.join(sorted(already)))); continue
        baked_path = e['mesh'].split('.')[0]
        name = baked_path.rsplit('/', 1)[-1]
        src_asset = unreal.load_asset(src_dir + name)
        dst_asset = unreal.load_asset(baked_path)
        if not src_asset or not dst_asset:
            print('REFUSED', key, 'pack body or baked body missing'); continue
        sv = verts(mesh_to_dyn(src_asset)); dv = verts(mesh_to_dyn(dst_asset))
        solved = solve_rigid(sv, dv)
        if not solved:
            print('REFUSED', key, 'degenerate'); continue
        R, t, apply_R, misses = solved
        n_src = sum(1 for p in sv if p is not None)
        share = misses / float(max(1, n_src))
        if share > MAX_UNMATCHED:
            print('REFUSED %s: %d of %d pack vertices do not land on the baked body (extents %s vs %s)' % (key, misses, n_src, box(sv), box(dv))); continue
        bake = unreal.Transform(location=V(t[0], t[1], t[2]), rotation=rot_to_unreal(R), scale=V(1, 1, 1))
        print('%s: bake recovered, %d/%d vertices unmatched; rows %s shift (%.2f, %.2f, %.2f)' % (key, misses, n_src, R, t[0], t[1], t[2]))

        body = mesh_to_dyn(dst_asset)
        before = body.get_triangle_count()
        for part_name, seat, rpy in todo:
            part_asset = unreal.load_asset(src_dir + part_name)
            if not part_asset:
                print('  MISSING part', part_name); continue
            n_mats = len(part_asset.get_editor_property('static_materials'))
            if n_mats != 1:
                print('  REFUSED part %s: %d material slots; only a one-material part can share the body slot' % (part_name, n_mats)); continue
            pd = mesh_to_dyn(part_asset)
            seat_xf = unreal.Transform(location=V(*seat), rotation=unreal.Rotator(roll=rpy[0], pitch=rpy[1], yaw=rpy[2]), scale=V(1, 1, 1))
            total = ML.compose_transforms(seat_xf, bake)   # seat it in the pack frame, then take the body's journey into HAC1
            unreal.GeometryScript_MeshTransforms.transform_mesh(pd, total)
            lo, hi = box(verts(pd))
            print('  %s: %d tris, in HAC1 x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f' % (part_name, pd.get_triangle_count(), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
            unreal.GeometryScript_MeshEdits.append_mesh(body, pd, unreal.Transform())
            already.add(part_name)
        if body.get_triangle_count() == before:
            continue
        if DRY_RUN:
            print('  dry run: %d -> %d tris' % (before, body.get_triangle_count())); continue
        to = unreal.GeometryScriptCopyMeshToAssetOptions()
        to.enable_recompute_normals = False
        to.enable_recompute_tangents = True
        to.replace_materials = False
        to.new_nanite_settings = dst_asset.get_editor_property('nanite_settings')
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(body, dst_asset, to, unreal.GeometryScriptMeshWriteLOD())
        unreal.EditorAssetLibrary.save_loaded_asset(dst_asset, False)
        b = dst_asset.get_bounds()
        far_x = round(b.origin.x + b.box_extent.x, 2)
        if e.get('muzzle') and far_x > e['muzzle'][0] + 0.05:
            print('  muzzle x %.2f -> %.2f (the far +X face moved)' % (e['muzzle'][0], far_x))
            e['muzzle'][0] = far_x
        e['merged_parts'] = sorted(already)
        changed = True
        print('  %s: %d -> %d tris, saved' % (name, before, body.get_triangle_count()))
    if changed:
        write_catalogue(doc)
        print('catalogue stamped')
except Exception:
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
    print(traceback.format_exc())
