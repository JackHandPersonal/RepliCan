"""Cut library: splits Synty character skeletal meshes into Head / Torso /
Arms / Legs part meshes on the shared UE4 Mannequin rig, so the player
can be assembled from any mix (leader-pose parts, like the Fantasy Hero).

Run in the editor: Tools/ue_remote.py --file Tools/cut_library.py
Each vertex goes to the part its heaviest bone belongs to; a triangle goes
with the majority of its vertices. Each part is written as a duplicate of
the source asset (keeps skeleton, materials, sockets) with the geometry
replaced, at /Game/RepliCan/CutLibrary/<Pack>/<Source>_<Part>.
Set ONLY to a list of source names to limit a run; existing outputs are
skipped unless FORCE."""
import unreal, json
ONLY = __ONLY__
FORCE = __FORCE__
PACKS = {
    'SciFiSpace': '/Game/PolygonSciFiSpace/Meshes/CharactersUE4',
    'SciFiWorlds': '/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin',
    'CyberCity': '/Game/PolygonCyberCity/Meshes/CharactersUE4Mannequin',   # Meshes/Characters is on Synty's own SKEL_HumanCharacter rig
    'Mech': '/Game/PolygonMech/Models/CharactersUE4',                      # the two mech pilots (POLYGON Mech), on the pack's mannequin copy
    'SciFiCity': '/Game/PolygonScifi/Meshes/New_Character',                  # POLYGON Sci-Fi City (the 4.25 pack): New_Character is its mannequin-rig set
    'Police': '/Game/Synty/PolygonPoliceStation/Meshes/CharactersUE4Mannequin',   # criminals and the forensic tech (POLYGON Police Station), on the pack's mannequin copy
}
OUT = '/Game/RepliCan/CutLibrary'
SKIP = ('Attach', 'Tail', 'Cat_', 'MurderKitten', 'Helper_Bot', 'Head_', 'Helmet', 'Armour', 'SK_MK', 'SK_Tail')
PARTS = ['Head', 'Torso', 'Arms', 'Legs']
def part_of(bone):
    b = bone.lower()
    if b in ('head', 'neck_01', 'neck_02') or b.startswith('eye') or b.startswith('jaw'): return 'Head'
    if b.startswith(('upperarm', 'lowerarm', 'hand', 'index', 'middle', 'ring', 'pinky', 'thumb')): return 'Arms'
    if b.startswith(('pelvis', 'thigh', 'calf', 'foot', 'ball')): return 'Legs'
    return 'Torso'   # spine_*, clavicles, root, ik bones, anything odd
BW = unreal.GeometryScript_BoneWeights; MQ = unreal.GeometryScript_MeshQueries
AU = unreal.GeometryScript_AssetUtils; SEL = unreal.GeometryScript_MeshSelection; ED = unreal.GeometryScript_MeshEdits
reg = unreal.AssetRegistryHelpers.get_asset_registry()
report = []
for pack, folder in PACKS.items():
    assets = reg.get_assets_by_path(unreal.Name(folder), False)
    for ad in assets:
        name = str(ad.asset_name)
        if not name.startswith('SK_') or any(s in name for s in SKIP): continue
        if ONLY and name not in ONLY: continue
        sk = unreal.load_asset(str(ad.package_name))
        if not sk: continue
        skel = sk.skeleton
        if not skel or 'Mannequin' not in skel.get_name(): report.append((name, 'skipped: skeleton %s' % (skel.get_name() if skel else None))); continue
        short = name[3:].replace('Chr_', '').replace('ScifiWorlds_', '')
        outdir = OUT + '/' + pack
        src = unreal.DynamicMesh()
        r = AU.copy_mesh_from_skeletal_mesh(sk, src, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); src = r[0] if isinstance(r, tuple) else src
        prof = unreal.GeometryScriptBoneWeightProfile()
        if not BW.mesh_has_bone_weights(src, prof): report.append((name, 'no bone weights')); continue
        # bone index -> part
        infos = BW.get_all_bones_info(src)
        if isinstance(infos, tuple): infos = [x for x in infos if isinstance(x, unreal.Array)][0]
        bone_part = {}
        for i, bi in enumerate(infos):
            bone_part[i] = part_of(str(bi.name))
        # vertex -> part by heaviest bone
        nv = MQ.get_num_vertex_i_ds(src); vpart = {}
        for v in range(nv):
            r = BW.get_largest_vertex_bone_weight(src, v, prof)
            bw = [x for x in r if isinstance(x, unreal.GeometryScriptBoneWeight)][0] if isinstance(r, tuple) else r
            vpart[v] = bone_part.get(int(bw.bone_index), 'Torso')
        # triangle -> part by majority
        nt = MQ.get_num_triangle_i_ds(src); tri_parts = {p: [] for p in PARTS}
        for t in range(nt):
            r = MQ.get_triangle_indices(src, t); idx = r[0] if isinstance(r, tuple) else r
            votes = [vpart.get(int(idx.x), 'Torso'), vpart.get(int(idx.y), 'Torso'), vpart.get(int(idx.z), 'Torso')]
            best = max(PARTS, key=lambda p: (votes.count(p), -PARTS.index(p)))
            tri_parts[best].append(t)
        counts = {}
        for part in PARTS:
            keep = tri_parts[part]
            if len(keep) < 4: counts[part] = 0; continue
            dst_path = '%s/%s_%s' % (outdir, short, part)
            if unreal.EditorAssetLibrary.does_asset_exist(dst_path) and not FORCE: counts[part] = 'exists'; continue
            if unreal.EditorAssetLibrary.does_asset_exist(dst_path): unreal.EditorAssetLibrary.delete_asset(dst_path)
            if not unreal.EditorAssetLibrary.duplicate_asset(str(ad.package_name), dst_path): counts[part] = 'dup failed'; continue
            dst = unreal.load_asset(dst_path)
            # delete every triangle not in this part
            drop = [t for p in PARTS if p != part for t in tri_parts[p]]
            r = SEL.convert_index_array_to_mesh_selection(src, drop, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
            sel = [x for x in r if isinstance(x, unreal.GeometryScriptMeshSelection)][0] if isinstance(r, tuple) else r
            cp = unreal.DynamicMesh()
            r = AU.copy_mesh_from_skeletal_mesh(sk, cp, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); cp = r[0] if isinstance(r, tuple) else cp
            r = ED.delete_selected_triangles_from_mesh(cp, sel); cp = r[0] if isinstance(r, tuple) else cp
            opts = unreal.GeometryScriptCopyMeshToAssetOptions()
            opts.set_editor_property('enable_recompute_normals', False); opts.set_editor_property('enable_recompute_tangents', False)
            opts.set_editor_property('replace_materials', False); opts.set_editor_property('enable_remove_degenerates', True)
            r = AU.copy_mesh_to_skeletal_mesh(cp, dst, opts, unreal.GeometryScriptMeshWriteLOD())
            outcome = r[-1] if isinstance(r, tuple) else r
            unreal.EditorAssetLibrary.save_asset(dst_path)
            counts[part] = len(keep)
        report.append((name, counts))
print('RESULT ' + json.dumps(report))
