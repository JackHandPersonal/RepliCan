"""Where belt gear sits on a character that has no SOC_ sockets (the Mech pilots are on the
plain UE4 mannequin rig). Run in the editor:
    python Tools/ue_remote.py --file Tools/measure_gear_points.py

For each mesh and each mount, the body surface is read off the vertices that the mount's bone
drives (reference pose, component space): the outermost point on the side the gear hangs on.
The gear is a flat plate centred on its own pivot (Synty's attachments here are), so its
centre goes half its thickness outside that point, its thin axis (+Y) pointing away from the
body. Everything is then expressed in the bone's frame -- location and rotation -- so a posed,
animated body carries it. Written to Tools/gear_points.json; facility_layout.py reads it.
"""
import unreal, json, io, collections
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils; BW = unreal.GeometryScript_BoneWeights
OUT = r'C:\Dev\Games\RepliCan\Tools\gear_points.json'
MESHES = ['/Game/PolygonMech/Models/CharactersUE4/SK_Chr_MechPilot_Male_01', '/Game/PolygonMech/Models/CharactersUE4/SK_Chr_MechPilot_Female_01']
# mount: the bone whose surface it hangs from, which way it hangs (component space, the
# character facing +Y), the height band on that bone's vertices to look in, and the gear's
# outward-facing yaw (gear +Y -> world side).
MOUNTS = {
    'thigh_r': {'bone': 'thigh_r', 'out': (-1, 0, 0), 'yaw': 90.0,  'band': (0.35, 0.75)},   # the right side is -X
    'thigh_l': {'bone': 'thigh_l', 'out': (1, 0, 0),  'yaw': -90.0, 'band': (0.35, 0.75)},
    'back':    {'bone': 'spine_03', 'out': (0, -1, 0), 'yaw': 180.0, 'band': (0.2, 0.8)},
    'hip_back': {'bone': 'pelvis', 'out': (0, -1, 0), 'yaw': 180.0, 'band': (0.3, 0.9)},
}


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


out = {}
for path in MESHES:
    mesh = unreal.load_asset(path); name = path.split('/')[-1]
    skel = mesh.get_editor_property('skeleton'); pose = skel.get_reference_pose()
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_skeletal_mesh(mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
    prof = unreal.GeometryScriptBoneWeightProfile()
    infos = BW.get_all_bones_info(dyn)
    if isinstance(infos, tuple): infos = [x for x in infos if isinstance(x, unreal.Array)][0]
    names = [str(b.name) for b in infos]
    by_bone = collections.defaultdict(list)
    for i in range(dyn.get_vertex_count()):
        p, ok = MQ.get_vertex_position(dyn, i)
        if not ok: continue
        r2 = BW.get_largest_vertex_bone_weight(dyn, i, prof)
        bw = [x for x in r2 if isinstance(x, unreal.GeometryScriptBoneWeight)][0] if isinstance(r2, tuple) else r2
        bi = int(bw.bone_index)
        if 0 <= bi < len(names): by_bone[names[bi]].append((p.x, p.y, p.z))
    entry = {}
    for mount, spec in MOUNTS.items():
        pts = by_bone.get(spec['bone'], [])
        if not pts: print(name, mount, 'no vertices on', spec['bone']); continue
        zs = sorted(p[2] for p in pts); lo = zs[int(len(zs) * spec['band'][0])]; hi = zs[min(len(zs) - 1, int(len(zs) * spec['band'][1]))]
        band = [p for p in pts if lo <= p[2] <= hi]
        ox, oy, oz = spec['out']
        ext = max(band, key=lambda p: p[0] * ox + p[1] * oy + p[2] * oz)   # the outermost point on that side
        # the mount point: the outermost extent, but centred laterally/vertically on the band
        cx = sum(p[0] for p in band) / len(band); cy = sum(p[1] for p in band) / len(band); cz = sum(p[2] for p in band) / len(band)
        surface = [ext[0] if ox else cx, ext[1] if oy else cy, cz]
        T = unreal.AnimPoseExtensions.get_bone_pose(pose, spec['bone'], unreal.AnimPoseSpaces.WORLD)
        world_rot = unreal.Rotator(0.0, spec['yaw'], 0.0)
        # bone-local: location of the surface point, and the rotation that gives the world yaw
        loc = unreal.MathLibrary.inverse_transform_location(T, unreal.Vector(*surface))
        rot = unreal.MathLibrary.inverse_transform_rotation(T, world_rot)
        entry[mount] = {'bone': spec['bone'], 'surface': [round(v, 2) for v in surface], 'out': list(spec['out']),
                        'local_loc': [round(loc.x, 2), round(loc.y, 2), round(loc.z, 2)],
                        'local_rot': [round(rot.pitch, 2), round(rot.yaw, 2), round(rot.roll, 2)],
                        'bone_verts': len(pts)}
        print('%-28s %-9s on %-9s surface (%.1f, %.1f, %.1f) from %d verts' % (name, mount, spec['bone'], surface[0], surface[1], surface[2], len(pts)))
    # the head: the bone frame itself; the hat mount is head + pitch -90 (measured once on the Space crew, see memory)
    T = unreal.AnimPoseExtensions.get_bone_pose(pose, 'head', unreal.AnimPoseSpaces.WORLD)
    entry['head'] = {'bone': 'head', 'head_cs': [round(T.translation.x, 2), round(T.translation.y, 2), round(T.translation.z, 2)]}
    out[name] = entry
io.open(OUT, 'w', encoding='utf-8', newline='\n').write(json.dumps(out, indent=1))
print('GEAR POINTS', len(out), '->', OUT)
