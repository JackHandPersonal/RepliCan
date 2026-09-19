"""The scope overlay mask: black everywhere except a circle you look through.

This is the piece that makes a magnified sight read as a sight. The scope body is a solid tube about
thirty centimetres long and the eye sits behind it, so looking "down" it shows a pipe, not a picture.
Every first person shooter solves this the same way: at full magnification the 3D scope stops being
drawn to the shooter, a full-screen mask with a circular cutout goes over the view, and the world FOV
narrows by the magnification. The cutout IS the sight picture.

A material rather than a texture, because the cutout has to move: the black ring grows when the
weapon is not settled (scope shadow / eyebox), and that wants a radius driven every frame rather
than a fixed image.

  Radius  how much of the screen height the opening takes (0.34 = a touch under a third)
  Soft    the feathered edge, in the same units
  Aspect  screen width over height, so the opening is round rather than an oval

THE TRAP THAT COST FOUR ROUNDS OF "the sights are a black screen":
connect_material_expressions RETURNS FALSE AND SAYS NOTHING when the input pin name does not match.
Length's single input is UNNAMED, so connecting it as 'Input' quietly did nothing -- the node took
the length of no input, which is zero, and the whole mask became smoothstep(R, R+S, 0): the same
value at every pixel, no opening anywhere. The graph looked perfect in the editor, every node
present and the opacity plugged in, and there was no warning in any log. So every link here is now
asserted, and the finished function is MEASURED rather than assumed.

The check builds the same graph a second time into a throwaway material with the mask driving
EMISSIVE instead of opacity -- a translucent material's opacity cannot be read back off a render
target, but its emissive can -- draws it, and reads the middle, the ring and the corner. The middle
of a scope mask must be clear and its corner must be solid.

  Tools/ue_remote --file Tools/make_scope_mask
"""
import unreal

MEL = unreal.MaterialEditingLibrary
OUT_DIR = '/Game/RepliCan/Materials'
NAME = 'M_ScopeMask'
CHECK = NAME + '_Check'


def link(a, out, b, pin):
    """Connect, and refuse to carry on if the pin name was wrong."""
    if not MEL.connect_material_expressions(a, out, b, pin):
        names = [str(n) for n in MEL.get_material_expression_input_names(b)]
        raise RuntimeError('%s -> %s.%s did not connect; its pins are %s'
                           % (a.get_class().get_name(), b.get_class().get_name(), pin, names))


def link_property(a, out, prop):
    if not MEL.connect_material_property(a, out, prop):
        raise RuntimeError('%s -> %s did not connect' % (a.get_class().get_name(), prop))


