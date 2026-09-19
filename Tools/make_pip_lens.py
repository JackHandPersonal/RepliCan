"""M_ScopePiP: the lens of a picture-in-picture optic, showing a scene capture.

THE OTHER WAY TO RENDER A MAGNIFIED SIGHT. Our scopes narrow the world's field of view and mask
everything outside a circle, so the whole screen becomes the sight picture and peripheral vision is
given up. Picture-in-picture instead puts a SceneCapture2D at the eye, renders the world into a
target at a narrow field of view, and lets the glass sample it: the magnified picture lives inside
the lens, the rest of the screen stays at normal FOV, and you keep your peripheral vision. It costs
a second scene render every frame that the sight is up.

HOW THE PICTURE IS AIMED. The capture looks along the VIEW, so the point of aim is the exact centre
of the render target. The glass samples that target by its OWN UVs, centred -- so the point of aim
lands at the centre of the glass, and the reticle is drawn at the centre of the glass too. The two
cannot disagree, because they are the same point by construction.

This was first built sampling by SCREEN position instead, reasoning that the picture would then
register with the world behind it. That reasoning was wrong: at any magnification above 1x the
magnified picture CANNOT line up with the 1x world around it, so the registration being bought did
not exist -- while the cost was real, because the aim point then sat at the centre of the screen
while the reticle sat at the centre of the glass, and those coincide only if the glass happens to be
screen-centred. Measured at full ADS on the marksman rifle, they were 89 px apart and every shot
landed high.

The picture is aspect-corrected off the view's own size, so a square-ish piece of glass shows an
undistorted window rather than a squashed one.

OPAQUE, not translucent: the captured picture replaces what is behind the glass. That is what makes
it read as a sight rather than as a screen stuck on the gun.

  Tools/ue_remote --file Tools/make_pip_lens
"""
import unreal

MEL = unreal.MaterialEditingLibrary
OUT_DIR = '/Game/RepliCan/Materials'
NAME = 'M_ScopePiP'


def link(a, ao, b, bi):
    if not MEL.connect_material_expressions(a, ao, b, bi):
        raise RuntimeError('%s -> %s.%s did not connect; its pins are %s'
                           % (a.get_class().get_name(), b.get_class().get_name(), bi,
                              [str(n) for n in MEL.get_material_expression_input_names(b)]))


def link_prop(a, ao, prop):
    if not MEL.connect_material_property(a, ao, prop):
        raise RuntimeError('%s -> %s did not connect' % (a.get_class().get_name(), prop))


path = OUT_DIR + '/' + NAME
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, OUT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    print('created ' + path)
else:
    print('rebuilding ' + path)

MEL.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
# SINGLE-SIDED. Tools/add_pip_lens_face.py gives the scope a lens face that already points at the
# eye, so nothing here needs to be drawn from behind. Two-sided was tried and drew the old glass
# disc as a big circle laid over the housing, which is itself single-sided and culled from inside
# the shroud.
mat.set_editor_property('two_sided', False)


