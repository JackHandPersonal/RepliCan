"""Bring every skeleton in the project up to the project's standards, in one sweep.

    python Tools/ue_remote.py --file Tools/standardise_skeletons.py

Two standards, both from Docs/HeldAssetStandard.md:

  1. ONE ANIMATION SET. Every pack ships its own copy of the UE4 Mannequin skeleton. They are
     bit-for-bit the same rig, but Unreal will not play an animation across two skeleton assets
     unless they are declared compatible. So: measure the bone overlap against the skeleton this
     project's animations are bound to, and where it is effectively identical, declare it --
     both ways round, so it does not matter which one an asset was authored against.

     Measured, not assumed: a skeleton that only shares half its bones is a DIFFERENT rig and
     gets left alone with a note, because linking that one would give silently broken poses
     rather than an error.

  2. EVERY HAND CAN HOLD A WEAPON. WeaponGrip_R and WeaponGrip_L on hand_r and hand_l, at the
     transform measured in Tools/derive_grip_socket.py. Promoted to the skeleton, so one edit
     covers every mesh on that rig.

Safe to re-run. Nothing already correct is touched, and everything changed is printed.
"""
import unreal, io, json, os, traceback, collections

HOME = '/Game/PolygonGoblinWarCamp/EpicContent/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton'
# Below this share of bones it is not the same rig and must not be linked.
SAME_RIG = 0.90
# Skeletons for props, vehicles and turrets have no business carrying a weapon socket.
SKIP = ('/Veh_', '/SK_Prop_', '/SK_Veh_', 'Turret', 'LandingGear', 'Sweepo')

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before editing assets')

    home = unreal.load_asset(HOME)
    if not home:
        raise RuntimeError('home skeleton missing: ' + HOME)
    home_bones = set(str(b) for b in home.get_reference_pose().get_bone_names())

    d = json.load(io.open(os.path.join(unreal.Paths.project_dir(), 'Tools', 'grip_socket.json'), encoding='utf-8'))
    loc = unreal.Vector(*d['location'])
    r = d['rotation']
    rot = unreal.Rotator(roll=r[2], pitch=r[0], yaw=r[1])
    SOCKETS = [('WeaponGrip_R', 'hand_r', loc, rot),
               ('WeaponGrip_L', 'hand_l', unreal.Vector(loc.x, -loc.y, loc.z),
                unreal.Rotator(roll=-rot.roll, pitch=rot.pitch, yaw=rot.yaw))]

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    tally = collections.Counter()

    # ---- 1. One animation set --------------------------------------------------------------
    print('--- skeletons ---')
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'Skeleton'), True):
        path = str(a.package_name)
        if path == HOME or any(k in path for k in SKIP):
            continue
        skel = unreal.load_asset(path)
        if not skel:
            continue
        bones = set(str(b) for b in skel.get_reference_pose().get_bone_names())
        if not bones:
            continue
        share = len(bones & home_bones) / float(max(len(bones), len(home_bones)))
        name = path.split('/')[-1]
        if 'hand_r' not in bones:
            tally['not a character rig'] += 1
            continue
        if share < SAME_RIG:
            tally['different rig, left alone'] += 1
            print('%-42s %3d bones, %.0f%% shared -- DIFFERENT RIG, needs a real retarget' % (name, len(bones), share * 100))
            continue
        try:
            home.add_compatible_skeleton(skel)
            skel.add_compatible_skeleton(home)
            unreal.EditorAssetLibrary.save_loaded_asset(skel, False)
            tally['linked'] += 1
            print('%-42s %3d bones, %.0f%% shared -- linked' % (name, len(bones), share * 100))
        except Exception as e:
            tally['link failed'] += 1
            print('%-42s link failed: %s' % (name, e))
    unreal.EditorAssetLibrary.save_loaded_asset(home, False)

    # ---- 2. Every hand can hold a weapon ---------------------------------------------------
    print('')
    print('--- grip sockets ---')
    touched = []
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'SkeletalMesh'), True):
        path = str(a.package_name)
        if any(k in path for k in SKIP):
            continue
        mesh = unreal.load_asset(path)
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
        for sname, bone, l, rr in SOCKETS:
            if bone not in bones or mesh.find_socket(sname):
                continue
            s = unreal.SkeletalMeshSocket(mesh)
            s.set_socket_parent(mesh, bone)
            s.set_socket_local_transform(unreal.Transform(location=l, rotation=rr, scale=unreal.Vector(1, 1, 1)))
            mesh.add_socket(s, True)
            fresh = [n for n in names() if n == 'Socket' or n.startswith('Socket_')]
            if fresh and mesh.rename_socket(fresh[0], sname):
                fin = mesh.find_socket(sname)
                if fin and str(fin.bone_name) != bone:
                    fin.set_socket_parent(mesh, bone)
                    fin.set_socket_local_transform(unreal.Transform(location=l, rotation=rr, scale=unreal.Vector(1, 1, 1)))
                added.append(sname)
        if added:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
            unreal.EditorAssetLibrary.save_loaded_asset(skel, False)
            tally['socketed'] += 1
            touched.append(path.split('/')[-1])
        else:
            tally['already had them'] += 1
    for t in touched[:12]:
        print('   +', t)
    if len(touched) > 12:
        print('   ... and %d more' % (len(touched) - 12))
    print('')
    print('STANDARDISE', dict(tally))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
