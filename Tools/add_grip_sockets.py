"""Author the WeaponGrip_R / WeaponGrip_L sockets on every character mesh.

    python Tools/ue_remote.py --file Tools/add_grip_sockets.py

Part 2 of Docs/HeldAssetStandard.md. The socket's job is to cancel out whatever frame that
rig's hand bone happens to use, so that a weapon baked into HAC1 grip space hangs off it
correctly with an IDENTITY relative transform -- no per-rig maths at play time.

Two things about UE's Python socket API, learned the hard way, that explain the shape of this:

  * USkeletalMeshSocket::SocketName is VisibleAnywhere, so set_editor_property cannot touch it.
    A socket is added under an auto-generated name ("Socket", "Socket_0", ...) and then renamed.
  * add_socket's second argument promotes the socket to the SKELETON as well, which is where it
    really belongs -- one socket covering every mesh on that rig. Promoting is done once per
    skeleton and the mesh-level copy comes along for free.

Re-running only adds what is missing, so a socket nudged by hand in the editor survives. Set
FORCE to replace them all.
"""
import unreal, io, json, os, traceback, collections

# Every mesh under these paths that has a hand_r bone gets the sockets. Broad on purpose: a
# character mesh that gets missed is a character who cannot hold anything, and that shows up
# much later as "the gun is in the wrong place" rather than as an error here.
ROOTS = ['/Game/RepliCan/Cut', '/Game/RepliCan/CutLibrary', '/Game/RepliCan/PlayerCharacter',
         '/Game/Characters', '/Game/ModularCharacters',
         '/Game/PolygonSciFiSpace/Meshes/CharactersUE4',
         '/Game/PolygonCyberCity/Meshes/CharactersUE4',
         '/Game/PolygonSciFiWorlds/Meshes/CharactersUE4']
FORCE = True

# Where the hand closes relative to the hand_r bone origin, and how a HAC1 weapon is turned to
# sit in it. hand_r's origin is at the wrist and its local axes are NOT the world's: the bone
# runs out along the fingers, so a weapon that points down its own +X has to be turned to point
# out of the fist. These are the measured values from the old runtime path re-expressed in the
# new convention -- and they are what to nudge in the Skeleton editor when a weapon sits
# slightly wrong, rather than anything in C++.
# MEASURED, by Tools/derive_grip_socket.py, from Lyra's rifle idle -- a pose authored by someone
# holding a rifle properly, so hand_r's rotation in it IS the grip orientation. The first version
# of these numbers was reasoned out by hand as "yaw -90 off hand_r" and was about 245 degrees
# wrong in yaw, which hung the weapon a quarter of a metre from the fist. Do not hand-edit them;
# re-run the deriving script.
def _measured(default_loc, default_rot):
    try:
        d = json.load(io.open(os.path.join(unreal.Paths.project_dir(), 'Tools', 'grip_socket.json'), encoding='utf-8'))
        l, r = d['location'], d['rotation']
        return unreal.Vector(*l), unreal.Rotator(roll=r[2], pitch=r[0], yaw=r[1])
    except Exception as e:
        print('grip_socket.json not read (%s); falling back to the hand-authored guess' % e)
        return default_loc, default_rot

_LOC, _ROT = _measured(unreal.Vector(-2.0, 4.0, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
# The left hand mirrors it: same palm, opposite twist about the barrel.
_LOC_L = unreal.Vector(_LOC.x, -_LOC.y, _LOC.z)
_ROT_L = unreal.Rotator(roll=-_ROT.roll, pitch=_ROT.pitch, yaw=_ROT.yaw)

SOCKETS = [
    ('WeaponGrip_R', 'hand_r', _LOC, _ROT),
    ('WeaponGrip_L', 'hand_l', _LOC_L, _ROT_L),
]

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before editing meshes')

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'SkeletalMesh'), True)
    tally = collections.Counter()
    touched = []

    for a in assets:
        path = str(a.package_name)
        if not any(path.startswith(r) for r in ROOTS):
            continue
        mesh = unreal.load_asset(path)
        if not mesh:
            continue
        skel = mesh.get_editor_property('skeleton')
        if not skel:
            continue
        bones = set(str(b) for b in skel.get_reference_pose().get_bone_names())
        if 'hand_r' not in bones:
            tally['no hands'] += 1
            continue

        names = lambda: [str(mesh.get_socket_by_index(i).socket_name) for i in range(mesh.num_sockets())]
        # Sweep any auto-named leftovers before doing anything, so a half-finished earlier run
        # cannot leave "Socket_3" sitting on a character forever.
        for junk in [n for n in names() if n == 'Socket' or n.startswith('Socket_')]:
            mesh.remove_socket(junk)
            tally['swept'] += 1

        added = []
        for name, bone, loc, rot in SOCKETS:
            if bone not in bones:
                continue
            if mesh.find_socket(name):
                if not FORCE:
                    tally['already had it'] += 1
                    continue
                mesh.remove_socket(name)
            xform = unreal.Transform(location=loc, rotation=rot, scale=unreal.Vector(1, 1, 1))
            sock = unreal.SkeletalMeshSocket(mesh)
            # BoneName is VisibleAnywhere like SocketName, so set_editor_property cannot touch
            # it either. set_socket_parent is the only way in, and it validates the bone against
            # the mesh's skeleton, which is why this is called on the mesh rather than the bone.
            sock.set_socket_parent(mesh, bone)
            sock.set_socket_local_transform(xform)
            # True promotes it to the skeleton too, which is where a rig-wide socket belongs.
            mesh.add_socket(sock, True)
            fresh = [n for n in names() if n == 'Socket' or n.startswith('Socket_')]
            if not fresh:
                tally['add failed'] += 1
                continue
            if not mesh.rename_socket(fresh[0], name):
                tally['rename failed'] += 1
                continue
            # Verify rather than assume: add_socket copies the socket, and a copy that lost its
            # bone leaves the weapon hanging at the character's feet with nothing in the log.
            fin = mesh.find_socket(name)
            if not fin or str(fin.bone_name) != bone:
                if fin:
                    fin.set_socket_parent(mesh, bone)
                    fin.set_socket_local_transform(xform)
                fin = mesh.find_socket(name)
            if not fin or str(fin.bone_name) != bone:
                tally['WRONG BONE'] += 1
                continue
            added.append(name)

        if added:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            unreal.EditorAssetLibrary.save_loaded_asset(skel, False)
            tally['meshes touched'] += 1
            touched.append(path.split('/')[-1] + ' +' + ','.join(added))

    for line in touched[:15]:
        print('  ' + line)
    if len(touched) > 15:
        print('  ... and %d more' % (len(touched) - 15))
    print('SOCKETS', dict(tally))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
