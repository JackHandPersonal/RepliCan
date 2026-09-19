"""Compare a baked weapon's UVs against the pack's original, to explain a wrong colour.

Every weapon in this project shares one material -- the pack's atlas -- so a weapon's COLOUR is
decided entirely by which part of that atlas its UVs land on. A gun that comes out pale is not a
lighting problem and not a skin problem; it is a gun whose UVs are sitting on a pale patch.

That can be the pack's own artwork, or it can be damage done when the mesh was baked into grip space.
The two are told apart by asking the ORIGINAL the same question: if the pack's copy covers the same
UV box, the paleness is the artwork and there is nothing to fix.

  Tools/ue_remote --file Tools/compare_weapon_uvs
"""
import unreal

Q = unreal.GeometryScript_MeshQueries
AU = unreal.GeometryScript_AssetUtils

PAIRS = [
    ('Pistol_02', '/Game/RepliCan/Weapons/Worlds/SM_Wep_Pistol_02',
     '/Game/PolygonSciFiWorlds/Models/Weapons/Parts/SM_Wep_Pistol_02'),
    ('Pistol_01', '/Game/RepliCan/Weapons/Worlds/SM_Wep_Pistol_01',
     '/Game/PolygonSciFiWorlds/Models/Weapons/Parts/SM_Wep_Pistol_01'),
    ('Assault_01', '/Game/RepliCan/Weapons/Worlds/SM_Wep_Assault_01',
     '/Game/PolygonSciFiWorlds/Models/Weapons/Parts/SM_Wep_Assault_01'),
]


def uv_box(path):
    """(u0, u1, v0, v1, triangle count) over UV channel 0, or None.

    get_uv_set_bounding_box answers this directly; walking the triangles by hand is both slower and,
    as it turns out, not an API that exists."""
    a = unreal.load_asset(path)
    if not a:
        return None
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(a, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                      unreal.GeometryScriptMeshReadLOD())
    dyn = r[0] if isinstance(r, tuple) else dyn
    out = Q.get_uv_set_bounding_box(dyn, 0)
    box = out[0] if isinstance(out, tuple) else out
    ok = out[1] if isinstance(out, tuple) and len(out) > 1 else True
    if not ok or box is None:
        return None
    lo, hi = box.min, box.max
    return (lo.x, hi.x, lo.y, hi.y, dyn.get_triangle_count())


for name, ours, theirs in PAIRS:
    a = uv_box(ours)
    b = uv_box(theirs)
    print('---- %s' % name)
    for label, box in (('ours ', a), ('synty', b)):
        if not box:
            print('   %s  could not read' % label)
            continue
        print('   %s  u %.4f..%.4f   v %.4f..%.4f   over %d tris'
              % (label, box[0], box[1], box[2], box[3], box[4]))
    if a and b:
        moved = max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]), abs(a[3] - b[3]))
        print('   largest difference in the UV box: %.5f  -- %s'
              % (moved, 'the bake moved them' if moved > 0.002 else 'the same patch of atlas, so the colour is the artwork'))
