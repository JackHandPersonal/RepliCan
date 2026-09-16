"""Every sight-like mesh we hold, measured for use as a red dot on a rifle.
    python Tools/ue_remote.py --file Tools/survey_optics.py

For each mesh: size (cm), triangles, which axis it is long along, and OPEN -- the share of
rays fired along the long axis through the upper part of its cross-section that pass clean
through. A reflex sight is a frame around a window (OPEN high); a scope is a tube with glass
or a solid body (OPEN near zero). Written to Saved/ClaudeAssist/optics_report.txt.
"""
import unreal, os
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils; SP = unreal.GeometryScript_MeshSpatial
W = '/Game/PolygonSciFiWorlds/Models/Weapons/Parts/'; C = '/Game/PolygonScifi/Meshes/Weapons/'; P = '/Game/Synty/PolygonPoliceStation/Meshes/Weapons/'
M = '/Game/PolygonMilitary/Meshes/Weapons/Modular/Attachments/Scopes/'; MA = '/Game/PolygonMilitary/Meshes/Weapons/Modular/Attachments/'
CANDIDATES = [
    '/Game/RepliCan/Optics/SM_Optic_RedDot_01',
    W + 'SM_Wep_Assault_01_Scope_01', W + 'SM_Wep_Assault_02_RedDot_01', W + 'SM_Wep_Launcher_03_RedDot_01',
    W + 'SM_Wep_Scope_Small_01', W + 'SM_Wep_Scope_Small_02', W + 'SM_Wep_Scope_Small_03', W + 'SM_Wep_Scope_Unique_01',
    W + 'SM_Wep_Pistol_01_Scope_01', W + 'SM_Wep_Pistol_05_Scope_01', W + 'SM_Wep_SMG_03_Scope_01',
    C + 'SM_Wep_Attachment_Redot_01', C + 'SM_Wep_Attachment_Redot_02', C + 'SM_Wep_Attachment_Redot_03',
    C + 'SM_Wep_Attachment_Ironsight_01', C + 'SM_Wep_Attachment_Ironsight_02', C + 'SM_Wep_Attachment_Ironsight_03',
    C + 'SM_Wep_Attachment_Scope_01', C + 'SM_Wep_Attachment_Scope_02', C + 'SM_Wep_Attachment_Scope_03',
    C + 'SM_Wep_Attachment_Handle_Scope_01', C + 'SM_Wep_Attachment_Handle_Scope_02', C + 'SM_Wep_Attachment_Handle_Scope_03',
    P + 'SM_Wep_Attach_Sight_01', P + 'SM_Wep_Attach_Scope_01',
    M + 'SM_Wep_Mod_Reddot_01', M + 'SM_Wep_Mod_Reddot_02', M + 'SM_Wep_Mod_Reddot_03', M + 'SM_Wep_Mod_Reddot_04', M + 'SM_Wep_Mod_Reddot_05',
    M + 'SM_Wep_Mod_Sight_Add_01', M + 'SM_Wep_Mod_Scope_Flip_01', M + 'SM_Wep_Mod_Scope_01', M + 'SM_Wep_Mod_Scope_02', M + 'SM_Wep_Mod_Scope_03',
    '/Game/PolygonCyberCity/Meshes/Weapons/SM_Wep_Sniper_Rifle_Scope_01',
]


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


lines = ['%-34s %-12s %6s %5s %6s  %s' % ('MESH', 'PACK', 'LONG', 'TRIS', 'OPEN', 'size x y z (cm)')]
for path in CANDIDATES:
    m = unreal.load_asset(path)
    name = path.split('/')[-1]; pack = path.split('/')[2] if '/Synty/' not in path else path.split('/')[3]
    if not m:
        lines.append('%-34s %-12s MISSING' % (name, pack)); continue
    b = m.get_bounding_box(); size = b.max - b.min
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(m, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
    r = SP.build_bvh_for_mesh(dyn); bvh = first(r, unreal.GeometryScriptDynamicMeshBVH)
    axis = 'x' if size.x >= size.y else 'y'
    # rays along the long axis through a grid over the upper 60% of the cross-section
    clear = 0; total = 0
    for i in range(5):
        for j in range(5):
            f = 0.3 + 0.4 * i / 4.0          # across: the middle 40%
            g = 0.40 + 0.55 * j / 4.0        # up: 40%..95% of the height
            z = b.min.z + size.z * g
            if axis == 'x':
                o = unreal.Vector(b.min.x - 5.0, b.min.y + size.y * f, z); d = unreal.Vector(1, 0, 0)
            else:
                o = unreal.Vector(b.min.x + size.x * f, b.min.y - 5.0, z); d = unreal.Vector(0, 1, 0)
            r = SP.find_nearest_ray_intersection_with_mesh(dyn, bvh, o, d, unreal.GeometryScriptSpatialQueryOptions())
            hit = first(r, unreal.GeometryScriptRayHitResult)
            total += 1
            if not (hit and hit.hit): clear += 1
    lines.append('%-34s %-12s %6s %5d %5.0f%%  %5.1f %5.1f %5.1f' % (name, pack, axis, dyn.get_triangle_count(), 100.0 * clear / max(1, total), size.x, size.y, size.z))
text = '\n'.join(lines); print(text)
d = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeAssist'); os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'optics_report.txt'), 'w', encoding='utf-8').write(text)
