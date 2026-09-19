"""Is M_Spark actually bright? Measured, in its own call.

THIS IS DELIBERATELY NOT PART OF make_spark_material.py. A material recompiled earlier in the SAME
blocking Python call has not finished compiling its shader when you draw it, and it renders black or
grey -- so a verification bolted onto the end of the generator cannot tell "the emissive is
unconnected" from "the shader was not ready yet". Run the generator, then run this.

    Tools/ue_remote.py --file Tools/make_spark_material.py
    Tools/ue_remote.py --file Tools/verify_spark_material.py

Reports the rendered centre pixel and the graph's own connection state, so a failure says WHICH of
the two it is.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
PATH = '/Game/RepliCan/Materials/M_Spark'

mat = unreal.load_asset(PATH)
if not mat:
    raise SystemExit('no material at ' + PATH)

# What the graph says, independent of rendering.
print('blend_mode    :', mat.get_editor_property('blend_mode'))
print('shading_model :', mat.get_editor_property('shading_model'))
print('two_sided     :', mat.get_editor_property('two_sided'))
try:
    print('emissive connected:', MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR) is not None)
except Exception as exc:
    print('emissive connected: (could not query:', exc, ')')

exprs = mat.get_editor_property('expressions') if hasattr(mat, 'expressions') else None
print('expression count  :', len(exprs) if exprs is not None else 'n/a')

# And what it actually renders.
W = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
rt = unreal.RenderingLibrary.create_render_target2d(W, 64, 64)
unreal.RenderingLibrary.clear_render_target2d(W, rt, unreal.LinearColor(0, 0, 0, 1))
unreal.RenderingLibrary.draw_material_to_render_target(W, rt, mat)
px = unreal.RenderingLibrary.read_render_target_raw_pixel(W, rt, 32, 32)
lum = (px.r + px.g + px.b) / 3.0
print('rendered centre pixel: r=%d g=%d b=%d (mean %.1f/255)' % (px.r, px.g, px.b, lum))

if lum >= 200.0:
    print('BRIGHT -- the material is not what is making the sparks dark.')
else:
    print('DARK (mean %.1f). If the emissive reports connected above, the graph is right and the '
          'problem is what the material is drawn ON -- an additive material contributes nothing '
          'where it is composited against something that discards it.' % lum)