def node(cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def scalar(name, default, x, y):
    s = node(unreal.MaterialExpressionScalarParameter, x, y)
    s.set_editor_property('parameter_name', name)
    s.set_editor_property('default_value', default)
    return s


def chan(src, x, y, r, g):
    m = node(unreal.MaterialExpressionComponentMask, x, y)
    link(src, '', m, '')
    m.set_editor_property('r', r)
    m.set_editor_property('g', g)
    m.set_editor_property('b', False)
    m.set_editor_property('a', False)
    return m


# ---- THE LENS'S OWN UVs, centred. Tools/add_pip_lens_face.py projects them straight down the bore
# and normalises both axes by the same radius, so this is isotropic: a circle in UV is a circle on
# the glass, and the reticle below needs no aspect correction of its own.
uv = node(unreal.MaterialExpressionTextureCoordinate, -1200, 0)
centred = node(unreal.MaterialExpressionSubtract, -1040, 0)
link(uv, '', centred, 'A')
centred.set_editor_property('const_b', 0.5)

# ---- THE PICTURE, sampled about the target's centre, which is the point of aim.
# The render target is viewport-shaped; the glass is not. Scaling u by 1/aspect keeps the window
# undistorted instead of stretching it across the wider axis.
viewsize = node(unreal.MaterialExpressionViewProperty, -1200, 300)
viewsize.set_editor_property('property', unreal.MaterialExposedViewProperty.MEVP_VIEW_SIZE)
aspect = node(unreal.MaterialExpressionDivide, -1000, 300)
link(chan(viewsize, -1100, 250, True, False), '', aspect, 'A')     # width
link(chan(viewsize, -1100, 370, False, True), '', aspect, 'B')     # height

inv_aspect = node(unreal.MaterialExpressionDivide, -840, 300)
inv_aspect.set_editor_property('const_a', 1.0)
link(aspect, '', inv_aspect, 'B')

one = node(unreal.MaterialExpressionConstant, -840, 400)
one.set_editor_property('r', 1.0)
aspect_vec = node(unreal.MaterialExpressionAppendVector, -700, 340)
link(inv_aspect, '', aspect_vec, 'A')
link(one, '', aspect_vec, 'B')

# How much of the captured picture the glass shows. 1.0 fills the glass with the target's full
# height; smaller crops in (more magnification), larger shows more of it.
fill = scalar('PiPFill', 1.0, -1000, 470)
scaled = node(unreal.MaterialExpressionMultiply, -540, 160)
link(centred, '', scaled, 'A')
link(aspect_vec, '', scaled, 'B')
zoomed = node(unreal.MaterialExpressionMultiply, -400, 160)
link(scaled, '', zoomed, 'A')
link(fill, '', zoomed, 'B')
rt_uv = node(unreal.MaterialExpressionAdd, -260, 160)
link(zoomed, '', rt_uv, 'A')
rt_uv.set_editor_property('const_b', 0.5)

# The render target the capture writes. Set per optic on a dynamic instance at equip time; the
# default is null, which samples black, so a mis-wired optic reads as a dead sight rather than as
# whatever texture happened to be lying around.
tex = node(unreal.MaterialExpressionTextureObjectParameter, -260, 380)
tex.set_editor_property('parameter_name', 'Scene')
sample = node(unreal.MaterialExpressionTextureSample, -60, 220)
link(tex, '', sample, 'Tex')
link(rt_uv, '', sample, 'UVs')

tint = node(unreal.MaterialExpressionVectorParameter, -60, 440)
tint.set_editor_property('parameter_name', 'GlassTint')
tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
shaded = node(unreal.MaterialExpressionMultiply, 160, 300)
link(sample, 'RGB', shaded, 'A')
link(tint, '', shaded, 'B')

# ---- THE RETICLE, at the centre of the glass -- which is the point of aim, because the picture is
# centred on it. Sizes are fractions of the glass's half-width.
absc = node(unreal.MaterialExpressionAbs, -900, 620)
link(centred, '', absc, '')

soft = scalar('ReticleSoft', 0.004, -1200, 1020)


def band(value, width, x, y):
    """1 where `value` is inside `width` of zero, falling off over `soft`."""
    hi = node(unreal.MaterialExpressionAdd, x - 170, y + 80)
    link(width, '', hi, 'A')
    link(soft, '', hi, 'B')
    ss = node(unreal.MaterialExpressionSmoothStep, x, y)
    link(width, '', ss, 'Min')
    link(hi, '', ss, 'Max')
    link(value, '', ss, 'Value')
    inv = node(unreal.MaterialExpressionOneMinus, x + 170, y)
    link(ss, '', inv, '')
    return inv


thick = scalar('ReticleThickness', 0.004, -1200, 760)
barv = band(chan(absc, -740, 580, True, False), thick, -480, 580)    # |u| small: vertical arm
barh = band(chan(absc, -740, 760, False, True), thick, -480, 760)    # |v| small: horizontal arm
cross = node(unreal.MaterialExpressionMax, -220, 660)
link(barv, '', cross, 'A')
link(barh, '', cross, 'B')

dist = node(unreal.MaterialExpressionLength, -900, 920)   # THE PIN THAT IS NOT CALLED 'Input'
link(centred, '', dist, '')

# The arms stop short of the rim and short of the middle, so the mark reads as a crosshair rather
# than two lines ruled across the picture, and the aiming point is not covered by its own reticle.
span = band(dist, scalar('ReticleSpan', 0.30, -1200, 900), -480, 920)
gapr = scalar('ReticleGap', 0.02, -1200, 1160)
gaphi = node(unreal.MaterialExpressionAdd, -660, 1140)
link(gapr, '', gaphi, 'A')
link(soft, '', gaphi, 'B')
gap = node(unreal.MaterialExpressionSmoothStep, -480, 1100)
link(gapr, '', gap, 'Min')
link(gaphi, '', gap, 'Max')
link(dist, '', gap, 'Value')

arms = node(unreal.MaterialExpressionMultiply, -60, 720)
link(cross, '', arms, 'A')
link(span, '', arms, 'B')
armsg = node(unreal.MaterialExpressionMultiply, -60, 860)
link(arms, '', armsg, 'A')
link(gap, '', armsg, 'B')

centre_dot = band(dist, scalar('ReticleDot', 0.006, -1200, 1280), -480, 1280)
mark = node(unreal.MaterialExpressionMax, 160, 800)
link(armsg, '', mark, 'A')
link(centre_dot, '', mark, 'B')

colour = node(unreal.MaterialExpressionVectorParameter, 160, 1000)
colour.set_editor_property('parameter_name', 'ReticleColour')
colour.set_editor_property('default_value', unreal.LinearColor(0.45, 1.0, 0.65, 1.0))

final = node(unreal.MaterialExpressionLinearInterpolate, 420, 500)
link(shaded, '', final, 'A')
link(colour, '', final, 'B')
link(mark, '', final, 'Alpha')

link_prop(final, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(mat)
unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outer()], False)

print('scalar params:', [str(n) for n in MEL.get_scalar_parameter_names(mat)])
print('vector params:', [str(n) for n in MEL.get_vector_parameter_names(mat)])
print('texture params:', [str(n) for n in MEL.get_texture_parameter_names(mat)])
node_in = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
print('emissive <-', node_in.get_class().get_name() if node_in else 'NOTHING CONNECTED')
print('M_ScopePiP ready: set its "Scene" texture to the optic capture target')
