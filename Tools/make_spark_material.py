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


def node(cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


colour = node(unreal.MaterialExpressionVectorParameter, -600, 0)
colour.set_editor_property('parameter_name', 'Colour')
colour.set_editor_property('default_value', unreal.LinearColor(1.6, 3.4, 7.5, 1.0))
heat = node(unreal.MaterialExpressionScalarParameter, -600, 200)
heat.set_editor_property('parameter_name', 'Heat')
heat.set_editor_property('default_value', 16.0)

glow = node(unreal.MaterialExpressionMultiply, -340, 60)
MEL.connect_material_expressions(colour, '', glow, 'A')
MEL.connect_material_expressions(heat, '', glow, 'B')
MEL.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

# Flat. NOT a Fresnel fade -- see the note above; that is what was eating the sparks.
full = node(unreal.MaterialExpressionConstant, -340, 320)
full.set_editor_property('r', 1.0)
MEL.connect_material_property(full, '', unreal.MaterialProperty.MP_OPACITY)

MEL.recompile_material(mat)
unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outer()], False)
print('spark material ready: unlit, additive, no fresnel fade')
