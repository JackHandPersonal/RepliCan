"""Three mouse-sized alien pests, built from primitives. No pack ships anything like them.

    python Tools/ue_remote.py --file Tools/make_pests.py

Writes /Game/RepliCan/Pests/SM_Pest_Tick_01, SM_Pest_Crawler_01, SM_Pest_Hopper_01 and the
material M_Pest they share.

WHY GENERATED. Every Synty pack in this project was searched for a critter and there is none --
the closest thing is a Niagara insect swarm, which is a cloud, not a creature you can watch dart
under a crate. Rather than press a prop into service, these are built from spheres, capsules and
cones: a dozen primitives each, which is about the polygon budget a Synty prop of this size
would have anyway, so they sit in the art style rather than against it.

SCALE. Real mouse: about 8 cm nose to tail, 3 cm at the shoulder. These are built at that size
in centimetres, so they spawn at scale 1 and need no fudge factor. At that size the silhouette
is all the player ever gets -- nobody will see the geometry -- so the shapes are chosen for
what they read as in a fast-moving glimpse:

  Tick     low, wide, six splayed legs. Reads as something that scuttles along a skirting board.
  Crawler  long segmented body, many short legs, a raised tail. Reads as centipede.
  Hopper   compact body, heavy back legs, antennae. Reads as something about to jump.

No UVs are authored. The material is a flat colour, because a 3 cm creature moving at speed in
a dim corridor is a silhouette with a specular highlight and nothing else -- a texture would be
invisible and an atlas lookup would need UVs to be right, which is work for no return.
"""
import unreal, io, traceback, math

PKG = '/Game/RepliCan/Pests'
MAT = '/Game/RepliCan/Materials/M_Pest'
# Chitin: almost black, slightly green, and smooth enough to catch a hard highlight as it moves.
BODY_COLOUR = (0.020, 0.030, 0.024)
ROUGHNESS = 0.28
METALLIC = 0.15

V = unreal.Vector
R = unreal.Rotator
P = unreal.GeometryScript_Primitives


def xf(x=0.0, y=0.0, z=0.0, pitch=0.0, yaw=0.0, roll=0.0, sx=1.0, sy=1.0, sz=1.0):
    return unreal.Transform(location=V(x, y, z), rotation=R(roll=roll, pitch=pitch, yaw=yaw), scale=V(sx, sy, sz))


def sphere(m, r, t, steps=8):
    opts = unreal.GeometryScriptPrimitiveOptions()
    return P.append_sphere_lat_long(m, opts, t, r, steps, steps)


def capsule(m, r, length, t, steps=8):
    opts = unreal.GeometryScriptPrimitiveOptions()
    return P.append_capsule(m, opts, t, r, length, steps, steps)


def cone(m, base_r, top_r, height, t, steps=6):
    opts = unreal.GeometryScriptPrimitiveOptions()
    return P.append_cone(m, opts, t, base_r, top_r, height, steps, 1)


def leg(m, hip, angle_deg, reach, drop, thickness):
    """One leg, as two tapered segments: out and down from the hip, then down to the floor.

    Two segments rather than one because a straight spike reads as a spine; the knee is what
    makes the eye call it a leg. At this scale that is the entire difference."""
    a = math.radians(angle_deg)
    # Thigh: from the hip, out and slightly up.
    knee = (hip[0] + math.cos(a) * reach, hip[1] + math.sin(a) * reach, hip[2] + drop * 0.35)
    mid = ((hip[0] + knee[0]) * 0.5, (hip[1] + knee[1]) * 0.5, (hip[2] + knee[2]) * 0.5)
    yaw = math.degrees(math.atan2(knee[1] - hip[1], knee[0] - hip[0]))
    seg = math.dist(hip, knee)
    cone(m, thickness, thickness * 0.7, seg, xf(mid[0], mid[1], mid[2], pitch=90.0, yaw=yaw), steps=5)
    # Shin: from the knee down to the deck.
    foot = (knee[0] + math.cos(a) * reach * 0.45, knee[1] + math.sin(a) * reach * 0.45, 0.25)
    mid2 = ((knee[0] + foot[0]) * 0.5, (knee[1] + foot[1]) * 0.5, (knee[2] + foot[2]) * 0.5)
    seg2 = math.dist(knee, foot)
    pitch = math.degrees(math.atan2(math.hypot(foot[0] - knee[0], foot[1] - knee[1]), knee[2] - foot[2]))
    yaw2 = math.degrees(math.atan2(foot[1] - knee[1], foot[0] - knee[0]))
    cone(m, thickness * 0.7, thickness * 0.35, seg2, xf(mid2[0], mid2[1], mid2[2], pitch=pitch, yaw=yaw2), steps=5)


