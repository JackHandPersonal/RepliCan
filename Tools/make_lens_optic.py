# The BugBuster's built-in scope, made to look through: the tube is measured off the weapon mesh
# (the highest cylinder on it, its rear opening), a glass disc wearing the parallax red-dot
# material (Tools/make_optics.py's M_RedDot) is built to fit that opening, and the weapon's entry
# in UI/Weapons.json is pointed at it -- optic Lens_Small mounted at the opening, with the FULL
# mesh as its body so the tube is not stripped the way a fitted optic normally strips the
# weapon's own scope -- but with the tube's two END CAPS cut out (2026-09-17), because a closed
# Synty cylinder is what the eye met behind the glass: a painted lid where the target should be.
# Run in the editor through the remote-exec tool from the project root.
import unreal, io, re, os, math
WEAPON = '/Game/RepliCan/Weapons/Worlds/SM_Wep_Pistol_05'
PKG = '/Game/RepliCan/Optics'; NAME = 'SM_Optic_Lens_Small'
GLASS = '/Game/RepliCan/Materials/M_RedDot'
WJ = 'C:/Dev/Games/RepliCan/UI/Weapons.json'
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if ues.get_game_world() is not None: raise RuntimeError('the editor is in Play')

# ---- measure the tube
mesh = unreal.load_asset(WEAPON)
dyn = unreal.DynamicMesh()
dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
n = dyn.get_vertex_count()
verts = []
for i in range(n):
    p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
    if ok: verts.append((p.x, p.y, p.z))
zmax = max(v[2] for v in verts)
# the scope is the highest thing on the weapon: everything within a tube's height of the top
top = [v for v in verts if v[2] > zmax - 3.4]
xmin = min(v[0] for v in top)
ring = [v for v in top if v[0] < xmin + 0.9]
cy = (max(v[1] for v in ring) + min(v[1] for v in ring)) * 0.5
cz = (max(v[2] for v in ring) + min(v[2] for v in ring)) * 0.5
radius = max(0.35, (max(v[2] for v in ring) - min(v[2] for v in ring)) * 0.5)
print('scope: top z %.2f; %d verts in the top band; rear ring x %.2f, %d verts, centre (y %.2f, z %.2f), radius %.2f' % (zmax, len(top), xmin, len(ring), cy, cz, radius))
mount = (round(xmin - 0.05, 2), round(cy, 2), round(cz, 2))

# ---- the glass: a disc facing downrange (+X), the reticle material on it
glass_mat = unreal.load_asset(GLASS)
if not glass_mat: raise RuntimeError('M_RedDot missing: run Tools/make_optics.py first')
# THE DOT WHERE THIS GLASS IS (2026-09-17): M_RedDot centres its reticle at LensCentre and sizes it by
# LensSize, both defaulting to the RDS-1's pane (2.2 up its base). This disc sits at its own origin and
# is a third of that pane's size, so with the defaults the dot was drawn two centimetres above the
# glass -- off it, nowhere to be seen. An instance puts the centre at the origin and the size at the disc.
MEL = unreal.MaterialEditingLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools()
mi_path = '/Game/RepliCan/Materials/MI_RedDot_LensSmall'
mi = unreal.load_asset(mi_path) if unreal.EditorAssetLibrary.does_asset_exist(mi_path) else tools.create_asset('MI_RedDot_LensSmall', '/Game/RepliCan/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property('parent', glass_mat)
MEL.set_material_instance_vector_parameter_value(mi, 'LensCentre', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
MEL.set_material_instance_vector_parameter_value(mi, 'LensSize', unreal.LinearColor(radius * 2.0, radius * 2.0, 1.0, 0.0))
MEL.update_material_instance(mi); unreal.EditorLoadingAndSavingUtils.save_packages([mi.get_outermost()], False)
glass_mat = mi
lens = unreal.DynamicMesh()
P = unreal.GeometryScript_Primitives
opts = unreal.GeometryScriptPrimitiveOptions()
xf = unreal.Transform(location=unreal.Vector(0, 0, 0), rotation=unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0), scale=unreal.Vector(1, 1, 1))
# THE GLASS FILLS THE HOLE. At 0.92 of the tube's radius the disc left an open ring all the way
# round it -- the cap is cut out at the full radius, so an undersized disc shows the inside of the
# tube and daylight straight through, which reads as missing polygons round the sight. Full radius.
P.append_disc(lens, opts, xf, radius, 32, 1, 0.0, 360.0, 0.0)
unreal.GeometryScript_Normals.set_per_face_normals(lens)
full = PKG + '/' + NAME
asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
if asset:
    to_opts = unreal.GeometryScriptCopyMeshToAssetOptions(); to_opts.enable_recompute_normals = True; to_opts.enable_recompute_tangents = True
    to_opts.replace_materials = True; to_opts.new_materials = [glass_mat]
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(lens, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
else:
    create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions(); create.enable_recompute_normals = True; create.enable_recompute_tangents = True
    asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(lens, full, create)
    if asset: asset.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=glass_mat)])
unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
print('optic', full, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Optics', NAME + '.uasset')))

# ---- the body: the full weapon with the scope's end caps cut out, so the tube can be looked through
# The scope is the baked part Tools/bake_weapon_parts.py wrote (exact HAC1 geometry); a body triangle
# is the scope's when its three vertices lie on the part's vertices, and a CAP when its face normal
# runs along the bore (+-X). The walls stay; the eye looks down an open tube at the glass in front.
Q = unreal.GeometryScript_MeshQueries; SEL = unreal.GeometryScript_MeshSelection; ED = unreal.GeometryScript_MeshEdits; AU = unreal.GeometryScript_AssetUtils
SCOPE = '/Game/RepliCan/Weapons/Worlds/Parts/SM_Wep_Pistol_05_Scope_01'
BODY = '/Game/RepliCan/Weapons/Worlds/Body/SM_Wep_Pistol_05_Body'
scope = unreal.load_asset(SCOPE)
body_path = None
if scope:
    pd = unreal.DynamicMesh(); pd, _ = AU.copy_mesh_from_static_mesh(scope, pd, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    grid = {}; grid_pts = []
    for i in range(pd.get_vertex_count()):
        p, ok = Q.get_vertex_position(pd, i)
        if ok: grid.setdefault((round(p.x), round(p.y), round(p.z)), []).append((p.x, p.y, p.z)); grid_pts.append((p.x, p.y, p.z))
    body = unreal.DynamicMesh(); body, _ = AU.copy_mesh_from_static_mesh(mesh, body, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    on_scope = {}
    def is_scope(vi):
        if vi not in on_scope:
            p, ok = Q.get_vertex_position(body, vi); hit = False
            if ok:
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        for dz in (-1, 0, 1):
                            for q in grid.get((round(p.x) + dx, round(p.y) + dy, round(p.z) + dz), ()):
                                if abs(q[0] - p.x) <= 0.05 and abs(q[1] - p.y) <= 0.05 and abs(q[2] - p.z) <= 0.05: hit = True
            on_scope[vi] = hit
        return on_scope[vi]
    # Only the two END DISCS go (2026-09-17): the first cut took every scope face whose normal ran
    # along the bore, which included the bevel rings at both ends, and the tube looked eaten. A cap
    # is a triangle whose three corners all lie on the scope's rearmost or foremost x.
    sx = [p[0] for p in grid_pts]
    x_lo, x_hi = min(sx), max(sx)
    pos_of = {}
    def px(vi):
        if vi not in pos_of:
            p, ok = Q.get_vertex_position(body, vi); pos_of[vi] = p.x if ok else 0.0
        return pos_of[vi]
    yz_of = {}
    def pyz(vi):
        """The vertex across the bore, for the distance-from-axis test below."""
        if vi not in yz_of:
            p, ok = Q.get_vertex_position(body, vi); yz_of[vi] = (p.y, p.z) if ok else (0.0, 0.0)
        return yz_of[vi]
    caps, walls = [], 0
    for t in range(Q.get_num_triangle_i_ds(body)):
        r = Q.get_triangle_indices(body, t); idx = r[0] if isinstance(r, tuple) else r
        vs = (int(idx.x), int(idx.y), int(idx.z))
        if not all(is_scope(v) for v in vs): continue
        xs_ = [px(v) for v in vs]
        # BOTH CAPS GO (2026-09-19). This cut the NEAR cap only, reasoning that an open pipe showed
        # the inside of the tube wall smeared to a rainbow, and that a closed flat end "is what a
        # sight looks like". It is not -- it is what a LENS CAP looks like. Measured on the shipped
        # asset: the ADS sight line crossed the un-cut mesh TWICE, at x 0.17 facing away (the near
        # cap) and x 12.28 facing the eye (the far one), and the cut body still crossed it once, at
        # 12.28. The player looked through the glass into a solid disc 12.4 cm down the tube, which
        # is precisely the reported "the optic is opaque" -- the single fault this whole run of work
        # was chasing. Seeing through a 1x sight is the requirement; the tube's interior looking
        # poor is a problem to fix on the material, never a reason to weld the sight shut.
        # ANYTHING ACROSS THE BORE, not just the discs at the two ends. Testing for x_lo/x_hi assumes
        # the only things blocking the sight line are the end caps, and on this mesh that is false:
        # after both ends were cut the line STILL crossed the body at x 12.28, a small disc of radius
        # 0.45 sitting 0.37 behind the front rim at 12.65 -- the objective lens, modelled as a plug
        # inside the tube, missed by a test keyed to the extremes. So the rule is what actually
        # matters: a triangle that lies in a plane perpendicular to the bore AND sits within HOLE_R
        # of the bore axis is in the way, wherever along the tube it is. That punches a clean round
        # hole through every such disc and leaves the outer annulus, so the rims and bevels survive
        # and the tube still reads as a tube -- which is what "the tube looked eaten" was about the
        # first time this was cut too wide.
        HOLE_R = 1.0
        planar = max(xs_) - min(xs_) < 0.25
        ctr_y = sum(pyz(v)[0] for v in vs) / 3.0 - cy
        ctr_z = sum(pyz(v)[1] for v in vs) / 3.0 - cz
        if planar and (ctr_y * ctr_y + ctr_z * ctr_z) ** 0.5 < HOLE_R: caps.append(t)
        else: walls += 1
    print('scope in the body: %d cap triangles cut from BOTH ends, %d wall triangles kept -- the tube is open and can be looked through' % (len(caps), walls))
    if caps:
        r = SEL.convert_index_array_to_mesh_selection(body, caps, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
        sel = [x for x in r if isinstance(x, unreal.GeometryScriptMeshSelection)][0] if isinstance(r, tuple) else r
        r = ED.delete_selected_triangles_from_mesh(body, sel); body = r[0] if isinstance(r, tuple) else body
        target = unreal.load_asset(BODY) if unreal.EditorAssetLibrary.does_asset_exist(BODY) else None
        if target:
            to = unreal.GeometryScriptCopyMeshToAssetOptions(); to.enable_recompute_normals = False; to.enable_recompute_tangents = True; to.replace_materials = False
            AU.copy_mesh_to_static_mesh(body, target, to, unreal.GeometryScriptMeshWriteLOD())
        else:
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions(); opts.enable_recompute_normals = False; opts.enable_recompute_tangents = True
            res = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(body, BODY, opts)
            target = next((x for x in res if isinstance(x, unreal.StaticMesh)), None) if isinstance(res, tuple) else res
            if not target: target = unreal.load_asset(BODY)
        mats = [sm.material_interface for sm in mesh.get_editor_property('static_materials')]
        # Looking down the open tube you look at the INSIDE of its walls, which a one-sided material
        # culls: the body wears a two-sided instance of the same material, so the tube reads as a tube.
        two = []
        for k, m in enumerate(mats):
            try:
                p2 = '/Game/RepliCan/Materials/MI_Wep_Pistol_05_TwoSided_%d' % k
                m2 = unreal.load_asset(p2) if unreal.EditorAssetLibrary.does_asset_exist(p2) else tools.create_asset('MI_Wep_Pistol_05_TwoSided_%d' % k, '/Game/RepliCan/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                m2.set_editor_property('parent', m)
                ov = m2.get_editor_property('base_property_overrides'); ov.set_editor_property('override_two_sided', True); ov.set_editor_property('two_sided', True); m2.set_editor_property('base_property_overrides', ov)
                MEL.update_material_instance(m2); unreal.EditorLoadingAndSavingUtils.save_packages([m2.get_outermost()], False); two.append(m2)
            except Exception as ex: print('two-sided instance not made:', ex); two.append(m)
        if target and mats: target.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=m) for m in two])
        unreal.EditorAssetLibrary.save_asset(BODY)
        body_path = BODY + '.' + BODY.rsplit('/', 1)[-1]
        print('body', BODY, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Weapons', 'Worlds', 'Body', 'SM_Wep_Pistol_05_Body.uasset')))
else:
    print('no scope part at', SCOPE, '-- the full mesh stays as the body')

# ---- the catalogue: the optic entry, and the BugBuster looking through it
w = io.open(WJ, encoding='utf-8').read()
if '"Lens_Small":' not in w:
    at = w.index('"optics":'); at = w.index('{', at) + 1; at = w.index('\n', at) + 1
    entry = ('\t\t"Lens_Small":\n\t\t{\n\t\t\t"mesh": "%s",\n\t\t\t"eye": [\n\t\t\t\t-0.6,\n\t\t\t\t0,\n\t\t\t\t0\n\t\t\t],\n\t\t\t"sit": 0,\n'
             '\t\t\t"name": "Built-in lens",\n\t\t\t"make": "Mikita",\n\t\t\t"model": "BugBuster lens",\n\t\t\t"icon": "",\n'
             '\t\t\t"comment": "Tools/make_lens_optic.py: a glass disc for a scope that is part of its weapon"\n\t\t},\n') % full
    w = w[:at] + entry + w[at:]
a = w.index('"Worlds/Wep_Pistol_05":'); b = w.index('"Worlds/Wep_Pistol_06":')
e = w[a:b]
e = re.sub(r'"optic_mount": \[[^\]]*\]', '"optic_mount": [\n\t\t\t\t%s,\n\t\t\t\t%s,\n\t\t\t\t%s\n\t\t\t]' % mount, e, count=1)
e = e.replace('"optic": ""', '"optic": "Lens_Small"', 1)
e = re.sub(r'"body_mesh": "[^"]*"', '"body_mesh": "%s"' % (body_path or ('%s.%s' % (WEAPON, WEAPON.rsplit('/', 1)[-1]))), e, count=1)
w = w[:a] + e + w[b:]
io.open(WJ, 'w', encoding='utf-8', newline='\n').write(w)
print('BugBuster: optic Lens_Small at', mount, '; the full mesh stays as its body')
