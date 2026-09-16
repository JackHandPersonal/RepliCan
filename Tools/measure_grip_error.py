"""Why the rifle is not sitting in the hand. Measured, not guessed.

    python Tools/ue_remote.py --file Tools/measure_grip_error.py

Two separate questions, and the answer matters because they have different fixes:

  1. Is the WEAPON where the socket says?   -- a socket problem, fix the socket
  2. Is the weapon's ORIGIN on its own pistol grip?  -- a bake problem, fix the mesh

Tools/normalise_weapons.py placed each gun's origin by a RULE: a fraction of the rear extent
back along the barrel and a fraction of the drop below the centreline. That is a decent guess
and it was never checked against the actual geometry. This checks it. The real pistol grip on a
gun is the rearmost thing hanging below the bore -- the magazine is the next lump forward, and
nothing else protrudes downward -- so it can be found properly:

  take centreline vertices below the bore line, bucket them along the barrel, and the pistol
  grip is the rearmost bucket with real depth to it.

The difference between that point and (0,0,0) is the bake error, in centimetres, per axis.
"""
import unreal, json, io, traceback, collections

WEAPON = '/Game/RepliCan/Weapons/Worlds/SM_Wep_Assault_01'
BODY = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Male_01'
SOCKET = 'WeaponGrip_R'
CENTRE_BAND = 3.0
BUCKET = 2.0        # cm along the barrel


def verts(mesh):
    dyn = unreal.DynamicMesh()
    dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    out = []
    for i in range(dyn.get_vertex_count()):
        p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
        if ok:
            out.append((p.x, p.y, p.z))
    return out


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # ---- 1. Where does the mesh think its grip is? --------------------------------------
    mesh = unreal.load_asset(WEAPON)
    pts = verts(mesh)
    xs = [p[0] for p in pts]
    zs = [p[2] for p in pts]
    print('weapon bounds  x %.1f..%.1f   y %.1f..%.1f   z %.1f..%.1f'
          % (min(xs), max(xs), min(p[1] for p in pts), max(p[1] for p in pts), min(zs), max(zs)))
    print('origin is at (0, 0, 0) by the HAC1 convention -- it should be ON the pistol grip.')

    # The bore line: the middle of the barrel's height, taken over the forward half where
    # there is nothing but barrel.
    fwd = [p for p in pts if p[0] > max(xs) * 0.55 and abs(p[1]) <= CENTRE_BAND]
    bore_z = (min(p[2] for p in fwd) + max(p[2] for p in fwd)) * 0.5 if fwd else 0.0
    print('bore line z = %.2f' % bore_z)

    below = [p for p in pts if p[2] < bore_z - 2.0 and abs(p[1]) <= CENTRE_BAND]
    buckets = collections.defaultdict(list)
    for p in below:
        buckets[int(p[0] // BUCKET)].append(p)
    print('')
    print('things hanging below the bore, back to front:')
    runs = []
    for b in sorted(buckets):
        col = buckets[b]
        depth = bore_z - min(p[2] for p in col)
        runs.append((b * BUCKET, depth, len(col)))
    # Group adjacent buckets into lumps.
    lumps = []
    cur = None
    for x, depth, n in runs:
        if cur and x - cur[-1][0] <= BUCKET * 1.5:
            cur.append((x, depth, n))
        else:
            if cur: lumps.append(cur)
            cur = [(x, depth, n)]
    if cur: lumps.append(cur)
    for i, lump in enumerate(lumps):
        x0, x1 = lump[0][0], lump[-1][0] + BUCKET
        deep = max(d for _, d, _ in lump)
        print('   lump %d: x %6.1f .. %6.1f   deepest %5.1f cm below bore   %d verts'
              % (i + 1, x0, x1, deep, sum(n for _, _, n in lump)))

    if lumps:
        grip = lumps[0]
        gx = sum(x for x, _, _ in grip) / len(grip) + BUCKET * 0.5
        gcol = [p for b in range(int(grip[0][0] // BUCKET), int(grip[-1][0] // BUCKET) + 1) for p in buckets.get(b, [])]
        gz = (min(p[2] for p in gcol) + bore_z) * 0.5
        print('')
        print('REAREST LUMP = the pistol grip, at about (%.1f, 0.0, %.1f)' % (gx, gz))
        print('BAKE ERROR: the origin is %.1f cm forward and %.1f cm above where the hand closes.'
              % (-gx, -gz))

    # ---- 2. Where does the socket put the weapon relative to the hand? ------------------
    Y0 = -70000.0
    for a in eas.get_all_level_actors():
        try:
            if abs(a.get_actor_location().y - Y0) < 3000.0: eas.destroy_actor(a)
        except Exception: pass
    body = eas.spawn_actor_from_object(unreal.load_asset(BODY), unreal.Vector(0, Y0, 0))
    sc = body.skeletal_mesh_component
    hand = sc.get_socket_location('hand_r')
    sock_t = sc.get_socket_transform(SOCKET, unreal.RelativeTransformSpace.RTS_WORLD)
    print('')
    print('hand_r world           %s' % hand)
    print('%-22s %s' % (SOCKET + ' world', sock_t.translation))
    print('socket offset from hand_r: %.2f, %.2f, %.2f'
          % (sock_t.translation.x - hand.x, sock_t.translation.y - hand.y, sock_t.translation.z - hand.z))
    print('socket rotation (world):  %s' % sock_t.rotation.rotator())
    print('body rotation (world):    %s' % body.get_actor_rotation())
    eas.destroy_actor(body)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
