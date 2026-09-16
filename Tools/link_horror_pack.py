"""Make the SciFi Horror characters usable: one skeleton link, and grip sockets.

    python Tools/ue_remote.py --file Tools/link_horror_pack.py

The pack ships 21 character meshes bound to its OWN copy of the UE4 Mannequin skeleton
(/Game/Synty/EpicContent/.../SKEL_UE4_Mannequin), while every animation in this project is
bound to the copy that came with the Goblin War Camp pack. Same rig, same bone names, same
reference pose -- different asset, and Unreal will not play an animation across that line.

The fix is a COMPATIBLE SKELETON link, not a retarget. A retarget would rebuild 500-odd clips
to say the thing they already say. Declaring the two skeletons compatible tells Unreal what is
already true, costs nothing, and leaves one set of animations driving every character in the
game. Done both ways round so it does not matter which skeleton an asset was authored against.

Then the grip sockets, because a character who cannot hold a rifle is set dressing.
"""
import unreal, io, traceback, collections

HOME = '/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton'
NEWCOMERS = ['/Game/Synty/EpicContent/Mannequin/Character/Mesh/SKEL_UE4_Mannequin']
PACK = '/Game/Synty/PolygonSciFiHorror'

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before editing assets')

    home = unreal.load_asset(HOME)
    if not home:
        raise RuntimeError('home skeleton missing: ' + HOME)
    home_bones = [str(b) for b in home.get_reference_pose().get_bone_names()]

    for path in NEWCOMERS:
        other = unreal.load_asset(path)
        if not other:
            print('MISSING', path); continue
        bones = [str(b) for b in other.get_reference_pose().get_bone_names()]
        shared = len(set(bones) & set(home_bones))
        print('%s: %d bones, %d shared with the project skeleton' % (path.split('/')[-1], len(bones), shared))
        if shared < 40:
            print('   NOT the same rig -- refusing to link; this one needs a real retarget')
            continue
        try:
            home.add_compatible_skeleton(other)
            other.add_compatible_skeleton(home)
            unreal.EditorAssetLibrary.save_loaded_asset(home, False)
            unreal.EditorAssetLibrary.save_loaded_asset(other, False)
            print('   linked both ways')
        except Exception as e:
            print('   link failed:', e)

    # ---- Grip sockets on the newcomers -------------------------------------------------
    import json, os
    d = json.load(io.open(os.path.join(unreal.Paths.project_dir(), 'Tools', 'grip_socket.json'), encoding='utf-8'))
    loc = unreal.Vector(*d['location'])
    r = d['rotation']
    rot = unreal.Rotator(roll=r[2], pitch=r[0], yaw=r[1])
    socks = [('WeaponGrip_R', 'hand_r', loc, rot),
             ('WeaponGrip_L', 'hand_l', unreal.Vector(loc.x, -loc.y, loc.z),
              unreal.Rotator(roll=-rot.roll, pitch=rot.pitch, yaw=rot.yaw))]

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    tally = collections.Counter()
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'SkeletalMesh'), True):
        p = str(a.package_name)
        if not p.startswith(PACK):
            continue
        mesh = unreal.load_asset(p)
        skel = mesh.get_editor_property('skeleton') if mesh else None
        if not skel:
            continue
        bones = set(str(b) for b in skel.get_reference_pose().get_bone_names())
        if 'hand_r' not in bones:
            tally['no hands'] += 1
            continue
        names = lambda: [str(mesh.get_socket_by_index(i).socket_name) for i in range(mesh.num_sockets())]
        for junk in [n for n in names() if n == 'Socket' or n.startswith('Socket_')]:
            mesh.remove_socket(junk)
        added = []
        for name, bone, l, rr in socks:
            if bone not in bones or mesh.find_socket(name):
                continue
            s = unreal.SkeletalMeshSocket(mesh)
            s.set_socket_parent(mesh, bone)
            s.set_socket_local_transform(unreal.Transform(location=l, rotation=rr, scale=unreal.Vector(1, 1, 1)))
            mesh.add_socket(s, True)
            fresh = [n for n in names() if n == 'Socket' or n.startswith('Socket_')]
            if fresh and mesh.rename_socket(fresh[0], name):
                fin = mesh.find_socket(name)
                if fin and str(fin.bone_name) != bone:
                    fin.set_socket_parent(mesh, bone)
                    fin.set_socket_local_transform(unreal.Transform(location=l, rotation=rr, scale=unreal.Vector(1, 1, 1)))
                added.append(name)
        if added:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            unreal.EditorAssetLibrary.save_loaded_asset(skel, False)
            tally['socketed'] += 1
        else:
            tally['already had them'] += 1
    print('SOCKETS', dict(tally))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
