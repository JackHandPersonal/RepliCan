"""Work out where a character's face actually is, for any head shape, and write it down.

    python Tools/ue_remote.py --file Tools/measure_face_planes.py

Synty faces are flat -- brows and a mouth painted onto a plane, no nose geometry -- so every
unhelmeted character needs a nose prop stuck on. One hand-measured offset covers the standard
head and nothing else: the BR soldier's face is a different shape at a different height, and
the same numbers left his nose hanging in mid-air.

THE ALGORITHM. Not a band, not a guess:

  1. Find the head bone in the reference pose, in component space.
  2. Keep only vertices within HEAD_RADIUS of it. That is the head and nothing else -- a chest
     plate, a pauldron or a backpack is further away than a skull is wide. A band by height
     cannot make that distinction, which is how the BR's nose ended up on the front of his
     ARMOUR, eight centimetres proud of his face.
  3. Of those, find how far forward the face is -- the FRONT_PERCENTILE of their forward
     coordinate, not the maximum. Synty heads are made of small flat facets and one of them
     (an ear stud, a fringe, a chin strap) is usually a centimetre proud of everything else;
     taking the maximum lands the nose on that instead of on the face.
  4. Take the centroid of the sheet of vertices at that depth. That is the middle of the face.

  5. Do NOT trust the absolute number. Express it as a DELTA from the one head whose nose
     placement has been confirmed by eye (REFERENCE_HEAD, offset REFERENCE_NOSE), and add that
     delta to the known-good offset. Absolute measurement has to get the bone's local axes,
     the nose prop's own pivot and its scale all right at once; a delta only has to get the
     difference between two heads on the same rig right, and those cancel out.

Everything is measured per mesh and written to Tools/face_planes.json, which
Tools/facility_layout.py reads. Re-run it after adding a character; nothing needs editing by
hand.
"""
import unreal, io, json, traceback

OUT = r'C:\Dev\Games\RepliCan\Tools\face_planes.json'
# A human head is about 20 cm front to back. 14 cm from the head bone reaches the whole skull
# and stops well short of the shoulders.
HEAD_RADIUS = 14.0
# How close to the face depth a vertex has to be to count as part of the face.
FACE_TOLERANCE = 1.5
# Which quantile of forward-ness counts as "the face". Not 1.0: a single proud facet is common
# and taking the maximum puts the nose on it.
FRONT_PERCENTILE = 0.90   # the old rule; measure() now votes by density, see there
# Where the eyes and mouth sit relative to the head bone, cm up: the confirmed nose is 6.9 up,
# the brow a few above that, the mouth a few below. The band keeps the fringe (above) and the
# collar (below) out of the vote for where the face is.
FACE_BAND = (0.0, 13.0)
# The head whose nose has been confirmed by eye, and the offset that confirmed it. Every other
# head is measured as a difference from this one. Bone-local, where x is up and y is forward.
REFERENCE_HEAD = 'SK_Chr_SpaceSoldier_Head_Male_01'
REFERENCE_NOSE = (6.89, 13.0)
# Every mesh that might carry a bare face. A helmet is skipped by the layout script, not here.
MESHES = [
    '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Head_Male_01',
    '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Head_Female_01',
    '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_BR_SpaceSoldier_Male_01',
    # The cafeteria line-up. These three carry their own heads rather than taking a separate
    # head mesh, so the body IS the face mesh and is measured as one.
    '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_Junker_Male_01',
    '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_Junker_Female_01',
    '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_Hunter_Female_01',
]


def head_bone_cs(skel, bone='head'):
    """Component-space position of a bone in the reference pose."""
    pose = skel.get_reference_pose()
    names = [str(b) for b in pose.get_bone_names()]
    if bone not in names:
        return None
    t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
    return t.translation


