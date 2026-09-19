"""Move each gun's origin onto its actual pistol grip. Finds the grip by SHAPE, not by fraction.

    python Tools/ue_remote.py --file Tools/reseat_grips.py

WHAT WENT WRONG. Tools/normalise_weapons.py placed the origin at "40% of the rear extent back,
45% of the drop down". On a pistol that lands in the grip, because a pistol is mostly grip. On a
rifle it lands in the BUTTSTOCK -- 27 cm behind the grip on the frontier assault rifle -- so the
weapon hung off the hand by a quarter of a metre no matter how good the socket was.

A fraction of a bounding box cannot find a grip. A grip is a shape, and it has an unmistakable
one in the side profile (print it with Tools/profile_weapon.py):

    stock      shallow, narrow
    grip       NARROW and DEEP        <- the only thing that is both
    magazine   WIDE and deep, just in front of the grip
    handguard  shallow again

So: find the deepest block under the weapon -- that is the magazine -- and the trigger grip is
the narrow, deep run immediately BEHIND it. That is what a magazine well is: the thing your
hand is behind.

BULLPUPS INVERT THIS. On a bullpup the magazine sits behind the trigger grip, in the stock, so
"the run behind the magazine" finds the buttplate instead. A weapon flagged "bullpup": true in
UI/Weapons.json looks in FRONT of the magazine instead. The flag is declared rather than
guessed: the two layouts are not reliably distinguishable from a profile, because a bullpup and
a conventional rifle with a long receiver produce much the same column of numbers. Candidates
are reported so a real one is easy to spot and flag.

SAFETY. A weapon already sitting within TOLERANCE of its grip is left alone: the pistols are
fine and moving them would be churn with a chance of making them worse. Everything moved is
listed with the distance, so a silly number is visible rather than silent. The bake is
destructive, so Backups/Weapons_preHAC1 is the way back.

The catalogue points (muzzle, sights, fore grip) are all in mesh space, so they are shifted by
the same amount rather than re-derived -- re-deriving would work too, but shifting cannot
disagree with what was actually done to the vertices.
"""
import unreal, json, io, traceback, collections

CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'
DRY_RUN = False
# Where the hand closes, in the mesh's CURRENT space, for weapons the shape rule gets wrong.
# Read off the side-view icon (RawArt/Icons, muzzle right, bounds centred): the Frontier
# assault rifle's rule found the magazine well, 17.5 cm ahead of the pistol grip.
GRIP_OVERRIDE = {
    'Worlds/Wep_Assault_01': (-17.5, 0.0, 3.0),
}
ONLY = []                # a few catalogue keys while iterating; empty means every gun
BUCKET = 2.0             # cm along the barrel
BAND = 3.0               # how near the centreline a vertex has to be
MIN_DEPTH = 7.0          # a grip hangs at least this far below the bore
WIDTH_FRACTION = 0.70    # ... and is no wider than this much of the weapon's widest point
GRIP_DOWN = 0.55         # the hand closes this far down the grip, not at its tip
TOLERANCE = 6.0          # already this close? leave it alone


