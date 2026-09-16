"""Bake every weapon mesh into HAC1 grip space. See Docs/HeldAssetStandard.md.

    python Tools/ue_remote.py --file Tools/normalise_weapons.py

The library arrived in five different local spaces -- guns along +Y, blades along +Z, a
handful of each backwards, one shuriken along +X. Rather than teach the game about all five,
this rotates and shifts the vertices themselves so that afterwards every weapon means the
same thing by its own axes:

    +X forward (muzzle, blade tip, shield face)
    +Z up      (sight rail, blade spine, top of the shield)
    origin     where the right hand closes

It is DESTRUCTIVE and idempotent: a weapon whose catalogue entry is stamped "space": "hac1"
is skipped, so re-running after adding weapons only touches the new ones. To redo one, list
its key in "force" in UI/WeaponGrips.json -- and restore the mesh from source control first,
because the tool has no idea what the un-normalised original looked like.

The muzzle is re-derived afterwards, since it was recorded in the old space.
"""
import unreal, json, io, traceback, collections

ROOT = r'C:\Dev\Games\RepliCan'
CAT = ROOT + r'\UI\Weapons.json'
RULES = ROOT + r'\UI\WeaponGrips.json'
ONLY = []          # a few catalogue keys while iterating; empty means the whole library
DRY_RUN = False    # True prints the transforms without touching a mesh

V = unreal.Vector
ML = unreal.MathLibrary


def resolve_axis(name, order):
    """order is (long_index, wide_index, thin_index)."""
    return {'long': order[0], 'wide': order[1], 'thin': order[2]}[name]


def box_of(mesh_asset):
    b = mesh_asset.get_bounds()
    c, e = b.origin, b.box_extent
    return ([c.x - e.x, c.y - e.y, c.z - e.z], [c.x + e.x, c.y + e.y, c.z + e.z])


def rotated_box(lo, hi, xform):
    """Corners through the rotation, so the grip can be read off the NEW bounds."""
    nlo = [1e9] * 3
    nhi = [-1e9] * 3
    for i in (0, 1):
        for j in (0, 1):
            for k in (0, 1):
                p = xform.transform_location(V((hi if i else lo)[0], (hi if j else lo)[1], (hi if k else lo)[2]))
                for a, val in enumerate((p.x, p.y, p.z)):
                    nlo[a] = min(nlo[a], val)
                    nhi[a] = max(nhi[a], val)
    return nlo, nhi


def grip_component(rule, lo, hi, axis):
    which, frac = rule
    if which == 'zero':
        return 0.0
    base = lo[axis] if which == 'min' else (hi[axis] if which == 'max' else (hi[axis] - lo[axis]))
    return base * frac


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before baking meshes')

    doc = json.load(io.open(CAT, encoding='utf-8'))
    cat = doc['weapons']
    rules = json.load(io.open(RULES, encoding='utf-8'))
    hints = rules['axis_hints']
    grips = rules['grip_rule']
    overrides = rules.get('overrides', {})
    force = set(rules.get('force', []))

    tally = collections.Counter()
    report = []

    for key in sorted(cat):
        if ONLY and key not in ONLY:
            continue
        e = cat[key]
        if e.get('space') == 'hac1' and key not in force:
            tally['already hac1'] += 1
            continue
        asset = unreal.load_asset(e['mesh'])
        if not asset:
            tally['missing'] += 1
            continue

        ov = overrides.get(key, {})
        hint = dict(hints.get('default'))
        hint.update(hints.get(e['kind'], {}))
        hint.update({k: v for k, v in ov.items() if k in ('forward', 'up', 'forward_sign', 'up_sign')})
        rule = grips.get(e['kind'], grips['default'])

        lo, hi = box_of(asset)
        ext = [hi[i] - lo[i] for i in range(3)]
        order = sorted(range(3), key=lambda i: -ext[i])          # long, wide, thin
        fa = resolve_axis(hint['forward'], order)
        ua = resolve_axis(hint['up'], order)
        if ua == fa:                                              # a hint that collides: take the next
            ua = next(i for i in order if i != fa)

        fs = hint['forward_sign']
        if fs == 'auto':
            fs = 1 if abs(hi[fa]) >= abs(lo[fa]) else -1
        us = int(hint['up_sign'])

        f = V(*[fs if i == fa else 0.0 for i in range(3)])
        u = V(*[us if i == ua else 0.0 for i in range(3)])
        # make_rot_from_xz builds the rotation that SENDS local X to f and local Z to u; the
        # bake wants the opposite journey, so invert it.
        rot = ML.invert_transform(unreal.Transform(location=V(0, 0, 0), rotation=ML.make_rot_from_xz(f, u), scale=V(1, 1, 1)))
        if ov.get('extra_rot'):
            p, y, r = ov['extra_rot']
            rot = ML.compose_transforms(rot, unreal.Transform(location=V(0, 0, 0), rotation=unreal.Rotator(roll=r, pitch=p, yaw=y), scale=V(1, 1, 1)))

        nlo, nhi = rotated_box(lo, hi, rot)
        grip = V(grip_component(rule['x'], nlo, nhi, 0),
                 grip_component(rule['y'], nlo, nhi, 1),
                 grip_component(rule['z'], nlo, nhi, 2))
        if ov.get('extra_loc'):
            grip = grip + V(*ov['extra_loc'])

        full = unreal.Transform(location=V(-grip.x, -grip.y, -grip.z), rotation=rot.rotation.rotator(), scale=V(1, 1, 1))
        report.append('%-34s %-15s %s%s up%s%s  grip(%.1f, %.1f, %.1f)  len %.1f' % (
            key, e['kind'], '+-'[fs < 0], 'XYZ'[fa], '+-'[us < 0], 'XYZ'[ua], grip.x, grip.y, grip.z, nhi[0] - nlo[0]))
        if DRY_RUN:
            tally['dry'] += 1
            continue

        dyn = unreal.DynamicMesh()
        opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset, dyn, opts, unreal.GeometryScriptMeshReadLOD())
        unreal.GeometryScript_MeshTransforms.transform_mesh(dyn, full)
        to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        to_opts.enable_recompute_normals = False
        to_opts.enable_recompute_tangents = False
        to_opts.replace_materials = False
        to_opts.new_nanite_settings = asset.get_editor_property('nanite_settings')
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)

        # The muzzle was recorded in the old space; in HAC1 it is the far +X face, on the bore
        # line. Read it back off the SAVED asset rather than trusting the maths.
        blo, bhi = box_of(asset)
        e['muzzle'] = [round(bhi[0], 2), 0.0, round((blo[2] + bhi[2]) * 0.5, 2)]
        e['space'] = 'hac1'
        tally['baked'] += 1

    if not DRY_RUN:
        io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    for line in report:
        print(line)
    print('NORMALISE', dict(tally))
except Exception:
    io.open(ROOT + r'\RawArt\render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
