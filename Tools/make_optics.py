"""A parallax red dot sight: the material, and an optic to hang it on.

    python Tools/ue_remote.py --file Tools/make_optics.py

Writes /Game/RepliCan/Optics/SM_Optic_RedDot_01 and the materials M_RedDot (the glass) and
M_OpticBody (the tube).

HOW A RED DOT ACTUALLY WORKS, AND WHY THAT MATTERS HERE. A real red dot is a collimator: the
dot is focused at infinity, so it sits on the target no matter where your eye is behind the
glass. Move your head and the dot stays put on what you are aiming at. That is the ENTIRE
point of the device -- it is why you can shoot with both eyes open and why head position stops
mattering. A dot painted flat on the lens does the opposite: it moves with your head and sits
on the target only from one exact eye position, which is worse than iron sights.

So the dot has to have parallax, and getting that is one line of maths rather than a render
target:

    the dot appears at infinity  <=>  its screen position depends ONLY on view DIRECTION

Take the vector from the lens to the camera, express it in the OPTIC'S own space (where +X is
downrange, per HAC1), and the direction the eye is looking through the glass is just its Y and
Z divided by its X. Offset the reticle by that and the dot is collimated. No second camera, no
render target, no cost beyond a handful of instructions -- and it works in third person too,
where a render-target approach would be drawing a scope view nobody is looking through.

The reticle itself is drawn in the material rather than sampled from a texture: a dot and a
ring are two distance tests, they stay sharp at any magnification, and their size, colour and
brightness end up as parameters that can be tuned on an instance without an art round trip.

MAGNIFYING SCOPES LATER. This same material is the reticle layer for those too; what a magnified
optic adds is the VIEW behind the reticle, which is either a narrowed camera FOV with an
overlay (free, right up to about 4x) or a SceneCapture2D on the lens (a real second render,
worth it past that). The parallax maths here does not change either way.
"""
import unreal, io, math, traceback

PKG = '/Game/RepliCan/Optics'
MAT_DIR = '/Game/RepliCan/Materials'
GLASS = MAT_DIR + '/M_RedDot'
MATERIAL_ONLY = True      # rebuild M_RedDot and stop: the RedDot_01 mesh and body stay as they are
class Done(BaseException): pass
BODY = MAT_DIR + '/M_OpticBody'

# The optic, in HAC1 terms: +X downrange, +Z up, origin where it clamps to the rail.
# An open frame. Small on purpose: a micro red dot is about 5 cm long and 3 across, and
# anything bigger crowds the view at a game field of view.
BASE_LONG = 4.4           # along the barrel
BASE_WIDE = 1.8
MOUNT_DROP = 1.1          # rail to the bottom of the glass
GLASS_WIDE = 2.6
GLASS_HEIGHT = 2.2
POST = 0.32               # the uprights and the top bar
LENS_AT = -0.6            # the glass sits just behind the middle of the base
# The same three numbers the mesh is built from, hoisted so the MATERIAL can use them too: the
# reticle is placed by the pane's local position, so the material has to know where the pane is.
# Derived rather than retyped, because a reticle centred somewhere the glass is not is a bug
# nobody would think to look for.
GLASS_WIDE_M = GLASS_WIDE
GLASS_HEIGHT_M = GLASS_HEIGHT
GLASS_Z_M = MOUNT_DROP + GLASS_HEIGHT * 0.5

V = unreal.Vector
R = unreal.Rotator
MEL = unreal.MaterialEditingLibrary


