"""A crack that can be drawn ON GLASS.

A bullet hole on a wall is a deferred decal, projected onto the GBuffer. A TRANSLUCENT material
never writes to the GBuffer, so a decal on glass spawns and then cannot draw -- which is why
shooting the facility windows left no mark at all. Measured: the window blocks the shot
(profile BlockAll, Visibility ECR_BLOCK) and the default impact rule did stamp a hole; it was simply
invisible, because M_PolygonSciFiSpace_Glass_01 is BLEND_TRANSLUCENT.

So the crack has to be a SURFACE, not a decal: this builds a small unlit translucent material that
gets drawn on a plane at the point of impact. Unlit because a crack in glass is a scatter of light,
not a lit surface, and lighting it would make it vanish in a dark room -- which is where most of
this map is. Synty's own MI_Generic_Decal_Crack_* cannot be reused: they are decal-domain materials
and will not apply to a mesh.

  Tools/ue_remote --file Tools/make_glass_crack
"""
import unreal

OUT_DIR = '/Game/RepliCan/Materials'
NAME = 'M_GlassCrack'
TEX = '/Game/Synty/PolygonGeneric/Textures/Decals/T_Generic_Decal_Crack_01'
MEL = unreal.MaterialEditingLibrary

path = '%s/%s' % (OUT_DIR, NAME)
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, OUT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    print('created %s' % path)
else:
    print('rebuilding %s' % path)

mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)

tex = unreal.load_asset(TEX)
if not tex:
    raise SystemExit('crack texture missing: %s' % TEX)

sample = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -640, 0)
sample.set_editor_property('texture', tex)
tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -640, 260)
tint.set_editor_property('parameter_name', 'CrackColour')
tint.set_editor_property('default_value', unreal.LinearColor(0.82, 0.92, 1.0, 1.0))   # cold, like the glass
mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -360, 60)
MEL.connect_material_expressions(sample, 'RGB', mul, 'A')
MEL.connect_material_expressions(tint, '', mul, 'B')
MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

# The texture alpha IS the crack: opaque where the glass is broken, clear everywhere else. A scalar
# lets the whole mark be faded without touching the artwork.
strength = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -640, 480)
strength.set_editor_property('parameter_name', 'CrackStrength')
strength.set_editor_property('default_value', 1.0)
omul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -360, 420)
MEL.connect_material_expressions(sample, 'A', omul, 'A')
MEL.connect_material_expressions(strength, '', omul, 'B')
MEL.connect_material_property(omul, '', unreal.MaterialProperty.MP_OPACITY)

MEL.recompile_material(mat)
unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outer()], False)
print('glass crack material ready: unlit, translucent, two-sided')
