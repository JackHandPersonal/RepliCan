"""Take the crosshair BARS back out of the optic meshes.

They were an attempt to build the reticle as geometry inside the scope tube. That approach has to be
right about orientation, depth, size, occlusion and the near clip plane all at once, and it was
wrong about several of them in turn; even done perfectly it is only correct from one eye position.
The reticle is drawn in SCREEN space now (ABaseHUD::DrawOpticReticle), locked to the actual point of
impact, so the bars are dead weight sitting inside every scope where nobody can see them.

The lens disc STAYS: that is glass, it is visible from outside, and it is what makes the sight look
like a sight in the world.

  Tools/ue_remote --file Tools/strip_scope_crosshairs
"""
import unreal, json, io, os

AU = unreal.GeometryScript_AssetUtils
MATS = unreal.GeometryScript_Materials
CAT = os.path.join(unreal.Paths.project_dir(), 'Content', 'GameData', 'UI', 'Weapons.json')

cat = json.load(io.open(CAT, encoding='utf-8'))
cleared = []
for key, o in cat.get('optics', {}).items():
    asset = unreal.load_asset((o.get('mesh') or '').split('.')[0])
    if not asset:
        continue
    slots = [s for s in asset.get_editor_property('static_materials')]
    idx = None
    for i, sm in enumerate(slots):
        if sm.material_interface and 'ScopeCrosshair' in sm.material_interface.get_name():
            idx = i
    if idx is None:
        continue
    d = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(asset, d, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                      unreal.GeometryScriptMeshReadLOD())
    d = r[0] if isinstance(r, tuple) else d
    MATS.enable_material_i_ds(d)
    before = d.get_triangle_count()
    MATS.delete_triangles_by_material_id(d, idx)
    after = d.get_triangle_count()
    to = unreal.GeometryScriptCopyMeshToAssetOptions()
    to.enable_recompute_normals = False
    to.enable_recompute_tangents = True
    AU.copy_mesh_to_static_mesh(d, asset, to, unreal.GeometryScriptMeshWriteLOD())
    # The slot is emptied rather than removed: taking one out renumbers every slot after it, and the
    # lens disc is addressed by its own index.
    rebuilt = [s for s in asset.get_editor_property('static_materials')]
    if idx < len(rebuilt):
        rebuilt[idx].material_interface = None
        rebuilt[idx].material_slot_name = 'Unused'
    asset.modify()
    asset.set_editor_property('static_materials', rebuilt)
    unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outer()], False)
    cleared.append((key, before - after))

print('crosshair bars removed from %d optic(s)' % len(cleared))
for k, n in cleared:
    print('   %-26s %d triangles' % (k, n))
