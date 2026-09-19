"""Bring the SciFi Worlds weapon PARTS into HAC1, aligned to the bodies we already baked.

    python Tools/ue_remote.py --file Tools/bake_weapon_parts.py

The Worlds pack is the one kit that ships its weapons in pieces: for SM_Wep_Assault_01 there is
also SM_Wep_Assault_01_Scope_01, _Mag_01 and _Trigger_01, all modelled IN PLACE in one shared
frame, plus a Scopes/ folder of eight optics that belong to no particular gun. That is the
built-in means of mixing optics -- but only if the parts end up in the same space as the body
they were drawn against, and the body has since been rotated, shifted to its grip and reseated
by Tools/normalise_weapons.py and Tools/reseat_grips.py, none of which wrote its transform down.

THE TRANSFORM IS RECOVERED, NOT REPLAYED. Every one of those steps was rigid, so the baked body
is the source body under one rotation-plus-translation, and that transform can be solved from
the two meshes directly: same vertices, same order, new positions. Three well-spread vertex
pairs fix it exactly; the fit is then checked against EVERY vertex and a weapon whose residual
is not near zero is refused rather than guessed at. The same transform is applied to each of
its parts, which lands them on the body to the millimetre because they were drawn in its frame.

Output: /Game/RepliCan/Weapons/Worlds/Parts/SM_Wep_<gun>_<Part>, and the catalogue entry gains

    "parts": {"scope": path, "mag": path, "trigger": path, ...}
    "parts_bounds": {"scope": [[x0,y0,z0],[x1,y1,z1]], ...}     HAC1 space

The universal Scopes/ optics have no body to align to; they are copied as they are, with their
own pivot as the mount point, and listed under "optics" at the top of the catalogue for
Tools/fit_optics.py to mount the way it mounts SM_Optic_RedDot_01.
"""
import unreal, io, json, os, re, traceback

CAT = os.path.join(unreal.Paths.project_dir(), 'Content', 'GameData', 'UI', 'Weapons.json')
SRC = '/Game/PolygonSciFiWorlds/Models/Weapons/Parts/'
SCOPES = '/Game/PolygonSciFiWorlds/Models/Weapons/Scopes/'
OUT_PKG = '/Game/RepliCan/Weapons/Worlds/Parts'
OPTIC_PKG = '/Game/RepliCan/Weapons/Optics'
PART_KINDS = {'Scope': 'scope', 'RedDot': 'reddot', 'Mag': 'mag', 'Trigger': 'trigger',
              'ForeBarrel': 'forebarrel', 'Forestock': 'forestock', 'Barrel': 'barrel',
              'Chamber': 'chamber', 'Cylander': 'cylinder', 'Hammer': 'hammer'}
MAX_RESIDUAL_CM = 0.05
DRY_RUN = False


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


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a):
    l = dot(a, a) ** 0.5
    return (a[0] / l, a[1] / l, a[2] / l) if l > 1e-9 else a


def frame(p0, p1, p2):
    """An orthonormal frame from three points: rows are the basis vectors."""
    e1 = norm(sub(p1, p0))
    v2 = sub(p2, p0)
    e3 = norm(cross(e1, v2))
    e2 = cross(e3, e1)
    return (e1, e2, e3)


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


