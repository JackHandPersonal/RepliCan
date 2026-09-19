"""Set the usage flags the map check asks for, and save the materials.

A material carries a flag per kind of thing it may be drawn on. Put one on a skeletal mesh or an
instanced static mesh without the matching flag and the editor turns the flag on for that session
and warns; the asset on disk still lacks it, so OUTSIDE the editor -- a cooked build -- the
material has no shader for that case and the thing renders wrong. The map check's line is
literally "If the material asset is not re-saved, it may not render correctly when run outside
the editor."

Run with the UE python against a running editor:
  Tools/ue_remote.py --file Tools/fix_material_usage_flags.py

Each row is (material, property, why). Add a row when the map check names a new one.
"""
import unreal

WANT = [
    # The cafeteria line-up wears the SciFi Space alternate palette on SKELETAL meshes
    # (SK_Chr_Junker_*, see facility_layout.py SOLDIER_MAT).
    ('/Game/PolygonSciFiSpace/Materials/Alternates/M_PolygonSciFiSpace_02_F', 'used_with_skeletal_mesh',
     'the cafeteria line-up (SOLDIER_MAT) is a skeletal mesh'),
    # The floor litter is one hierarchical instanced static mesh component per piece
    # (ALitterActor), and some pieces are CyberCity props painted with this.
    ('/Game/PolygonCyberCity/Materials/Misc/M_Posters_01', 'used_with_instanced_static_meshes',
     'litter pieces are drawn as instances (ALitterActor HISM)'),
]

# The flag READS BACK TRUE even on a broken asset: the editor turns it on in memory the moment it
# meets the usage, which is the same moment it warns. So the flag is not the test -- the saved
# package is. Set it if it is off, then SAVE regardless of what the flag says.
for path, prop, why in WANT:
    mat = unreal.load_asset(path)
    if not mat:
        print('MISSING %s' % path)
        continue
    was = mat.get_editor_property(prop)
    if not was:
        mat.set_editor_property(prop, True)
    dirty = mat.get_outer().is_dirty() if hasattr(mat.get_outer(), 'is_dirty') else None
    saved = unreal.EditorAssetLibrary.save_asset(path, False)   # False: save whether or not it is dirty
    after = unreal.load_asset(path).get_editor_property(prop)
    print('%s: flag_in_memory_was=%s dirty=%s saved=%s now=%s -- %s' % (path.rsplit('/', 1)[-1], was, dirty, saved, after, why))
print('DONE')