def build(mat, mask_to_emissive=False):
    """The graph. With mask_to_emissive the mask value comes out as colour, which can be read back."""
    MEL.delete_all_material_expressions(mat)   # a rebuild, not a pile of old nodes under the new ones
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)

    def node(cls, x, y):
        return MEL.create_material_expression(mat, cls, x, y)

    uv = node(unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    half = node(unreal.MaterialExpressionConstant2Vector, -1100, 160)
    half.set_editor_property('r', 0.5)
    half.set_editor_property('g', 0.5)
    centred = node(unreal.MaterialExpressionSubtract, -900, 40)
    link(uv, '', centred, 'A')
    link(half, '', centred, 'B')

    aspect = node(unreal.MaterialExpressionScalarParameter, -1100, 340)
    aspect.set_editor_property('parameter_name', 'Aspect')
    aspect.set_editor_property('default_value', 1.7777)
    one = node(unreal.MaterialExpressionConstant, -1100, 440)
    one.set_editor_property('r', 1.0)
    axes = node(unreal.MaterialExpressionAppendVector, -900, 380)
    link(aspect, '', axes, 'A')
    link(one, '', axes, 'B')

    # The opening is round on SCREEN, so the horizontal axis is stretched by the aspect before the
    # distance is taken -- otherwise a widescreen view looks through an oval.
    scaled = node(unreal.MaterialExpressionMultiply, -700, 140)
    link(centred, '', scaled, 'A')
    link(axes, '', scaled, 'B')

    # THE PIN THAT IS NOT CALLED 'Input'. Length takes one unnamed input, so its name is the empty
    # string; asking for 'Input' is what silently broke this material.
    dist = node(unreal.MaterialExpressionLength, -520, 140)
    link(scaled, '', dist, '')

    radius = node(unreal.MaterialExpressionScalarParameter, -700, 460)
    radius.set_editor_property('parameter_name', 'Radius')
    radius.set_editor_property('default_value', 0.34)
    soft = node(unreal.MaterialExpressionScalarParameter, -700, 560)
    soft.set_editor_property('parameter_name', 'Soft')
    soft.set_editor_property('default_value', 0.025)
    outer = node(unreal.MaterialExpressionAdd, -520, 500)
    link(radius, '', outer, 'A')
    link(soft, '', outer, 'B')

    # 0 inside the opening, 1 outside it: the alpha of the black that surrounds the picture.
    step = node(unreal.MaterialExpressionSmoothStep, -320, 260)
    link(radius, '', step, 'Min')
    link(outer, '', step, 'Max')
    link(dist, '', step, 'Value')

    if mask_to_emissive:
        link_property(step, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        link_property(step, '', unreal.MaterialProperty.MP_OPACITY)
        # Black surround. A scope body is not a light source.
        black = node(unreal.MaterialExpressionConstant, -320, 60)
        black.set_editor_property('r', 0.0)
        link_property(black, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Left on the SURFACE domain deliberately: AHUD::DrawMaterialSimple draws through
    # FCanvasTileItem, which takes a surface material's emissive and opacity directly. The UI
    # domain is for Slate and UMG brushes, and 5.8 has no UI entry in MaterialUsage to set.
    MEL.recompile_material(mat)


def material(name):
    p = OUT_DIR + '/' + name
    m = unreal.load_asset(p)
    if not m:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, OUT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        print('created ' + p)
    return m


mat = material(NAME)
build(mat, mask_to_emissive=False)
unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outer()], False)
print('rebuilt ' + OUT_DIR + '/' + NAME)

# ---- and now look at it ------------------------------------------------------------------------
probe = material(CHECK)
try:
    build(probe, mask_to_emissive=True)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_game_world() or ues.get_editor_world()
    RL = unreal.RenderingLibrary
    rt = RL.create_render_target2d(world, 256, 256, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, probe)
    mid.set_scalar_parameter_value('Aspect', 1.0)    # a square target: the opening should be round in it
    mid.set_scalar_parameter_value('Radius', 0.34)
    mid.set_scalar_parameter_value('Soft', 0.025)
    RL.clear_render_target2d(world, rt, unreal.LinearColor(0.0, 1.0, 0.0, 1.0))
    RL.draw_material_to_render_target(world, rt, mid)

    def at(x, y):
        return RL.read_render_target_pixel(world, rt, x, y).r

    # At 256 across, a pixel's distance from the middle is (128 - y) / 256 in the material's units,
    # so y = 100 is 0.11 (well inside a 0.34 opening) and y = 16 is 0.44 (well outside its feather).
    centre, inner, outer_px, corner = at(128, 128), at(128, 100), at(128, 16), at(6, 6)
    print('mask value (0 = look through, 255 = covered): centre %d, inside %d, outside %d, corner %d'
          % (centre, inner, outer_px, corner))
    if centre > 16 or inner > 16:
        raise RuntimeError('the opening is not open (centre %d of 255): this mask blacks out the view' % centre)
    if outer_px < 239 or corner < 239:
        raise RuntimeError('the surround is not solid (corner %d of 255): this mask surrounds nothing' % corner)
    print('scope mask ready and checked: Radius / Soft / Aspect')
finally:
    unreal.EditorAssetLibrary.delete_asset(OUT_DIR + '/' + CHECK)