def bbox_centre(pts):
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]; zs = [p[2] for p in pts]
    return ((min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5, (min(zs) + max(zs)) * 0.5)


def solve_rigid(src, dst):
    """(R, t, apply_R, misses): dst ~ R*src + t, robust to the baked mesh carrying EXTRA parts.

    The bake rotation is one of the 24 axis swaps (normalise_weapons builds it from two unit
    axes). The translation cannot come from bounding boxes: the baked rifles were imported with
    their magazines merged in, which drops the box centre by half a magazine and threw every
    vertex off by exactly that. So the translation is VOTED for instead -- a sample of source
    vertices each nominate q - R*p for every baked vertex q, and the true offset collects one
    vote per sampled vertex while everything else scatters. The winner is then checked against
    every source vertex with an exact position lookup, and the rotation with the fewest misses
    is the answer. Extra geometry in the baked mesh costs nothing: the source only has to be a
    SUBSET of it."""
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
            continue    # no consensus under this rotation
        # Refine the coarse half-centimetre bin to the exact offset from one agreeing pair.
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
    ML = unreal.MathLibrary
    fx = unreal.Vector(R[0][0], R[1][0], R[2][0])   # image of local X
    fz = unreal.Vector(R[0][2], R[1][2], R[2][2])   # image of local Z
    return ML.make_rot_from_xz(fx, fz)


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before baking')
    doc = json.load(io.open(CAT, encoding='utf-8'))
    cat = doc['weapons']
    AR = unreal.AssetRegistryHelpers.get_asset_registry()
    all_parts = [a.asset_name for a in AR.get_assets_by_path(SRC, recursive=False)]
    all_parts = [str(n) for n in all_parts]

    done, refused, skipped = [], [], []
    for key, e in sorted(cat.items()):
        if not key.startswith('Worlds/'):
            continue
        baked_path = e['mesh'].split('.')[0]
        name = baked_path.rsplit('/', 1)[-1]                  # SM_Wep_Assault_01
        base = re.sub(r'^SM_', '', name)                       # Wep_Assault_01
        parts = [p for p in all_parts if p.startswith(name + '_') and p != name]
        if not parts:
            skipped.append((key, 'no parts in the pack'))
            continue
        src_asset = unreal.load_asset(SRC + name)
        dst_asset = unreal.load_asset(baked_path)
        if not src_asset or not dst_asset:
            refused.append((key, 'source or baked body missing')); continue
        sv = verts(mesh_to_dyn(src_asset)); dv = verts(mesh_to_dyn(dst_asset))
        solved = solve_rigid(sv, dv)
        if not solved:
            refused.append((key, 'degenerate')); continue
        R, t, apply_R, misses = solved
        n_src = sum(1 for p in sv if p is not None)
        worst = misses / float(max(1, n_src))
        # A few misses is float noise on a rounded lookup; a fifth of the mesh is a wrong
        # rotation, or a body that was edited rather than moved.
        if worst > 0.02:
            def ext(v):
                v = [q for q in v if q]
                return tuple(round(max(q[i] for q in v) - min(q[i] for q in v), 1) for i in range(3))
            refused.append((key, '%d of %d source vertices unmatched; source extents %s, baked extents %s'
                            % (misses, n_src, ext(sv), ext(dv)))); continue

        xf = unreal.Transform(location=unreal.Vector(t[0], t[1], t[2]), rotation=rot_to_unreal(R),
                              scale=unreal.Vector(1, 1, 1))
        e.setdefault('parts', {}); e.setdefault('parts_bounds', {})
        for part in parts:
            suffix = part[len(name) + 1:]                      # Scope_01
            kind = PART_KINDS.get(re.sub(r'_\d+$', '', suffix), re.sub(r'_\d+$', '', suffix).lower())
            src_part = unreal.load_asset(SRC + part)
            if not src_part:
                continue
            pd = mesh_to_dyn(src_part)
            unreal.GeometryScript_MeshTransforms.transform_mesh(pd, xf)
            out_name = part
            full = OUT_PKG + '/' + out_name
            if not DRY_RUN:
                target = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
                if target:
                    to = unreal.GeometryScriptCopyMeshToAssetOptions()
                    to.enable_recompute_normals = False; to.enable_recompute_tangents = True
                    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(pd, target, to, unreal.GeometryScriptMeshWriteLOD())
                else:
                    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
                    opts.enable_recompute_normals = False; opts.enable_recompute_tangents = True
                    # Returns (outcome, asset) in 5.8, not the asset.
                    res = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(pd, full, opts)
                    target = next((x for x in res if isinstance(x, unreal.StaticMesh)), None) if isinstance(res, tuple) else res
                    if not target:
                        target = unreal.load_asset(full)
                # The create options carry no material list in 5.8, and a re-run over an
                # existing asset must not leave it on the default material either: the part
                # keeps the source's materials by setting them afterwards, the way
                # Tools/make_optics.py does.
                mats = [sm.material_interface for sm in src_part.get_editor_property('static_materials')]
                if target and mats:
                    target.set_editor_property('static_materials',
                        [unreal.StaticMaterial(material_interface=m) for m in mats])
                unreal.EditorAssetLibrary.save_asset(full)
            pv = [p for p in verts(pd) if p]
            lo = [round(min(p[i] for p in pv), 2) for i in range(3)]
            hi = [round(max(p[i] for p in pv), 2) for i in range(3)]
            e['parts'][kind] = full + '.' + out_name
            e['parts_bounds'][kind] = [lo, hi]
        done.append((key, len(parts), worst))

    # The universal optics: copied as they are.
    doc.setdefault('optics', {})
    for a in AR.get_assets_by_path(SCOPES, recursive=False):
        n = str(a.asset_name)
        src = unreal.load_asset(SCOPES + n)
        if not src:
            continue
        full = OPTIC_PKG + '/' + n
        if not DRY_RUN and not unreal.EditorAssetLibrary.does_asset_exist(full):
            unreal.EditorAssetLibrary.duplicate_asset(SCOPES + n, full)
            unreal.EditorAssetLibrary.save_asset(full)
        b = src.get_bounds()
        doc['optics'][n] = {'mesh': full + '.' + n,
                            'size': [round(b.box_extent.x * 2, 2), round(b.box_extent.y * 2, 2), round(b.box_extent.z * 2, 2)],
                            'origin': [round(b.origin.x, 2), round(b.origin.y, 2), round(b.origin.z, 2)]}

    if not DRY_RUN:
        io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('baked parts for %d weapons; %d refused; %d had none; %d universal optics'
          % (len(done), len(refused), len(skipped), len(doc['optics'])))
    for key, n, worst in done:
        print('   %-28s %d parts   unmatched vertices %.1f%%' % (key, n, 100.0 * worst))
    for key, why in refused:
        print('   REFUSED %-20s %s' % (key, why))
    print('   no parts: ' + ', '.join(k for k, _ in skipped))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
