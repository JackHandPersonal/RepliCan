# Floor grime as decals (2026-09-17). Imports the stamps Tools/make_grime_textures writes and
# builds: M_FloorNoise, one decal per room laid over the whole floor, reading a tileable noise through
# WORLD position so every tile stops matching its neighbour at no per-actor cost; and M_Grime, a stamp
# decal with texture, tint, opacity and roughness parameters, with six instances (MI_Grime_01..06: oil,
# a dried puddle, scuffs, rust, drag marks, dust) for the layout's seeded scatter. Deferred decals cost
# by the pixels they cover, not by their count. Run in the editor through the remote-exec tool from the
# project root, after Tools/make_grime_textures has written the PNGs.
import unreal, os
RAW = 'C:/Dev/Games/RepliCan/RawArt'; TEXDIR = '/Game/RepliCan/Textures'; MATDIR = '/Game/RepliCan/Materials'
tools = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None: raise RuntimeError('the editor is in Play')

def import_tex(name, srgb=True):
    t = unreal.AssetImportTask(); t.filename = RAW + '/' + name + '.png'; t.destination_path = TEXDIR; t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(TEXDIR + '/' + name)
    if tex:
        tex.set_editor_property('srgb', srgb)
        if not srgb: tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
        eal.save_asset(TEXDIR + '/' + name)
    return tex
noise = import_tex('T_FloorNoise', srgb=False)
stamps = [import_tex('T_Grime_%02d' % k) for k in range(1, 7)]

def material(name):
    full = MATDIR + '/' + name
    m = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset(name, MATDIR, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    return m
def node(m, cls, x, y): return MEL.create_material_expression(m, cls, x, y)
def scalar(m, name, v, x, y): p = node(m, unreal.MaterialExpressionScalarParameter, x, y); p.set_editor_property('parameter_name', name); p.set_editor_property('default_value', v); return p
def finish(m):
    MEL.recompile_material(m); unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)

# ---- the whole-floor noise stain
m = material('M_FloorNoise')
wp = node(m, unreal.MaterialExpressionWorldPosition, -1300, 0)
mask = node(m, unreal.MaterialExpressionComponentMask, -1100, 0); mask.set_editor_property('r', True); mask.set_editor_property('g', True); mask.set_editor_property('b', False); mask.set_editor_property('a', False)
MEL.connect_material_expressions(wp, '', mask, '')
scale = scalar(m, 'Scale', 1.0 / 700.0, -1100, 160)   # one sheet of noise per seven metres
uv = node(m, unreal.MaterialExpressionMultiply, -900, 40)
MEL.connect_material_expressions(mask, '', uv, 'A'); MEL.connect_material_expressions(scale, '', uv, 'B')
ts = node(m, unreal.MaterialExpressionTextureSample, -700, 0); ts.set_editor_property('texture', noise); ts.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
MEL.connect_material_expressions(uv, '', ts, 'UVs')
sub = node(m, unreal.MaterialExpressionSubtract, -480, 0); MEL.connect_material_expressions(ts, 'R', sub, 'A'); MEL.connect_material_expressions(scalar(m, 'Floor', 0.42, -700, 220), '', sub, 'B')
gain = node(m, unreal.MaterialExpressionMultiply, -300, 0); MEL.connect_material_expressions(sub, '', gain, 'A'); MEL.connect_material_expressions(scalar(m, 'Strength', 2.2, -480, 220), '', gain, 'B')
sat = node(m, unreal.MaterialExpressionSaturate, -140, 0); MEL.connect_material_expressions(gain, '', sat, '')
op = node(m, unreal.MaterialExpressionMultiply, 0, 0); MEL.connect_material_expressions(sat, '', op, 'A'); MEL.connect_material_expressions(scalar(m, 'Opacity', 0.55, -140, 220), '', op, 'B')
MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
col = node(m, unreal.MaterialExpressionVectorParameter, -300, 380); col.set_editor_property('parameter_name', 'Colour'); col.set_editor_property('default_value', unreal.LinearColor(0.035, 0.032, 0.028, 1.0))
MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(scalar(m, 'Roughness', 0.85, -300, 560), '', unreal.MaterialProperty.MP_ROUGHNESS)
finish(m); print('M_FloorNoise saved')

# ---- the stamp
m = material('M_Grime')
tp = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0); tp.set_editor_property('parameter_name', 'Tex'); tp.set_editor_property('texture', stamps[0])
tint = node(m, unreal.MaterialExpressionVectorParameter, -900, 300); tint.set_editor_property('parameter_name', 'Tint'); tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
bc = node(m, unreal.MaterialExpressionMultiply, -600, 100); MEL.connect_material_expressions(tp, 'RGB', bc, 'A'); MEL.connect_material_expressions(tint, '', bc, 'B')
MEL.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
op = node(m, unreal.MaterialExpressionMultiply, -600, 400); MEL.connect_material_expressions(tp, 'A', op, 'A'); MEL.connect_material_expressions(scalar(m, 'Opacity', 0.9, -900, 500), '', op, 'B')
MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
MEL.connect_material_property(scalar(m, 'Roughness', 0.8, -600, 600), '', unreal.MaterialProperty.MP_ROUGHNESS)
finish(m); print('M_Grime saved')

# ---- the six stamps as instances: texture, tint, opacity, roughness (oil is wet)
KINDS = [('MI_Grime_01', 0, (1.0, 1.0, 1.0), 0.95, 0.25), ('MI_Grime_02', 1, (1.0, 1.0, 1.0), 0.8, 0.85), ('MI_Grime_03', 2, (1.0, 1.0, 1.0), 0.7, 0.9),
         ('MI_Grime_04', 3, (1.0, 0.95, 0.9), 0.9, 0.95), ('MI_Grime_05', 4, (1.0, 1.0, 1.0), 0.75, 0.9), ('MI_Grime_06', 5, (1.0, 1.0, 1.0), 0.6, 1.0)]
for name, k, tint_v, opac, rough in KINDS:
    p = MATDIR + '/' + name
    mi = unreal.load_asset(p) if eal.does_asset_exist(p) else tools.create_asset(name, MATDIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', m)
    MEL.set_material_instance_texture_parameter_value(mi, 'Tex', stamps[k])
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(tint_v[0], tint_v[1], tint_v[2], 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Opacity', opac)
    MEL.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
    MEL.update_material_instance(mi)
    unreal.EditorLoadingAndSavingUtils.save_packages([mi.get_outermost()], False)
print('grime instances saved:', all(os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', n + '.uasset')) for n, _, _, _, _ in KINDS))
