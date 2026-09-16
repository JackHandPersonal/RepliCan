"""M_BloodDecal: the mark a hit leaves on a body, attached to the bone it struck (ShotReactions).
    python Tools/ue_remote.py --file Tools/make_blood_decal.py
A deferred decal over the CyberCity pack's blood stain: the stain's alpha is the shape, the
colour a dark red over it, faded at the edge of the decal box so a square never shows.
"""
import unreal, traceback
PKG = '/Game/RepliCan/Materials'; NAME = 'M_BloodDecal'
STAIN = '/Game/PolygonCyberCity/Textures/Misc/T_Stain_Blood_01'
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None: raise RuntimeError('the editor is in Play')
    MEL = unreal.MaterialEditingLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools()
    full = PKG + '/' + NAME
    mat = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    try: mat.set_editor_property('decal_blend_mode', unreal.DecalBlendMode.DBM_TRANSLUCENT)
    except Exception: pass
    def node(cls, x, y): return MEL.create_material_expression(mat, cls, x, y)
    tex = node(unreal.MaterialExpressionTextureSample, -700, 0); tex.set_editor_property('texture', unreal.load_asset(STAIN))
    tint = node(unreal.MaterialExpressionConstant3Vector, -700, 220); tint.set_editor_property('constant', unreal.LinearColor(0.22, 0.01, 0.008, 1.0))
    col = node(unreal.MaterialExpressionMultiply, -450, 60)
    MEL.connect_material_expressions(tex, 'RGB', col, 'A'); MEL.connect_material_expressions(tint, '', col, 'B')
    MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # opacity: the stain's own alpha (or its darkness where there is none), times a radial fade
    uv = node(unreal.MaterialExpressionTextureCoordinate, -900, 400)
    centre = node(unreal.MaterialExpressionConstant2Vector, -900, 520); centre.set_editor_property('r', 0.5); centre.set_editor_property('g', 0.5)
    off = node(unreal.MaterialExpressionSubtract, -720, 440); MEL.connect_material_expressions(uv, '', off, 'A'); MEL.connect_material_expressions(centre, '', off, 'B')
    dist = node(unreal.MaterialExpressionLength, -580, 440); MEL.connect_material_expressions(off, '', dist, '')
    twice = node(unreal.MaterialExpressionMultiply, -450, 440); twice.set_editor_property('const_b', 2.0); MEL.connect_material_expressions(dist, '', twice, 'A')
    inv = node(unreal.MaterialExpressionOneMinus, -320, 440); MEL.connect_material_expressions(twice, '', inv, '')
    fade = node(unreal.MaterialExpressionSaturate, -200, 440); MEL.connect_material_expressions(inv, '', fade, '')
    shaped = node(unreal.MaterialExpressionMultiply, -60, 300); MEL.connect_material_expressions(tex, 'A', shaped, 'A'); MEL.connect_material_expressions(fade, '', shaped, 'B')
    strength = node(unreal.MaterialExpressionMultiply, 80, 300); strength.set_editor_property('const_b', 0.9); MEL.connect_material_expressions(shaped, '', strength, 'A')
    MEL.connect_material_property(strength, '', unreal.MaterialProperty.MP_OPACITY)
    rough = node(unreal.MaterialExpressionConstant, -450, 640); rough.set_editor_property('r', 0.35)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    print('BLOOD DECAL', full, 'saved')
except Exception:
    print('ERROR', traceback.format_exc()[-500:])
