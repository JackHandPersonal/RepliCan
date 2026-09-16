"""The scorch mark a round leaves on a wall. Creates /Game/RepliCan/Materials/M_ImpactScorch.

    python Tools/ue_remote.py --file Tools/make_impact_materials.py

A deferred decal, built from maths rather than a texture: the station has no bullet-hole art
and one radial falloff looks better than a placeholder photograph would. What it draws is a
dark core that fades to nothing at the edge, so the mark reads as burnt plate rather than as a
sticker -- decals with a hard edge are the single most obvious tell that a hit is a decal.

  opacity  = (1 - r)^Sharpness, r being distance from the decal's centre in UV space
  colour   = near black at the centre, warming slightly toward the rim, like heat staining

Run once. Re-running rebuilds it, which is harmless.

MATERIAL COMPILE TIMING: a material that has never been used renders black or grey on its first
frame. The first shot after this script runs may show nothing; the second will be right. That is
the same trap the icon backdrop and the palette swatches both hit -- it is not a bug in the
decal.
"""
import unreal, traceback, io

PKG = '/Game/RepliCan/Materials'
NAME = 'M_ImpactScorch'
SHARPNESS = 1.9     # higher = tighter core, softer rim
CORE = (0.03, 0.025, 0.02)
RIM = (0.18, 0.10, 0.05)

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before creating assets')

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    full = '%s/%s' % (PKG, NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    mat = tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError('could not create ' + full)

    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        mat.set_editor_property('decal_blend_mode', unreal.DecalBlendMode.DBM_TRANSLUCENT)
    except Exception:
        pass

    def node(cls, x, y):
        return unreal.MaterialEditingLibrary.create_material_expression(mat, cls, x, y)

    # r = distance from the middle of the decal, 0 at the centre and 1 at the corners.
    uv = node(unreal.MaterialExpressionTextureCoordinate, -900, 0)
    centre = node(unreal.MaterialExpressionConstant2Vector, -900, 160)
    centre.set_editor_property('r', 0.5)
    centre.set_editor_property('g', 0.5)
    offset = node(unreal.MaterialExpressionSubtract, -700, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv, '', offset, 'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(centre, '', offset, 'B')

    dist = node(unreal.MaterialExpressionLength, -540, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(offset, '', dist, '')
    # The UV square's half-width is 0.5, so double it to get 0..1 across the decal.
    twice = node(unreal.MaterialExpressionMultiply, -400, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(dist, '', twice, 'A')
    twice.set_editor_property('const_b', 2.0)

    inv = node(unreal.MaterialExpressionOneMinus, -260, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(twice, '', inv, '')
    clamped = node(unreal.MaterialExpressionClamp, -140, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(inv, '', clamped, '')

    falloff = node(unreal.MaterialExpressionPower, 0, 40)
    unreal.MaterialEditingLibrary.connect_material_expressions(clamped, '', falloff, 'Base')
    falloff.set_editor_property('const_exponent', SHARPNESS)

    # Colour: black core warming to a faint burn at the rim, driven by the same falloff.
    core = node(unreal.MaterialExpressionConstant3Vector, -400, 300)
    core.set_editor_property('constant', unreal.LinearColor(*CORE, 1.0))
    rim = node(unreal.MaterialExpressionConstant3Vector, -400, 420)
    rim.set_editor_property('constant', unreal.LinearColor(*RIM, 1.0))
    blend = node(unreal.MaterialExpressionLinearInterpolate, -160, 340)
    unreal.MaterialEditingLibrary.connect_material_expressions(rim, '', blend, 'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(core, '', blend, 'B')
    unreal.MaterialEditingLibrary.connect_material_expressions(falloff, '', blend, 'Alpha')

    unreal.MaterialEditingLibrary.connect_material_property(blend, '', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(falloff, '', unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    print('SCORCH DECAL', full)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
