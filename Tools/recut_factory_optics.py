"""Re-cut each weapon's factory optic as everything the body mesh does not have.

Tools/strip_scopes.py cut the moulded-on sight into Parts/<weapon>_Scope_01 and left the rest as
body_mesh -- but the two do not add up. Measured on the Spear 350: the whole mesh is 1691 triangles,
the body 1399 and the scope 236, so 56 triangles are in NEITHER. Among them is the rail the scope
sits on, running x 0.9 to 38.75 at z 19.4-21.6, which is why the scope floats above the gun with a
gap under it. The carve took the scope by a bounding box, and the mount was too low to be caught by
it and too high to be left on the body.

Rather than hunt for the missing piece, this defines the optic as the COMPLEMENT: every triangle of
the whole mesh that the body does not have. That cannot lose anything -- body + optic == whole, by
construction, and the script checks exactly that before it writes.

Triangles are matched by centroid, which is safe here because the two meshes came from the same
source geometry and were never re-welded: an identical triangle has an identical centroid.

Writes Parts/<weapon>_Optic_01 (a NEW asset, the old _Scope_01 is left alone) and points the
catalogue's factory-scope entry at it.

  Tools/ue_remote --file Tools/recut_factory_optics
"""
import unreal, json, io, os, collections

AU = unreal.GeometryScript_AssetUtils
Q = unreal.GeometryScript_MeshQueries
ED = unreal.GeometryScript_MeshEdits
CAT = os.path.join(unreal.Paths.project_dir(), 'Content', 'GameData', 'UI', 'Weapons.json')
TWO_SIDED = '/Game/RepliCan/Optics/MI_PolygonScifiWorlds_01_A_TwoSided'


def load_mesh(path):
    a = unreal.load_asset(path.split('.')[0])
    if not a:
        return None, None
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(a, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                      unreal.GeometryScriptMeshReadLOD())
    return (r[0] if isinstance(r, tuple) else dyn), a


def centroids(dyn):
    """triangle id -> rounded centroid key, and the key set."""
    out, keys = {}, set()
    for t in range(dyn.get_triangle_count()):
        vs = Q.get_triangle_positions(dyn, t)
        if not (isinstance(vs, tuple) and len(vs) >= 4):
            continue
        a, b, c = vs[1], vs[2], vs[3]
        k = (round((a.x + b.x + c.x) / 3.0, 3), round((a.y + b.y + c.y) / 3.0, 3), round((a.z + b.z + c.z) / 3.0, 3))
        out[t] = k
        keys.add(k)
    return out, keys


d = json.load(io.open(CAT, encoding='utf-8'), object_pairs_hook=collections.OrderedDict)
weapons, optics = d.get('weapons', {}), d.get('optics', {})
report = []

for wkey, w in weapons.items():
    scope = (w.get('parts') or {}).get('scope')
    body_path = w.get('body_mesh')
    if not scope or not body_path:
        continue
    whole_dyn, whole_asset = load_mesh(w.get('mesh', ''))
    body_dyn, _ = load_mesh(body_path)
    if not whole_dyn or not body_dyn:
        print('   could not read the meshes for %s' % wkey)
        continue

    whole_c, _ = centroids(whole_dyn)
    _, body_keys = centroids(body_dyn)
    # Everything the body does not have IS the optic -- scope, mount and any offcut between them.
    remove = [t for t, k in whole_c.items() if k in body_keys]
    keep_n = whole_dyn.get_triangle_count() - len(remove)
    if keep_n <= 0:
        print('   %s: nothing left after removing the body, skipped' % wkey)
        continue

    # A raw python list is not a GeometryScriptIndexList; go through a selection, which does accept
    # one. These calls return tuples (mesh, value), so unpack defensively.
    conv = unreal.GeometryScript_MeshSelection.convert_index_array_to_mesh_selection(
        whole_dyn, remove, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
    sel = conv[1] if isinstance(conv, tuple) and len(conv) > 1 else conv
    ED.delete_selected_triangles_from_mesh(whole_dyn, sel)
    # Triangle ids go sparse after a delete; compact before anything else reads them.
    unreal.GeometryScript_MeshRepair.compact_mesh(whole_dyn)
    if whole_dyn.get_triangle_count() != keep_n:
        print('   %s: expected %d triangles after the cut, got %d -- skipped'
              % (wkey, keep_n, whole_dyn.get_triangle_count()))
        continue

    name = scope.split('.')[0].split('/')[-1].replace('_Scope_01', '_Optic_01')
    folder = '/'.join(scope.split('.')[0].split('/')[:-1])
    out_path = '%s/%s' % (folder, name)
    # The same call shape Tools/bake_weapon_parts.py uses: it lives on NewAssetUtils, not AssetUtils,
    # and in 5.8 it hands back a tuple whose asset has to be picked out by type.
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.enable_recompute_normals = False
    opts.enable_recompute_tangents = True
    if unreal.EditorAssetLibrary.does_asset_exist(out_path):
        made = unreal.load_asset(out_path)
        to = unreal.GeometryScriptCopyMeshToAssetOptions()
        to.enable_recompute_normals = False
        to.enable_recompute_tangents = True
        AU.copy_mesh_to_static_mesh(whole_dyn, made, to, unreal.GeometryScriptMeshWriteLOD())
    else:
        res = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(whole_dyn, out_path, opts)
        made = next((x for x in res if isinstance(x, unreal.StaticMesh)), None) if isinstance(res, tuple) else res
        if not made:
            made = unreal.load_asset(out_path)
    if not made:
        print('   %s: could not write %s' % (wkey, out_path))
        continue
    two = unreal.load_asset(TWO_SIDED)
    if two:
        slots = [s for s in made.get_editor_property('static_materials')]
        rebuilt = []
        for s in slots:
            s.material_interface = two          # looked THROUGH, so it must be solid from behind
            rebuilt.append(s)
        made.modify()
        made.set_editor_property('static_materials', rebuilt)
    unreal.EditorLoadingAndSavingUtils.save_packages([made.get_outer()], False)

    okey = 'Factory_' + wkey.split('/')[-1]
    if okey in optics:
        optics[okey]['mesh'] = '%s.%s' % (out_path, name)
    report.append((wkey, whole_asset.get_name(), keep_n, len(remove), out_path))

io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(d, indent=1) + '\n')
print('factory optics re-cut as the complement of the body: %d' % len(report))
for wkey, whole, keep, removed, path in report:
    print('   %-28s %4d tris kept, %4d were the body   -> %s' % (whole, keep, removed, path.split('/')[-1]))
