"""A material for sparks, instead of borrowing the laser beam's.

The spark motes were drawn with M_LaserBeam, whose opacity is 1 - Fresnel. That is a sensible soft
edge for a BEAM: a cylinder seen from the side presents a wide band of surface facing the camera, so
most of it is opaque and only the silhouette fades. A SPHERE presents one point facing the camera
and curves away from it everywhere else, so most of a three-centimetre mote sits at a grazing angle
where Fresnel is high and 1 - Fresnel is nearly zero. On an additive material opacity scales the
whole contribution, so the motes were being faded out almost entirely -- and no amount of colour or
Heat could put back what the opacity was taking away. That is why they read as dark flecks.

This is the same idea with the fade removed: unlit, additive, emissive = Colour x Heat, opacity flat.
A burning speck has no silhouette to soften; it is light, and light does not have edges.

  Tools/ue_remote --file Tools/make_spark_material
"""
import unreal

MEL = unreal.MaterialEditingLibrary
OUT_DIR = '/Game/RepliCan/Materials'
NAME = 'M_Spark'

path = OUT_DIR + '/' + NAME
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, OUT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    print('created ' + path)
else:
    print('rebuilding ' + path)

mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)

# THE FLAG THAT WAS ACTUALLY MAKING THE SPARKS DARK, and it is not in the graph at all.
#
# A burst is drawn as instances in two UInstancedStaticMeshComponents (SparkFx.cpp). A material can
# only be used on one if it declares that usage: without it the renderer does not fail, it quietly
# substitutes the DEFAULT GREY LIT MATERIAL. So every mote was being drawn correctly, at the right
# size, in the right place -- in dull grey. Measured on this asset 2026-09-19:
# used_with_instanced_static_meshes was False.
#
# This is why the earlier fixes did not help. The Fresnel/opacity rewrite and the mote-size fix were
# both real, and both were improving a material that was never the one on screen. Nothing warns:
# not the log, not the material editor, not the asset.
mat.set_editor_property('used_with_instanced_static_meshes', True)


def node(cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


colour = node(unreal.MaterialExpressionVectorParameter, -600, 0)
colour.set_editor_property('parameter_name', 'Colour')
colour.set_editor_property('default_value', unreal.LinearColor(1.6, 3.4, 7.5, 1.0))
heat = node(unreal.MaterialExpressionScalarParameter, -600, 200)
heat.set_editor_property('parameter_name', 'Heat')
heat.set_editor_property('default_value', 16.0)

# EVERY CONNECTION IS CHECKED. connect_material_expressions and connect_material_property return
# False on a wrong or unrecognised pin name and raise NOTHING, so a generator that ignores the
# return value writes a material with an UNCONNECTED emissive -- which is black -- and then prints
# that it succeeded. That is exactly how M_ScopeMask shipped a black scope: the same two calls, the
# same unchecked booleans. A spark material with no emissive is a dark speck, which is the bug this
# file exists to fix, so it must not be able to leave that way again.
def wire(src, src_pin, dst, dst_pin):
    if not MEL.connect_material_expressions(src, src_pin, dst, dst_pin):
        raise RuntimeError('connect FAILED: %s[%s] -> %s[%s] -- check the pin name'
                           % (src.get_class().get_name(), src_pin or '(out)',
                              dst.get_class().get_name(), dst_pin or '(out)'))


def wire_prop(src, prop, label):
    if not MEL.connect_material_property(src, '', prop):
        raise RuntimeError('connect FAILED: %s -> %s' % (src.get_class().get_name(), label))


glow = node(unreal.MaterialExpressionMultiply, -340, 60)
wire(colour, '', glow, 'A')
wire(heat, '', glow, 'B')
wire_prop(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR, 'Emissive')

# Flat. NOT a Fresnel fade -- see the note above; that is what was eating the sparks.
full = node(unreal.MaterialExpressionConstant, -340, 320)
full.set_editor_property('r', 1.0)
wire_prop(full, unreal.MaterialProperty.MP_OPACITY, 'Opacity')

MEL.recompile_material(mat)
unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outer()], False)

# WHAT IS *NOT* CHECKED HERE, and why.
#
# This used to render the material to a target and assert the centre pixel was bright. That check
# was wrong twice over and failed on a material that was correct: draw_material_to_render_target
# goes through Canvas, which does not composite an ADDITIVE material the way the scene does, and a
# material recompiled earlier in this same blocking call has not finished its shader anyway. It
# reported mean 1.0/255 for a graph whose emissive was verifiably connected. A check that fails on
# a correct input is worse than no check: it sends the next person looking at the graph.
#
# What IS worth asserting is above -- every connection, and the usage flag -- because those are the
# two things that fail silently and leave a material that looks right in the editor.
if not mat.get_editor_property('used_with_instanced_static_meshes'):
    raise RuntimeError('used_with_instanced_static_meshes did not stick -- the sparks will draw in '
                       'the default grey material however the graph looks.')
print('spark material ready: unlit, additive, no fresnel fade, ISM usage set')