def xf(x=0.0, y=0.0, z=0.0, pitch=0.0, yaw=0.0, roll=0.0, sx=1.0, sy=1.0, sz=1.0):
    return unreal.Transform(location=V(x, y, z), rotation=R(roll=roll, pitch=pitch, yaw=yaw), scale=V(sx, sy, sz))


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before creating assets')
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    # ---- The glass -------------------------------------------------------------------------
    mat = unreal.load_asset(GLASS)
    if not mat:
        mat = tools.create_asset('M_RedDot', MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError('could not create ' + GLASS)

    MEL.delete_all_material_expressions(mat)   # a rebuild, not a pile of old nodes under the new ones
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    # Two-sided so the reticle is there from behind the weapon too, in third person.
    mat.set_editor_property('two_sided', True)

    def node(cls, x, y):
        return MEL.create_material_expression(mat, cls, x, y)

    def scalar(name, value, x, y):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', value)
        return n

    def const(value, x, y):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property('r', value)
        return n

    def link(a, ao, b, bi):
        MEL.connect_material_expressions(a, ao, b, bi)

    # -- the eye's direction through the glass, in the optic's own space --
    cam = node(unreal.MaterialExpressionCameraVectorWS, -1500, 0)
    to_local = node(unreal.MaterialExpressionTransform, -1330, 0)
    to_local.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    to_local.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(cam, '', to_local, '')

    # Y and Z are across the glass; X is downrange. Dividing by X is what puts the dot at
    # infinity: the offset then depends on the DIRECTION the eye looks, never on where it is.
    yz = node(unreal.MaterialExpressionComponentMask, -1150, -60)
    yz.set_editor_property('r', False); yz.set_editor_property('g', True)
    yz.set_editor_property('b', True); yz.set_editor_property('a', False)
    link(to_local, '', yz, '')

    xonly = node(unreal.MaterialExpressionComponentMask, -1150, 90)
    xonly.set_editor_property('r', True); xonly.set_editor_property('g', False)
    xonly.set_editor_property('b', False); xonly.set_editor_property('a', False)
    link(to_local, '', xonly, '')
    xabs = node(unreal.MaterialExpressionAbs, -1000, 90)
    link(xonly, '', xabs, '')
    # Never divide by zero at grazing angles, where the dot is off the glass anyway.
    xsafe = node(unreal.MaterialExpressionMax, -870, 90)
    link(xabs, '', xsafe, 'A')
    xsafe.set_editor_property('const_b', 0.05)

    perspective = node(unreal.MaterialExpressionDivide, -720, -20)
    link(yz, '', perspective, 'A')
    link(xsafe, '', perspective, 'B')

    para = scalar('ParallaxScale', 1.0, -900, -180)
    shifted = node(unreal.MaterialExpressionMultiply, -560, -60)
    link(perspective, '', shifted, 'A')
    link(para, '', shifted, 'B')

    # -- distance from the reticle centre --
    #
    # FROM THE PANE'S POSITION, NOT ITS UVs. The parallax term above is in the optic's own Y and
    # Z; if the reticle term is in texture space then the two only agree when the pane's UVs
    # happen to line up with those axes, and when they do not the dot travels sideways to the
    # head and smears into a streak. That is exactly what went wrong: the pane was built in the
    # XY plane and rotated, so its U ran up the optic and its V ran across it -- the reticle was
    # rotated ninety degrees relative to the parallax that was supposed to move it.
    #
    # Reading the shading point's LOCAL position instead puts both terms in the same space by
    # construction, and no UV layout can break it again.
    wp = node(unreal.MaterialExpressionWorldPosition, -1700, 300)
    local = node(unreal.MaterialExpressionTransformPosition, -1530, 300)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    link(wp, '', local, '')

    # The middle of the glass, in the optic's own space -- the same numbers the mesh was built to.
    # Per optic: where the pane sits in the optic's own frame and how big it is. Parameters, so a
    # part baked from a pack (Tools/bake_optic_part.py) sets its own on an instance; the defaults
    # are this script's RedDot_01. With constants here, RedDot_02's dot was drawn a pane-height
    # below its glass and never seen.
    lens_centre = node(unreal.MaterialExpressionVectorParameter, -1700, 420)
    lens_centre.set_editor_property('parameter_name', 'LensCentre')
    lens_centre.set_editor_property('default_value', unreal.LinearColor(LENS_AT, 0.0, GLASS_Z_M, 0.0))
    on_glass = node(unreal.MaterialExpressionSubtract, -1360, 340)
    link(local, '', on_glass, 'A')
    link(lens_centre, '', on_glass, 'B')

    across = node(unreal.MaterialExpressionComponentMask, -1200, 340)
    across.set_editor_property('r', False); across.set_editor_property('g', True)
    across.set_editor_property('b', True); across.set_editor_property('a', False)
    link(on_glass, '', across, '')

    # Normalised to the pane, so -0.5..0.5 across and up it whatever size the glass is.
    lens_size_p = node(unreal.MaterialExpressionVectorParameter, -1420, 460)
    lens_size_p.set_editor_property('parameter_name', 'LensSize')
    lens_size_p.set_editor_property('default_value', unreal.LinearColor(GLASS_WIDE_M, GLASS_HEIGHT_M, 1.0, 0.0))
    lens_size = node(unreal.MaterialExpressionComponentMask, -1200, 460)   # the pane's width and height
    lens_size.set_editor_property('r', True); lens_size.set_editor_property('g', True)
    lens_size.set_editor_property('b', False); lens_size.set_editor_property('a', False)
    link(lens_size_p, '', lens_size, '')
    from_centre = node(unreal.MaterialExpressionDivide, -1020, 360)
    link(across, '', from_centre, 'A')
    link(lens_size, '', from_centre, 'B')

    # The eye moves the dot the OPPOSITE way to the head, which is what keeps it on target.
    offset = node(unreal.MaterialExpressionAdd, -400, 140)
    link(from_centre, '', offset, 'A')
    link(shifted, '', offset, 'B')

    # Back to centimetres before the size tests: the dot and ring are physical sizes on the
    # glass, the same on every optic, not fractions of a pane that may be twice as wide.
    offset_cm = node(unreal.MaterialExpressionMultiply, -330, 140)
    link(offset, '', offset_cm, 'A')
    link(lens_size, '', offset_cm, 'B')
    dist = node(unreal.MaterialExpressionLength, -200, 140)
    link(offset_cm, '', dist, '')

    def falloff(edge_node, x, y, power):
        """saturate(1 - d/edge) ^ power -- a soft round blob with no texture involved."""
        d = node(unreal.MaterialExpressionDivide, x, y)
        link(dist, '', d, 'A')
        link(edge_node, '', d, 'B')
        inv = node(unreal.MaterialExpressionOneMinus, x + 140, y)
        link(d, '', inv, '')
        sat = node(unreal.MaterialExpressionSaturate, x + 270, y)
        link(inv, '', sat, '')
        p = node(unreal.MaterialExpressionPower, x + 400, y)
        link(sat, '', p, 'Base')
        p.set_editor_property('const_exponent', power)
        return p

    dot_size = scalar('DotSize', 0.025, -250, 400)    # cm: the dot's radius on the glass (a quarter of the first cut, per the user)
    dot = falloff(dot_size, -80, 300, 1.6)

    # The ring: the same falloff about a radius rather than about zero.
    ring_r = scalar('RingRadius', 0.15, -250, 620)     # cm
    ring_w = scalar('RingWidth', 0.02, -250, 720)      # cm
    ring_d = node(unreal.MaterialExpressionSubtract, -80, 620)
    link(dist, '', ring_d, 'A')
    link(ring_r, '', ring_d, 'B')
    ring_abs = node(unreal.MaterialExpressionAbs, 60, 620)
    link(ring_d, '', ring_abs, '')
    ring_div = node(unreal.MaterialExpressionDivide, 200, 620)
    link(ring_abs, '', ring_div, 'A')
    link(ring_w, '', ring_div, 'B')
    ring_inv = node(unreal.MaterialExpressionOneMinus, 340, 620)
    link(ring_div, '', ring_inv, '')
    ring_sat = node(unreal.MaterialExpressionSaturate, 470, 620)
    link(ring_inv, '', ring_sat, '')
    ring_amt = scalar('RingStrength', 0.55, 340, 760)
    ring = node(unreal.MaterialExpressionMultiply, 610, 660)
    link(ring_sat, '', ring, 'A')
    link(ring_amt, '', ring, 'B')

    shape = node(unreal.MaterialExpressionAdd, 760, 420)
    link(dot, '', shape, 'A')
    link(ring, '', shape, 'B')
    shape_sat = node(unreal.MaterialExpressionSaturate, 900, 420)
    link(shape, '', shape_sat, '')
    # A RED DOT IS A DOT ONLY FOR THE EYE BEHIND IT: from any other angle the glass should be
    # glass. The game drives this from nought to one as the weapon comes to the eye
    # (ABaseCharacter::TickOpticDot), and it gates the reticle in both the emissive and the opacity
    # so the dot does not merely dim -- it goes.
    dot_vis = scalar('DotVisible', 1.0, 900, 300)
    shape_vis = node(unreal.MaterialExpressionMultiply, 1040, 420)
    link(shape_sat, '', shape_vis, 'A')
    link(dot_vis, '', shape_vis, 'B')

    # -- colour, brightness, and a hint of glass --
    tint = node(unreal.MaterialExpressionVectorParameter, 760, 0)
    tint.set_editor_property('parameter_name', 'DotColour')
    tint.set_editor_property('default_value', unreal.LinearColor(1.0, 0.08, 0.05, 1.0))
    bright = scalar('Brightness', 26.0, 760, 140)
    lit = node(unreal.MaterialExpressionMultiply, 1040, 40)
    link(tint, '', lit, 'A')
    link(bright, '', lit, 'B')
    emissive = node(unreal.MaterialExpressionMultiply, 1180, 120)
    link(lit, '', emissive, 'A')
    link(shape_vis, '', emissive, 'B')

    # The glass is very slightly visible even where the dot is not, or the lens looks like a
    # hole in the weapon.
    glass_a = scalar('GlassOpacity', 0.10, 900, 560)
    opacity = node(unreal.MaterialExpressionAdd, 1180, 520)
    link(shape_vis, '', opacity, 'A')
    link(glass_a, '', opacity, 'B')
    opacity_sat = node(unreal.MaterialExpressionSaturate, 1320, 520)
    link(opacity, '', opacity_sat, '')

    MEL.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity_sat, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    print('GLASS', GLASS)
    if MATERIAL_ONLY: raise Done()

    # ---- The tube --------------------------------------------------------------------------
    body = unreal.load_asset(BODY)
    if not body:
        body = tools.create_asset('M_OpticBody', MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        col = MEL.create_material_expression(body, unreal.MaterialExpressionConstant3Vector, -400, 0)
        col.set_editor_property('constant', unreal.LinearColor(0.025, 0.027, 0.030, 1.0))
        rough = MEL.create_material_expression(body, unreal.MaterialExpressionConstant, -400, 160)
        rough.set_editor_property('r', 0.35)
        MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        MEL.recompile_material(body)
        unreal.EditorAssetLibrary.save_loaded_asset(body, False)
    print('BODY ', BODY)

    # ---- The optic -------------------------------------------------------------------------
    # AN OPEN FRAME, NOT A TUBE. The first version was a cylinder, and append_cylinder builds a
    # CAPPED one: the result was a solid disc of geometry exactly where the player is supposed
    # to be looking. Opaque, and obviously so the moment it was aimed down.
    #
    # A frame is also the better design. A micro red dot is a base, two uprights and a pane of
    # glass; there is nothing to see through because there is nothing in the way. At this
    # polygon count a tube would be a dark octagon crowding the view even if it were hollow.
    P = unreal.GeometryScript_Primitives
    dyn = unreal.DynamicMesh()
    opts = unreal.GeometryScriptPrimitiveOptions()

    GLASS_Z = GLASS_Z_M
    # Base: sits on the rail, runs along the barrel.
    P.append_box(dyn, opts, xf(0.0, 0.0, MOUNT_DROP * 0.5), BASE_LONG, BASE_WIDE, MOUNT_DROP)
    # Two uprights, one either side of the glass, and a bar across the top. The gap between
    # them is what the player looks through.
    for side in (1, -1):
        P.append_box(dyn, opts, xf(LENS_AT, side * (GLASS_WIDE * 0.5 + POST * 0.5), GLASS_Z),
                     POST, POST, GLASS_HEIGHT)
    P.append_box(dyn, opts, xf(LENS_AT, 0.0, MOUNT_DROP + GLASS_HEIGHT + POST * 0.5),
                 POST, GLASS_WIDE + POST * 2.0, POST)
    unreal.GeometryScript_Normals.set_per_face_normals(dyn)
    unreal.GeometryScript_Normals.recompute_normals(dyn, unreal.GeometryScriptCalculateNormalsOptions())

    # The glass: a flat pane facing the shooter, filling the frame. Its own material slot.
    #
    # PITCH, not roll. append_rectangle_xy builds the pane in the local XY plane, so its normal
    # starts along +Z and has to be turned to point back down the barrel at the eye. Measured
    # (see the rotator-axis note in memory): pitch -90 sends +Z to +X, roll +90 sends +Z to +Y.
    # The first version rolled it, which left the pane standing EDGE-ON to the shooter -- a
    # two-sided sliver you could just about see a red smear through. Pitch 90 sends +Z to -X,
    # which is the face the eye is on.
    #
    # Pitch 90 also carries the rectangle's own X axis to the optic's +Z and its Y axis to the
    # optic's +Y, so the two dimensions swap: pass height first to end up GLASS_WIDE across and
    # GLASS_HEIGHT tall.
    lens = unreal.DynamicMesh()
    P.append_rectangle_xy(lens, opts, xf(LENS_AT, 0.0, GLASS_Z, pitch=90.0), GLASS_HEIGHT, GLASS_WIDE, 1, 1)
    unreal.GeometryScript_Normals.set_per_face_normals(lens)
    # The glass needs its OWN material slot, so it is appended with a material id of 1 rather
    # than merged into the frame's. Without that the reticle material would be applied to the
    # whole optic and the frame would glow red.
    # Appending WITH MATERIAL LISTS is what keeps the glass on its own slot: the call combines
    # the two lists and remaps the appended triangles onto the combined one. Merging them any
    # other way puts the reticle material on the whole optic and the frame glows red.
    body_mat = unreal.load_asset(BODY)
    glass_mat = unreal.load_asset(GLASS)
    dyn, combined = unreal.GeometryScript_MeshEdits.append_mesh_with_materials(
        dyn, [body_mat], lens, [glass_mat], unreal.Transform())

    full = PKG + '/SM_Optic_RedDot_01'
    asset = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
    to_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    to_opts.enable_recompute_normals = True
    to_opts.enable_recompute_tangents = True
    to_opts.replace_materials = True
    to_opts.new_materials = list(combined) if combined else [body_mat, glass_mat]
    if asset:
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
    else:
        create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        create.enable_recompute_normals = True
        create.enable_recompute_tangents = True
        asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
        if asset:
            asset.set_editor_property('static_materials',
                [unreal.StaticMaterial(material_interface=m) for m in (combined if combined else [body_mat, glass_mat])])
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    b = asset.get_bounds()
    print('OPTIC %s  %d tris  %.1f x %.1f x %.1f cm'
          % (full, dyn.get_triangle_count(), b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))
    print('lens centre (the new rear sight) at local (%.2f, 0.00, %.2f)' % (LENS_AT, MOUNT_DROP + GLASS_HEIGHT * 0.5))
except Done:
    print('material only: done')
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