def measure(path):
    mesh = unreal.load_asset(path)
    if not mesh:
        return {'error': 'missing'}
    skel = mesh.get_editor_property('skeleton')
    head = head_bone_cs(skel)
    if head is None:
        return {'error': 'no head bone'}

    dyn = unreal.DynamicMesh()
    dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())

    r2 = HEAD_RADIUS * HEAD_RADIUS
    near = []
    for i in range(dyn.get_vertex_count()):
        p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
        if not ok:
            continue
        dx, dy, dz = p.x - head.x, p.y - head.y, p.z - head.z
        if dx * dx + dy * dy + dz * dz <= r2:
            near.append((p.x, p.y, p.z))
    if not near:
        return {'error': 'no vertices near the head bone'}

    # Synty characters face +Y at yaw zero, so forward is +Y in mesh space.
    #
    # THE FACE IS THE DENSEST SHEET, NOT THE FRONT-MOST ONE. The percentile rule (the 90th
    # forward-ness of every head vertex) worked on bare heads and failed on the Junker and
    # Hunter women: a fringe, a hood and goggles put a quarter of the head's vertices well
    # ahead of the face, so the 90th percentile landed on the hair and the nose hung seven
    # centimetres in front of the face. A painted Synty face is one big flat sheet -- the
    # most vertices at one depth anywhere on the front of the head, at eye-to-mouth height --
    # so the face is found as the mode of the forward coordinate in the band the eyes and
    # mouth occupy, then refined to the centroid of that sheet.
    band = [v for v in near if FACE_BAND[0] <= v[2] - head.z <= FACE_BAND[1] and v[1] > head.y]
    if len(band) < 8:
        band = [v for v in near if v[1] > head.y] or near
    bins = {}
    for v in band:
        bins.setdefault(int(round(v[1] / FACE_TOLERANCE)), []).append(v)
    # Ties go to the front: a sheet of hair behind the face can be as dense as the face, but
    # the face is what is in front of the skull at eye height.
    best = max(bins, key=lambda k: (len(bins[k]), k))
    plane = [v for v in band if abs(v[1] - best * FACE_TOLERANCE) <= FACE_TOLERANCE]
    front = sum(v[1] for v in plane) / len(plane)
    cz = sum(v[2] for v in plane) / len(plane)

    return {
        'head_bone': [round(head.x, 2), round(head.y, 2), round(head.z, 2)],
        'head_verts': len(near),
        'face_verts': len(plane),
        # Component space, relative to the head bone. up is +Z, forward is +Y on this rig.
        'face_up': round(cz - head.z, 2),
        'face_forward': round(front - head.y, 2),
    }


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')
    raw = {}
    for path in MESHES:
        raw[path.split('/')[-1]] = measure(path)

    ref = raw.get(REFERENCE_HEAD, {})
    if 'face_up' not in ref:
        raise RuntimeError('the reference head did not measure: ' + json.dumps(ref))

    out = {}
    for name, m in raw.items():
        if 'face_up' not in m:
            print('%-44s %s' % (name, json.dumps(m)))
            continue
        # The delta from the confirmed head, added to the confirmed offset. Absolute placement
        # would have to get the bone's local axes, the nose prop's pivot and its scale all
        # right at once; a delta only has to get the difference between two heads on one rig
        # right, and everything else cancels.
        m['nose_offset'] = [round(REFERENCE_NOSE[0] + (m['face_up'] - ref['face_up']), 2),
                            round(REFERENCE_NOSE[1] + (m['face_forward'] - ref['face_forward']), 2)]
        out[name] = m
        print('%-44s face up%7.2f fwd%7.2f  ->  nose (%.2f, %.2f)%s'
              % (name, m['face_up'], m['face_forward'], m['nose_offset'][0], m['nose_offset'][1],
                 '   <- reference' if name == REFERENCE_HEAD else ''))
    io.open(OUT, 'w', encoding='utf-8', newline=chr(10)).write(json.dumps(out, indent=1))
    print('FACES', len(out), '->', OUT)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
