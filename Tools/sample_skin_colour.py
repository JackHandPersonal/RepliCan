"""What colour is a character's painted skin, measured off the atlas rather than guessed.

    python Tools/ue_remote.py --file Tools/sample_skin_colour.py

The cafeteria line-up wears the SciFi Space alternate palette M_PolygonSciFiSpace_02_F, and the
skin swatch in that variant is not skin-coloured at all -- the junkers came out green. Naming a
colour by eye is not good enough to turn into an Appearance entry, because the entry is a TINT
multiplied against the painted base, so the base has to be a number.

HOW. The head's UVs say which part of the atlas the face is painted from; the atlas says what
colour is there. So:

  1. Take the mesh's vertices within HEAD_RADIUS of the head bone -- the same test
     Tools/measure_face_planes.py uses, for the same reason: a radius around the skull is the
     only thing that reliably separates a face from a collar.
  2. Keep the ones on the FACE (the front percentile), and read their UVs.
  3. Synty atlases are a grid of flat swatches, so every vertex of one painted region shares a
     swatch. The UV centroid therefore lands inside that swatch rather than between two.
  4. Read the atlas at that UV.

The texture is read through a render target: a Texture2D's own pixels are not exposed to Python,
but drawing it into a render target and reading that back is, and the draw is a straight copy.
"""
import unreal, io, json, os, collections, traceback

OUT = os.path.join(unreal.Paths.project_dir(), 'Tools', 'skin_colours.json')
HEAD_RADIUS = 14.0
FRONT_PERCENTILE = 0.90
FACE_TOLERANCE = 1.5
RT_SIZE = 1024

SM = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/'
# mesh -> the atlas its material samples. The line-up is dressed by Tools/facility_layout.py in
# the 02_F alternate, which is where the green comes from.
SUBJECTS = [
    (SM + 'SK_Chr_Junker_Male_01', '/Game/PolygonSciFiSpace/Textures/Alternates/T_PolygonSciFiSpace_02_F'),
    (SM + 'SK_Chr_Junker_Female_01', '/Game/PolygonSciFiSpace/Textures/Alternates/T_PolygonSciFiSpace_02_F'),
    (SM + 'SK_Chr_Hunter_Female_01', '/Game/PolygonSciFiSpace/Textures/Alternates/T_PolygonSciFiSpace_02_F'),
    # The reference: the standard head with the pack's own first palette, which is the skin the
    # player's tints were built against.
    (SM + 'SK_Chr_SpaceSoldier_Head_Male_01', '/Game/PolygonSciFiSpace/Textures/T_PolygonSciFiSpace_01_A'),
]


def head_bone_cs(skel, bone='head'):
    pose = skel.get_reference_pose()
    if bone not in [str(b) for b in pose.get_bone_names()]:
        return None
    return unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation


def face_uv(path):
    mesh = unreal.load_asset(path)
    if not mesh:
        return None, 'missing mesh'
    head = head_bone_cs(mesh.get_editor_property('skeleton'))
    if head is None:
        return None, 'no head bone'

    dyn = unreal.DynamicMesh()
    dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())

    r2 = HEAD_RADIUS * HEAD_RADIUS
    near = []
    for i in range(dyn.get_vertex_count()):
        p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
        if not ok:
            continue
        dx, dy, dz = p.x - head.x, p.y - head.y, p.z - head.z
        if dx * dx + dy * dy + dz * dz <= r2:
            near.append((i, p))
    if not near:
        return None, 'no vertices near the head bone'

    ys = sorted(p.y for _i, p in near)
    front = ys[min(len(ys) - 1, int(len(ys) * FRONT_PERCENTILE))]
    plane = [i for i, p in near if abs(p.y - front) <= FACE_TOLERANCE]
    if not plane:
        return None, 'no face plane'

    # There is no per-vertex UV query; the whole channel comes back at once, indexed by vertex id.
    _dyn, uv_list, valid, _gaps, _split = unreal.GeometryScript_UVs.get_mesh_per_vertex_u_vs(dyn, 0)
    if not valid:
        return None, 'no UV channel 0'
    # GeometryScriptUVList is an opaque struct; it hands its contents over as an array.
    uvs = uv_list.convert_uv_list_to_array()
    # EVERY face vertex's UV, not their average. A Synty atlas is a grid of flat swatches and
    # the face touches several of them (skin, brow, mouth, collar); the centroid of those UVs
    # lands BETWEEN swatches and reads whatever happens to be there -- which is how this first
    # reported a flat grey for a face the eye plainly sees as green. Sampling each vertex and
    # taking the most common colour reads the swatch the face is actually mostly painted from.
    out = [(uvs[i].x, uvs[i].y) for i in plane if i < len(uvs)]
    if not out:
        return None, 'no UVs'
    return out, '%d face verts' % len(plane)