def grip_of(pts, bullpup=False):
    xs = [p[0] for p in pts]
    lo, hi = min(xs), max(xs)
    fwd = [p for p in pts if p[0] > hi * 0.55 and abs(p[1]) <= BAND]
    if not fwd:
        return None
    bore = (min(p[2] for p in fwd) + max(p[2] for p in fwd)) * 0.5
    widest = max(abs(p[1]) for p in pts) * 2.0

    band = collections.defaultdict(list)
    for p in pts:
        if abs(p[1]) <= BAND:
            band[int(p[0] // BUCKET)].append(p)

    cols = []
    for b in sorted(band):
        below = [p for p in band[b] if p[2] < bore]
        if not below:
            continue
        cols.append((b, bore - min(p[2] for p in below), max(abs(p[1]) for p in below) * 2.0))
    if not cols:
        return None

    # The deepest thing under a rifle is the magazine. That is the landmark: on a conventional
    # weapon the trigger hand is immediately BEHIND it, and on a bullpup immediately in FRONT,
    # because a bullpup's magazine lives in the stock.
    mag = max(cols, key=lambda c: c[1])

    # Either side of the magazine, the grip is the run of buckets deep enough to be a grip and
    # NARROWER than the magazine. Narrow-and-deep is the grip's signature; the receiver floor is
    # deep but as wide as the body, and the stock is neither.
    if bullpup:
        near = [c for c in cols if c[0] > mag[0] and c[1] >= MIN_DEPTH and c[2] < mag[2] * 0.95]
    else:
        near = [c for c in cols if c[0] < mag[0] and c[1] >= MIN_DEPTH and c[2] < mag[2] * 0.95]
    if near:
        # Walk outward from the magazine, stopping at the first gap: the run touching the
        # magazine well is the grip, not some further feature beyond it.
        ordered = near if bullpup else list(reversed(near))
        run = [ordered[0]]
        for c in ordered[1:]:
            if abs(c[0] - run[-1][0]) <= 1:
                run.append(c)
            else:
                break
        run.sort(key=lambda c: c[0])
    else:
        # No separate grip: the deepest thing IS what the hand holds, which is the usual shape
        # of a pistol.
        run = [c for c in cols if abs(c[0] - mag[0]) <= 1]

    gx = (run[0][0] * BUCKET + run[-1][0] * BUCKET + BUCKET) * 0.5
    depth = max(c[1] for c in run)
    return (gx, 0.0, bore - depth * GRIP_DOWN)


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before baking meshes')

    doc = json.load(io.open(CAT, encoding='utf-8'))
    tally = collections.Counter()
    POINTS = ('muzzle', 'sight', 'rear_sight', 'front_sight', 'fore_grip', 'optic_mount')

    for key in sorted(doc['weapons']):
        if ONLY and key not in ONLY:
            continue
        e = doc['weapons'][key]
        if not e.get('ranged') or e.get('space') != 'hac1':
            continue
        mesh = unreal.load_asset(e['mesh'])
        if not mesh:
            tally['mesh missing'] += 1
            continue
        dyn = unreal.DynamicMesh()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        pts = []
        for i in range(dyn.get_vertex_count()):
            p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
            if ok:
                pts.append((p.x, p.y, p.z))
        grip = GRIP_OVERRIDE.get(key) or grip_of(pts, bool(e.get('bullpup')))
        if not grip:
            tally['no grip found'] += 1
            print('%-32s no grip shape found' % key)
            continue

        gx, gy, gz = grip
        dist = (gx * gx + gz * gz) ** 0.5
        if dist <= TOLERANCE:
            tally['already seated'] += 1
            continue

        note = ''
        if not e.get('bullpup'):
            xs2 = [p[0] for p in pts]
            lo2, hi2 = min(xs2), max(xs2)
            # The grip ending up in the back fifth of the weapon usually means the magazine was
            # behind it and this is really a bullpup.
            if gx < lo2 + (hi2 - lo2) * 0.2:
                note = '   <-- possibly a bullpup; set "bullpup": true and re-run'
        print('%-32s grip at (%6.2f, %5.2f)  moving origin %5.1f cm%s' % (key, gx, gz, dist, note))
        tally['reseated'] += 1
        if DRY_RUN:
            continue

        shift = unreal.Transform(location=unreal.Vector(-gx, -gy, -gz),
                                 rotation=unreal.Rotator(roll=0, pitch=0, yaw=0),
                                 scale=unreal.Vector(1, 1, 1))
        unreal.GeometryScript_MeshTransforms.transform_mesh(dyn, shift)
        opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        opts.enable_recompute_normals = False
        opts.enable_recompute_tangents = False
        opts.replace_materials = False
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, mesh, opts, unreal.GeometryScriptMeshWriteLOD())
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        for f in POINTS:
            if f in e and isinstance(e[f], list) and len(e[f]) >= 3:
                e[f] = [round(e[f][0] - gx, 2), round(e[f][1] - gy, 2), round(e[f][2] - gz, 2)]
        # Everything else authored in this mesh's space moves with it: the baked parts and the
        # stripped body (meshes, transformed the same way) and the parts' bounds.
        for kind, box in e.get('parts_bounds', {}).items():
            e['parts_bounds'][kind] = [[round(box[0][0] - gx, 2), round(box[0][1] - gy, 2), round(box[0][2] - gz, 2)],
                                       [round(box[1][0] - gx, 2), round(box[1][1] - gy, 2), round(box[1][2] - gz, 2)]]
        for path in list(e.get('parts', {}).values()) + ([e['body_mesh']] if e.get('body_mesh') else []):
            other = unreal.load_asset(path.split('.')[0])
            if not other:
                print('   sibling mesh missing, not moved:', path); continue
            od = unreal.DynamicMesh()
            od, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(other, od, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
            unreal.GeometryScript_MeshTransforms.transform_mesh(od, shift)
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(od, other, opts, unreal.GeometryScriptMeshWriteLOD())
            unreal.EditorAssetLibrary.save_loaded_asset(other, False)
        e['grip_reseated'] = [round(e.get('grip_reseated', [0, 0, 0])[0] - gx, 2), round(e.get('grip_reseated', [0, 0, 0])[1] - gy, 2), round(e.get('grip_reseated', [0, 0, 0])[2] - gz, 2)]

    if not DRY_RUN:
        io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('')
    print('RESEAT', dict(tally), '(dry run)' if DRY_RUN else '')
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
