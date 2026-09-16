"""Where the eye goes on every gun, measured off the geometry.

    python Tools/ue_remote.py --file Tools/derive_sights.py

Adds a "sight" to every ranged entry in UI/Weapons.json: the point the player's eye lines up
behind when aiming. ABaseCharacter::TickSightAlignment puts that point on the camera's view
axis, so what the player looks through is the weapon's own sight line.

THIS IS ONLY POSSIBLE BECAUSE OF HAC1. Every weapon mesh is now baked into one space -- +X down
the barrel, +Z up, origin at the grip -- so the sight line is not a per-weapon mystery, it is a
measurement anyone could repeat:

  the rear sight is the highest point on the centreline, in the rear third of the weapon.

That is true of every gun ever made, and in normalised space it is three lines of code. Before
normalisation the same question needed to know which of five axis conventions this particular
mesh used before it could even start.

Two refinements that matter:

  * The percentile trick from Tools/measure_face_planes.py. A carry handle, an aerial or a
    scope mount is often the single highest vertex and is not the sight; taking the 92nd
    percentile of height finds the top of the RECEIVER instead.
  * The eye sits a little above the rear sight in practice, not exactly on it, because a
    shooter looks over the top of the notch rather than through the middle of the metal. That
    is SIGHT_RISE.

A weapon with no sensible answer -- a shield, a sword -- simply gets no "sight" field, and the
runtime aims down the bore instead.
"""
import unreal, json, io, traceback, collections

CAT = r'C:\Dev\Games\RepliCan\UI\Weapons.json'
# Which slice along the barrel the rear sight lives in, as fractions of total length from the back.
REAR_FROM = 0.05
REAR_TO = 0.45
# How near the centreline a vertex has to be to count. Sights are on the middle of the weapon.
CENTRE_BAND = 2.2
# Ignore the top few outliers: a carry handle is not a sight.
TOP_PERCENTILE = 0.92
# The eye is above the notch, not in it.
SIGHT_RISE = 0.8


def measure(mesh):
    dyn = unreal.DynamicMesh()
    dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    n = dyn.get_vertex_count()
    pts = []
    for i in range(n):
        p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
        if ok:
            pts.append((p.x, p.y, p.z))
    if not pts:
        return None
    xs = [p[0] for p in pts]
    lo, hi = min(xs), max(xs)
    length = hi - lo
    if length <= 1.0:
        return None
    x0 = lo + length * REAR_FROM
    x1 = lo + length * REAR_TO
    band = [p for p in pts if x0 <= p[0] <= x1 and abs(p[1]) <= CENTRE_BAND]
    if len(band) < 4:
        # A weapon too narrow or too odd for the centre band; widen once rather than give up.
        band = [p for p in pts if x0 <= p[0] <= x1 and abs(p[1]) <= CENTRE_BAND * 2.5]
    if len(band) < 4:
        return None
    zs = sorted(p[2] for p in band)
    top = zs[min(len(zs) - 1, int(len(zs) * TOP_PERCENTILE))]
    at = [p for p in band if abs(p[2] - top) <= 1.0]
    sx = sum(p[0] for p in at) / len(at)
    return (round(sx, 2), 0.0, round(top + SIGHT_RISE, 2), round(length, 1))


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')

    doc = json.load(io.open(CAT, encoding='utf-8'))
    tally = collections.Counter()
    for key in sorted(doc['weapons']):
        e = doc['weapons'][key]
        if not e.get('ranged'):
            e.pop('sight', None)
            tally['melee, skipped'] += 1
            continue
        if e.get('space') != 'hac1':
            tally['NOT NORMALISED'] += 1
            print('   not hac1, skipped:', key)
            continue
        mesh = unreal.load_asset(e['mesh'])
        if not mesh:
            tally['mesh missing'] += 1
            continue
        got = measure(mesh)
        if not got:
            tally['no sight found'] += 1
            print('   no sight line:', key)
            continue
        sx, sy, sz, length = got
        e['sight'] = [sx, sy, sz]
        tally['derived'] += 1
        print('%-34s len%7.1f  sight (%6.2f, %4.1f, %5.2f)' % (key, length, sx, sy, sz))

    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('SIGHTS', dict(tally))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
