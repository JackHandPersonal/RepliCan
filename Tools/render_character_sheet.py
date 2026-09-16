"""Contact sheet of every Synty sci-fi character mesh in the project.

    python Tools/ue_remote.py --file Tools/render_character_sheet.py
    powershell Tools/character_sheet.ps1

Renders one front-on shot per character into RawArt/Chars/<AssetName>.png, using the same booth
the weapon icons use. Tools/character_sheet.ps1 then tiles them with the asset name printed under
each, which is the point: picking a Synty character off a wall of unlabelled thumbnails is a
guessing game, and a labelled grid turns it into reading a name.

Skips the Epic mannequins and the attachment-only pieces, which are not characters in their own
right.
"""
import unreal, io, os, traceback
OUT = r'C:\Dev\Games\RepliCan\RawArt\Chars'
SIZE = 512
# Characters are an order of magnitude bigger than a weapon, and the rig scales with the
# subject, so the same lamps land far less light on them. Calibrated separately here.
LIGHT_SCALE = 0.85
SKIP = ('SK_Mannequin', 'SK_Chr_Attach_')
# Limit to one pack when reviewing a single library; '' renders every sci-fi pack.
PACK = '/PolygonSciFiSpace'
# How much of the figure's height the frame covers, measured down from the crown.
HEAD_FRACTION = 0.34
# The rig is built for a figure this tall whatever the subject actually is.
NOMINAL_HEIGHT = 180.0
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before rendering')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    w = ues.get_editor_world()
    Y0 = -60000.0
    for act in eas.get_all_level_actors():
        try:
            if abs(act.get_actor_location().y - Y0) < 4000.0: eas.destroy_actor(act)
        except Exception:
            pass

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.wait_for_completion()
    paths = []
    for a in ar.get_all_assets():
        p = str(a.package_name)
        cls = str(a.asset_class_path.asset_name) if hasattr(a, 'asset_class_path') else ''
        if cls != 'SkeletalMesh': continue
        if (PACK or '/PolygonSciFi') not in p: continue
        name = p.rsplit('/', 1)[-1]
        # Characters only: the packs keep animated props (turret bases and the like) as skeletal
        # meshes in the same folders, and they are not what anyone is browsing for.
        if not name.startswith('SK_Chr_'): continue
        if any(s in name for s in SKIP): continue
        paths.append(p)
    paths = sorted(set(paths))
    print('CHARACTERS', len(paths))

    black = unreal.load_asset('/Game/RepliCan/Materials/M_IconBackBlack')
    os.makedirs(OUT, exist_ok=True)
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    rt = unreal.RenderingLibrary.create_render_target2d(w, SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)

    done = 0
    for p in paths:
        mesh = unreal.load_asset(p)
        if not mesh: print('MISSING', p); continue
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))   # face the lens, not the back of the head
        if not a: print('NOSPAWN', p); continue
        o, ext = a.get_actor_bounds(False)
        a.add_actor_world_offset(unreal.Vector(0.0, Y0, 0.0) - o, False, False)
        o, ext = a.get_actor_bounds(False)
        # Head and chest only. A whole figure at thumbnail size is a silhouette and every Synty
        # character has much the same one; the face, the helmet and the collar are what tell
        # them apart, so the frame is the top of the figure rather than all of it.
        total_h = max(1.0, 2.0 * ext.z)
        span = max(total_h * HEAD_FRACTION, 2.0 * ext.y * 0.80)
        frame_z = ext.z - total_h * HEAD_FRACTION * 0.5
        # The lighting rig is held at a fixed size rather than scaled to the subject, so a
        # head-only mesh is lit exactly like a full figure instead of sitting on top of the
        # lamps and blowing out, which is what happened to the two SpaceSoldier heads.
        L = NOMINAL_HEIGHT / 100.0
        back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(120.0 * L, Y0, frame_z), unreal.Rotator(roll=0, pitch=90, yaw=0))
        back.set_actor_scale3d(unreal.Vector(span / 20.0, span / 20.0, 1.0))
        back.static_mesh_component.set_material(0, black)
        lights = []
        for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
            l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * L, Y0 + y * L, frame_z + z * L)); lc = l.light_component
            lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i * LIGHT_SCALE); lc.set_attenuation_radius(600 * L); lights.append(l)
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-260.0 * L, Y0, frame_z), unreal.Rotator(roll=0, pitch=0, yaw=0))
        c = cap.get_editor_property('capture_component2d'); c.texture_target = rt
        c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        c.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC
        c.ortho_width = span
        c.set_editor_property('capture_every_frame', False)
        for _ in range(3): c.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s.png' % p.rsplit('/', 1)[-1])
        for x in [cap, back, a] + lights: eas.destroy_actor(x)
        done += 1
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('RENDERED', done)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written')
