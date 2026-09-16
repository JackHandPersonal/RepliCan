"""Materials the environment director needs but the packs do not provide.

    python Tools/ue_remote.py --file Tools/make_env_materials.py

M_DotMatrix: the door signs' face. The lamp grid is BAKED into the canvas texture by ASignActor
now, so this material only has to show it: no Cols/Rows reconstruction, which is what dropped
columns out of the font and lost characters. Emissive only, unlit, so the sign glows in a dark
corridor without being lit by anything.

M_DustMote: an unlit translucent card for the motes in the air. Unlit so the fog and the lamps
do not light it (a mote is a hint, not geometry), translucent so it reads as a speck rather than
a white square, and deliberately dim: dust you notice is dust that is too strong.
"""
import unreal, traceback, io
try:
    MEL = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = '/Game/RepliCan/Materials/M_DustMote'
    # Reuse rather than delete-and-recreate: deleting an asset and making one at the same path
    # in the same tick leaves create_asset returning None, which is how the first attempt failed.
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not m:
        m = tools.create_asset('M_DustMote', '/Game/RepliCan/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if not m:
        raise RuntimeError('could not create or load ' + path)
    # Start from a clean graph so a re-run does not stack duplicate nodes on the old ones.
    for e in list(unreal.MaterialEditingLibrary.get_used_material_expressions(m) if hasattr(unreal.MaterialEditingLibrary, 'get_used_material_expressions') else []):
        try: unreal.MaterialEditingLibrary.delete_material_expression(m, e)
        except Exception: pass
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', True)
    col = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, -100)
    col.set_editor_property('constant', unreal.LinearColor(0.72, 0.80, 0.78, 1.0))
    MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Opacity falls off from the middle of the card, so a mote is a soft speck not a hard chip.
    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 120)
    sub = MEL.create_material_expression(m, unreal.MaterialExpressionSubtract, -700, 120)
    sub.set_editor_property('const_b', 0.5)   # scalar, broadcast across U and V: centres the coordinates
    MEL.connect_material_expressions(tex, '', sub, 'A')
    length = MEL.create_material_expression(m, unreal.MaterialExpressionLength, -550, 120)
    MEL.connect_material_expressions(sub, '', length, '')
    one_minus = MEL.create_material_expression(m, unreal.MaterialExpressionOneMinus, -420, 120)
    MEL.connect_material_expressions(length, '', one_minus, '')
    power = MEL.create_material_expression(m, unreal.MaterialExpressionPower, -300, 120)
    power.set_editor_property('const_exponent', 3.0)
    MEL.connect_material_expressions(one_minus, '', power, 'Base')
    scale = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -180, 120)
    scale.set_editor_property('const_b', 0.30)
    MEL.connect_material_expressions(power, '', scale, 'A')
    MEL.connect_material_property(scale, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    eal.save_asset(path)
    print('WROTE', path)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written')

# ---- M_DotMatrix -----------------------------------------------------------
try:
    MEL = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = '/Game/RepliCan/Materials/M_DotMatrix'
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not m:
        m = tools.create_asset('M_DotMatrix', '/Game/RepliCan/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if not m:
        raise RuntimeError('could not create or load ' + path)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided', False)

    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, 0)
    tex.set_editor_property('parameter_name', 'Text')
    col = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, 220)
    col.set_editor_property('parameter_name', 'Color')
    col.set_editor_property('default_value', unreal.LinearColor(0.35, 1.0, 0.45, 1.0))
    inten = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, 380)
    inten.set_editor_property('parameter_name', 'Intensity')
    inten.set_editor_property('default_value', 6.0)

    # Emissive = the baked face, tinted and driven. The face already carries its own dark grid,
    # so the unlit lamps come through as near black rather than as holes.
    mul1 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 60)
    MEL.connect_material_expressions(tex, 'R', mul1, 'A')
    MEL.connect_material_expressions(col, '', mul1, 'B')
    mul2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 60)
    MEL.connect_material_expressions(mul1, '', mul2, 'A')
    MEL.connect_material_expressions(inten, '', mul2, 'B')
    MEL.connect_material_property(mul2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    eal.save_asset(path)
    print('WROTE', path)
except Exception:
    import traceback as _tb
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(_tb.format_exc()); print('ERROR written (dotmatrix)')
