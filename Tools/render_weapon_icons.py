"""Icon renders for every baked weapon (the Reference screen's cards and the inventory squares).

One shot per weapon, over a flat black backdrop -- the panels these sit on are black anyway, so
the icons need no alpha cutout and no matte pass. Driven from UI/Weapons.json, so the icon names
match what the cards look up (T_Icon_<icon>, icon = "<Pack>_<asset>").

    python Tools/ue_remote.py --file Tools/render_weapon_icons.py
    python Tools/ue_remote.py --file Tools/import_icons.py

Two things this guards against, both of which showed up in the shipped icons:
  * Staging actors are spawned far out at Y0 and destroyed at the end of each item. If a run is
    interrupted the leftovers stay in the level, and the NEXT run's shot then contains that
    weapon as well as its own -- which is why several icons showed two guns. Every run now
    sweeps the staging area clean before it starts.
  * The capture wrote tone-mapped sRGB bytes into a linear-tagged RGBA8 target, so everything was
    encoded to gamma twice and came out washed out. The target is sRGB now and the source is the
    finished LDR image, so the two agree.
  * The booth was badly overlit, which is what actually drained the colour out of the icons:
    clipped highlights carry no hue at all. Exposure overrides on a scene capture do nothing
    here, so the lights are the only control. Measured on the launcher, at the old intensity a
    fifth of the lit pixels were clipped to white and saturation was 0.17; at the intensity used
    now it is 0.65. LIGHT_SCALE is that control.
  * The backdrop material's SHADER has to be compiled before the batch runs, or every plane in
    the batch draws the engine's grey checker instead. Shader compilation is asynchronous and its
    results are collected on the game thread between ticks -- which never come while this script
    holds the thread. So run Tools/warm_icon_backdrop.py first (it parks a plane wearing the
    material in the level), give the editor half a minute, then run this; the warm-up script's
    second form removes the plane again. Measured: the same batch, grey with a cold shader and
    black with a warm one.

Set ONLY to a list of icon keys to re-render a few; FORCE re-renders ones already on disk.
"""
import unreal, re, json, io, os, traceback
ONLY = []
FORCE = True
SIZE = 1024
# No exposure override: sweeps from EV -2 to +12 came out pixel-identical, so the three lights
# below are the only brightness control that works here. Measured on the launcher:
#   scale 1.00 -> mean 193, saturation 0.17, 20% of lit pixels clipped to white
#   scale 0.25 -> mean 142, saturation 0.43
#   scale 0.60 -> mean 172, saturation 0.26
#   scale 0.40 -> mean 162, saturation 0.33
#   scale 0.25 -> mean 144, saturation 0.44   <- used
# Measured against THIS rig, with the lamps scaled to the piece. Numbers taken against any other
# arrangement do not transfer: the lamp distances move with the subject.
LIGHT_SCALE = 0.25
# A weapon much longer than it is tall wastes most of a square icon. Turning it corner to corner
# uses the diagonal instead, which is about 1.4 times the side. Applied when the piece is at
# least this much wider than tall on screen.
DIAGONAL_RATIO = 1.5
DIAGONAL_DEGREES = 45.0
# Room left around the piece once it has been fitted.
FIT_MARGIN = 1.10
OUT = r'C:\Dev\Games\RepliCan\RawArt\Icons'
cat = json.load(io.open(r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json', encoding='utf-8'))['weapons']
def icon_name(item): return 'T_Icon_' + re.sub(r'[^A-Za-z0-9]+', '_', item).strip('_')
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        # spawn_actor_from_object returns None during Play, which used to leave a half-finished
        # batch and a stale error file rather than an obvious failure.
        raise RuntimeError('the editor is in Play; stop the session before rendering icons')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    w = ues.get_editor_world()
    MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    Y0 = -60000.0

    # Anything still standing in the staging area is debris from an interrupted run; it would be
    # photographed alongside the next weapon. Clear it before anything else.
    swept = 0
    for act in eas.get_all_level_actors():
        try:
            if abs(act.get_actor_location().y - Y0) < 4000.0:
                eas.destroy_actor(act); swept += 1
        except Exception:
            pass
    print('SWEPT', swept)

    # load_asset hands back None for a material that never compiled, and a null material draws the
    # engine's grey checker: build the backdrop whenever it does not load.
    def flat_unlit(name, v):
        path = '/Game/RepliCan/Materials/' + name
        found = unreal.load_asset(path)
        if found: return found
        if eal.does_asset_exist(path): eal.delete_asset(path)
        m = tools.create_asset(name, '/Game/RepliCan/Materials', unreal.Material, unreal.MaterialFactoryNew())
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
        c.set_editor_property('constant', unreal.LinearColor(v, v, v, 1.0))
        MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        MEL.recompile_material(m); eal.save_asset(path)
        return m
    black = flat_unlit('M_IconBackBlack', 0.0)
    white = flat_unlit('M_IconBackWhite', 1.0)
    print('BACKDROP', black.get_name() if black else None, white.get_name() if white else None)
    os.makedirs(OUT, exist_ok=True)

    # Temporal AA carries one item's history into the next shot's edges; FXAA has no history.
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    # sRGB target + the finished LDR image: the two have to agree or the icon is encoded twice.
    rt = unreal.RenderingLibrary.create_render_target2d(w, SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)

    done = 0
    for key, e in sorted(cat.items()):
        icon = e['icon']
        if ONLY and icon not in ONLY: continue
        if not FORCE and os.path.exists(os.path.join(OUT, icon_name(icon) + '_black.png')): continue
        mesh = unreal.load_asset(e['mesh'])
        if not mesh: print('MISSING', e['mesh']); continue
        b = mesh.get_bounding_box(); centre = (b.min + b.max) * 0.5
        extent = max(b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z) * 1.25
        # A STRAIGHT SIDE VIEW, muzzle to the right: HAC1 pieces point +X, the capture looks
        # down +X with +Y to its right, so yaw 90 lays the piece across the frame nose-right.
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        # No rim shell here: a fresnel overlay on a flat side view lays a pale glow over the whole
        # face and washes the colours out. The green edge is drawn on the PNG afterwards by
        # Tools/icon_outline.ps1, where the silhouette is simply "not the black backdrop".

        def recentre():
            """Put the piece's BOUNDS on the shot's origin, whatever its own pivot is."""
            o, _ = a.get_actor_bounds(False)
            a.add_actor_world_offset(unreal.Vector(0.0, Y0, 0.0) - o, False, False)

        recentre()
        # The capture looks down +X, so the piece's spread across a square icon is its Y and its
        # height is its Z. A long thin weapon laid flat leaves the top and bottom of the icon
        # empty; turned corner to corner it uses the diagonal and comes out noticeably larger.
        # (The corner-to-corner tilt for long pieces is out: the icons are plain side views now.)
        o, ext = a.get_actor_bounds(False)
        # The rig scales with the piece, so a dagger and a launcher are lit the same way rather
        # than the launcher sitting almost on top of the lamps. LIGHT_SCALE was measured against
        # this scaled rig; leaving the lamps at fixed positions is what blew the colour out.
        L = extent / 100.0
        back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(80.0 * L, Y0, 0), unreal.Rotator(roll=0, pitch=90, yaw=0))
        back.set_actor_scale3d(unreal.Vector(extent / 20.0, extent / 20.0, 1.0))
        back.static_mesh_component.set_material(0, black)
        lights = []
        for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
            l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * L, Y0 + y * L, z * L)); lc = l.light_component
            lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i * LIGHT_SCALE); lc.set_attenuation_radius(600 * L); lights.append(l)
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-200.0 * L, Y0, 0), unreal.Rotator(roll=0, pitch=0, yaw=0))
        c = cap.get_editor_property('capture_component2d'); c.texture_target = rt
        c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        # Fitted to what the piece actually projects, so a dagger and a launcher fill the icon
        # to the same degree instead of being framed off one crude multiple of their longest side.
        c.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC
        c.ortho_width = max(2.0 * ext.y, 2.0 * ext.z) * FIT_MARGIN
        c.set_editor_property('capture_every_frame', False)
        # TWO SHOTS, over black and over white. The same pixel differs between them by exactly
        # how much backdrop shows through it, which is the silhouette's alpha to sub-pixel
        # precision -- so a black stock is as much "the weapon" as a yellow receiver. Tools/
        # icon_matte.ps1 turns the pair into one PNG with that alpha; the outline follows it.
        for backdrop, suffix in ((black, '_black'), (white, '_white')):
            back.static_mesh_component.set_material(0, backdrop)
            for _ in range(3): c.capture_scene()   # a few passes so nothing from the previous shot lingers
            unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s%s.png' % (icon_name(icon), suffix))
        for x in [cap, back, a] + lights: eas.destroy_actor(x)
        done += 1
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('RENDERED', done)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written to RawArt/render_error.txt')