def build_tick(m):
    """Low and wide. 7 cm long, 2.2 cm tall: it hugs the deck."""
    sphere(m, 2.4, xf(0, 0, 2.0, sx=1.45, sy=1.15, sz=0.62))          # abdomen
    sphere(m, 1.5, xf(2.9, 0, 1.9, sx=1.0, sy=0.95, sz=0.70))         # thorax
    sphere(m, 0.95, xf(4.3, 0, 1.8, sx=1.0, sy=0.85, sz=0.85))        # head
    for side in (1, -1):
        for i, (hx, ang) in enumerate([(3.2, 35.0), (1.6, 75.0), (-0.4, 115.0)]):
            leg(m, (hx, side * 1.0, 1.9), side * ang, 2.2, 0.3, 0.30)
    # Two short palps at the front, which is what makes it read as a head end.
    for side in (1, -1):
        cone(m, 0.22, 0.05, 1.5, xf(5.0, side * 0.45, 2.1, pitch=78.0, yaw=side * 22.0), steps=4)
    return m


def build_crawler(m):
    """Long and segmented. 9 cm of body, low, with a raised tail."""
    for i in range(6):
        x = -3.4 + i * 1.5
        r = 1.5 - abs(i - 2) * 0.13
        sphere(m, r, xf(x, 0, 1.5, sx=1.0, sy=1.05, sz=0.80))
    sphere(m, 1.25, xf(5.1, 0, 1.6, sx=1.05, sy=0.9, sz=0.85))        # head
    for side in (1, -1):
        for i in range(5):
            leg(m, (-3.0 + i * 1.5, side * 0.85, 1.5), side * (60.0 + i * 6.0), 1.7, 0.2, 0.22)
    # Tail, lifted: the one silhouette cue that says "this end is not the head".
    cone(m, 0.55, 0.12, 3.2, xf(-4.6, 0, 2.6, pitch=138.0), steps=5)
    for side in (1, -1):
        cone(m, 0.18, 0.04, 1.8, xf(5.9, side * 0.4, 1.9, pitch=72.0, yaw=side * 28.0), steps=4)
    return m


def build_hopper(m):
    """Compact, with heavy back legs. Reads as coiled even standing still."""
    sphere(m, 2.2, xf(0, 0, 2.3, sx=1.15, sy=1.0, sz=0.95))           # body
    sphere(m, 1.3, xf(2.6, 0, 2.5, sx=0.95, sy=0.9, sz=0.9))          # head
    sphere(m, 1.0, xf(-2.0, 0, 2.6, sx=1.0, sy=0.85, sz=0.8))         # rear hump
    # Two heavy back legs with a high knee, and two small front ones.
    for side in (1, -1):
        knee = (-1.6, side * 1.7, 3.6)
        cone(m, 0.55, 0.40, 2.6, xf(-0.9, side * 1.1, 3.1, pitch=55.0, yaw=side * 62.0), steps=5)
        cone(m, 0.40, 0.16, 3.4, xf(-0.4, side * 2.0, 1.9, pitch=145.0, yaw=side * 48.0), steps=5)
        leg(m, (1.8, side * 0.9, 2.2), side * 42.0, 1.6, 0.2, 0.24)
    for side in (1, -1):
        cone(m, 0.16, 0.03, 2.4, xf(3.3, side * 0.5, 3.1, pitch=58.0, yaw=side * 20.0), steps=4)
    return m


BUILDERS = [('SM_Pest_Tick_01', build_tick), ('SM_Pest_Crawler_01', build_crawler),
            ('SM_Pest_Hopper_01', build_hopper)]

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before creating assets')

    tools = unreal.AssetToolsHelpers.get_asset_tools()

    # ---- The shared material ----
    mat = unreal.load_asset(MAT) if unreal.EditorAssetLibrary.does_asset_exist(MAT) else None
    if mat:
        print('M_Pest already exists; leaving it alone')
    else:
        mat = tools.create_asset('M_Pest', '/Game/RepliCan/Materials', unreal.Material, unreal.MaterialFactoryNew())
        if not mat:
            raise RuntimeError('could not create ' + MAT)
    col = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    col.set_editor_property('constant', unreal.LinearColor(*BODY_COLOUR, 1.0))
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 160)
    rough.set_editor_property('r', ROUGHNESS)
    metal = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 260)
    metal.set_editor_property('r', METALLIC)
    unreal.MaterialEditingLibrary.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    print('MATERIAL', MAT)

    # ---- The meshes ----
    for name, build in BUILDERS:
        full = '%s/%s' % (PKG, name)
        dyn = unreal.DynamicMesh()
        build(dyn)
        unreal.GeometryScript_Normals.set_per_face_normals(dyn)
        unreal.GeometryScript_Normals.recompute_normals(dyn, unreal.GeometryScriptCalculateNormalsOptions())
        # There is no StaticMeshFactoryNew in Python; GeometryScript makes the asset straight
        # from the mesh instead, which is the documented route and does the LOD/material setup
        # in one call.
        asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
        opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        opts.enable_recompute_normals = True
        opts.enable_recompute_tangents = True
        opts.replace_materials = True
        opts.new_materials = [unreal.load_asset(MAT)]
        if asset:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, opts, unreal.GeometryScriptMeshWriteLOD())
        else:
            create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            create.enable_recompute_normals = True
            create.enable_recompute_tangents = True
            asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
            if asset:
                asset.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=unreal.load_asset(MAT))])
        if not asset:
            print('could not create', full); continue
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        b = asset.get_bounds()
        print('%-22s %d tris, %.1f x %.1f x %.1f cm' % (name, dyn.get_triangle_count(),
              b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))
    print('PESTS built in', PKG)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