def atlas_pixel(tex_path, u, v, cache={}):
    """The atlas colour at a UV, read back through a render target."""
    tex = unreal.load_asset(tex_path)
    if not tex:
        return None, 'missing texture'
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    rt = cache.get(tex_path)
    if rt is None:
        # RGBA8_SRGB, not the RGBA16F default: read_render_target_uv treats an HDR target as
        # linear and an LDR one as sRGB, and an 8-bit sRGB target is exactly what the atlas is.
        rt = unreal.RenderingLibrary.create_render_target2d(
            world, RT_SIZE, RT_SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        canvas, _size, ctx = unreal.RenderingLibrary.begin_draw_canvas_to_render_target(world, rt)
        canvas.draw_texture(tex, unreal.Vector2D(0.0, 0.0), unreal.Vector2D(RT_SIZE, RT_SIZE),
                            unreal.Vector2D(0.0, 0.0), unreal.Vector2D(1.0, 1.0),
                            unreal.LinearColor(1.0, 1.0, 1.0, 1.0),
                            unreal.BlendMode.BLEND_OPAQUE)
        unreal.RenderingLibrary.end_draw_canvas_to_render_target(world, ctx)
        cache[tex_path] = rt
    c = unreal.RenderingLibrary.read_render_target_uv(world, rt, u % 1.0, v % 1.0)
    return (c.r, c.g, c.b), 'uv read'


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before sampling')

    out = {}
    for path, tex in SUBJECTS:
        name = path.split('/')[-1]
        uvs, note = face_uv(path)
        if not uvs:
            print('%-36s %s' % (name, note))
            continue
        tally = collections.Counter()
        for u, v in uvs:
            rgb, _ = atlas_pixel(tex, u, v)
            if rgb:
                tally[(int(rgb[0]), int(rgb[1]), int(rgb[2]))] += 1
        if not tally:
            print('%-36s no samples' % name)
            continue
        print('%-36s %s' % (name, note))
        for rgb, n in tally.most_common(5):
            lin = [round(srgb_to_linear(c), 4) for c in rgb]
            mx, mn = max(rgb), min(rgb)
            sat = 0.0 if mx == 0 else (mx - mn) / float(mx)
            print('     x%-3d  sRGB (%3d, %3d, %3d)  linear (%.3f, %.3f, %.3f)  sat %.2f'
                  % (n, rgb[0], rgb[1], rgb[2], lin[0], lin[1], lin[2], sat))
        # The skin is the most common SATURATED swatch: the face also touches flat greys (the
        # collar, the visor surround) and those are not what anyone means by skin tone.
        coloured = [(rgb, n) for rgb, n in tally.most_common()
                    if max(rgb) > 20 and (max(rgb) - min(rgb)) / float(max(rgb)) > 0.10]
        pick = coloured[0][0] if coloured else tally.most_common(1)[0][0]
        out[name] = {'atlas': tex, 'samples': len(uvs),
                     'skin_srgb_255': list(pick),
                     'skin_linear': [round(srgb_to_linear(c), 4) for c in pick],
                     'all': [{'srgb': list(k), 'n': v} for k, v in tally.most_common(8)]}
        print('     SKIN -> sRGB %s  linear %s' % (list(pick), out[name]['skin_linear']))

    io.open(OUT, 'w', encoding='utf-8', newline='\n').write(json.dumps(out, indent=1))
    print('wrote', OUT)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
