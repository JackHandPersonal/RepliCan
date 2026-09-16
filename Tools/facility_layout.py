"""Lvl_AsteroidFacility layout, additive and deletion-aware.

Run inside the editor (Tools/ue_remote.py --file Tools/facility_layout.py).
The map is the source of truth: this script never wipes the level. For
each planned item (by actor label) it
  - leaves it alone if it is in the level,
  - spawns it if it has never been placed before,
  - SKIPS it if it was placed before and is now missing: that means you
    deleted it in the editor, and it stays deleted (it is recorded in
    Tools/LevelManifest/<map>.json under "removed"; delete the entry there
    to allow it back),
  - revises it in place only when its label is listed in REVISE below
    (an intentional change to something already placed).
Labels in REMOVE are deleted now and recorded as removed.
Structure (walls, floors, ceilings, pillars), props, lights and the PCS
decals are all level actors saved into the .umap, nothing is spawned at
runtime except the intro's characters.
Measured conventions: wall pieces run X 0..500 from their origin, span
local Y 0..89, the ROOM is on their local +Y side; Pillar_Wide is 150 x
150 from its origin; Ceiling_01's visible underside is coffered: z 420 in
the middle column of the tile (x 200..300), 400 along its x edges, 427 to
446 elsewhere (its top is at 449)."""
import unreal, json, os, io
B = '/Game/PolygonSciFiSpace/Meshes/Buildings/'
P = '/Game/PolygonSciFiSpace/Meshes/Props/'
GRIME = '/Game/RepliCan/Materials/M_Facility_Grime'
GRIME_LIFT = '/Game/RepliCan/Materials/M_Facility_Grime_Lift'   # the same with the base colour multiplied up, for near-black Synty pieces
CLEAN = '/Game/PolygonSciFiSpace/Materials/M_PolygonSciFiSpace_01_A'   # the palette without the dirt; nothing uses it now that the cafeteria has gone the same way as the rest
MAP = '/Game/RepliCan/Maps/Lvl_AsteroidFacility'
MANIFEST = os.path.join(unreal.Paths.project_dir(), 'Tools', 'LevelManifest', 'Lvl_AsteroidFacility.json')
FIXTURE = 'SM_Prop_Light_Panel_01'     # flat back at its z=0 face, stepped lens at z 7..10 -> flipped, back to the ceiling
CELL = 500.0; NX, NY = 2, 3; W, H = NX * CELL, NY * CELL
WALL_T = 89.0; EXT = 60.0
FY0 = H + 2 * WALL_T; FNY = 2; FY1 = FY0 + FNY * CELL
SX = (W + 2 * WALL_T) / W; SYB = (H + 2 * WALL_T) / H; SYF = (FNY * CELL + 2 * WALL_T) / (FNY * CELL)
# Floors: the kit has no plain panel tile (01-03/08 carry hazard bands, 04-06/011 cable strips, 07 a hatch);
# rooms use the clean octagonal plate 09 everywhere, the hall the strip tile 011 laid along its length.
ROOM_FLOOR = 'SM_Bld_Floor_09'; HALL_FLOOR = 'SM_Bld_Floor_011'
CEIL_MID = 420.0                       # ceiling underside in a tile's middle column
CEIL_ARM = 427.0 - 50.0                # ... where the bed arms hang (50 down with the bay's ceiling, see BAY_CEIL_DROP)

# Intentional revisions this run, and removals.
REVISE = {'Fixture_SW', 'Fixture_SE', 'Fixture_NW', 'Fixture_NE', 'Fixture_SW_Light', 'Fixture_SE_Light', 'Fixture_NW_Light', 'Fixture_NE_Light',
          'Foyer_Fixture_W', 'Foyer_Fixture_E', 'Foyer_Fixture_W_Light', 'Foyer_Fixture_E_Light', 'CeilingArm_1', 'CeilingArm_2',
          'RedLamp_1_Glow', 'RedLamp_2_Glow', 'RedLamp_3_Glow', 'RedLamp_4_Glow', 'RedLamp_5_Glow',
          'RedLamp_6_Glow', 'RedLamp_7_Glow', 'RedLamp_8_Glow', 'RedLamp_9_Glow',
          'Sign_Bay_Door', 'Sign_Bay_Badge', 'Sign_Bay_Stencil', 'Sign_Foyer_Airlock', 'Sign_Foyer_Exit', 'Sign_Foyer_Stencil', 'PCS_Poster_Bay', 'PCS_Poster_Frame', 'Foyer_Pipes', 'Crate_2'}
REVISE |= {'Door_Trim_Bay', 'Door_Trim_Foyer', 'DoorLamp_Bay', 'DoorLamp_Bay_Light', 'DoorLamp_Airlock', 'DoorLamp_Airlock_Light', 'JunLamp', 'JunLamp_Light', 'Door_BayFoyer', 'Fixture_NW', 'Fixture_NE', 'Fixture_NW_Light', 'Fixture_NE_Light', 'Foyer_Fixture_W', 'Foyer_Fixture_E', 'Foyer_Fixture_W_Light', 'Foyer_Fixture_E_Light'}   # onto the strip centre
REMOVE = {'Pipes', 'CableTray_N', 'CableTray_S', 'Foyer_CableTray', 'Fixture_SW', 'Fixture_SE', 'Fixture_SW_Light', 'Fixture_SE_Light',
          'PCS_Bay_DoorSign', 'PCS_Bay_Badge', 'PCS_Bay_Stencil', 'PCS_Foyer_AirlockSign', 'PCS_Foyer_ExitMark', 'PCS_Foyer_Stencil',
          'Sign_Bay_Stencil', 'RedLamp_1', 'RedLamp_2', 'RedLamp_3', 'RedLamp_4', 'RedLamp_5',
          'Bay_DoorFrame_N', 'Bay_DoorLeaves_N', 'Foyer_DoorFrame_S', 'Foyer_DoorLeaves_S'}   # two back-to-back frames -> one sliding door on the shared line   # lamp heads out: bare glows behind the clutter now   # DBuffer decals never showed; signs are masked planes now
REMOVE |= {'Foyer_Wall_E1', 'Foyer_Wall_W1'}   # the foyer's north side segments become the crew-hall and cafeteria doors
REVISE |= {'Foyer_Cart', 'Foyer_Tank', 'Sign_Foyer_Stencil'}   # out of the new doorways
REVISE |= {'Hall_Fixture_%d_Light' % t for t in range(7)} | {'Cabin_S1_Fixture_Light'} | {'Caf_Fixture_%d_%d_Light' % (i, j) for i in range(3) for j in range(3)}   # brighter

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_editor_world()
if not world or not world.get_path_name().startswith(MAP):
    les.load_level(MAP); world = ues.get_editor_world()
grime = unreal.load_asset(GRIME)
# The editor must not be in Play. EditorActorSubsystem cannot spawn into a PIE world and
# returns None with no error, so a run during a play session gets partway through and dies on
# the first thing it tries to touch -- leaving the level dirty and half built. Better to refuse.
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
    raise RuntimeError('the editor is in Play; stop the session before running the layout')

manifest = {'placed': [], 'removed': []}
if os.path.isfile(MANIFEST):
    manifest = json.load(open(MANIFEST, encoding='utf-8'))
placed = set(manifest.get('placed', [])); removed = set(manifest.get('removed', []))
existing = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
# The wing was mirrored (hall west, cafeteria east): everything in it moves this run.
REVISE |= {l for l in existing if l.startswith(('Hall_', 'Cabin_', 'Caf_', 'Door_FoyerHall', 'Door_FoyerCaf', 'Door_Trim_Hall', 'Door_Trim_Caf', 'DoorLamp_Hall', 'DoorLamp_Caf'))}
REVISE |= {'Desk', 'Chair', 'Desk_Monitor', 'Desk_Tubes', 'Desk_Pad'}   # a plainer desk in the bay
# The dot-matrix door strips: their size, text and grid change as the sign code develops, and
# without this they keep whatever they were first built with.
REVISE |= {l for l in existing if l.startswith('Sign_Door_')}
REVISE |= {l for l in existing if l.startswith('Caf_Trooper_') and l.endswith(('_Head', '_Nose'))}
REVISE |= {l for l in existing if l.startswith('Foyer_Fixture_')}   # overhead panels brighter everywhere but the bay
REVISE |= {l for l in existing if l.endswith('_Light')}   # every light re-applied: the flicker pick
REVISE |= {l for l in existing if l.startswith('Haze_')}   # haze is still being tuned
REVISE |= {l for l in existing if l.startswith(('BayMist_', 'BayJet_', 'S10_Fog', 'S10_Steam'))}   # atmosphere: tuned, and swapped to the unselectable class
REVISE |= {l for l in existing if l.startswith('Lift_')}   # the lift is still being built
REVISE |= {l for l in existing if l.startswith('Caf_Lineup_')}   # the review line-up moves as the room does
REMOVE |= {l for l in existing if l.startswith('Caf_Lineup_Horror')}   # horror models stay out
# Anything taken out and then asked for again has to be let back in: a label in `removed` is
# refused by ensure() forever, which is the whole point of that list -- it is how a hand
# deletion survives a re-run. Putting three of them back is a deliberate act, so it says so.
# Their noses have to be let back in BY NAME as well. The 31-model line-up gave everyone a nose,
# including the robots; reverting it deleted all 62 actors, so `Caf_Lineup_..._Nose` is on the
# removed list too and ensure() refuses it. Letting the body back in and not the face is exactly
# how three of them ended up standing there bare-faced on the next run.
for _back in ('Caf_Lineup_Space_Junker_Male', 'Caf_Lineup_Space_Junker_Female', 'Caf_Lineup_Space_Hunter_Female'):
    removed.discard(_back)
    removed.discard(_back + '_Nose')
    removed.discard(_back + '_Head')
REVISE |= {l for l in existing if l.startswith('Caf_Lineup_') and l.endswith('_Nose')}
removed.discard('Bay_Wall_S1')   # the lift moved to the foyer; the bay wants its wall back
REMOVE |= {'Horror_Landing_0', 'Horror_Landing_1'}           # first pass, laid against the old deck position
REVISE |= {l for l in existing if l.startswith('Caf_')}   # the cafeteria loses its clean palette
REVISE |= {l for l in existing if '_Floor' in l}   # the floor tile change
REVISE |= {'Foyer_Wall_E0', 'Caf_Wall_W0', 'Foyer_Wall_N2'}   # the window wall; N2 is being shortened
REVISE |= {l for l in existing if l.startswith(('CCTV_', 'Foyer_Board_', 'Door_Fill_'))}
REMOVE |= {l for l in existing if l.startswith('CCTV_')}   # 2026-09-16: every security camera and its arms and pod came out (user's call); the cctv() helper stays for later
REMOVE |= {'Caf_Table_Item_4', 'Caf_Table_Item_5', 'Caf_Table_Item_6'}   # CAF_DRESS put a burger, a plate and a snack on the same spots; the audit showed the pairs overlapping
REMOVE |= {'S10_FogG_%d' % k for k in range(18)} | {'S10_FogH_%d' % k for k in range(6)}   # eighteen emitters at 0.9, then six at 2.0, both cost frames: translucent cost is screen COVERAGE, so four at 1.0 (build_sub_room)
REMOVE |= {'Hall_Wall_E', 'Hall_Pillar_SE', 'Hall_Pillar_NE', 'Caf_Wall_W1'}   # the pre-mirror hall end and the cafeteria's old door-side wall
REMOVE |= {'Cabin_S1_Footlocker', 'Cabin_S1_Footlocker_Lid'}   # now a loot box actor
REMOVE |= {'Cabin_S1_Shower'}   # replicants do not shower
REMOVE |= {'Foyer_DoorFrame_N', 'Foyer_DoorLeaves_N'}   # the lift shaft wall is the door there now
# The first service deck (30 x 15 m, labelled Sub_) is retired: the basement is being built up
# ITERATIVELY now, one room at a time, starting with a 3 x 5 at the foot of the lift (S10_).
REMOVE |= {l for l in existing if l.startswith('Sub_')}
# Shaft walls at unserviced floors: see LIFT_SERVICED. They were being swept through by the car.
REMOVE |= {l for l in existing if l.startswith('Lift_Shaft_')
           and int(l.rsplit('_', 1)[1]) not in (0, -10)}
# The seal capped a cut through the middle of a demo map, and the transplant it capped is
# retired: a BUILT room has its own walls. Listed up here because the removal pass runs early.
REMOVE |= {l for l in existing if l.startswith(('Horror_Wall_', 'Horror_Pillar_', 'Horror_Landing', 'Horror_Door_Frame'))}
REMOVE |= {l for l in existing if l.startswith(('Steam_', 'Haze_'))}   # sprite fog is out; the air's
# thickness comes from the height fog and ambient occlusion instead, which are densities
# rather than textured cards and can therefore actually be turned down a little.
REMOVE |= {l for l in existing if l.startswith('Haze_') and '_Ceil_' in l}   # ceiling layer dropped; floor only
REMOVE |= {'Haze_Bay_Floor_3', 'Haze_Bay_Floor_4', 'Haze_Foyer_Floor_3'}   # thinned out
REMOVE |= {'DoorLamp_Airlock', 'DoorLamp_Airlock_Light', 'DoorLamp_Hall_A', 'DoorLamp_Hall_A_Light', 'DoorLamp_Hall_B', 'DoorLamp_Hall_B_Light', 'DoorLamp_Caf_A', 'DoorLamp_Caf_A_Light', 'DoorLamp_Caf_B', 'DoorLamp_Caf_B_Light'}   # no bar lamps over the sliding doors
if not placed:
    placed = set(existing.keys())      # first run after the generated map: everything here was ours
stats = {'kept': 0, 'spawned': 0, 'skipped_deleted': 0, 'revised': 0, 'removed': 0}
skipped = []

def ensure(label, spawn, revise=None):
    """spawn() -> actor; revise(actor) updates one in place."""
    if label in removed: return None
    if label in existing:
        a = existing[label]
        if label in REVISE and revise: revise(a); stats['revised'] += 1
        else: stats['kept'] += 1
        return a
    if label in placed:
        removed.add(label); skipped.append(label); stats['skipped_deleted'] += 1
        return None
    a = spawn()
    if a:
        a.set_actor_label(label); placed.add(label); existing[label] = a; stats['spawned'] += 1
    return a

# Labels a re-run is ALLOWED to move. Normally empty, and that is the point.
#
# A revise used to re-apply the transform, so anything on the REVISE list was dragged back to
# the script's coordinates every run -- which meant a piece nudged into place by hand in the
# editor would not stay nudged. The map is the source of truth for WHERE things are; this script
# is the source of truth for what they are MADE of. So a revise now refreshes the mesh, the
# material, the scale and the tags, and leaves the transform alone.
#
# When a coordinate in this file genuinely changes and the thing has to follow, put its prefix
# in here for that run, or simply delete the actor in the editor and let it be re-spawned.
REPOSITION = ()

def may_move(label):
    """True while an actor is being PLACED for the first time, False once it exists.

    Several helpers share one apply() between spawn and revise, so the test cannot simply be
    "never move anything" -- a brand-new actor has to be put somewhere. `existing` holds what was
    in the level when this run started, and ensure() only adds a label to it after spawn()
    returns, so a label missing from it is a first placement."""
    if label is None or label not in existing:
        return True
    return bool(REPOSITION) and label.startswith(REPOSITION)


def mesh_actor(path, x, y, z=0.0, yaw=0.0, roll=0.0, pitch=0.0, scale=None, mat=True, material=None, tags=None, label=None):
    def override(c):
        mm = unreal.load_asset(material) if material else None
        if mm:
            for i in range(c.get_num_materials()): c.set_material(i, mm)
    def spawn():
        mesh = unreal.load_asset(path)
        if not mesh: print('MISSING', path); return None
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
        if scale: a.set_actor_scale3d(unreal.Vector(x=scale[0], y=scale[1], z=scale[2]))
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.STATIC)
        if mat and grime:
            for i in range(c.get_num_materials()): c.set_material(i, grime)
        override(c)
        if tags is not None: a.set_editor_property('tags', [unreal.Name(t) for t in tags])
        return a
    def revise(a):
        mesh = unreal.load_asset(path)
        c = a.static_mesh_component
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        if mesh and c.static_mesh != mesh:
            c.set_static_mesh(mesh)
        # ALWAYS re-apply, not just when the mesh changed. The material is as much a decision as
        # the mesh is, and gating it on a mesh swap meant a piece whose PALETTE changed kept the
        # old one forever -- which is exactly what left the cafeteria clean after it was told to
        # wear the same grime as the rest of the station.
        if mat and grime:
            for i in range(c.get_num_materials()): c.set_material(i, grime)
        override(c)
        # Hand placement wins. See REPOSITION above.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw), False, True)
        if scale: a.set_actor_scale3d(unreal.Vector(x=scale[0], y=scale[1], z=scale[2]))
        override(c)
        c.set_mobility(unreal.ComponentMobility.STATIC)
        if tags is not None: a.set_editor_property('tags', [unreal.Name(t) for t in tags])
    return spawn, revise

# ---- Steam and vapour ----------------------------------------------------------------------
# A pressurised station leaks. Nine looks, so a corridor of vents does not read as one effect
# repeated: hard jets from the SciFi Space pack, softer plumes from CyberCity, drifting smoke
# and settled ground fog from SciFi Worlds, plus two hazard looks for the places nobody should
# be standing in.
#
# Every vent is also written to UI/SteamVents.json. UAmbientPlayer reads that file and hangs a
# positional hiss loop on each one, so a vent you can see is a vent you can hear from the right
# direction -- and the coordinates exist once, here, rather than being typed again in C++.
STEAM_KINDS = {
    'jet':       ('/Game/PolygonSciFiSpace/FX/Niagara/NS_FX_Steam',                 1.0,  'steam_hiss_1', 0.30),
    'jet_small': ('/Game/PolygonSciFiSpace/FX/Niagara/NS_FX_Steam',                 0.45, 'steam_hiss_1', 0.18),
    'plume':     ('/Game/PolygonCyberCity/FX/Niagara/NS_FX_Steam',                  1.1,  'steam_hiss_2', 0.26),
    'wisp':      ('/Game/PolygonSciFiWorlds/FX/Niagara/NS_Smoke_Small_01',          0.6,  'steam_hiss_2', 0.12),
    'billow':    ('/Game/PolygonSciFiWorlds/FX/Niagara/NS_Smoke_Large_White_01',    0.5,  'steam_hiss_2', 0.16),
    'groundfog': ('/Game/PolygonSciFiWorlds/FX/Niagara/NS_Fog_Flat_01',             1.0,  '',             0.0),
    'hazard':    ('/Game/PolygonSciFiWorlds/FX/Niagara/NSS_Radioactive_Fog_01',     0.8,  'steam_hiss_2', 0.14),
    'bubbles':   ('/Game/PolygonSciFiWorlds/FX/Niagara/NS_Radioactive_Bubbles_01',  0.8,  '',             0.0),
    'dust':      ('/Game/PolygonCyberCity/FX/Niagara/NS_FX_Dust_Big',               0.5,  '',             0.0),
    # The horror pack's own FX, for the deck at the bottom of the shaft. Its screenshots are
    # made of these; using the SciFi Space steam down there would look like the wrong building.
    'horror_steam':     ('/Game/Synty/PolygonSciFiHorror/FX/NS_Steam_01',            1.0,  'steam_hiss_1', 0.26),
    'horror_burst':     ('/Game/Synty/PolygonSciFiHorror/FX/NS_Steam_Burst_01',      1.0,  '',             0.0),
    'horror_groundfog': ('/Game/Synty/PolygonSciFiHorror/FX/NS_Fog_Ground_01',       1.0,  '',             0.0),
    'horror_sparks':    ('/Game/Synty/PolygonSciFiHorror/FX/NS_Sparks_01',           1.0,  '',             0.0),
    'horror_surge':     ('/Game/Synty/PolygonSciFiHorror/FX/NS_Electricity_Surge_01', 1.0, '',             0.0),
    'horror_dust':      ('/Game/Synty/PolygonSciFiHorror/FX/NS_Dust_Spots_Small_01', 1.0,  '',             0.0),
}
STEAM_VENTS = []

def steam(label, kind, x, y, z, yaw=0.0, pitch=0.0, scale=1.0, zscale=1.0, volume=None, inner=260.0, falloff=900.0):
    """One vent. pitch 0 blows along +X at the given yaw; pitch 90 blows straight up."""
    asset, base_scale, sound, base_vol = STEAM_KINDS[kind]
    sys_asset = unreal.load_asset(asset)
    if not sys_asset:
        print('STEAM SYSTEM MISSING', asset); return
    sc = base_scale * scale

    def apply(a, place_it=True):
        if place_it:
            a.set_actor_location_and_rotation(unreal.Vector(x, y, z),
                unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw), False, True)
        a.set_actor_scale3d(unreal.Vector(sc, sc, sc * zscale))
        c = a.niagara_component
        try: c.set_asset(sys_asset)
        except Exception: c.set_editor_property('asset', sys_asset)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        a.set_editor_property('tags', [unreal.Name('steam'), unreal.Name('fx')])
        _tidy_fx(a)

    def spawn():
        # AAtmosphereFXActor, not NiagaraActor: identical, except that clicks in the editor
        # viewport pass through it. See Source/RepliCan/AtmosphereFXActor.h.
        a = eas.spawn_actor_from_class(unreal.AtmosphereFXActor, unreal.Vector(x, y, z),
                                       unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        if a: apply(a, place_it=True)
        return a

    def revise(a):
        # A vent laid before the class existed is a plain NiagaraActor and stays clickable
        # forever; the class of an actor cannot be changed in place, so it is re-spawned where
        # it stands, under its own label, and the manifest never notices.
        if a.get_class().get_name() != 'AtmosphereFXActor':
            keep_loc, keep_rot = a.get_actor_location(), a.get_actor_rotation()
            eas.destroy_actor(a)
            b = eas.spawn_actor_from_class(unreal.AtmosphereFXActor, keep_loc, keep_rot)
            if b:
                apply(b, place_it=False)
                b.set_actor_label(label)
                existing[label] = b
            return
        apply(a, place_it=False)

    ensure(label, spawn, revise)
    if sound:
        STEAM_VENTS.append({'label': label, 'kind': kind, 'at': [round(x, 1), round(y, 1), round(z, 1)],
                            'sound': sound, 'volume': round(base_vol if volume is None else volume, 3),
                            'inner': inner, 'falloff': falloff})


def pulsed_steam(label, kind, x, y, z, yaw=0.0, pitch=0.0, scale=1.0, zscale=1.0,
                 burst=2.2, quiet=17.0, jitter=0.35, sound='steam_burst_1.wav', volume=0.30):
    """A vent that lets go every so often instead of hissing forever.

    Same geometry arguments as steam(), but the actor is an APulsedFXActor, which keeps the
    system deactivated and turns it on in bursts. The hiss is a positional ONE-SHOT fired with
    each burst rather than the loop steam() registers in UI/SteamVents.json -- a jet that only
    goes now and then must not be audible in between, or the effect is worse than a constant
    one."""
    asset, base_scale, _snd, _vol = STEAM_KINDS[kind]
    sys_asset = unreal.load_asset(asset)
    if not sys_asset:
        print('STEAM SYSTEM MISSING', asset); return
    sc = base_scale * scale

    def apply(a, place_it=True):
        if place_it:
            a.set_actor_location_and_rotation(unreal.Vector(x, y, z),
                unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw), False, True)
        a.set_actor_scale3d(unreal.Vector(sc, sc, sc * zscale))
        c = a.get_editor_property('fx')
        try: c.set_asset(sys_asset)
        except Exception: c.set_editor_property('asset', sys_asset)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        for k, v in (('burst_seconds', burst), ('quiet_seconds', quiet), ('jitter', jitter),
                     ('burst_sound', sound), ('burst_volume', volume)):
            try: a.set_editor_property(k, v)
            except Exception as e: print('PULSED FX property', k, e)
        a.set_editor_property('tags', [unreal.Name('steam'), unreal.Name('fx')])
        _tidy_fx(a)

    def spawn():
        a = eas.spawn_actor_from_class(unreal.PulsedFXActor, unreal.Vector(x, y, z),
                                       unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        if a: apply(a, place_it=True)
        return a

    ensure(label, spawn, lambda a: apply(a, place_it=False))


def _tidy_fx(a):
    """Keep atmosphere out of the way of editing.

    The last round of fog was removed partly because it kept getting picked up by a click in the
    viewport -- a Niagara actor is a billboard sprite the size of a barn and it sits in front of
    whatever you were actually aiming at. Two things fix that without deleting the effect: the
    actor is locked so a stray drag cannot move it, and everything atmospheric goes in one
    outliner folder so it can be hidden as a group with one click."""
    try: a.set_folder_path('Atmosphere')
    except Exception: pass
    try: a.set_editor_property('lock_location', True)
    except Exception:
        try: a.set_editor_property('b_lock_location', True)
        except Exception: pass


def place(label, path, x, y, z=0.0, yaw=0.0, roll=0.0, pitch=0.0, scale=None, mat=True, material=None, tags=None):
    # The cafeteria used to be kept: it was the one room given the clean palette while the rest
    # of the station wore the grime. It is not kept any more -- same dirt as the foyer, because a
    # mess hall nobody is maintaining is a stronger statement than one that is.
    s, r = mesh_actor(path, x, y, z, yaw, roll, pitch, scale, mat, material, tags, label=label)
    return ensure(label, s, r)

import zlib
# About one warm-white light in four is a little yellower and carries a very slight
# flicker (a light-function material, four phase variants), chosen by the label's hash
# so the pick is the same every run.
FLICKER_SALT = 4
def flicker_of(label, color):
    warm = color[1] > 0.6 and color[2] > 0.4 and color[0] >= color[2]
    h = zlib.crc32((label + '#%d' % FLICKER_SALT).encode('utf-8'))   # the salt spreads the quarter over every room
    return (h % 4 == 0) and warm, h
def light(label, x, y, z, intensity, radius, color, shadows=False, tags=None):
    def apply(a):
        if tags is not None: a.set_editor_property('tags', [unreal.Name(t) for t in tags])
        c = a.light_component
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_intensity_units(unreal.LightUnits.CANDELAS)
        c.set_intensity(intensity); c.set_attenuation_radius(radius)
        flick, h = flicker_of(label, color)
        col = (color[0], color[1] * 0.93, color[2] * 0.72) if flick else color
        c.set_light_color(unreal.LinearColor(r=col[0], g=col[1], b=col[2], a=1.0))
        c.set_cast_shadows(shadows)
        c.set_light_function_material(unreal.load_asset('/Game/RepliCan/Materials/MI_LightFlicker_%d' % ((h // 4) % 4)) if flick else None)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location(unreal.Vector(x, y, z), False, True)
    def spawn():
        a = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z)); apply(a); return a
    return ensure(label, spawn, apply)

def fixture(label, x, y, z_ceiling, intensity=18.0, shadows=False, yaw=0, tags=None):
    # Back plate against the ceiling underside, lens 10 below it, point light beneath.
    place(label, P + FIXTURE, x, y, z_ceiling, yaw=yaw, roll=180, mat=False)
    light(label + '_Light', x, y, z_ceiling - 28.0, intensity, 900, (1.0, 0.82, 0.6), shadows=shadows, tags=tags)

def door_lamp(label, x, y, z, yaw):
    place(label, P + 'SM_Prop_Light_Small_02', x, y, z, yaw=yaw, roll=90, mat=False)
    light(label + '_Light', x, y + (-16 if yaw == 0 else 16), z - 12, 12, 600, (1.0, 0.56, 0.26))

def tile_center(i, j, y0, sy):
    return (i * CELL * SX - WALL_T + 250.0 * SX, y0 + j * CELL * sy - WALL_T + 250.0 * sy)

# Ceiling_01's flat light strip (underside z 419.5) runs the tile's length at
# local x 190..265, so its centre is x 227.5, not the tile's 250: fixtures go there.
STRIP_X = 227.5
def strip_center(i, j, y0, sy):
    return (i * CELL * SX - WALL_T + STRIP_X * SX, y0 + j * CELL * sy - WALL_T + 250.0 * sy)

def shell(x0, y0, nx, ny, tag, south='wall', north='wall', ceiling_z=0.0):
    w_, h_ = nx * CELL, ny * CELL
    sx = (w_ + 2 * WALL_T) / w_; sy = (h_ + 2 * WALL_T) / h_
    for i in range(nx):
        for j in range(ny):
            place('%s_Floor_%d_%d' % (tag, i, j), B + ROOM_FLOOR, x0 + i * CELL * sx - WALL_T, y0 + j * CELL * sy - WALL_T, 0, scale=(sx, sy, 1.0))
            place('%s_Ceiling_%d_%d' % (tag, i, j), B + 'SM_Bld_Ceiling_01', x0 + i * CELL * sx - WALL_T, y0 + j * CELL * sy - WALL_T, ceiling_z, scale=(sx, sy, 1.0))
    ov = (1.001, 1.0, 1.0); ovx = ((CELL + EXT) / CELL, 1.0, 1.0)
    if south != 'none':
        for k, (xs, sc) in enumerate([(x0 - 250.0 - EXT, ovx), (x0 + 250.0, ov), (x0 + 750.0, ovx)]):
            if k == 1 and south == 'open': continue   # a door actor fills the gap
            if k == 1 and south == 'door':
                place('%s_DoorFrame_S' % tag, B + 'SM_Bld_Wall_Doorframe_02', xs, y0 - 44.0, 0, yaw=0)
                place('%s_DoorLeaves_S' % tag, B + 'SM_Bld_Wall_Doorframe_Door_02', xs + 250.0, y0 - 44.0, 0, yaw=0)
            else:
                place('%s_Wall_S%d' % (tag, k), B + ('SM_Bld_Wall_01_Alt' if k % 2 else 'SM_Bld_Wall_01'), xs, y0 - WALL_T, 0, yaw=0, scale=sc)
    if north != 'none':
        for k, (xs, sc) in enumerate([(x0 + 250.0, ovx), (x0 + 750.0, ov), (x0 + 1250.0 + EXT, ovx)]):
            if k == 1 and north == 'open': continue
            if k == 1 and north == 'door':
                place('%s_DoorFrame_N' % tag, B + 'SM_Bld_Wall_Doorframe_02', xs, y0 + h_ + 44.0, 0, yaw=180)
                place('%s_DoorLeaves_N' % tag, B + 'SM_Bld_Wall_Doorframe_Door_02', xs - 250.0, y0 + h_ + 44.0, 0, yaw=180)
            else:
                place('%s_Wall_N%d' % (tag, k), B + 'SM_Bld_Wall_01', xs, y0 + h_ + WALL_T, 0, yaw=180, scale=sc)
    for j in range(ny):
        sc = ovx if j in (0, ny - 1) else ov
        place('%s_Wall_W%d' % (tag, j), B + ('SM_Bld_Wall_01' if j % 2 else 'SM_Bld_Wall_01_Alt'), x0 - WALL_T, y0 + (j + 1) * CELL + (EXT if j == ny - 1 else 0.0), 0, yaw=-90, scale=sc)
        place('%s_Wall_E%d' % (tag, j), B + ('SM_Bld_Wall_01_Alt' if j % 2 else 'SM_Bld_Wall_01'), x0 + w_ + WALL_T, y0 + j * CELL - (EXT if j == 0 else 0.0), 0, yaw=90, scale=sc)
    for (x, y, yaw, nm) in [(x0, y0, 180, 'SW'), (x0 + w_, y0, -90, 'SE'), (x0 + w_, y0 + h_, 0, 'NE'), (x0, y0 + h_, 90, 'NW')]:
        place('%s_Pillar_%s' % (tag, nm), B + 'SM_Bld_Wall_Corner_Pillar_Wide_01', x, y, 0, yaw=yaw)

# ---- Removals first ----------------------------------------------------------
for label in REMOVE:
    if label in existing:
        eas.destroy_actor(existing.pop(label)); stats['removed'] += 1
    removed.add(label); placed.discard(label)

# ---- The bay -----------------------------------------------------------------
# The bay's ceiling and everything hung from it sit BAY_CEIL_DROP lower: a gap showed at the
# back of the bay between the ceiling's underside and the wall tops (the walls run on above it).
BAY_CEIL_DROP = 50.0
REVISE |= {l for l in existing if l.startswith(('Bay_Ceiling_', 'CeilingArm_', 'Back_Tray_', 'Back_CeilingPipes'))}   # re-placed with the drop (a kept label is never moved)
shell(0, 0, NX, NY, 'Bay', south='wall', north='open', ceiling_z=-BAY_CEIL_DROP)
for k, x in enumerate([300.0, 700.0]):
    place('CryoBed_%d' % (k + 1), P + 'SM_Prop_CryoBed_01', x, 720.0, 0, yaw=0)
    place('CeilingArm_%d' % (k + 1), P + 'SM_Prop_MedicalArms_01', x, 720.0, CEIL_ARM, yaw=0)
place('Cart_R', P + 'SM_Prop_Medical_Cart_02', 500.0, 570.0, 0, yaw=90)
place('WasteCart_1', P + 'SM_Prop_Cart_Filled_01', 140.0, 1130.0, 0, yaw=37)
place('WasteCart_2', P + 'SM_Prop_Medical_Cart_Bare_01', 860.0, 1120.0, 0, yaw=-28)
place('Barrel', P + 'SM_Prop_Barrel_01', 905.0, 1330.0, 0, yaw=12)
place('Crate_1', P + 'SM_Prop_Crate_Wide_01', 880.0, 1420.0, 0, yaw=15)
place('Crate_2', P + 'SM_Prop_Crate_Wide_01', 765.0, 1438.0, 0, yaw=-8)      # off the stack: both bay crates open, on the floor
# Lids off and propped on the rims, as if just lifted; the pack's Crate_Wide_Lid_01 fits Crate_Wide_01.
place('Crate_1_Lid', P + 'SM_Prop_Crate_Wide_Lid_01', 880.0, 1420.0, 6.0, yaw=15, roll=-38)
place('Crate_2_Lid', P + 'SM_Prop_Crate_Wide_Lid_01', 765.0, 1438.0, 6.0, yaw=-8, roll=42)
# Tech detritus in the two open crates: positions are in each crate's frame (x along its length).
import math
def crate_fill(tag, cx, cy, yaw, items):
    for k, (mesh, lx, ly, lz, lyaw, roll) in enumerate(items):
        a = math.radians(yaw)
        place('%s_Junk_%d' % (tag, k + 1), P + mesh, cx + lx * math.cos(a) - ly * math.sin(a), cy + lx * math.sin(a) + ly * math.cos(a), lz, yaw=yaw + lyaw, roll=roll, mat=False)
crate_fill('Crate_1', 880.0, 1420.0, 15, [
    ('SM_Prop_Screen_02',      -28.0,  -8.0,  5.0,   20, 0),
    ('SM_Prop_Buttons_04',      18.0,  10.0,  5.0,  -35, 0),
    ('SM_Prop_HandScanner_01',  36.0, -12.0,  5.0,   70, 0),
    ('SM_Prop_Keycard_02',     -10.0,  14.0,  5.0,   10, 90),
    ('SM_Prop_Detail_Wire_05',   0.0,  -4.0,  8.0,   50, 0),
    ('SM_Prop_FoodPacket_03',   28.0,  16.0,  8.0,  -60, 0),
])
crate_fill('Crate_2', 765.0, 1438.0, -8, [
    ('SM_Prop_Joystick_01',    -30.0,   6.0,  5.0,  -20, 0),
    ('SM_Prop_Screen_05',       12.0, -10.0,  5.0,   40, 0),
    ('SM_Prop_Light_Small_08',  34.0,  12.0,  5.0,    0, 0),
    ('SM_Prop_Buttons_01',     -12.0, -14.0,  5.0,   80, 0),
    ('SM_Prop_Screen_11',       -2.0,  12.0,  8.0,  -15, 0),
    ('SM_Prop_Keycard_01',      24.0,  -4.0,  5.0,  110, 90),
])
place('Box_1', P + 'SM_Prop_Detail_Box_01', 960.0, 1250.0, 0, yaw=-80)
place('Hose', P + 'SM_Prop_Hose_01', 680.0, 400.0, 0, yaw=40)
place('Hose_2', P + 'SM_Prop_Hose_01', 205.0, 335.0, 0, yaw=110)
place('Hose_3', P + 'SM_Prop_Hose_01', 830.0, 345.0, 0, yaw=-25)
place('Machine_L', P + 'SM_Prop_Medical_Machine_01', 300.0, 250.0, 0, yaw=0)
place('Machine_R', P + 'SM_Prop_Medical_Machine_01', 700.0, 250.0, 0, yaw=0)
place('Machine_C', P + 'SM_Prop_Engine_Construction_01', 905.0, 250.0, 0, yaw=0)
place('Cabinet', P + 'SM_Prop_Detail_Box_03', 60.0, 460.0, 0, yaw=90)
place('Console_W', P + 'SM_Prop_Detail_Panel_01', 55.0, 300.0, 0, yaw=90)
place('Wires_Back', P + 'SM_Prop_Wires_03', 120.0, 12.0, 300.0, yaw=0)
place('O2_1', P + 'SM_Prop_Oxygen_Tank_Large', 955.0, 420.0, 0, yaw=0)
place('O2_2', P + 'SM_Prop_Oxygen_Tank_Large', 950.0, 470.0, 0, yaw=35)
place('Tank_E', P + 'SM_Prop_Detail_Tank_01', 945.0, 560.0, 0, yaw=15)
place('Sample_1', P + 'SM_Prop_Test_Tube_01', 860.0, 380.0, 0, yaw=-20)
place('Wirebox', P + 'SM_Prop_Detail_Wirebox_01', 975.0, 640.0, 0, yaw=90)
# A plain table as the desk (the Worlds pack's medium table, top at z 78) with a simple chair.
place('Desk', '/Game/PolygonSciFiWorlds/Models/Props/SM_Prop_Table_Medium_01', 150.0, 1440.0, 0, yaw=0, mat=False, material='/Game/RepliCan/Materials/MI_DrabTable')   # flat drab grey-brown
place('Chair', P + 'SM_Prop_Stool_02', 150.0, 1345.0, 0, yaw=180)
place('Desk_Monitor', P + 'SM_Prop_Screen_Small_02', 135.0, 1458.0, 108.0, yaw=180)
place('Desk_Tubes', P + 'SM_Prop_TestTubes_01', 200.0, 1450.0, 78.5, yaw=0)
place('Desk_Pad', P + 'SM_Prop_Screen_01', 110.0, 1425.0, 78.5, yaw=25)
# Fixtures at the centres of the corner tiles (the ceiling's flat z=420 band), clear of the bed arms.
for (i, j, nm, sh) in [(0, 2, 'NW', False), (1, 2, 'NE', True)]:   # the back pair was removed; the back is lit by the machines and the fire-red lamps
    cx, cy = strip_center(i, j, 0.0, SYB); fixture('Fixture_' + nm, cx, cy, CEIL_MID - BAY_CEIL_DROP, shadows=sh)   # the bay's ceiling is lower
# Back-of-room clutter. Wall plates: Greeble_Panel lies flat (thickness +Z),
# so pitch 90 + yaw 90 stands it on the south wall with its face into the
# room and its long side along the wall; pitch 90 alone does the west wall.
place('Back_Plate_1', P + 'SM_Prop_Greeble_Panel_01', 280.0, 6.0, 250.0, yaw=90, pitch=90)
place('Back_Plate_2', P + 'SM_Prop_Greeble_Panel_02', 760.0, 6.0, 300.0, yaw=90, pitch=90)
place('Back_Plate_3', P + 'SM_Prop_Greeble_Panel_03', 6.0, 150.0, 280.0, yaw=0, pitch=90)
place('Back_WallPanel_1', P + 'SM_Prop_Wall_Panel_Small_01', 520.0, 0.0, 330.0, yaw=0)
place('Back_WallPanel_2', P + 'SM_Prop_Wall_Panel_Small_02', 600.0, 0.0, 330.0, yaw=0)
place('Back_Vent', P + 'SM_Prop_AirVent_Small_01', 860.0, 21.0, 340.0, yaw=0)
place('Back_Strip_1', P + 'SM_Prop_Detail_Lights_02', 150.0, 15.0, 0.0, yaw=180, mat=False)      # floor skirting strips, as in the demo
place('Back_Strip_2', P + 'SM_Prop_Detail_Lights_02', 850.0, 15.0, 0.0, yaw=180, mat=False)
place('Back_PipePillar', P + 'SM_Prop_Detail_Pipe_Pillar_01', 940.0, 60.0, 0.0, yaw=0)
place('Back_Wires_2', P + 'SM_Prop_Wires_01', 620.0, 60.0, 286.0, yaw=0)
place('Back_Wires_3', P + 'SM_Prop_Wires_02', 60.0, 330.0, 300.0, yaw=0)
# Ceiling: cable trays run along Y inside the tiles' flat x band (z 420), the
# pipe run's top raised to that band (buried where the coffers are lower).
place('Back_Tray_W', P + 'SM_Prop_Detail_CeilingBox_01', 205.0, 0.0, CEIL_MID - 18.0 - BAY_CEIL_DROP, yaw=90)
place('Back_Tray_E', P + 'SM_Prop_Detail_CeilingBox_01', 794.0, 0.0, CEIL_MID - 18.0 - BAY_CEIL_DROP, yaw=90)
place('Back_CeilingPipes', B + 'SM_Bld_Ceiling_Pipes_Straight_01', 100.0, 0.0, CEIL_MID - 401.0 - BAY_CEIL_DROP, yaw=0)
place('Back_CeilingBox', P + 'SM_Prop_Detail_CeilingBox_02', 250.0, 470.0, CEIL_MID - 18.0, yaw=0)
# Fire-red pulsing lamps: a lamp head (a 26 cm column standing on its z=0
# base) laid with its base on the wall -- roll 90 points +Z into the room
# from a south wall, pitch -90 from a west wall, pitch 90 from an east wall
# -- and a crimson flicker light just in front of it.
CRIMSON = unreal.LinearColor(r=0.85, g=0.03, b=0.06, a=1.0)
def red_lamp(label, x, y, z, pitch, roll, dx, dy, seed, base=5.0):
    place(label, P + 'SM_Prop_Light_Small_08', x, y, z, yaw=0, pitch=pitch, roll=roll, mat=False)
    def apply(a):
        a.set_editor_property('base_intensity', base); a.set_editor_property('seed', seed)
        a.set_editor_property('attenuation_radius', 480.0); a.set_editor_property('speed', 0.8 + 0.3 * seed)
        a.set_editor_property('color', CRIMSON)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location(unreal.Vector(x + dx, y + dy, z), False, True)
    def spawn():
        a = eas.spawn_actor_from_class(unreal.FlickerLightActor, unreal.Vector(x + dx, y + dy, z)); apply(a); return a
    ensure(label + '_Glow', spawn, apply)
AMBER_GLOW = unreal.LinearColor(r=1.0, g=0.38, b=0.05, a=1.0); COLD_GLOW = unreal.LinearColor(r=0.55, g=0.7, b=1.0, a=1.0)
def red_glow(label, x, y, z, seed, base=5.0, color=None, regularity=0.0, period=2.0, flares=0.5, speed=None):
    def apply(a):
        a.set_editor_property('base_intensity', base); a.set_editor_property('seed', seed)
        a.set_editor_property('attenuation_radius', 520.0); a.set_editor_property('speed', speed if speed is not None else 0.8 + 0.3 * seed)
        a.set_editor_property('color', color or CRIMSON)
        a.set_editor_property('regularity', regularity); a.set_editor_property('period', period)
        a.set_editor_property('flares_per_second', flares)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location(unreal.Vector(x, y, z), False, True)
    def spawn():
        a = eas.spawn_actor_from_class(unreal.FlickerLightActor, unreal.Vector(x, y, z)); apply(a); return a
    ensure(label, spawn, apply)
# Tucked behind the machines, cabinet and pipe pillar so only the glow shows.
red_glow('RedLamp_1_Glow', 150.0, 110.0, 70.0, 0.3, regularity=1.0, period=1.7)                 # steady beat
red_glow('RedLamp_2_Glow', 760.0, 70.0, 90.0, 1.1, regularity=0.5, period=2.6)                  # beat with flicker on it
red_glow('RedLamp_3_Glow', 40.0, 330.0, 60.0, 2.2, base=4.0, color=AMBER_GLOW, regularity=0.0, flares=3.4, speed=2.6)   # amber, hard stutter
red_glow('RedLamp_4_Glow', 520.0, 40.0, 330.0, 3.4, base=4.5, regularity=1.0, period=3.1)       # slow steady beat, high
red_glow('RedLamp_5_Glow', 965.0, 110.0, 40.0, 4.1, base=3.5, color=COLD_GLOW, regularity=0.0, flares=2.8, speed=2.2)   # a cold one, hard stutter
# More of them along the back, a couple failing outright: the back of the bay should never settle.
red_glow('RedLamp_6_Glow', 300.0, 55.0, 215.0, 5.3, base=5.5, regularity=0.0, flares=4.2, speed=3.1)                    # the worst of them
red_glow('RedLamp_7_Glow', 690.0, 45.0, 150.0, 6.7, base=4.2, regularity=0.0, flares=3.6, speed=2.8)
red_glow('RedLamp_8_Glow', 120.0, 250.0, 300.0, 7.9, base=3.8, color=AMBER_GLOW, regularity=0.0, flares=2.4, speed=1.9)
red_glow('RedLamp_9_Glow', 880.0, 210.0, 260.0, 9.1, base=4.6, regularity=0.0, flares=3.9, speed=3.4)                   # and its neighbour
light('BedGlow', 300, 720, 150, 9, 480, (0.55, 1.0, 0.65))
door_lamp('DoorLamp_Bay', 500.0, H + WALL_T - 34.0 - 8.0, 368.0, 180)
light('MachineGlow', 500, 260, 300, 7, 600, (1.0, 0.45, 0.2))
# Button panels on the back (south) wall between and beside the machines. The
# props are plates with their back at z=0 and face +Z; FRotator(0, 180, -90)
# turns +Z onto +Y (into the room) with the panel's +Y up. Wall_01's main face
# sits 14 cm inside the wall line (local y 75 of 89), so the plates go at y=-13.
def wall_panel(label, mesh, x, z, y=-13.0):
    place(label, P + mesh, x, y, z, yaw=180, roll=-90, mat=False)
wall_panel('Back_Panel_Buttons_03', 'SM_Prop_Buttons_03', 500.0, 205.0)   # the big red-square board
wall_panel('Back_Panel_Buttons_04', 'SM_Prop_Buttons_04', 448.0, 150.0)
wall_panel('Back_Panel_Buttons_02', 'SM_Prop_Buttons_02', 500.0, 150.0)
wall_panel('Back_Panel_Buttons_06', 'SM_Prop_Buttons_06', 553.0, 150.0)

# ---- The back of the bay breathes --------------------------------------------------------------
# Atmosphere came out of this map once already, and for a good reason: it was everywhere, it was
# either invisible or a wall of white, and its billboards kept getting selected in the editor.
# What goes back in is deliberately the opposite of that. It is in ONE place -- the back third of
# the bay, which is the machinery end and the part of the room that never gets tidied -- and the
# jets are events rather than a permanent hiss.
#
# New label prefixes on purpose. `Steam_` and `Haze_` are on the manifest's REMOVE list forever,
# and they should stay there; this is a new, smaller idea, not the old one creeping back.
#
# All of it lands in the `Atmosphere` outliner folder and is location-locked (see _tidy_fx), so
# a stray click in the viewport cannot grab it and the whole lot hides with one eye icon.
BAY_MIST_Z = 6.0          # sits ON the floor; any higher and the card's own edge shows
BAY_MIST_SCALE = 1.6      # how far one card spreads
BAY_MIST_FLAT = 0.30      # squashed down: fog that pools, not fog that fills the room
for _i, _mx in enumerate((190.0, 500.0, 810.0)):
    steam('BayMist_Back_%d' % _i, 'groundfog', _mx, 210.0, BAY_MIST_Z,
          yaw=_i * 37.0, scale=BAY_MIST_SCALE, zscale=BAY_MIST_FLAT)

# Two jets, and only two. The first is the wall vent that is already modelled there
# (Back_Vent at 860, 21, 340) -- a vent that visibly does something is worth more than six that
# do not. yaw 90 blows it along +Y, out of the wall and into the room; a little pitch lifts it
# so it climbs rather than spraying the floor.
pulsed_steam('BayJet_Vent', 'jet_small', 855.0, 45.0, 330.0, yaw=90.0, pitch=12.0, scale=0.85,
             burst=1.9, quiet=21.0, jitter=0.35, sound='steam_burst_1.wav', volume=0.30)
# The second is down at the pipe pillar in the corner (Back_PipePillar at 940, 60), venting
# sideways along the back wall at knee height. Different length, different gap and a different
# sample, so the two never read as one effect on a timer.
pulsed_steam('BayJet_Pipe', 'jet_small', 925.0, 105.0, 55.0, yaw=180.0, pitch=6.0, scale=0.6,
             burst=1.986, quiet=33.0, jitter=0.4, sound='steam_burst_2.wav', volume=0.22)
wall_panel('Back_Panel_Buttons_07', 'SM_Prop_Buttons_07', 120.0, 190.0)
wall_panel('Back_Panel_Buttons_01', 'SM_Prop_Buttons_01', 170.0, 190.0)
wall_panel('Back_Panel_Buttons_05', 'SM_Prop_Buttons_05', 120.0, 140.0)
wall_panel('Back_Panel_Buttons_10', 'SM_Prop_Buttons_10', 170.0, 140.0)
wall_panel('Back_Panel_Buttons_08', 'SM_Prop_Buttons_08', 850.0, 200.0)
wall_panel('Back_Panel_Buttons_12', 'SM_Prop_Buttons_12', 812.0, 150.0)
# Buttons_01's red bars are not emissive (checked unlit); a small red glow in front of it.
light('Back_Panel_Buttons_01_Light', 170.0, 6.0, 188.0, 1.2, 140, (1.0, 0.12, 0.06))
# A small green work lamp on the cart beside Jun (590,500 facing NW): its glow lifts his face out of the shadow.
place('JunLamp', P + 'SM_Prop_Light_Small_08', 588.0, 722.0, 162.0, yaw=60, mat=False)   # on the cart (user moved it to 570,710)
light('JunLamp_Light', 598.0, 716.0, 178.0, 4.5, 320, (0.32, 1.0, 0.45))   # CRT green

# ---- The foyer ---------------------------------------------------------------
# north='open' now, not 'door'. The foyer's north doorway is the LIFT: SM_Bld_Lift_Wall_01 is a
# complete 500 wall piece (x 0..500, 91.5 thick, z -99.5..400) with the shaft opening already in
# it, and the lift actor's own leaves fill that opening and work on approach. shell()'s 'door'
# put a STATIC Doorframe_02 + Doorframe_Door_02 pair in the same slot, 31 cm in front of the
# shaft doors: a solid mesh that never opens, standing between the player and the lift. That is
# the door you could see and could not get through.
shell(0, FY0, NX, FNY, 'Foyer', south='open', north='open')
# shell() lays the last north piece with the EXT overhang -- pivot at x0 + 1250 + 60, stretched
# 1.12 -- so Foyer_Wall_N2 runs from 750 to 1310 at yaw 180. The foyer ends at 1000 and the
# cafeteria's grid starts at GX 1089, so the piece's last 220 cm stand INSIDE the cafeteria
# with the black back out; the corner pillar (995..1150) hides some of it and Tools/
# audit_wall_backs.py found the rest, uncapped, at x 1180..1300. Laid again here to end under
# the pillar: pivot 1150, and 400/500 of the length so its west end stays at 750.
place('Foyer_Wall_N2', B + 'SM_Bld_Wall_01', 1150.0, FY0 + FNY * CELL + WALL_T, 0, yaw=180, scale=(0.8, 1.0, 1.0))

# The bay/foyer door: one Doorframe_05 centred on the shared grid line the
# two back-to-back walls meet on (Synty's own convention, see
# Docs/SyntySpaceKit_Conventions.md), with sliding split leaves. The actor's
# origin is the frame pivot: left end of the 500 segment, yaw along the wall.
def sliding_door(label, x, y, yaw, kind='wide', swing=1.0, locked=False, hold_open=False, material=None):
    door_mat = (unreal.load_asset(material) if material else None) or grime
    def apply(a):
        a.configure({'hatch': unreal.DoorKind.HATCH, 'lift': unreal.DoorKind.LIFT, 'cabin': unreal.DoorKind.CABIN}.get(kind, unreal.DoorKind.WIDE), swing, locked)
        a.set_editor_property('hold_open', hold_open)
        a.get_editor_property('frame').set_mobility(unreal.ComponentMobility.MOVABLE)   # a static frame would stay at the origin
        a.get_editor_property('frame').set_relative_location(unreal.Vector(0, 0, 0), False, True)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x, y, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False, True)
        if door_mat:
            for c in (a.get_editor_property('frame'), a.get_editor_property('leaf_l'), a.get_editor_property('leaf_r')):
                for i in range(c.get_num_materials()): c.set_material(i, door_mat)
    def spawn():
        a = eas.spawn_actor_from_class(unreal.SlidingDoorActor, unreal.Vector(x, y, 0.0)); apply(a); return a
    ensure(label, spawn, apply)
sliding_door('Door_BayFoyer', 250.0, H + WALL_T, 0.0)
# Our own trim (RawArt-free, GeometryScript, Synty greys) on the frame's flat
# plates: strips beside the door and the band above it, both rooms. The plate
# faces are 34 either side of the frame line; yaw 180 mirrors the mesh for the bay.
place('Door_Trim_Bay', '/Game/RepliCan/Trim/SM_Trim_Doorframe05_01', 250.0, H + WALL_T - 34.0, 0, yaw=0)
place('Door_Trim_Foyer', '/Game/RepliCan/Trim/SM_Trim_Doorframe05_01', 750.0, H + WALL_T + 34.0, 0, yaw=180)
# The doorway floor: the two rooms' floors stop at their walls, so the 178 cm
# between them under the door is floored with two small tiles squeezed to fit.
for k, x in enumerate([250.0, 500.0]):
    place('Door_Threshold_%d' % (k + 1), B + 'SM_Bld_Floor_Small_02', x, H, 0, yaw=0, scale=(1.0, (2 * WALL_T) / 250.0, 1.0))
place('Foyer_Crate_1', P + 'SM_Prop_Crate_Wide_01', 900.0, FY0 + 120.0, 0, yaw=5)
place('Foyer_Crate_2', P + 'SM_Prop_Crate_01', 900.0, FY0 + 200.0, 0, yaw=-12)
place('Foyer_Crate_3', P + 'SM_Prop_Crate_01', 905.0, FY0 + 120.0, 49.0, yaw=22)
# Lids on: the foyer crates are stowed, not in use.
place('Foyer_Crate_2_Lid', P + 'SM_Prop_Crate_Lid_01', 900.0, FY0 + 200.0, 0, yaw=-12)
place('Foyer_Crate_3_Lid', P + 'SM_Prop_Crate_Lid_01', 905.0, FY0 + 120.0, 49.0, yaw=22)
place('Foyer_Tank', P + 'SM_Prop_Detail_Tank_01', 75.0, FY0 + 935.0, 0, yaw=-10)      # north of the cafeteria door
place('Foyer_Barrel', P + 'SM_Prop_Barrel_01', 75.0, FY0 + 120.0, 0, yaw=40)
place('Foyer_Cart', P + 'SM_Prop_Cart_01', 95.0, FY0 + 345.0, 0, yaw=5)                # west side, clear of both new doors
place('Foyer_Pipes', P + 'SM_Prop_Detail_Pipes_02', 989.0, FY0 + 150.0, 0, yaw=90)   # off the doorway, onto the east wall
place('Foyer_Wires', P + 'SM_Prop_Wires_01', 960.0, FY0 + 400.0, 250.0, yaw=0)
for (i, j, nm, sh) in [(0, 0, 'W', True), (1, 1, 'E', False), (0, 1, 'N', False), (1, 0, 'S', False)]:   # one per tile
    cx, cy = strip_center(i, j, FY0, SYF); fixture('Foyer_Fixture_' + nm, cx, cy, CEIL_MID, intensity=22.0, shadows=sh)
door_lamp('DoorLamp_Exit', 500.0, FY1 - 3.0, 340.0, 180)

# ---- PCS signs: masked planes a hair off the wall (decals did not show) ----
# Plane +Z is turned onto the wall (measured: pitch -90 sends +Z to +X, roll
# +90 sends +Z to +Y): pitch 90 faces -X (east wall), pitch -90 faces +X
# (west wall), roll -90 faces -Y (a north wall's inner face), roll 90 faces
# +Y (a south wall's). Pitched planes have local X vertical, so they
# take the quarter-turned textures; rolled planes keep local X horizontal.
def sign(label, mat_name, x, y, z, pitch, roll, sx, sy):
    def spawn():
        plane = unreal.load_asset('/Engine/BasicShapes/Plane'); mat = unreal.load_asset('/Game/RepliCan/Materials/' + mat_name)
        if not plane or not mat: print('MISSING sign plane/material', mat_name); return None
        a = eas.spawn_actor_from_object(plane, unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=0))
        a.set_actor_scale3d(unreal.Vector(sx, sy, 1.0))
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.STATIC); c.set_material(0, mat)
        return a
    def revise(a):
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.MOVABLE)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=0), False, True); a.set_actor_scale3d(unreal.Vector(sx, sy, 1.0))
        c.set_mobility(unreal.ComponentMobility.STATIC)
    return ensure(label, spawn, revise)
sign('Sign_Bay_Door', 'M_Sign_Wide', 500.0, H - 2.0, 335.0, 0, -90, 3.0, 0.75)          # north wall, over the door
sign('Sign_Bay_Badge', 'M_Sign_Badge_L', 2.0, 1000.0, 235.0, -90, 0, 1.7, 1.7)         # west wall
sign('Sign_Foyer_Airlock', 'M_Sign_Wide', 500.0, FY0 + 2.0, 335.0, 0, 90, 3.0, 0.75)  # foyer south wall
sign('Sign_Foyer_Exit', 'M_Sign_Mark', 500.0, FY1 - 2.0, 340.0, 0, -90, 0.84, 0.84)     # foyer north wall
sign('Sign_Foyer_Stencil', 'M_Sign_Stencil_L', 2.0, FY0 + 220.0, 190.0, -90, 0, 0.9, 1.8)  # foyer west wall, south of the cafeteria door

# The PCS poster behind Hannah (she stands at 470,720 facing -X; the camera
# looks past her at the east wall). A basic plane pitched onto the wall so
# its +Z faces -X into the room; scale X is the poster's height, Y its width.
def poster(label, x, y, z, height, width):
    def spawn():
        plane = unreal.load_asset('/Engine/BasicShapes/Plane')
        mat = unreal.load_asset('/Game/RepliCan/Materials/M_PCS_Poster')
        if not plane or not mat: print('MISSING poster plane/material'); return None
        a = eas.spawn_actor_from_object(plane, unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=90, yaw=0))   # pitch 90: +Z faces -X, into the room
        a.set_actor_scale3d(unreal.Vector(height / 100.0, width / 100.0, 1.0))
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.STATIC); c.set_material(0, mat)
        return a
    def revise(a):
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.MOVABLE)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=90, yaw=0), False, True)
        a.set_actor_scale3d(unreal.Vector(height / 100.0, width / 100.0, 1.0))
        c.set_mobility(unreal.ComponentMobility.STATIC)
    return ensure(label, spawn, revise)
# A wall-hung screen: Radar_Panel_02 (128 x 128, 24 deep, pivot on its back)
# as the bezel, stretched to 140 x 192 and mounted on the east wall to the
# right of Hannah as seen from the bed, with the poster plane on its face.
# The billboard covers the east wall's recessed panel on the y 1000..1500
# segment (measured: recess spans local x 19..481, z 80..234 behind the
# wall's main face at local y 75, i.e. world x 1014): a 462 x 154 plane a
# centimetre proud of that face.
# ... now on the recess floor itself (local y 29 -> world x 1060): a
# Greeble_Panel_01 plate (470 x 116 x 10, pivot on its back) as the mounting
# frame, 423 x 145 after scaling, lying on that floor; the 400 x 133 board a
# centimetre in front of the plate.
place('PCS_Poster_Frame', P + 'SM_Prop_Greeble_Panel_01', 1060.0, 1275.0, 157.0, yaw=0, pitch=90, scale=(1.25, 0.5, 1.0))   # 235 x 145 plate
poster('PCS_Poster_Bay', 1049.0, 1275.0, 157.0, 133.0, 215.0)   # golden ratio: 133 tall, 215 wide

# The whiteboard opposite the billboard: same plate on the west wall's recess
# (local y 29 -> world x -60), pitched the other way so it faces +X.
def board(label, mat_name, x, y, z, pitch, height, width, yaw=0.0):
    def spawn():
        plane = unreal.load_asset('/Engine/BasicShapes/Plane'); mat = unreal.load_asset('/Game/RepliCan/Materials/' + mat_name)
        if not plane or not mat: print('MISSING board plane/material', mat_name); return None
        a = eas.spawn_actor_from_object(plane, unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        a.set_actor_scale3d(unreal.Vector(height / 100.0, width / 100.0, 1.0))
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.STATIC); c.set_material(0, mat)
        return a
    def revise(a):   # a revised board takes the current material, place and size
        mat = unreal.load_asset('/Game/RepliCan/Materials/' + mat_name)
        c = a.static_mesh_component; c.set_mobility(unreal.ComponentMobility.MOVABLE)
        if mat: c.set_material(0, mat)
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=pitch, yaw=yaw), False, True)
        a.set_actor_scale3d(unreal.Vector(height / 100.0, width / 100.0, 1.0))
    return ensure(label, spawn, revise)
place('Whiteboard_Frame', P + 'SM_Prop_Greeble_Panel_01', -60.0, 1275.0, 157.0, yaw=0, pitch=-90, scale=(1.25, 0.5, 1.0))
board('Whiteboard_Bay', 'M_PCS_Whiteboard', -49.0, 1275.0, 157.0, -90, 133.0, 215.0)

# Sticky notes on the whiteboard. The basement is covered in them, so somebody up here has the
# same habit.
#
# MEASURED, because these do not sit the way a prop usually does. The pack authors a note
# STANDING and HANGING: x -8.21..8.21 (its width), y -0.00..0.68 (the sheet itself -- it is thin
# in Y, so its face points -Y), z -16.37..0.04 (it hangs DOWN from a pivot at its top edge).
#
# The board is a Plane at x -49 pitched -90 so its normal is +X, spanning y 1167.5..1382.5 and
# z 90.5..223.5. To turn a note's -Y face onto +X the actor is yawed +90 (yaw +90 sends +Y to
# -X, so -Y goes to +X). That also carries the note's width onto world Y, which is the way the
# board runs.
#
# The jaunty angles are PITCH, not roll. After the yaw, the note's own Y axis is the world X
# axis, and a rotation about it is what tips a note in the plane of the board; roll would tip it
# out of the wall. Rotators apply roll, then pitch, then yaw, so the pitch is taken in the
# note's own frame and comes out right.
NOTE_MESH = '/Game/Synty/PolygonSciFiHorror/Meshes/Props/'
NOTE_X = -48.2          # the sheet's face, a couple of millimetres proud of the board
NOTES = [
    ('01', 'SM_Prop_StickyNote_01',        1205.0, 210.0,  -6.0),
    ('02', 'SM_Prop_StickyNote_Curved_02', 1248.0, 196.0,   4.5),
    ('03', 'SM_Prop_StickyNote_03',        1300.0, 214.0,  -2.0),
    ('04', 'SM_Prop_StickyNote_Curved_05', 1349.0, 203.0,   7.0),
    ('05', 'SM_Prop_StickyNote_05',        1222.0, 160.0,   3.0),
    ('06', 'SM_Prop_StickyNote_Curved_08', 1331.0, 152.0,  -5.5),
]
for _tag, _mesh, _ny, _nz, _tilt in NOTES:
    # mat=False: a note is paper and keeps the horror pack's own painted colour rather than
    # taking the station's grime like a wall panel would.
    place('Whiteboard_Note_' + _tag, NOTE_MESH + _mesh, NOTE_X, _ny, _nz,
          yaw=90.0, pitch=_tilt, mat=False)

# Blank backboards on the foyer's flat wall faces (traced: west face x -37.2, north
# faces y 2715.2, south faces y 1640.8): a black 235 x 145 slab against the wall
# with the blank 215 x 133 board 1 cm in front of it, nothing on them yet. yaw
# turns the slab's X / the pitched board's normal onto the wall's normal (-Y: -90, +Y: 90).
CUBE = '/Engine/BasicShapes/Cube'
def backboard(label, fx, fy, nx, ny, z=175.0, mat='MI_BlankBoard'):
    yaw = {(1, 0): 0.0, (0, -1): -90.0, (0, 1): 90.0, (-1, 0): 180.0}[(nx, ny)]
    place(label + '_Frame', CUBE, fx + 2.0 * nx, fy + 2.0 * ny, z, yaw=yaw, scale=(0.04, 2.35, 1.45), mat=False, material='/Game/RepliCan/Materials/M_Black')   # a plain black slab, 4 thick
    board(label, mat, fx + 5.0 * nx, fy + 5.0 * ny, z, -90, 133.0, 215.0, yaw=yaw)
# Candidate posters (RawArt/T_Poster_*_Upright.png to review; drawn by the scratch
# posters.ps1, imported as T_Poster_* / M_Poster_*).
backboard('Foyer_Board_W', -37.2, 1928.0, 1, 0, z=157.0, mat='M_Poster_IronGate')   # centred on the wall's panel band
backboard('Foyer_Board_N0', 150.0, 2715.2, 0, -1, mat='M_Poster_Forget')
backboard('Foyer_Board_N2', 850.0, 2715.2, 0, -1, mat='M_Poster_Dreams')
backboard('Foyer_Board_S0', 80.0, 1640.8, 0, 1, mat='M_Poster_Smile')
backboard('Foyer_Board_S2', 920.0, 1640.8, 0, 1, mat='M_Poster_Hydrate')

# =============================================================================
# The crew wing and the cafeteria: Synty's own convention.
# New rooms follow Docs/SyntySpaceKit_Conventions.md exactly: unstretched 500
# tiles, wall bodies INSIDE the tile they stand on (pivot on the grid line,
# detailed face into the room, main face 75 in from the line), door frames
# and crew modules straddling the shared line (they clip cleanly into the
# wall bodies at the corners), corner pillars at wall/wall inside corners.
# Floors and ceilings stay continuous, so no thresholds are needed. The
# foyer's stretched floor and ceiling reach its wall lines (x -89 / 1089),
# which is where the two new grids start.
# =============================================================================
WP = '/Game/PolygonSciFiWorlds/Models/Props/'; CP = '/Game/PolygonCyberCity/Meshes/Props/'   # other packs: keep their own materials (mat=False)
TRIM = '/Game/RepliCan/Trim/SM_Trim_Doorframe05_01'

def tile(label, x, y, floor=None, ceiling_yaw=0.0, floor_yaw=0.0):
    floor = floor or ROOM_FLOOR
    if floor_yaw == 90.0: place(label + '_Floor', B + floor, x + CELL, y, 0, yaw=90)   # pivot at the tile's +x corner: covers x..x+500
    else: place(label + '_Floor', B + floor, x, y, 0, yaw=0)
    if ceiling_yaw == 0.0: place(label + '_Ceiling', B + 'SM_Bld_Ceiling_01', x, y, 0, yaw=0)            # light strip along y at x + 227.5
    else: place(label + '_Ceiling', B + 'SM_Bld_Ceiling_01', x, y + CELL, 0, yaw=-90)                   # light strip along x at y + 272.5

def line_piece(label, side, x0, y0, mesh, offset=0.0, scale=None):
    """A wall-like piece (pivot at its left end, detailed side local +y) on one side of tile (x0, y0),
    body inside the tile / open side into it. `offset` walks along the line from the side's start.
    `mesh` is a kit mesh name, or a full asset path (starts with /) for a cut piece."""
    if mesh.startswith('/'): mesh = mesh[len(B):] if mesh.startswith(B) else mesh
    path = mesh if mesh.startswith('/') else B + mesh
    if side == 'S': place(label, path, x0 + offset, y0, 0, yaw=0, scale=scale)
    elif side == 'N': place(label, path, x0 + CELL - offset, y0 + CELL, 0, yaw=180, scale=scale)
    elif side == 'W': place(label, path, x0, y0 + CELL - offset, 0, yaw=-90, scale=scale)
    elif side == 'E': place(label, path, x0 + CELL, y0 + offset, 0, yaw=90, scale=scale)
def wall_in(label, side, x0, y0, mesh='SM_Bld_Wall_01'): line_piece(label, side, x0, y0, mesh)

def pillar_in(label, corner, x0, y0, mesh='SM_Bld_Wall_Corner_Pillar_01'):
    """An inside corner pillar of tile (x0, y0): pivot on the grid corner, growing into the tile."""
    x, y, yaw = {'SW': (x0, y0, 0.0), 'SE': (x0 + CELL, y0, 90.0), 'NE': (x0 + CELL, y0 + CELL, 180.0), 'NW': (x0, y0 + CELL, -90.0)}[corner]
    place(label, B + mesh, x, y, 0, yaw=yaw)

def rot(x, y, yaw):
    a = math.radians(yaw); return (x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a))
def door_trims(label, x, y, yaw):
    """The Doorframe_05 trim on both plate faces (34 either side of the line), as at the bay door."""
    dx, dy = rot(0.0, -34.0, yaw); place(label + '_A', TRIM, x + dx, y + dy, 0, yaw=yaw)
    dx, dy = rot(500.0, 34.0, yaw); place(label + '_B', TRIM, x + dx, y + dy, 0, yaw=yaw + 180.0)
def loot_box(label, x, y, yaw, items):
    """A container the player can open (ALootBoxActor: crate + lid, the transfer screen)."""
    def apply(a):
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x, y, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False, True)
        a.set_editor_property('items', items)
        if grime:
            for c in (a.get_editor_property('crate'), a.get_editor_property('lid')):
                for i in range(c.get_num_materials()): c.set_material(i, grime)
    def spawn():
        a = eas.spawn_actor_from_class(unreal.LootBoxActor, unreal.Vector(x, y, 0.0)); apply(a); return a
    ensure(label, spawn, apply)

def door_sign(label, x, y, yaw, text, width=470.0):
    """A dot-matrix LED strip in the lintel plate of the door's far-side (B) trim: the trim's
    inset plate spans local x 60..440, z 352..384 on its outer face (local y -8)."""
    if not hasattr(unreal, 'SignActor'): print('SignActor not built yet: skipping', label); return
    byaw = yaw + 180.0
    bx, by = rot(500.0, 34.0, yaw)
    sx, sy = rot(250.0, -8.6, byaw)
    def apply(a):
        # Hand placement wins; see REPOSITION.
        if may_move(label):
            a.set_actor_location_and_rotation(unreal.Vector(x + bx + sx, y + by + sy, 373.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=byaw - 90.0), False, True)
        # Sized to the door bay rather than to the trim's inset plate: measured, the bay between
        # the wall segments is 500 wide and the bulkhead above starts at z 396, so 470 x 42
        # centred on 373 fills it with ~15 cm of padding at each end and clears the bulkhead.
        # The housing cube scales with these, so the backing plate grows with the face.
        a.set_editor_property('width', width)
        a.set_editor_property('height', 42.0)
        # set_text() is what rebuilds the face. Writing the 'text' property alone changes the
        # value and nothing else -- OnConstruction does not fire for a scripted property write,
        # so every text change made that way sat invisible until the level was reloaded.
        a.set_text(text)
    def spawn():
        a = eas.spawn_actor_from_class(unreal.SignActor, unreal.Vector(x, y, 373.0)); apply(a); return a
    ensure(label, spawn, apply)

def door_lamps(label, x, y, yaw, z=368.0):
    """Lintel lamps on both sides, the same geometry as DoorLamp_Airlock relative to its door."""
    for suffix, (lx, ly), dyaw, ny in (('_A', (250.0, -42.0), 180.0, 16.0), ('_B', (250.0, 42.0), 0.0, -16.0)):
        dx, dy = rot(lx, ly, yaw); ex, ey = rot(0.0, ny, yaw)
        place(label + suffix, P + 'SM_Prop_Light_Small_02', x + dx, y + dy, z, yaw=yaw + dyaw, roll=90, mat=False)
        light(label + suffix + '_Light', x + dx + ex, y + dy + ey, z - 12, 12, 600, (1.0, 0.56, 0.26))

# ---- The foyer's side doors: RIGHT to the crew hall, LEFT to the cafeteria ----
# Coming north out of the bay the player faces +Y; in Unreal's left-handed frame
# the right hand then points -X (west). So the hall runs west, the cafeteria east.
GX = W + WALL_T            # 1089: the foyer's east wall line, where the cafeteria's grid starts
CX = -WALL_T               # -89: the foyer's west wall line, where the crew hall's grid starts
HY = FY0 + CELL            # 2178: the foyer's second tile row = the hall's row (both frames replace Foyer_Wall_E1 / W1)
sliding_door('Door_FoyerHall', CX, HY + CELL, -90.0); door_trims('Door_Trim_Hall', CX, HY + CELL, -90.0)
sliding_door('Door_FoyerCaf', GX, HY, 90.0); door_trims('Door_Trim_Caf', GX, HY, 90.0)
# The wall between the foyer and the cafeteria south of the door becomes a window wall on both
# sides (Wall_04 with its glass), openings aligned: the foyer side is re-laid unstretched from
# y 1678 (the SE pillar already covers the 60 cm below it).
place('Foyer_Wall_E0', B + 'SM_Bld_Wall_04', GX, FY0, 0, yaw=90, scale=(1.0, 1.0, 1.0))
place('Foyer_Window_Glass', B + 'SM_Bld_Wall_Glass_04', GX, FY0, 0, yaw=90, mat=False)
place('Caf_Wall_W0', B + 'SM_Bld_Wall_04', GX, FY0 + CELL, 0, yaw=-90)
place('Caf_Window_Glass', B + 'SM_Bld_Wall_Glass_04', GX, FY0 + CELL, 0, yaw=-90, mat=False)
# The wide doors stand 400 tall under a coffered ceiling whose raised channels
# (underside 420, top 449) cross the door line: a bulkhead beam fills z 396..449
# over each door's 500 width, the trims' depth (80).
BULKHEAD = '/Game/RepliCan/Materials/MI_Bulkhead'
place('Door_Fill_Bay', CUBE, 500.0, 1589.0, 422.5, yaw=0, scale=(5.0, 0.8, 0.53), mat=False, material=BULKHEAD)
place('Door_Fill_Hall', CUBE, CX, HY + 250.0, 422.5, yaw=-90, scale=(5.0, 0.8, 0.53), mat=False, material=BULKHEAD)
place('Door_Fill_Caf', CUBE, GX, HY + 250.0, 422.5, yaw=90, scale=(5.0, 0.8, 0.53), mat=False, material=BULKHEAD)
# =============================================================================
# Security cameras (Cyber City's CCTV parts) in the rooms' upper corners -- ALL REMOVED on
# 2026-09-16 at the user's request; the calls are kept commented so the placements survive.
# wall plate (Camera_01, 28 x 41, stub out at local +Y 15.7, z -1) on a traced
# flat wall face at z 345, the down-angled arm (Arm_01, plain end at the stub, hex
# end at local (0, 24.3, -20.8)) and the camera pod (Base_01, lens at +Y, side
# bracket bolts at local (+-15, 14.5, 27)) hung beside the arm's hex so the plate
# and arm stay visible, nosed down 25 and yawed into the room. Dark palette 03_B. psi turns the plate's +Y onto the wall normal (+Y 0, -Y 180, +X -90,
# -X 90); phi is the pod's yaw off the wall normal.
CC = '/Game/PolygonCyberCity/Meshes/Props/'; CCTV_MAT = '/Game/RepliCan/Materials/MI_CCTV_Wall'   # palette 04_B (white, blue stripe) at 0.4 emissive: matches the walls; MI_CCTV_Dark is the navy 03_B alternative
def cctv(label, fx, fy, psi, phi=0.0, z=345.0):
    # Synty's own assembly, read off Cyber City's Demo_Interior map (plate-local, plate at yaw 0
    # with its stub out along +Y): Arm_01 (the down link) at (-1.4, 10.7, -1.1) unrotated,
    # Arm_02 (the straight link) at (-1.4, 34.7, -21.8) rolled 19, the pod at (-1.4, 67.1, -26.7)
    # aimed freely (they roll it 14-46 nose-down and yaw it up to 45 off the wall). The pod's
    # back ends 43 clear of the wall. psi turns the plate's +Y onto the wall normal
    # (+Y 0, -Y 180, +X -90, -X 90); phi is the pod's yaw off that normal, into the room.
    n = rot(0.0, 1.0, psi)
    px, py = fx + n[0], fy + n[1]
    T = unreal.Transform(unreal.Vector(px, py, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=psi), unreal.Vector(1, 1, 1))
    j1 = T.transform_location(unreal.Vector(-1.4, 10.7, -1.1))
    j2 = T.transform_location(unreal.Vector(-1.4, 34.7, -21.8))
    pd = T.transform_location(unreal.Vector(-1.4, 67.1, -26.7))
    place(label + '_Plate', CC + 'SM_Prop_Camera_01', px, py, z, yaw=psi, mat=False, material=CCTV_MAT)
    place(label + '_Arm', CC + 'SM_Prop_Camera_01_Arm_01', j1.x, j1.y, j1.z, yaw=psi, mat=False, material=CCTV_MAT)
    place(label + '_Arm2', CC + 'SM_Prop_Camera_01_Arm_02', j2.x, j2.y, j2.z, yaw=psi, roll=19.0, mat=False, material=CCTV_MAT)
    place(label + '_Pod', CC + 'SM_Prop_Camera_01_Base_01', pd.x, pd.y, pd.z, yaw=psi + phi, roll=30.0, mat=False, material=CCTV_MAT)
# cctv('CCTV_Bay_SW', 23.0, -37.2, 0.0, -25.0)
# cctv('CCTV_Bay_NE', 977.0, 1537.2, 180.0, -25.0)
# cctv('CCTV_Foyer_SW', 23.0, 1640.8, 0.0, -25.0)
# cctv('CCTV_Foyer_NE', 900.0, 2715.2, 180.0, -25.0)     # 150 in: the NE pillar stands proud of the wall
# cctv('CCTV_Caf_SW', 1201.0, 1729.8, 0.0, -25.0)
# cctv('CCTV_Caf_NE', 2477.0, 3126.2, 180.0, -25.0)
# cctv('CCTV_Hall_W', -3537.2, 2567.0, -90.0, -10.0)     # the west end wall, looking down the hall
# cctv('CCTV_Hall_E', -183.0, 2228.5, 0.0, 45.0)         # by the foyer door, turned down the hall
# (no camera in the cabin: a replicant's room is not watched, or at least not visibly)
# LED signs over the three double doors, on their foyer-side lintel plates.
door_sign('Sign_Door_Bay', 250.0, H + WALL_T, 0.0, 'PROMPT CRITICAL|SERVICES')
door_sign('Sign_Door_Hall', CX, HY + CELL, -90.0, 'REPLICANT CABINS|CREW HALL  S1-S4')
door_sign('Sign_Door_Caf', GX, HY, 90.0, 'CAFETERIA|INOPERATIVE')

# ---- The crew hall: 7 tiles west from the foyer door, four cabins on each side ----
HN = 7
def hall_tile_x(t): return CX - (t + 1) * CELL           # tile t counted from the foyer end, pivot at its west edge
for t in range(HN):
    tile('Hall_%d' % t, hall_tile_x(t), HY, floor=HALL_FLOOR, ceiling_yaw=-90.0, floor_yaw=90.0)   # the cable strips run along the hall, under the wall lines
wall_in('Hall_Wall_W', 'W', hall_tile_x(HN - 1), HY)
pillar_in('Hall_Pillar_SW', 'SW', hall_tile_x(HN - 1), HY); pillar_in('Hall_Pillar_NW', 'NW', hall_tile_x(HN - 1), HY)
# Each long side, from the foyer end: [blank 250] then four times [hatch door 500][blank 250]: 3500 = the 7 tiles.
# The cabin behind each door is a tile on the far side of the line; only cabin S1 (room four:
# out of the foyer door, right, second door on the right) is built so far, the other seven doors are locked.
CAB_X = [CX - 750.0 - k * 750.0 for k in range(4)]      # door / cabin tile pivots: -839, -1589, -2339, -3089
DOOR_D = 15.0                                           # the cabin tile's door edge sits this far short of the hall's door line: the inner Doorframe_06 is centred 7.5 behind the line and is 45 thick, so its cabin face is at HY - 15 (the lift pair needed 92)
def hall_side(side):
    y = HY + CELL if side == 'N' else HY
    def blank(label, x0, mesh):   # 250 filler covering x0..x0+250 on the line, open side into the hall
        place(label, B + mesh, x0 + (250.0 if side == 'N' else 0.0), y, 0, yaw=(180.0 if side == 'N' else 0.0))
    blank('Hall_%s_Blank_0' % side, CX - 250.0, 'SM_Bld_Crew_Blank_01')
    for k, x in enumerate(CAB_X):
        mine = side == 'S' and k == 1
        # Two lift-wall frames back to back on every door, like every other wall line: the sliding
        # door's own frame shows its outer face to the hall, a second frame behind it shows an outer
        # face to the cabin, and both shaft interiors (Synty paints them near-black) are buried between
        # the two boxes. The pair is DOOR_D deeper than one frame, so the cabins step back by that.
        # A person-sized door (Doorframe_06: 100 x 198 opening, one sliding leaf) in place of the
        # lift door that was here: the survey of every door mesh we hold (Tools/survey_doors.py)
        # found it the one Space-kit wall piece with a cabin-scale doorway. Two frames back to
        # back are 90 thick against the hall face; the 93 cm the lift pair took beyond that is
        # cabin floor now, with the inner frame's painted face as the cabin's wall.
        if side == 'S':
            sliding_door('Cabin_S%d_Door' % k, x, y + 52.5, 0.0, kind='cabin', locked=not mine, hold_open=mine)   # box y+30..y+75, painted face to the hall (+y)
            place('Cabin_S%d_DoorIn' % k, B + 'SM_Bld_Wall_Doorframe_06', x + 500.0, y + 7.5, 0, yaw=180)                # box y-15..y+30, painted face to the cabin (-y)
        else:
            sliding_door('Cabin_N%d_Door' % k, x + 500.0, y + 22.5, 180.0, kind='cabin', locked=not mine, hold_open=mine)   # box y..y+45, painted face to the hall (-y)
            place('Cabin_N%d_DoorIn' % k, B + 'SM_Bld_Wall_Doorframe_06', x, y + 67.5, 0, yaw=0)                # box y+45..y+90, painted face to the cabin (+y)
        blank('Hall_%s_Blank_%d' % (side, k + 1), x - 250.0, 'SM_Bld_Crew_Blank_01')   # plain fillers only: no button boxes in the hall
    blank('Hall_%s_Blank_5' % side, CX - HN * CELL, 'SM_Bld_Crew_Blank_01')
hall_side('N'); hall_side('S')
for t in range(HN):
    fixture('Hall_Fixture_%d' % t, hall_tile_x(t) + 250.0, HY + 272.5, CEIL_MID, intensity=24.0, shadows=(t == 3), yaw=90)   # long axis along the hall

# ---- Cabin S1, room four: the crew cabin (its siblings will be copies of this) ----
def cabin(tag, x0, y0, door_side):
    """One-tile crew cabin on tile (x0, y0) with its hatch door on `door_side` (S: the cabin is north of the hall).
    Synty's crew modules do the furnishing in the wall bands: bunks opposite the door, shower and
    toilet niches on the west line, a blank and the fold-down desk niche on the east line."""
    north = door_side == 'S'
    tile(tag, x0, y0)
    line_piece(tag + '_Bunk', 'N' if north else 'S', x0, y0, 'SM_Bld_Crew_Beds_01')
    # A replicant cabin: no shower. The sink/toilet niche sits centred on the west line, a
    # half-width blank (Synty scales its own blanks in the demo) filling each end.
    line_piece(tag + '_Toilet', 'W', x0, y0, 'SM_Bld_Crew_Toilet_01', offset=125.0)
    line_piece(tag + '_Blank_W1', 'W', x0, y0, 'SM_Bld_Crew_Blank_01', offset=0.0, scale=(0.5, 1.0, 1.0))
    line_piece(tag + '_Blank_W2', 'W', x0, y0, 'SM_Bld_Crew_Blank_01', offset=375.0, scale=(0.5, 1.0, 1.0))
    near_e = 0.0 if north else 250.0        # E line walks from y0 up
    # The east blank's button box is cut out into its own inspectable piece (Cabin_S1_Blank_Button,
    # placed by hand from the scratch cut_button.py); the wall uses the copy without those polygons.
    line_piece(tag + '_Blank', 'E', x0, y0, 'SM_Bld_Crew_Blank_02', offset=near_e)   # (a cut piece, when one exists, is made by Tools/cut_feature.py and swapped in by hand)
    line_piece(tag + '_Desk', 'E', x0, y0, 'SM_Bld_Crew_Desk_01', offset=250.0 - near_e)
    def ly(v): return y0 + (v if north else CELL - v)     # cabin-local y, mirrored for a south cabin
    def lyaw(v): return v if north else -v
    ex = x0 + CELL - 70.0                                  # the east modules' face
    # Locker and footlocker against the east blank, by the door; stool and terminal at the desk niche.
    # Props whose front is their local +y and that face -x (into the room) keep yaw 90 in both mirror cases.
    place(tag + '_Locker', WP + 'SM_Prop_Locker_01', ex - 35.0, ly(82.0), 0, yaw=90, mat=False)
    loot_box(tag + '_Box', ex - 36.0, ly(192.0), lyaw(90) + 6.0, ['Pocket pistol'])   # a few degrees off square: hand placed   # a very small, weak sidearm: the PCS complimentary gear
    place(tag + '_Stool', P + 'SM_Prop_Stool_01', ex - 55.0, ly(375.0), 0, yaw=lyaw(90), tags=['inspectable', 'name:Stool', 'desc:A metal stool, bolted where it stands.', 'action:Sit', 'seat:66,0,4,38'])   # seat:<top height>,<facing yaw>,<whole-body lean deg>,<spine hunch deg>: sit facing the desk, upright
    place(tag + '_Terminal', P + 'SM_Prop_Screen_Small_01', x0 + CELL - 17.0, ly(375.0), 118.0, yaw=90, tags=['inspectable', 'name:Terminal', 'desc:', 'action:Use'])   # on the niche's desk top (z 89), screen to the room; Use is the whole menu
    # On the desk top (z 89): a keyboard in front of the screen, a data pad, a keycard and a drink can; a small lamp in the alcove's top.
    place(tag + '_Keyboard', CP + 'SM_Prop_Keyboard_01', x0 + CELL - 44.0, ly(375.0), 80.5, yaw=lyaw(90) + 175.0, mat=False)   # keys toward the chair, resting on the desk top (traced z 80.1; mesh bottom -0.4), 5 degrees off square
    place(tag + '_Desk_Pad', P + 'SM_Prop_Pad_01', x0 + CELL - 30.0, ly(440.0), 80.1, yaw=lyaw(70))
    place(tag + '_Desk_Card', P + 'SM_Prop_Keycard_01', x0 + CELL - 50.0, ly(432.0), 80.6, yaw=lyaw(110))
    place(tag + '_Desk_Can', CP + 'SM_Prop_Junk_Can_01', x0 + CELL - 22.0, ly(312.0), 80.1, yaw=lyaw(30), mat=False)
    light(tag + '_Desk_Light', x0 + CELL - 6.0, ly(300.0), 160.0, 2.5, 220, (1.0, 0.85, 0.65), tags=['lightid:%s_desk' % tag.lower()])   # the alcove's top right corner, seen from the chair (traced: alcove y 1730..1890, z 85..170, 55 deep behind the face at -1130); the panel's Desk switch
    light(tag + '_Bath_Light', x0 - 11.0, y0 + 284.0, 185.0, 3.0, 220, (0.86, 0.95, 1.0), tags=['lightid:%s_bath' % tag.lower()])   # inside the toilet nook (traced: y 1912..2012, floor 13, ceiling 197, it runs back past the wall line): the panel's Bath switch
    fixture(tag + '_Fixture', x0 + STRIP_X, y0 + 250.0, CEIL_MID, intensity=32.0, shadows=True, tags=['lightid:%s_room' % tag.lower()])   # the wall panel's Room switch
cabin('Cabin_S1', CAB_X[1], HY - CELL - DOOR_D, 'N')   # room four, on the right (south) walking west down the hall; its north edge meets the inner door frame

# ---- The cafeteria: 3 x 3 tiles east of the foyer --------------------------
CXN, CYN = 3, 3
CX0 = GX                       # 1089..2589
CY0 = FY0                      # 1678: the door (y 2178..2678) is the middle tile of the west line
for i in range(CXN):
    for j in range(CYN):
        tile('Caf_%d_%d' % (i, j), CX0 + i * CELL, CY0 + j * CELL)
for i in range(CXN):
    wall_in('Caf_Wall_S%d' % i, 'S', CX0 + i * CELL, CY0, mesh=('SM_Bld_Wall_01_Alt' if i % 2 else 'SM_Bld_Wall_01'))
    if i != 1: wall_in('Caf_Wall_N%d' % i, 'N', CX0 + i * CELL, CY0 + (CYN - 1) * CELL)
for j in range(CYN):
    wall_in('Caf_Wall_E%d' % j, 'E', CX0 + (CXN - 1) * CELL, CY0 + j * CELL, mesh=('SM_Bld_Wall_01' if j % 2 else 'SM_Bld_Wall_01_Alt'))
    if j == 2: wall_in('Caf_Wall_W%d' % j, 'W', CX0, CY0 + j * CELL)
# The galley unit fills the middle segment of the north line; its pivot is at its right-hand end (x -528..-28).
# Posters in the recesses of the cafeteria's plain Wall_01 pieces: the recess floor sits
# 29 in from the wall line (local x 19..481, z 80..234), so the slab goes there.
backboard('Caf_Board_S0', CX0 + 250.0, CY0 + 29.0, 0, 1, z=157.0, mat='M_Poster_Menu')
backboard('Caf_Board_S2', CX0 + 2 * CELL + 250.0, CY0 + 29.0, 0, 1, z=157.0, mat='M_Poster_Tray')
backboard('Caf_Board_E1', CX0 + CXN * CELL - 29.0, CY0 + CELL + 250.0, -1, 0, z=157.0, mat='M_Poster_Open')
backboard('Caf_Board_W2', CX0 + 29.0, CY0 + 2 * CELL + 322.0, 1, 0, z=157.0, mat='M_Poster_Can')   # shifted north, clear of the unit by the door
place('Caf_Kitchen', B + 'SM_Bld_Crew_Kitchen_01', CX0 + CELL - 28.0, CY0 + CYN * CELL, 0, yaw=180)
pillar_in('Caf_Pillar_SW', 'SW', CX0, CY0); pillar_in('Caf_Pillar_SE', 'SE', CX0 + 2 * CELL, CY0)
pillar_in('Caf_Pillar_NE', 'NE', CX0 + 2 * CELL, CY0 + 2 * CELL); pillar_in('Caf_Pillar_NW', 'NW', CX0, CY0 + 2 * CELL)
for i in range(CXN):
    for j in range(CYN):
        fixture('Caf_Fixture_%d_%d' % (i, j), CX0 + i * CELL + STRIP_X, CY0 + j * CELL + 250.0, CEIL_MID, intensity=26.0, shadows=(i == 1 and j == 1))
NF = CY0 + CYN * CELL - 75.0   # north wall face (y 3103); S face 1753; W face 1164; E face 2514
# Serving line in front of the galley, the cold store and the ration shelf either side of it.
place('Caf_Counter_1', CP + 'SM_Prop_Work_Bench_01', 1940.0, 2950.0, 0, yaw=0, mat=False)
place('Caf_Counter_2', CP + 'SM_Prop_Work_Bench_01', 1730.0, 2950.0, 0, yaw=0, mat=False)
place('Caf_Fridge', CP + 'SM_Prop_Fridge_01', 1356.0, NF - 40.0, 0, yaw=180, mat=False)
place('Caf_Shelf', CP + 'SM_Prop_Food_Shelf_01', 2339.0, NF - 37.0, 0, yaw=180, mat=False)
for k, (mesh, x, y, yaw) in enumerate([('SM_Prop_FoodTray_01', 1990.0, 2940.0, -10), ('SM_Prop_FoodTray_02', 1905.0, 2962.0, 15), ('SM_Prop_FoodTray_03', 1760.0, 2945.0, -25), ('SM_Prop_FoodPacket_01', 1690.0, 2972.0, -40), ('SM_Prop_FoodPacket_03', 1840.0, 2935.0, 50)]):
    place('Caf_Counter_Item_%d' % (k + 1), P + mesh, x, y, 88.0, yaw=yaw)
# Vending machines along the south wall's east half.
place('Caf_Vend_1', WP + 'SM_Prop_Vending_Machine_01', 2204.0, CY0 + 75.0 + 59.0, 0, yaw=0, mat=False)
place('Caf_Vend_2', CP + 'SM_Prop_Vending_Machine_01', 2045.0, CY0 + 75.0 + 36.0, 0, yaw=0, mat=False)
place('Caf_Vend_3', WP + 'SM_Prop_Vending_Machine_03', 1830.0, CY0 + 75.0 + 80.0, 0, yaw=0, mat=False)
# A booth in the south-west corner (the corner seat grows +x +y from its pivot; yawed 90 it grows -x +y).
place('Caf_Booth', P + 'SM_Prop_CornerSeat_01', 1476.0, CY0 + 75.0, 0, yaw=90)
place('Caf_Booth_Table', P + 'SM_Prop_CornerTable_01', 1366.0, CY0 + 185.0, 0, yaw=90)
# Tables: two square tables by the east wall, the long canteen table in the middle, stools round them.
for n, (tx, ty) in enumerate([(2300.0, 2300.0), (2300.0, 2750.0)]):
    place('Caf_SqTable_%d' % (n + 1), P + 'SM_Prop_SquareTable_01', tx, ty, 0, yaw=0)
    for m, (dx, dy) in enumerate([(-140.0, 0.0), (140.0, 0.0), (0.0, -140.0), (0.0, 140.0)]):
        place('Caf_SqTable_%d_Stool_%d' % (n + 1, m + 1), P + 'SM_Prop_Stool_01', tx + dx, ty + dy, 0, yaw=m * 90)
place('Caf_LongTable', P + 'SM_Prop_Table_01', 1800.0, 2450.0, 0, yaw=0)
for m, (dx, dy) in enumerate([(-120.0, -130.0), (0.0, -130.0), (120.0, -130.0), (-120.0, 130.0), (0.0, 130.0), (120.0, 130.0)]):
    place('Caf_LongTable_Stool_%d' % (m + 1), P + 'SM_Prop_Stool_01', 1800.0 + dx, 2450.0 + dy, 0, yaw=m * 60)
# Leftovers on the tables (Space trays on the Space tables, a couple of Cyber City noodle boxes).
place('Caf_Table_Item_1', P + 'SM_Prop_FoodTray_01', 1870.0, 2420.0, 85.0, yaw=20)
place('Caf_Table_Item_2', P + 'SM_Prop_FoodPacket_02', 1740.0, 2482.0, 85.0, yaw=-60)
place('Caf_Table_Item_3', CP + 'SM_Prop_Food_Plates_01', 1790.0, 2400.0, 85.0, yaw=-15, mat=False)
place('Caf_Table_Item_4', CP + 'SM_Prop_Noodle_Box_01', 2282.0, 2322.0, 87.0, yaw=-30, mat=False)
place('Caf_Table_Item_5', CP + 'SM_Prop_Food_Tray_01', 2320.0, 2740.0, 87.0, yaw=35, mat=False)
place('Caf_Table_Item_6', CP + 'SM_Prop_Noodle_Box_02', 2250.0, 2770.0, 87.0, yaw=-100, mat=False)

# ---- Mess hall dressing, from the Cyber City prop set ------------------------
# The room had furniture but almost nothing ON it, which reads as a showroom rather than a place
# people eat in. These are scattered across the surfaces that already exist: long table top at
# z 85, the two square tables at 87, the serving counters at 88. All Cyber City props, so mat is
# left alone -- they carry their own atlas and the cafeteria's clean palette would flatten them.
# Chosen off Tools/render_prop_sheet.py rather than by name; several likely sounding assets
# (the food shelves, the hologram table) are frames that need their glass and insert pieces and
# render as almost nothing on their own.
CAF_DRESS = [
    # long table: someone's half-finished meal and a couple of drinks
    ('Drink_1',      'SM_Prop_Drink_01',            1845.0, 2470.0, 85.0,  20),
    ('Drink_2',      'SM_Prop_Drink_Syncola_01',    1762.0, 2432.0, 85.0, -35),
    ('Snack_1',      'SM_Prop_Snack_02',            1722.0, 2418.0, 85.0,  70),
    ('Plate_1',      'SM_Prop_Plates_02',           1884.0, 2482.0, 85.0,  -8),
    ('Snack_2',      'SM_Prop_Snack_05',            1838.0, 2408.0, 85.0, 115),
    # square table by the east wall, south
    ('Boba_1',       'SM_Prop_Drink_Boba_01',       2332.0, 2278.0, 87.0,   0),
    ('Burger_1',     'SM_Prop_Food_Burger_01',      2268.0, 2342.0, 87.0, -25),
    ('Snack_3',      'SM_Prop_Snack_04',            2246.0, 2284.0, 87.0,  50),
    # square table by the east wall, north
    ('Drink_3',      'SM_Prop_Drink_03',            2272.0, 2722.0, 87.0,  10),
    ('Plate_2',      'SM_Prop_Food_Plates_03',      2342.0, 2782.0, 87.0, -40),
    ('Snack_4',      'SM_Prop_Snack_01',            2258.0, 2792.0, 87.0,  95),
    # the booth in the south-west corner
    ('Noodle_1',     'SM_Prop_Noodle_Box_02',       1352.0, 1922.0, 85.0,  25),
    ('Drink_4',      'SM_Prop_Drink_05',            1382.0, 1956.0, 85.0, -15),
    # the serving counters
    ('Skewers',      'SM_Prop_Food_Sticks_Holder_01', 1862.0, 2958.0, 88.0, 0),
    ('PlateStack',   'SM_Prop_Plates_01',           1698.0, 2936.0, 88.0,   0),
    ('Tray_1',       'SM_Prop_Food_Tray_02',        1992.0, 2966.0, 88.0, -12),
    ('Tray_2',       'SM_Prop_Food_Tray_03',        1706.0, 2968.0, 88.0,  18),
]
for _name, _mesh, _x, _y, _z, _yaw in CAF_DRESS:
    place('Caf_Dress_' + _name, CP + _mesh, _x, _y, _z, yaw=_yaw, mat=False)

# A wash-up point on the north wall, west of the galley, and a two-seat table in the west aisle
# so the room has somewhere to sit that is not the main run of benches.
# A WALL sink, so it hangs on the wall: probed, the mesh runs local y -1.3..71.7 out from its
# back and z -18..46 about its pivot, and at (1560, NF-30, 0) it stood on the floor 15 cm off
# the wall with its end in the galley. Wall_01 has a recess 29 in from the wall line between
# z 80 and 234 (local x 19..481, so world x 1108..1570 on N0): the sink hangs in that recess,
# back to the recess wall, clear of the galley's west end (x 1589).
place('Caf_Sink', CP + 'SM_Prop_Sink_Wall_01', 1500.0, 3118.0 - 1.3, 100.0, yaw=180, mat=False)
place('Caf_SmallTable', CP + 'SM_Prop_Small_Table_01', 1430.0, 2520.0, 0, yaw=0, mat=False)
place('Caf_SmallTable_Stool_1', CP + 'SM_Prop_Stool_02', 1430.0, 2400.0, 0, yaw=0, mat=False)
place('Caf_SmallTable_Stool_2', CP + 'SM_Prop_Stool_02', 1430.0, 2640.0, 0, yaw=180, mat=False)


# ---- Atmosphere, exposure, start (kept if present) --------------------------
def sky_spawn():
    a = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(W / 2, H / 2, 300))
    a.light_component.set_intensity(0.06); a.light_component.set_cast_shadows(False); a.light_component.set_mobility(unreal.ComponentMobility.MOVABLE); return a
ensure('Ambient', sky_spawn)
def ppv_spawn():
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0)); ppv.set_editor_property('unbound', True)
    s = ppv.settings
    s.set_editor_property('override_auto_exposure_method', True); s.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM)
    s.set_editor_property('override_auto_exposure_bias', True); s.set_editor_property('auto_exposure_bias', -0.8)
    s.set_editor_property('override_auto_exposure_min_brightness', True); s.set_editor_property('auto_exposure_min_brightness', 0.05)
    s.set_editor_property('override_auto_exposure_max_brightness', True); s.set_editor_property('auto_exposure_max_brightness', 1.2)
    ppv.set_editor_property('settings', s); return ppv
ensure('Exposure', ppv_spawn)
def fog_spawn():
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)); fc = fog.component
    fc.set_fog_density(0.028); fc.set_fog_height_falloff(0.001); fc.set_fog_inscattering_color(unreal.LinearColor(r=0.30, g=0.26, b=0.19, a=1.0)); return fog
ensure('Fog', fog_spawn)
ensure('PlayerStart', lambda: eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(300, 960, 100), unreal.Rotator(roll=0, pitch=0, yaw=90)))
ws = world.get_world_settings()
if ws.get_editor_property('default_game_mode') != unreal.load_class(None, '/Script/RepliCan.BaseGameMode'):
    ws.modify(); ws.set_editor_property('default_game_mode', unreal.load_class(None, '/Script/RepliCan.BaseGameMode'))


# ---- Cafeteria: five troopers standing about talking ------------------------
# PLACEHOLDERS. The names and descriptions below are stand-ins until the faction writing exists;
# they are here to put people in the room and to exercise NPC placement, the palette swap and
# idle playback on the Synty sci-fi rig. Every one carries a 'placeholder' tag so they are easy
# to find and replace later.
#
# The Synty sci-fi characters and Epic's UE4 mannequin share a skeleton, so the mannequin's
# standing idle plays on them directly with no retarget. It is a neutral arms-down stance, which
# is what people talking look like; the weapon idles in the Lyra set pose the hands around a gun
# that is not there.
# The red trooper palette. Synty's alternates are whole-atlas recolours, and which one turns a
# SOLDIER red cannot be read off the texture: it depends where that mesh's UVs land. Found by
# rendering the soldier against all eighteen and measuring the hue of his chest
# (Tools/soldier_palette_swatches.py + Tools/palette_pick.ps1). Group 02 comes out at hue 13,
# saturation 0.78; group 01 is the yellow-and-grey the mesh ships with.
SOLDIER_MAT = '/Game/PolygonSciFiSpace/Materials/Alternates/M_PolygonSciFiSpace_02_F'
# Skin does not belong in the armour's palette. Synty's alternates recolour the WHOLE atlas, so
# a bare head wearing the red armour palette comes out green-skinned; helmets are fine in it
# because they are armour. Heads therefore keep the base palette and helmets take the red one.
SOLDIER_SKIN_MAT = '/Game/PolygonSciFiSpace/Materials/M_PolygonSciFiSpace_01_A'
# The player's own nose prop, so trooper faces match the one the player wears.
NOSE_MESH = '/Game/Characters/KnightDemo/SM_Nose.SM_Nose'
# Placement in the HEAD BONE's own frame, the same values AFaceController uses on the player.
# They transfer directly because both heads sit on UE4_Mannequin_Skeleton. An earlier attempt to
# "correct" them was chasing a nose that had never attached at all and was lying on the floor.
NOSE_OFFSET = (6.89, 13.0, 0.0)
# Where each face mesh wants its nose, measured off the geometry by
# Tools/measure_face_planes.py -- see that file for the algorithm and for why it is expressed
# as a delta from the one head that was confirmed by eye. Anything not in the file falls back
# to NOSE_OFFSET, which is that confirmed head. Re-run the measuring script after adding a
# character; nothing here is hand-tuned.
def _load_face_planes():
    try:
        d = json.load(io.open(os.path.join(unreal.Paths.project_dir(), 'Tools', 'face_planes.json'), encoding='utf-8'))
        return {k: tuple(v['nose_offset']) for k, v in d.items() if 'nose_offset' in v}
    except Exception as e:
        print('FACE PLANES not loaded (%s); every nose falls back to the standard head' % e)
        return {}
NOSE_BY_FACE = _load_face_planes()
NOSE_PITCH = -90.0
NOSE_SCALE = 1.0
SOLDIER_IDLE = '/Game/PolygonSciFiSpace/EpicContent/Mannequin/Animations/ThirdPersonIdle'
SM = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/'

def soldier(label, mesh_path, head_path, x, y, face_deg, start, rate, disp, desc, nose=True):
    """One trooper, posed and looking at the middle of the group.

    face_deg is a compass angle measured from +X. These meshes face +Y at yaw zero, so the yaw
    that points them along that angle is face_deg - 90."""
    anim = unreal.load_asset(SOLDIER_IDLE)
    mat = unreal.load_asset(SOLDIER_MAT)
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        print('SOLDIER MESH MISSING', mesh_path); return
    def apply(a, place_it=False):
        # Position is set ONLY when the trooper is first spawned. A re-run refreshes the mesh,
        # the palette, the idle and the tags, and leaves the transform exactly where it is --
        # otherwise every layout run drags hand-placed people back onto the script's guesses.
        if place_it:
            a.set_actor_location(unreal.Vector(x, y, 0.0), False, True)
            a.set_actor_rotation(unreal.Rotator(0.0, 0.0, face_deg - 90.0), False)
        c = a.skeletal_mesh_component
        try: c.set_skeletal_mesh_asset(mesh)
        except Exception: c.set_editor_property('skeletal_mesh_asset', mesh)
        if mat:
            for i in range(c.get_num_materials()): c.set_material(i, mat)
        if anim:
            c.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            d = c.get_editor_property('animation_data')
            # FSingleAnimationPlayData spells these with a Saved prefix; 'looping' and 'playing'
            # are the component's own accessors, not fields on the struct.
            d.set_editor_property('anim_to_play', anim)
            d.set_editor_property('saved_looping', True)
            d.set_editor_property('saved_playing', True)
            d.set_editor_property('saved_play_rate', rate)
            # A different start for each, or five people breathe in lockstep.
            d.set_editor_property('saved_position', start)
            c.set_editor_property('animation_data', d)
            # Only meaningful on a placed instance, and refused on a template, so it is allowed
            # to fail rather than taking the whole layout run down with it.
            try: c.set_editor_property('update_animation_in_editor', True)
            except Exception: pass
        a.set_editor_property('tags', [unreal.Name(t) for t in
            ['inspectable', 'placeholder', 'name:' + disp, 'desc:' + desc]])
    def spawn():
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(x, y, 0.0))
        if a: apply(a, place_it=True)
        return a
    body = ensure(label, spawn, apply)

    # The SpaceSoldier bodies ship headless: the pack keeps heads and helmets as separate meshes
    # on the same skeleton, which is why four of the five troopers were standing there with bare
    # collars. The head is its own actor rather than a component so it lives in the level like
    # everything else here and the manifest can manage it; it follows the body through
    # SetLeaderPoseComponent, which works because both are on UE4_Mannequin_Skeleton.
    # A trooper with no separate head piece is not finished here: the BR body carries its own
    # head, and he still wants a nose on it.
    if not body:
        return
    head_mesh = unreal.load_asset(head_path) if head_path else None
    if head_path and not head_mesh:
        print('SOLDIER HEAD MISSING', head_path); return
    def head_apply(h):
        # Snapped to wherever the BODY actually is, not to the script's coordinates, so a
        # trooper moved by hand keeps his head on.
        h.set_actor_location_and_rotation(body.get_actor_location(), body.get_actor_rotation(), False, True)
        hc = h.skeletal_mesh_component
        try: hc.set_skeletal_mesh_asset(head_mesh)
        except Exception: hc.set_editor_property('skeletal_mesh_asset', head_mesh)
        # A helmet is armour and takes the unit's colours; a bare head is skin and does not.
        head_mat = mat if 'Helmet' in head_path else unreal.load_asset(SOLDIER_SKIN_MAT)
        if head_mat:
            for i in range(hc.get_num_materials()): hc.set_material(i, head_mat)
        h.attach_to_actor(body, '', unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.KEEP_WORLD, False)
        # Driven by the body's pose, so the head breathes and turns with the idle.
        hc.set_leader_pose_component(body.skeletal_mesh_component)
    def head_spawn():
        h = eas.spawn_actor_from_object(head_mesh, body.get_actor_location())
        if h: head_apply(h)
        return h
    head_actor = ensure(label + '_Head', head_spawn, head_apply) if head_mesh else None

    # Synty faces are flat: the bare heads have brows and a mouth painted on but no nose
    # geometry at all, so an unhelmeted trooper reads as a mannequin. The player already solves
    # this with a nose prop on the head bone (AFaceController), so the same mesh and the same
    # offsets are reused here rather than inventing a second set of numbers. Helmeted troopers
    # skip it: there is no face to put it on.
    # Whoever is carrying the face gets the nose: the separate head actor when there is one, or
    # the body itself for a trooper like the BR whose head is already part of his mesh. He is on
    # the same skeleton as the rest, so the head bone is in the same place and the offsets carry
    # over unchanged. Helmets are skipped: there is no face under one.
    face_actor = head_actor if head_actor else body
    # Which mesh is carrying the face, so its measured offsets can be looked up.
    face_mesh_name = (head_path or mesh_path).split('/')[-1].split('.')[0]
    # A nose belongs on a bare human face and nowhere else. The review line-up asks for the
    # models exactly as the packs ship them, and half of them are robots, aliens or sealed
    # helmets -- none of which have anywhere to put one.
    if not nose or not face_actor or (head_path and 'Helmet' in head_path):
        return
    nose_mesh = unreal.load_asset(NOSE_MESH)
    if not nose_mesh:
        print('NOSE MESH MISSING', NOSE_MESH); return
    # A nose is part of a face, so it wears the FACE'S material, whatever that turns out to be
    # -- not a constant. The BR soldier is the case that proves it: his whole body including his
    # head is one material slot carrying the unit's palette, so a nose painted with the standard
    # skin material sat on a green face in a different colour entirely.
    face_comp = face_actor.skeletal_mesh_component
    skin = None
    try:
        skin = face_comp.get_material(0)
    except Exception:
        pass
    if not skin:
        skin = unreal.load_asset(SOLDIER_SKIN_MAT)
    # Where the face actually is. Measured per mesh off the geometry (frontmost centreline
    # vertex in a band around head height, via GeometryScript -- see Tools/probe_br.py) rather
    # than assumed, because the bodies do not agree: the standard Synty head's face plane sits
    # 13.5 cm forward at z 158-162, and the BR's is 22.3 cm forward at z 154. One constant put
    # the BR's nose a hand's width in front of his face, hanging in the air.
    up, forward = NOSE_BY_FACE.get(face_mesh_name, NOSE_OFFSET[:2])
    def nose_apply(n):
        # Movable FIRST. A StaticMeshActor defaults to STATIC mobility, and Unreal refuses to
        # attach a static actor to a movable parent -- the call just returns, which is why every
        # nose stayed on the floor at the trooper's feet with no error to show for it.
        n.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        # attach_to_actor with a bone name, not attach_to_component: the component form does not
        # take here either.
        n.attach_to_actor(face_actor, 'head',
                          unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                          unreal.AttachmentRule.SNAP_TO_TARGET, False)
        n.set_actor_relative_location(unreal.Vector(up, forward, NOSE_OFFSET[2]), False, False)
        n.set_actor_relative_rotation(unreal.Rotator(roll=0.0, pitch=NOSE_PITCH, yaw=0.0), False, False)
        n.set_actor_relative_scale3d(unreal.Vector(NOSE_SCALE, NOSE_SCALE, NOSE_SCALE))
        c = n.static_mesh_component
        if skin:
            for i in range(c.get_num_materials()): c.set_material(i, skin)
        c.set_cast_shadow(False)
    def nose_spawn():
        n = eas.spawn_actor_from_object(nose_mesh, face_actor.get_actor_location())
        if n: nose_apply(n)
        return n
    ensure(label + '_Nose', nose_spawn, nose_apply)

# A loose ring rather than a circle: unequal radii and gaps, so it reads as people who drifted
# into a conversation rather than a briefing formation. Clear of the tables (y 2300-2750) and
# the counters (y 2950).
_CX, _CY = 2150.0, 2130.0   # 1.5 m north of the old spot: the vending machines line y ~1800
_PL = 'Placeholder NPC. Name, unit and dialogue not written yet.'
_GROUP = [
    ('A', SM + 'SK_Chr_SpaceSoldier_Male_01',    SM + 'SK_Chr_SpaceSoldier_Head_Male_01',            15.0, 118.0, 0.00, 1.00, 'TROOPER [PLACEHOLDER A]', _PL),
    ('B', SM + 'SK_Chr_SpaceSoldier_Female_01',  SM + 'SK_Chr_Attach_SpaceSoldier_Female_Helmet_01',  85.0, 108.0, 0.63, 0.96, 'TROOPER [PLACEHOLDER B]', _PL),
    ('C', SM + 'SK_Chr_BR_SpaceSoldier_Male_01', None,                                               155.0, 126.0, 1.29, 1.04, 'TROOPER [PLACEHOLDER C]', _PL),
    ('D', SM + 'SK_Chr_SpaceSoldier_Male_01',    SM + 'SK_Chr_Attach_SpaceSoldier_Male_Helmet_01',   225.0, 112.0, 1.91, 0.92, 'TROOPER [PLACEHOLDER D]', _PL),
    ('E', SM + 'SK_Chr_SpaceSoldier_Female_01',  SM + 'SK_Chr_SpaceSoldier_Head_Female_01',          300.0, 132.0, 2.44, 1.08, 'TROOPER [PLACEHOLDER E]', _PL),
]
for _tag, _mesh, _head, _ang, _rad, _start, _rate, _name, _desc in _GROUP:
    _rx = _CX + _rad * math.cos(math.radians(_ang))
    _ry = _CY + _rad * math.sin(math.radians(_ang))
    # Facing the middle of the group, knocked a few degrees off so nobody stares dead centre.
    _face = _ang + 180.0 + (7.0 if _tag in ('B', 'D') else -5.0)
    soldier('Caf_Trooper_' + _tag, _mesh, _head, _rx, _ry, _face, _start, _rate, _name, _desc)

os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
# ---- Character line-up -------------------------------------------------------------------------
# One of every character model neither pack has used yet, stood in the cafeteria so they can be
# looked at side by side. Not set dressing: a reference sheet you can walk around.
#
# Placed by SEARCHING for clear floor rather than by laying a grid and hoping. Every candidate
# spot is checked against the bounds of everything already in the room, and a blocked spot is
# stepped past -- the room is full of tables, counters and vending machines, and a grid that
# ignores them puts half the cast inside the furniture.
SPACE_UNUSED = [
    'SK_Chr_Alien_01', 'SK_Chr_BR_BigAlien_01', 'SK_Chr_BR_BigAlien_02', 'SK_Chr_BR_EVA_Suit_01',
    'SK_Chr_BR_War_Robot_01', 'SK_Chr_CrewCaptain_Female_01', 'SK_Chr_CrewCaptain_Male_01',
    'SK_Chr_Crew_Female_01', 'SK_Chr_Crew_Male_01', 'SK_Chr_Cryo_Female_01', 'SK_Chr_Cryo_Male_01',
    'SK_Chr_Hunter_Female_01', 'SK_Chr_Junker_Female_01', 'SK_Chr_Junker_Male_01',
    'SK_Chr_Medic_Male_01', 'SK_Chr_Psionic_01', 'SK_Chr_RobotFemale_01',
]
HORROR_UNUSED = [
    'SK_Chr_Alien_01', 'SK_Chr_Android_01', 'SK_Chr_Crew_01_F', 'SK_Chr_Crew_01_M',
    'SK_Chr_Crew_02_F', 'SK_Chr_Crew_02_M', 'SK_Chr_Crew_03_F', 'SK_Chr_Crew_03_M',
    'SK_Chr_Mining_Suit_01', 'SK_Chr_Space_Suit_01_F', 'SK_Chr_Space_Suit_01_M',
    'SK_Chr_Space_Suit_02_F', 'SK_Chr_Space_Suit_02_M', 'SK_Chr_Zub_01',
]
# Three of them, not seventeen. A rank of every unused model reads as a display case; three
# people standing in a mess hall read as people. The rest of SPACE_UNUSED is left listed above
# so the next one is a line, not a search.
LINEUP = [('Space', SM + n) for n in ('SK_Chr_Junker_Male_01', 'SK_Chr_Junker_Female_01', 'SK_Chr_Hunter_Female_01')]
# Placeholders until they are written: a name to inspect and a line that says what they are.
LINEUP_TEXT = {
    'Junker_Male':    ('JUNKER [PLACEHOLDER]',
                       'Salvage crew. Wears what the last job paid for. Name and business not written yet.'),
    'Junker_Female':  ('JUNKER [PLACEHOLDER]',
                       'The other half of the salvage pair. Does the talking, apparently. Not written yet.'),
    'Hunter_Female':  ('HUNTER [PLACEHOLDER]',
                       'Came in off a contract and has not said which one. Not written yet.'),
}

# The clear-ish north-east quarter of the cafeteria, searched on a 150 cm lattice.
_LX0, _LY0 = GX + 260.0, FY0 + 260.0
_LSTEP = 135.0
_LCOLS = 2
_CLEAR = 85.0        # how much room a person needs from anything already standing there

def _occupied():
    """Everything already in the room, as (centre, radius) pairs. Read once."""
    out = []
    for a in eas.get_all_level_actors():
        # The line-up must not treat ITSELF as an obstacle. On a second run the models placed by
        # the first are standing in the room, so every candidate spot looks taken and the search
        # gives up a third of the way through -- which is exactly what it did.
        if a.get_actor_label().startswith('Caf_Lineup_'):
            continue
        try:
            loc = a.get_actor_location()
        except Exception:
            continue
        if not (GX - 200 < loc.x < GX + 2600 and FY0 - 200 < loc.y < FY0 + 2200):
            continue
        try:
            _, ext = a.get_actor_bounds(False)
            r = max(ext.x, ext.y)
        except Exception:
            r = 40.0
        # Floors and ceilings are not obstacles; anything wide and flat is the room itself.
        if r > 240.0:
            continue
        out.append((loc.x, loc.y, r))
    return out

_BLOCKERS = _occupied()

def _clear_spot(taken):
    for i in range(_LCOLS * 12):
        x = _LX0 + (i % _LCOLS) * _LSTEP
        y = _LY0 + (i // _LCOLS) * _LSTEP
        if any((x - px) ** 2 + (y - py) ** 2 < (pr + _CLEAR) ** 2 for px, py, pr in _BLOCKERS):
            continue
        if any((x - tx) ** 2 + (y - ty) ** 2 < (_LSTEP * 0.8) ** 2 for tx, ty in taken):
            continue
        taken.append((x, y))
        return x, y
    return None

_taken = []
for _i, (_pack, _path) in enumerate(LINEUP):
    _spot = _clear_spot(_taken)
    if _spot is None:
        print('LINE-UP: ran out of clear floor at', _i, 'of', len(LINEUP))
        break
    _lx, _ly = _spot
    _short = _path.split('/')[-1].replace('SK_Chr_', '').replace('_01', '')
    _disp, _desc = LINEUP_TEXT.get(_short, ('%s [PLACEHOLDER]' % _short.replace('_', ' ').upper(),
                                            'Not written yet.'))
    soldier('Caf_Lineup_%s_%s' % (_pack, _short), _path, None, _lx, _ly,
            # Turned toward each other rather than all one way: three people in a room are
            # having a conversation, not standing inspection.
            # nose=True: all three are bare-faced humans. The flag exists for the robots and
            # sealed helmets in SPACE_UNUSED, not for people.
            140.0 + _i * 95.0, (_i * 0.37) % 1.0, 1.0, _disp, _desc, nose=True)

# ---- The service lift ------------------------------------------------------------------------
# IN THE FOYER, on the wall across from the bay. Measured, and the room turned out to have been
# waiting for it: the foyer's north wall is built as Foyer_Wall_N0 (x -310..250) and N2 (750..
# 1310) with NOTHING BETWEEN THEM -- a 500 cm gap centred on x=500, which is exactly the span of
# one lift wall and exactly the line the bay door is centred on. The slot was already there.
#
# At yaw 180 the wall mesh's local +X runs to world -X, so a pivot at x=750 spans 250..750; and
# its local depth maps so that a pivot at y=2753 fills the same 2678..2767 band the rest of the
# north wall occupies. The car then sits at y 2753..3071 -- behind the wall, outside the room,
# which is where a shaft belongs.
LIFT_X, LIFT_Y, LIFT_YAW = 750.0, 2753.0, 180.0
LIFT_UP, LIFT_DOWN = 4, 10
# Which floors are fitted out. Matches AElevatorActor::ServicedFloors, and it decides where a
# shaft WALL goes as well as where the doors will open.
#
# A wall at every level was the cause of the clipping you see riding the car: measured, the wall
# piece occupies local y -16.5..75 and the car occupies -340..3.5, so the wall's inner lip pokes
# 20 cm into the car's swept volume. Parked at a floor that is exactly how Synty's own demo
# assembles it -- every piece at the same XY -- but passing thirteen of them on the way down
# drags thirteen slabs straight through the car. Only serviced floors get one now; the rest of
# the shaft is bare, unlit and behind shut doors, which is what a shaft looks like anyway.
LIFT_SERVICED = (0, -LIFT_DOWN)
# Where the shaft's back wall stands: the car's far face is at y 3093.
SHAFT_BACK_Y = 3100.0

for _f in range(-LIFT_DOWN, LIFT_UP + 1):
    if _f in LIFT_SERVICED:
        place('Lift_Shaft_%+03d' % _f, B + 'SM_Bld_Lift_Wall_01', LIFT_X, LIFT_Y, _f * CELL, yaw=LIFT_YAW)
    else:
        # THE SHAFT IS BACK, and this time it clears the car. Taking these out fixed the clipping
        # and made the lift disappear from the editor at the same time -- fifteen wall pieces
        # were the only thing that drew the shaft, and the car itself only got its mesh at
        # BeginPlay. At an unserviced floor nothing is on the far side of the wall, so it can
        # sit 20 cm further out than the serviced ones: measured, the car's front face is at
        # local y 3.5 and the wall's inner lip reaches to -16.5, so 20 cm is exactly the overlap.
        # New labels, because the old ones are on the removed list.
        place('Lift_Shaft2_%+03d' % _f, B + 'SM_Bld_Lift_Wall_01', LIFT_X, LIFT_Y - 20.0, _f * CELL, yaw=LIFT_YAW)
    # The ENCLOSURE. The front is the lift wall above; this is the back and the two sides, so the
    # car rides in a shaft rather than in the open. Measured: SM_Bld_Wall_01 is 500 x 89.2,
    # 499.5 tall from z -99.5 (the storey exactly), textured on its local +Y face with the body
    # spanning local y 0..89.2 from the pivot. The car occupies x 327..672 and y 2749.5..3093, so
    # the shaft is the lift wall's own span, x 250..750, closed at y 3100, with 77 cm of daylight
    # either side of the car and nothing in its way.
    _front = LIFT_Y - 20.0 + 16.5 if _f not in LIFT_SERVICED else LIFT_Y + 16.5   # the front wall's back face
    _back = SHAFT_BACK_Y
    _run = _back - _front                                                          # side wall length
    # Back wall: textured face toward the car (-Y), so yaw 180; at yaw 180 the body runs from
    # the pivot toward -Y, so the pivot sits at the far face.
    place('LiftShaft_B%+03d' % _f, B + 'SM_Bld_Wall_01', 750.0, _back + WALL_T, _f * CELL, yaw=180)
    # West side, face toward +X: yaw -90 puts the body at pivot_x..pivot_x+89 and runs it -Y from
    # the pivot, so the pivot is at the back and outside. East is the mirror at yaw 90.
    place('LiftShaft_W%+03d' % _f, B + 'SM_Bld_Wall_01', 250.0 - WALL_T, _back, _f * CELL, yaw=-90, scale=(_run / CELL, 1.0, 1.0))
    place('LiftShaft_E%+03d' % _f, B + 'SM_Bld_Wall_01', 750.0 + WALL_T, _front, _f * CELL, yaw=90, scale=(_run / CELL, 1.0, 1.0))
    # CORNER COLUMNS. The side walls are Wall_01 scaled to the shaft's depth, and that depth
    # changes by 20 at the serviced floors (the front wall stands 20 further in), so their panel
    # lines and their ends jog from storey to storey, and where two Wall_01 pieces meet at right
    # angles their open ends show. The kit's answer is its corner pillar: Wall_Corner_Pillar_01,
    # the piece the cafeteria's corners use, 100 across with a 20 overhang behind the corner. At
    # 0.6 it is 60 across, and the car (x 327..672) keeps 17 cm clear of it. The front pair stand
    # on the serviced-floor line at every storey so the column runs straight past the step.
    # Outside, the side and back walls leave an 89 x 89 notch at each back corner; a 0.9 pillar
    # turned outward fills it.
    _pf = LIFT_Y + 16.5
    for _tag, (_px, _py, _yaw, _sc) in (('FW', (250.0, _pf, 0.0, 0.6)), ('FE', (750.0, _pf, 90.0, 0.6)), ('BE', (750.0, _back, 180.0, 0.6)), ('BW', (250.0, _back, -90.0, 0.6)),
                                          ('OW', (250.0 - WALL_T, _back + WALL_T, -90.0, 0.9)), ('OE', (750.0 + WALL_T, _back + WALL_T, 180.0, 0.9))):
        place('LiftShaft_P%s%+03d' % (_tag, _f), B + 'SM_Bld_Wall_Corner_Pillar_01', _px, _py, _f * CELL, yaw=_yaw, scale=(_sc, _sc, 1.0))

def lift_actor(label, x, y, z):
    def apply(a, place_it=True):
        # The lift always goes where the script says. Unlike the troopers -- which are left
        # wherever they were dragged to -- this is structure, and structure that has moved
        # needs to move.
        a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=0, yaw=LIFT_YAW), False, True)
        a.set_editor_property('floors_up', LIFT_UP)
        a.set_editor_property('floors_down', LIFT_DOWN)
        a.set_editor_property('floor_height', CELL)
        a.set_editor_property('tags', [unreal.Name('inspectable'), unreal.Name('name:Service lift')])
    def spawn():
        a = eas.spawn_actor_from_class(unreal.ElevatorActor, unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=0, yaw=LIFT_YAW))
        if a: apply(a, place_it=True)
        return a
    ensure(label, spawn, lambda a: apply(a, place_it=False))

lift_actor('Lift_Car', LIFT_X, LIFT_Y, 0.0)
# Capped top and bottom. The ceiling piece sits at z 400..449 of its storey, so at the top
# floor's base it closes the shaft just above that floor's walls; the pit floor is a metre
# under the lowest stop, where the car's underside (-66) never reaches.
_depth = (SHAFT_BACK_Y - (LIFT_Y + 16.5)) / CELL
place('LiftShaft_Roof', B + 'SM_Bld_Ceiling_01', 250.0, LIFT_Y + 16.5, LIFT_UP * CELL, yaw=0, scale=(1.0, _depth, 1.0))
place('LiftShaft_Pit', B + 'SM_Bld_Floor_01', 250.0, LIFT_Y + 16.5, -LIFT_DOWN * CELL - 100.0, yaw=0, scale=(1.0, _depth, 1.0))
# door_sign offsets by half a door in its OWN frame, so at yaw 180 that offset runs the other
# way: 750 here is what centres the strip on x=500, the same line as the opening.
# 54 back from where the bay door's rule put it: that rule offsets for a door TRIM the lift
# does not have, and left the strip 46 cm inside the shaft wall (probed: sign y 2724, the
# wall's foyer face 2678). At 2670 it hangs 8 cm proud of the wall over the lift opening.
# Sized to its text. The face is 16 lamp rows (two lines of 7 and a gap of 2) and a lamp is 1.25
# wide for its height, so columns = 16 * 1.25 * width / 42: 252 wide is 120 columns, the 17
# characters take 102, and 9 columns (19 cm) of housing show each side of the text.
door_sign('Sign_Door_Lift', 750.0, FY1 + WALL_T - 54.0, 180.0, 'PERSONNEL LIFT B4|AT BAY 01', width=252.0)
# The sign is the lift's status board: the car rewrites its second line as it moves (see
# AElevatorActor::UpdateSign). The text above is only what it shows before play begins. Both
# actors exist by this point, placed or kept, so the reference can be set on every run.
if 'Lift_Car' in existing and 'Sign_Door_Lift' in existing:
    try:
        existing['Lift_Car'].set_editor_property('status_sign', existing['Sign_Door_Lift'])
        existing['Lift_Car'].set_editor_property('lift_name', 'PERSONNEL LIFT B4')
    except Exception as e:
        print('LIFT SIGN not wired:', e)

# ---- Sub 10: the derelict ----------------------------------------------------------------------
# What is at the bottom of the shaft. Not built piece by piece from the horror kit -- lifted whole
# out of the pack's own demo, which is the only place the people who made the kit wrote down how
# it is meant to go together.
#
# Tools/horror_capture.py did the taking. It found that the horror demo is ONE building with no
# separable rooms, so instead of hunting for a room it scored every tile-aligned 30 m window on
# how many pieces its edge would slice, and this one -- a cryo deck, 1559 pieces, 67 cryopods,
# 52 doors, 39 lights -- came out at 8.7%, the cleanest cut of any window containing cryopods.
# The recipe is Tools/HorrorSection/horror_section.json: every piece's mesh, transform and
# material overrides, relative to the window's own south-west corner at deck height.
#
# WHERE IT GOES. Directly under the facility, at Sub 10, with its NORTH edge landing on the lift
# wall: y0 + 3000 = 2753, the lift's own y. x0 is zero, so the deck's local x is world x and the
# shaft opening (world x 250..750 at yaw 180) lands on the deck's north wall without a fudge
# factor. Stepping out of the lift at the bottom puts you in it.
#
# MANAGED AS ONE THING. 1559 ensure() entries would be 1559 manifest lines and a layout run that
# takes minutes. So the group has a SINGLE manifest label, HORROR_LABEL: placed once, skipped
# ever after, and honoured if you delete it. Set HORROR_REBUILD to tear the pieces down and lay
# them again after re-capturing.
HORROR_JSON = os.path.join(unreal.Paths.project_dir(), 'Tools', 'HorrorSection', 'horror_section.json')
HORROR_LABEL = 'Horror_Section'
HORROR_PREFIX = 'HS_'
# The lift wall at pivot y=LIFT_Y yaw 180 fills y 2678..2767, so its SOUTH FACE is FY1. Putting
# the deck's north edge exactly there means stepping out of the car puts you on their floor with
# nothing between. x0 is zero so the deck's local x is world x and the shaft opening (world x
# 250..750) needs no fudge factor.
HORROR_X, HORROR_Y = 0.0, FY1 - 3000.0
HORROR_Z = -10 * CELL                                     # Sub 10
HORROR_SIZE = 3000.0
HORROR_REBUILD = False    # set True to tear up ~1500 actors and lay them again after a re-capture
# The transplant is retired. It was a faithful slice of the demo and that was the problem: the
# demo is a warren, so a slice of it is a warren. Sub 10 is BUILT now, out of the same kit, as
# one open hall -- see THE SERVICE DECK at the foot of this file. Setting this tears the old
# pieces out; the recipe and Tools/horror_capture.py stay, because cutting a chunk is still the
# right tool for somewhere that wants to be a warren.
HORROR_TRANSPLANT_OUT = True

# The hole at the threshold. Kept to the WALL BAND only -- the first cut was 480 cm deep and
# 380 tall and it took five floor pieces, five ceiling pieces, a whole door assembly and a
# pillar with them, which is a crater rather than a doorway. Floors and ceilings are never
# carved: a hole in the floor at the lift door is the one thing that must not happen.
# Not just the wall band: a LANDING. The first cut opened their north wall and nothing else,
# and 2 m inside the doorway there is a wall run with a closed SM_Bld_Door_01 across it -- a
# solid, static mesh, so stepping out of the lift walked straight into it. The box now reaches
# 3.8 m into the deck, which clears that run and leaves somewhere to stand.
HORROR_DOORWAY = (200.0, 800.0, FY1 - 380.0, FY1 + 40.0, HORROR_Z + 10.0, HORROR_Z + 360.0)
HORROR_KEEP = ('Floor', 'Ceiling')


def horror_section():
    if HORROR_TRANSPLANT_OUT:
        old = [(lbl, a) for lbl, a in list(existing.items()) if lbl.startswith(HORROR_PREFIX)]
        for lbl, a in old:
            try:
                eas.destroy_actor(a)
                existing.pop(lbl, None)
            except Exception:
                pass
        if old:
            print('HORROR: transplant retired, %d pieces removed' % len(old))
        removed.add(HORROR_LABEL)
        placed.discard(HORROR_LABEL)
        return
    if not os.path.exists(HORROR_JSON):
        print('HORROR: no recipe at', HORROR_JSON, '-- run Tools/horror_capture.py'); return
    if HORROR_LABEL in removed:
        print('HORROR: deleted by hand; left out'); return

    old = [a for lbl, a in existing.items() if lbl.startswith(HORROR_PREFIX)]
    if old and not HORROR_REBUILD:
        print('HORROR: %d pieces already down' % len(old)); return
    if old and HORROR_REBUILD:
        for a in old:
            try: eas.destroy_actor(a)
            except Exception: pass
        print('HORROR: tore down %d pieces' % len(old))

    doc = json.load(io.open(HORROR_JSON, encoding='utf-8'))
    ax0, ax1, ay0, ay1, az0, az1 = HORROR_DOORWAY
    cache = {}
    n, carved, missing = 0, 0, 0
    for i, piece in enumerate(doc['pieces']):
        wx = HORROR_X + piece['loc'][0]
        wy = HORROR_Y + piece['loc'][1]
        wz = HORROR_Z + piece['loc'][2]
        # The doorway is cut by simply not laying the pieces that are in the way. Their own wall
        # is 89 thick and made of half a dozen parts; carving it by volume takes whatever is
        # actually there rather than assuming which mesh it is.
        if (ax0 <= wx <= ax1 and ay0 <= wy <= ay1 and az0 <= wz <= az1
                and not any(k in piece['name'] for k in HORROR_KEEP)):
            carved += 1
            continue
        mesh = cache.get(piece['mesh'])
        if mesh is None:
            mesh = unreal.load_asset(piece['mesh'])
            cache[piece['mesh']] = mesh or False
        if not mesh:
            missing += 1
            continue
        r = piece['rot']
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(wx, wy, wz),
                                        unreal.Rotator(roll=r[0], pitch=r[1], yaw=r[2]))
        if not a:
            continue
        sc = piece['scale']
        a.set_actor_scale3d(unreal.Vector(sc[0], sc[1], sc[2]))
        c = a.static_mesh_component
        # Their materials, not ours. The whole point of taking the chunk whole is that it looks
        # like the pack intended, and the facility's grime is a different kit's idea.
        for slot, mp in enumerate(piece['mats']):
            if not mp:
                continue
            mm = cache.get(mp)
            if mm is None:
                mm = unreal.load_asset(mp)
                cache[mp] = mm or False
            if mm:
                c.set_material(slot, mm)
        c.set_mobility(unreal.ComponentMobility.STATIC)
        a.set_actor_label('%s%04d' % (HORROR_PREFIX, i))
        try: a.set_folder_path('HorrorSection')
        except Exception: pass
        n += 1

    for j, L in enumerate(doc.get('lights', [])):
        cls = unreal.SpotLight if 'Spot' in L.get('class', '') else unreal.PointLight
        a = eas.spawn_actor_from_class(cls, unreal.Vector(HORROR_X + L['loc'][0], HORROR_Y + L['loc'][1], HORROR_Z + L['loc'][2]),
                                       unreal.Rotator(roll=L['rot'][0], pitch=L['rot'][1], yaw=L['rot'][2]))
        if not a:
            continue
        lc = a.light_component
        # MOVABLE, like every other light in this map. A PointLight spawned from Python defaults
        # to STATIONARY, which wants a baked lightmap and puts "LIGHTING NEEDS TO BE REBUILT"
        # across the screen -- for 39 lights that arrived with the transplant, over a level of
        # two thousand actors that has never been baked and does not need to be. The facility's
        # own light() helper has always set this; the transplant forgot to.
        lc.set_mobility(unreal.ComponentMobility.MOVABLE)
        try:
            # UNITS FIRST. Intensity means nothing without them: every Synty pack lights in
            # candelas, and setting 1200 against a lumens default is a different lamp entirely.
            # This is where the red emergency glow in the pack's own screenshots comes from --
            # their numbers, not ours. Our facility runs far brighter and that look is wrong here.
            units = L.get('units', '')
            if 'CANDELAS' in str(units).upper():
                lc.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
            elif 'LUMENS' in str(units).upper():
                lc.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
            elif 'EV' in str(units).upper():
                lc.set_editor_property('intensity_units', unreal.LightUnits.EV)
            lc.set_intensity(float(L.get('intensity', 1000.0)))
            col = L.get('color', [1.0, 1.0, 1.0])
            lc.set_light_color(unreal.LinearColor(r=col[0], g=col[1], b=col[2], a=1.0))
            lc.set_attenuation_radius(float(L.get('radius', 500.0)))
            lc.set_cast_shadows(bool(L.get('shadows', False)))
            if L.get('use_temperature'):
                lc.set_editor_property('use_temperature', True)
                lc.set_editor_property('temperature', float(L.get('temperature', 6500.0)))
            if cls is unreal.SpotLight:
                for k in ('outer_cone', 'inner_cone'):
                    if k in L:
                        lc.set_editor_property(k.replace('cone', 'cone_angle'), float(L[k]))
        except Exception as e:
            print('HORROR light', e)
        a.set_actor_label('%sLight_%03d' % (HORROR_PREFIX, j))
        try: a.set_folder_path('HorrorSection')
        except Exception: pass

    placed.add(HORROR_LABEL)
    print('HORROR: %d pieces, %d lights, %d carved for the doorway, %d meshes missing'
          % (n, len(doc.get('lights', [])), carved, missing))


horror_section()

# ---- Sealing the cut -----------------------------------------------------------------------
# The window was cut through the middle of the demo, so three of its four edges are open floor
# with the world ending just past them -- you can see out of the level. They get capped.
#
# Capped with the FACILITY's own wall, not the horror kit's, and that is a choice rather than
# laziness: the edge of this section is where the station sealed off something it found, so a
# bulkhead in our kit meeting their corridors is the story the seam is telling. Inside the seal
# it is entirely their pack.
#
# Conventions are the ones documented at the top of this file and used by shell(): a wall piece
# at yaw 0 spans x..x+500 from its pivot, at yaw 180 it spans x-500..x, at yaw 90 it spans
# y..y+500 and at yaw -90 it spans y-500..y. WALL_T thick, room on the local +Y side.
HORROR_TILES = int(HORROR_SIZE // CELL)


# The seal was there to cap a cut through the middle of a demo map, and the transplant it capped
# is retired: a BUILT room has its own walls. Both it and the threshold pieces are removed.



# THE EPILOGUE LIVES AT THE FOOT OF THIS FILE. It writes the manifest and SAVES THE LEVEL, so
# anything laid out after it is built and then thrown away when the editor is next closed --
# which is exactly what happened to the service deck the first time: 900 actors spawned, none of
# them counted and none of them saved.


# ================================================================================================
# SUB 10 -- THE SERVICE DECK
# ================================================================================================
# Built, not transplanted. The first version lifted a 30 m chunk of the horror demo whole, and it
# worked but it was a warren: the demo is a set of tight, twisting rooms and a slice of it is a
# slice of a warren. What was actually wanted was ONE OPEN HALL with the pack's own vocabulary --
# grated floors with machinery underneath, overhead conduit, stairs up to a gantry, red emergency
# lighting, steam.
#
# EVERY NUMBER HERE IS MEASURED, by Tools/horror_kit_survey.py, off the kit and off the demo's own
# usage of it:
#
#     tile             250 cm, not our 500 -- Base_Floor_01 and Base_Ceiling_01 are 250 x 250
#     storey           300 cm -- Base_Wall_01 is 300.6 tall, and the demo puts ceilings at z 300
#     wall             250 wide, 22.5 thick, pivot at one end and CENTRED in its thickness
#     floor / ceiling  flat planes, pivot at a corner, spanning +X and +Y
#     pillar           43 x 43 x 301
#     stairs           Base_Stairs_01 climbs 150 over 250 of run, x 0..250, y -250..0, z -33..150
#     railing          250 long, 112 tall, pivot at one end, centred in thickness
#     pipe runs        250 long along their own +Y; the Large_Bundle is 500 long along +X
#     ceiling fan      Extractor_Fan_01 is 243 x 107 x 243 and thin in Y, so it FACES -Y
#
# and the FX heights are the demo's own, so the room breathes at the same altitudes its screenshots
# do: ground fog -25..50, steam 125..250, sparks 175..225, dust 200..300.
#
# The grate is not a mesh. The pack does it with MATERIALS on a plain floor plane --
# MI_Floor_Panel_Grill_03 for the grating, MI_Pipes_01 for the machinery you see through it -- so
# a trench is a sunken plane wearing the pipe material with grated planes laid over the top.
GEN = '/Game/Synty/PolygonGeneric/Meshes/Base/'
HOR = '/Game/Synty/PolygonSciFiHorror/Meshes/Buildings/'
HPR = '/Game/Synty/PolygonSciFiHorror/Meshes/Props/'
HMAT = '/Game/Synty/PolygonSciFiHorror/Materials/'
HFX = '/Game/Synty/PolygonSciFiHorror/FX/'

M_FLOOR   = HMAT + 'Floor/MI_Floor_Panel_01'
M_GRILL   = HMAT + 'Floor/MI_Floor_Panel_Grill_03'
M_PIPES   = HMAT + 'Floor/MI_Pipes_01'
M_WALL    = HMAT + 'Wall/MI_Wall_02'
M_TRIM    = HMAT + 'Wall/MI_Wall_Trims_01'
M_BODY    = HMAT + 'Alts/MI_PolygonSciFiHorror_Mat_01_A'

T = 250.0                      # the horror kit's tile
STOREY = 300.0                 # its storey height
SUB_Z = HORROR_Z               # Sub 10, the floor the lift already stops at
# A long hall rather than a square room: 12 tiles by 6 is 30 m by 15, which reads as a service
# deck you can see the far end of. The north wall lands on the lift (FY1) and the hall is set out
# so the shaft opening at world x 250..750 is in it.
SUB_NX, SUB_NY = 12, 6
SUB_X0 = -1000.0
SUB_Y1 = FY1                   # north edge, on the lift wall
SUB_Y0 = SUB_Y1 - SUB_NY * T
SUB_X1 = SUB_X0 + SUB_NX * T
# The trench runs the length of the hall, one tile wide, just south of the middle so it is not
# dead centre. Its floor is a metre down and wears the pipe material.
TRENCH_ROW = 2
TRENCH_DROP = 110.0
# The gantry: a raised walkway one tile deep along the south wall, half the hall's length.
GANTRY_Z = SUB_Z + 150.0
GANTRY_FROM, GANTRY_TO = 4, 10


def sub_tile(label, mesh, ix, iy, z, mat, yaw=0.0, roll=0.0, pitch=0.0):
    place(label, mesh, SUB_X0 + ix * T, SUB_Y0 + iy * T, z, yaw=yaw, roll=roll, pitch=pitch,
          mat=False, material=mat)


def build_sub_deck():
    # NOT gated on HORROR_LABEL. That label belongs to the retired transplant and is now on the
    # removed list by definition, so guarding on it here built nothing at all.
    # ---- floor, trench and ceiling ------------------------------------------------------------
    for ix in range(SUB_NX):
        for iy in range(SUB_NY):
            if iy == TRENCH_ROW:
                # The trench: a sunken plane wearing the machinery material, with a grated plane
                # laid over it at floor level so you walk on it and see through it.
                sub_tile('Sub_Trench_%d' % ix, GEN + 'SM_Bld_Base_Floor_01', ix, iy,
                         SUB_Z - TRENCH_DROP, M_PIPES)
                sub_tile('Sub_Grate_%d' % ix, GEN + 'SM_Bld_Base_Floor_01', ix, iy, SUB_Z, M_GRILL)
            else:
                sub_tile('Sub_Floor_%d_%d' % (ix, iy), GEN + 'SM_Bld_Base_Floor_01', ix, iy,
                         SUB_Z, M_FLOOR)
            sub_tile('Sub_Ceil_%d_%d' % (ix, iy), GEN + 'SM_Bld_Base_Ceiling_01', ix, iy,
                     SUB_Z + STOREY, M_FLOOR)

    # Something to see under the grating. The pipe runs are 250 long along their own +Y, so yaw 90
    # turns them along the hall.
    for ix in range(SUB_NX):
        kind = ('SM_Prop_Pipe_Straight_Full_02', 'SM_Prop_Pipe_Straight_Full_03',
                'SM_Prop_Pipe_Straight_Full_01')[ix % 3]
        sub_tile('Sub_TrenchPipe_%d' % ix, HPR + kind, ix, TRENCH_ROW, SUB_Z - TRENCH_DROP + 40.0,
                 M_BODY, yaw=90.0)
    for ix in range(1, SUB_NX, 4):
        sub_tile('Sub_TrenchCable_%d' % ix, HPR + 'SM_Prop_Cable_Pile_08', ix, TRENCH_ROW,
                 SUB_Z - TRENCH_DROP + 5.0, M_BODY, yaw=35.0 * ix)

    # ---- the shell ----------------------------------------------------------------------------
    # Walls are 250 wide with their pivot at one end and centred in their 22.5 thickness, so a
    # wall laid at a tile corner spans exactly that tile. The lift opening (world x 250..750) is
    # left out of the north run -- that gap is the way in.
    for ix in range(SUB_NX):
        wx = SUB_X0 + ix * T
        if not (240.0 <= wx < 750.0):
            place('Sub_Wall_N%d' % ix, GEN + 'SM_Bld_Base_Wall_01', wx, SUB_Y1, SUB_Z,
                  yaw=0, mat=False, material=M_WALL)
        place('Sub_Wall_S%d' % ix, GEN + 'SM_Bld_Base_Wall_01', wx, SUB_Y0, SUB_Z,
              yaw=0, mat=False, material=M_WALL)
        # Skirting trim along both runs, the way the demo dresses every wall it owns.
        place('Sub_Trim_N%d' % ix, HOR + 'SM_Bld_Wall_Trim_02', wx, SUB_Y1 - 12.0, SUB_Z,
              yaw=180, mat=False, material=M_TRIM)
        place('Sub_Trim_S%d' % ix, HOR + 'SM_Bld_Wall_Trim_02', wx, SUB_Y0 + 12.0, SUB_Z,
              yaw=0, mat=False, material=M_TRIM)
    # East and west ends: yaw 90 turns a wall's own +X onto world +Y, so it spans y..y+250.
    for iy in range(SUB_NY):
        wy = SUB_Y0 + iy * T
        place('Sub_Wall_W%d' % iy, GEN + 'SM_Bld_Base_Wall_01', SUB_X0, wy, SUB_Z,
              yaw=90, mat=False, material=M_WALL)
        place('Sub_Wall_E%d' % iy, GEN + 'SM_Bld_Base_Wall_01', SUB_X1, wy, SUB_Z,
              yaw=90, mat=False, material=M_WALL)
    for _cx, _cy in ((SUB_X0, SUB_Y0), (SUB_X1, SUB_Y0), (SUB_X0, SUB_Y1), (SUB_X1, SUB_Y1)):
        place('Sub_Pillar_%d_%d' % (_cx, _cy), GEN + 'SM_Bld_Base_Pillar_01', _cx, _cy, SUB_Z,
              mat=False, material=M_BODY)

    # ---- the gantry ---------------------------------------------------------------------------
    # One tile deep against the south wall, 150 up -- exactly what Base_Stairs_01 climbs, so the
    # stairs meet it without a step at the top.
    for ix in range(GANTRY_FROM, GANTRY_TO):
        sub_tile('Sub_Gantry_%d' % ix, GEN + 'SM_Bld_Base_Floor_01', ix, 0, GANTRY_Z, M_GRILL)
        # Railing along its open edge, one tile north of the wall.
        place('Sub_GantryRail_%d' % ix, HOR + 'SM_Bld_Railing_01',
              SUB_X0 + ix * T, SUB_Y0 + T, GANTRY_Z, yaw=0, mat=False, material=M_BODY)
    # Stairs at the east end of the gantry, climbing south-to-north out of the room. The mesh runs
    # x 0..250 and y -250..0 with its top at z 150, so its pivot goes at the gantry's own corner.
    place('Sub_Stairs', GEN + 'SM_Bld_Base_Stairs_01',
          SUB_X0 + GANTRY_TO * T, SUB_Y0 + T, SUB_Z, yaw=0, mat=False, material=M_BODY)
    place('Sub_StairRail', HOR + 'SM_Bld_Railing_Stairs_01',
          SUB_X0 + GANTRY_TO * T, SUB_Y0 + T, SUB_Z, yaw=0, mat=False, material=M_BODY)

    # ---- overhead conduit ---------------------------------------------------------------------
    # The Large_Bundle is 500 long along its own +X, so two tiles per piece and no rotation.
    for ix in range(0, SUB_NX, 2):
        for _row, _off in ((1, 0.0), (4, 0.0)):
            place('Sub_Conduit_%d_%d' % (ix, _row), HPR + 'SM_Prop_Pipe_Large_Bundle_01',
                  SUB_X0 + ix * T, SUB_Y0 + _row * T + 125.0, SUB_Z + STOREY - 62.0,
                  yaw=0, mat=False, material=M_BODY)
    # Hangers, at the height the demo hangs them.
    for ix in range(1, SUB_NX, 3):
        for _row in (1, 4):
            place('Sub_Strut_%d_%d' % (ix, _row), HPR + 'SM_Prop_Pipe_Strut_03',
                  SUB_X0 + ix * T, SUB_Y0 + _row * T + 125.0, SUB_Z + STOREY - 20.0,
                  yaw=90, mat=False, material=M_BODY)

    # ---- fans, lights, and the red ------------------------------------------------------------
    # The fan is thin in Y so its face is -Y; roll -90 sends +Z to -Y, which turns that face
    # downward into the room.
    for ix in (2, 6, 10):
        place('Sub_Fan_%d' % ix, HPR + 'SM_Prop_Extractor_Fan_01',
              SUB_X0 + ix * T, SUB_Y0 + 3 * T, SUB_Z + STOREY - 4.0,
              roll=-90, mat=False, material=M_BODY)
        place('Sub_FanBlades_%d' % ix, HPR + 'SM_Prop_Extractor_Fan_01_Blades_01',
              SUB_X0 + ix * T, SUB_Y0 + 3 * T, SUB_Z + STOREY - 4.0,
              roll=-90, mat=False, material=M_BODY)

    # Working light: white panels down the middle of the ceiling, each with its own lamp. Their
    # numbers are the pack's, in candelas, not the facility's much brighter fittings.
    for ix in range(1, SUB_NX, 2):
        lx = SUB_X0 + ix * T + 125.0
        ly = SUB_Y0 + 3 * T
        place('Sub_Lamp_%d' % ix, HPR + 'SM_Prop_Light_Grid_01', lx, ly, SUB_Z + STOREY - 6.0,
              roll=-90, mat=False, material=M_BODY)
        light('Sub_Lamp_%d_Light' % ix, lx, ly, SUB_Z + STOREY - 60.0, 14.0, 700.0,
              (1.0, 0.94, 0.86))
    # And the red. This is the pack's signature and most of what its screenshots are made of:
    # low, wall-mounted, deeply saturated, spaced so the hall reads in bands rather than evenly.
    for ix in range(0, SUB_NX, 3):
        for _wy, _yaw in ((SUB_Y0 + 14.0, 0.0), (SUB_Y1 - 14.0, 180.0)):
            place('Sub_Red_%d_%d' % (ix, int(_wy)), HPR + 'SM_Prop_Light_04',
                  SUB_X0 + ix * T + 125.0, _wy, SUB_Z + 190.0, yaw=_yaw,
                  mat=False, material=M_BODY)
            light('Sub_Red_%d_%d_Light' % (ix, int(_wy)), SUB_X0 + ix * T + 125.0,
                  _wy + (40.0 if _yaw == 0.0 else -40.0), SUB_Z + 190.0,
                  9.0, 560.0, (1.0, 0.08, 0.06))

    # ---- what makes it breathe ----------------------------------------------------------------
    # Heights straight off the demo: ground fog -25..50, steam 125..250, sparks 175..225,
    # dust 200..300. The steam is pulsed rather than constant, for the same reason as the bay's.
    for _i, _ix in enumerate((1, 5, 9)):
        steam('Sub_Fog_%d' % _i, 'horror_groundfog', SUB_X0 + _ix * T + 125.0,
              SUB_Y0 + 3 * T, SUB_Z + 20.0, scale=0.75)
    pulsed_steam('Sub_Steam_A', 'horror_steam', SUB_X0 + 3 * T, SUB_Y0 + T + 40.0, SUB_Z + 130.0,
                 yaw=90.0, pitch=8.0, burst=2.1, quiet=19.0, sound='steam_burst_1.wav', volume=0.28)
    pulsed_steam('Sub_Steam_B', 'horror_burst', SUB_X0 + 8 * T, SUB_Y1 - 60.0, SUB_Z + 230.0,
                 yaw=-90.0, pitch=-16.0, burst=1.6, quiet=27.0, sound='steam_burst_2.wav', volume=0.24)
    steam('Sub_Sparks_A', 'horror_sparks', SUB_X0 + 6 * T + 60.0, SUB_Y1 - 40.0, SUB_Z + 200.0,
          yaw=172.0)
    steam('Sub_Sparks_B', 'horror_sparks', SUB_X0 + 10 * T, SUB_Y0 + 40.0, SUB_Z + 180.0, yaw=-8.0)
    steam('Sub_Surge', 'horror_surge', SUB_X0 + 2 * T, SUB_Y0 + 40.0, SUB_Z + 200.0, yaw=-147.0)
    for _i, _ix in enumerate((2, 7, 11)):
        steam('Sub_Dust_%d' % _i, 'horror_dust', SUB_X0 + _ix * T, SUB_Y0 + 3 * T, SUB_Z + 200.0)

    # ---- a little clutter, kept sparse --------------------------------------------------------
    for _tag, _mesh, _ix, _iy, _yaw in (
            ('Crate_A', 'SM_Prop_Crate_01', 1, 4, 12.0),
            ('Crate_B', 'SM_Prop_Crate_02', 1, 4, -24.0),
            ('Barrel_A', 'SM_Prop_Barrel_01', 10, 1, 0.0),
            ('Barrel_B', 'SM_Prop_Barrel_02', 10, 1, 40.0),
            ('Crate_C', 'SM_Prop_Crate_03', 6, 5, 5.0),
            ('Vent', 'SM_Prop_Vent_02', 4, 5, 180.0)):
        sub_tile('Sub_' + _tag, HPR + _mesh, _ix, _iy, SUB_Z, M_BODY, yaw=_yaw)



# ================================================================================================
# SUB 10, ROOM ONE -- the lift lobby
# ================================================================================================
# Three tiles by five, the lift door centred on the north short wall, the grate-over-pipes trench
# running down the middle the long way. Nothing else yet: this is the first room of an iterative
# build and the point of it is to look at it.
#
# The lift's shaft wall (SM_Bld_Lift_Wall_01, world x 250..750, its doorway at 410..590) stands
# just behind the north wall's plane; the room is centred on its doorway's x 500 and hangs south
# from FY1, the lift wall's south face. The north wall itself is laid in the horror kit with a
# horror door wall over the lift's doorway (see NORTH below), so nothing of the Space kit shows
# but the shaft leaves.
R1_NX, R1_NY = 5, 10           # was 3 x 5; grown on request
R1_X0 = 500.0 - R1_NX * T * 0.5
R1_Y1 = FY1
R1_Y0 = R1_Y1 - R1_NY * T
R1_X1 = R1_X0 + R1_NX * T
R1_Z = SUB_Z
R1_TRENCH_COL = R1_NX // 2     # the middle column
# This room is being iterated on. With REBUILD set, everything labelled S10_ is torn down at the
# start of the run and laid again from the numbers above -- WITHOUT going through the manifest's
# removed list, which would refuse the labels forever. Resizing a room moves every tile, and a
# re-run that keeps existing actors where they are (the rule for everything else in this file)
# would leave the old 3 x 5 standing inside the new 5 x 10.
R1_REBUILD = False   # the user has hand-tweaked the basement: keep, never re-lay
# Two courses of wall, so the ceiling is at 600 -- the demo's own second-storey height (its
# recipe has ceilings at z 600). Nothing in the kit is 450 tall, so the choice is 300 or 600.
R1_COURSES = 2
# A storey and a half. The kit has no half-height wall, so the upper course is the full wall
# squashed to half (Z scale 0.5); almost none of it shows -- the belt course at 300 and the
# cornice hanging 85 from the ceiling leave a 65 cm strip -- and everything that stands on
# that course (pilasters, alcoves, the pieces over the lift door) is squashed with it.
R1_UPPER_SCALE = 0.5
R1_H = STOREY + STOREY * R1_UPPER_SCALE
# The walkways are a LOW GANTRY, not a storey. The reference shot has a few steps up to a grated
# platform along each wall, and the steps are the pack's own SM_Bld_Stairs_01 -- measured: 250
# wide along its X, 154 deep, top at z 76, and it CLIMBS TOWARD ITS OWN -Y (bottom step at local
# y +24, top at y -128). So the platform sits at 76, and each side is: a flight at row 2 climbing
# north onto a platform over rows 3..6, and a flight at row 7 climbing south onto it -- two
# stairs a side, both running ALONG the wall the way the reference does, none sticking out into
# the room. Under the platform there is 76 cm for low clutter and a lamp.
#
# Every horror-kit piece keeps ITS OWN material from here on (material=None): the first pass
# forced the horror body material onto a generic Base stair whose UVs were never cut for it,
# which is where the "crazy textures" came from.
R1_STAIR_TOP = 76.0
R1_WALK_Z = R1_Z + R1_STAIR_TOP
# Full length now, both sides, with the flights PERPENDICULAR: each one comes down off the
# walkway's edge into the room, at a quarter and three quarters of the length.
R1_WALK_ROWS = range(0, R1_NY)
R1_STAIR_ROWS = (2, 7)


def r1_tile(label, mesh, ix, iy, z, mat, yaw=0.0, roll=0.0, pitch=0.0):
    place(label, mesh, R1_X0 + ix * T, R1_Y0 + iy * T, z, yaw=yaw, roll=roll, pitch=pitch,
          mat=False, material=mat)


def build_sub_room():
    if R1_REBUILD:
        gone = 0
        for lbl in [l for l in existing if l.startswith('S10_')]:
            try:
                eas.destroy_actor(existing.pop(lbl)); gone += 1
            except Exception:
                pass
            placed.discard(lbl); removed.discard(lbl)
        # And every S10_ label the manifest still remembers, whether or not it is standing: a
        # prop that was SKIPPED last run (no clear floor) is on the placed list from an older run
        # with no actor behind it, which ensure() reads as a deletion by hand and refuses forever.
        # Nothing under S10_ is the manifest's to remember; the room is laid whole each run.
        for lbl in [l for l in (placed | removed) if l.startswith('S10_')]:
            placed.discard(lbl); removed.discard(lbl)
        print('S10: rebuilt from scratch, %d old pieces down' % gone)

    # ---- floor: plain columns, and the trench down the middle --------------------------------
    for ix in range(R1_NX):
        for iy in range(R1_NY):
            if ix == R1_TRENCH_COL:
                r1_tile('S10_Trench_%d' % iy, GEN + 'SM_Bld_Base_Floor_01', ix, iy,
                        R1_Z - TRENCH_DROP, M_PIPES)
                r1_tile('S10_Grate_%d' % iy, GEN + 'SM_Bld_Base_Floor_01', ix, iy, R1_Z, M_GRILL)
            else:
                r1_tile('S10_Floor_%d_%d' % (ix, iy), GEN + 'SM_Bld_Base_Floor_01', ix, iy,
                        R1_Z, M_FLOOR)
            r1_tile('S10_Ceil_%d_%d' % (ix, iy), GEN + 'SM_Bld_Base_Ceiling_01', ix, iy,
                    R1_Z + R1_H, M_FLOOR)
    # What is under the grating. The trench runs along Y and so do the pipe meshes (250 along
    # their own +Y), so no rotation.
    for iy in range(R1_NY):
        kind = ('SM_Prop_Pipe_Straight_Full_02', 'SM_Prop_Pipe_Straight_Full_03',
                'SM_Prop_Pipe_Straight_Full_01')[iy % 3]
        r1_tile('S10_TrenchPipe_%d' % iy, HPR + kind, R1_TRENCH_COL, iy,
                R1_Z - TRENCH_DROP + 40.0, None)
    r1_tile('S10_TrenchCable', HPR + 'SM_Prop_Cable_Pile_08', R1_TRENCH_COL, 2,
            R1_Z - TRENCH_DROP + 5.0, None, yaw=40.0)

    # ---- the sill: floor under the lift wall's thickness, so the threshold is not a hole ------
    # Floor_Half is 250 x 125 (x 0..250, y 0..125). Two of them span the 500 opening and reach
    # from the room's edge at FY1 to 125 beyond it, which is past the wall (91.5 thick) and just
    # into the car. A centimetre down, so it cannot fight the car's own floor where they meet.
    for k, sx in enumerate((250.0, 500.0)):
        place('S10_Sill_%d' % k, GEN + 'SM_Bld_Base_Floor_Half_01', sx, R1_Y1, R1_Z - 1.0,
              mat=False, material=M_FLOOR)

    # ---- walls ---------------------------------------------------------------------------------
    # Walls are 250 wide, pivot at one end, centred in their 22.5 thickness. Yaw 90 turns +X onto
    # +Y for the long sides.
    # Measured (Tools scratch, UV footprint per face): the Base kit's wall is textured on BOTH
    # faces -- only its bottom edge is painted black -- so unlike the Space kit it does not care
    # which way round it stands. One yaw per side is fine.
    for course in range(R1_COURSES):
        cz = R1_Z + course * STOREY
        zsc = (1.0, 1.0, R1_UPPER_SCALE) if course > 0 else None
        for iy in range(R1_NY):
            # Every third tile of the upper course is an alcove -- a recess 45 deep into the wall
            # -- so the long walls are not two flat planes. The alcove mesh keeps the kit's own
            # material; its recess is on its -Y, which at yaw 90 is +X: the room side for the
            # west wall and the outside for the east, so the east alcoves are yawed the other way.
            alcove = False   # the upper course sits behind the 146-tall cornice; an alcove there would never be seen
            if alcove:
                place('S10_Wall_W%d_%d' % (course, iy), HOR + 'SM_Bld_Wall_Alcove_02', R1_X0, R1_Y0 + iy * T, cz, yaw=90, mat=False, scale=zsc)
                place('S10_Wall_E%d_%d' % (course, iy), HOR + 'SM_Bld_Wall_Alcove_02', R1_X1, R1_Y0 + (iy + 1) * T, cz, yaw=-90, mat=False, scale=zsc)
            else:
                place('S10_Wall_W%d_%d' % (course, iy), GEN + 'SM_Bld_Base_Wall_01', R1_X0, R1_Y0 + iy * T, cz,
                      yaw=90, mat=False, material=M_WALL, scale=zsc)
                place('S10_Wall_E%d_%d' % (course, iy), GEN + 'SM_Bld_Base_Wall_01', R1_X1, R1_Y0 + iy * T, cz,
                      yaw=90, mat=False, material=M_WALL, scale=zsc)
        for ix in range(R1_NX):
            place('S10_Wall_S%d_%d' % (course, ix), GEN + 'SM_Bld_Base_Wall_01', R1_X0 + ix * T, R1_Y0, cz,
                  yaw=0, mat=False, material=M_WALL, scale=zsc)
    # NORTH: THE LIFT DOOR, IN THE HORROR KIT. The Space lift wall stays where it is behind the
    # horror plane -- the lift needs its frame and its leaves run in it. On the horror plane the
    # north wall is one continuous horror wall with a horror DOOR WALL over the lift's doorway,
    # so the surround belongs to this room. Measured with complex traces (Tools/measure_doorwalls):
    # SM_Bld_Wall_Door_Double_01 is 258 wide with its opening at x 18..240 -- 222 wide, 243 tall
    # -- wider and a hair shorter than the Space doorway (180 x 249), so the shaft leaves show
    # through it with a strip of the Space jamb either side and nothing of the grille. Centred on
    # x 500 it spans 371..629; each flank is then 496: a whole tile and a 246 tile, the second
    # scaled 0.984 in x, which no eye can see. The upper course repeats the same five pieces
    # (the door span as a 258-wide tile, x-scaled 1.032) so the vertical seams line up.
    DOOR_W = 258.0
    DOOR_X0 = 500.0 - DOOR_W * 0.5
    DOOR_X1 = DOOR_X0 + DOOR_W
    NORTH_SEGS = ((R1_X0, T), (R1_X0 + T, DOOR_X0 - R1_X0 - T), (DOOR_X0, DOOR_W), (DOOR_X1, R1_X1 - T - DOOR_X1), (R1_X1 - T, T))
    for course in range(R1_COURSES):
        cz = R1_Z + course * STOREY
        zs = R1_UPPER_SCALE if course > 0 else 1.0
        for k, (sx, sw) in enumerate(NORTH_SEGS):
            if course == 0 and k == 2:
                place('S10_LiftSurround', HOR + 'SM_Bld_Wall_Door_Double_01', sx, R1_Y1, cz, yaw=0, mat=False)
                continue
            place('S10_Wall_N%d_%d' % (course, k), GEN + 'SM_Bld_Base_Wall_01', sx, R1_Y1, cz,
                  yaw=0, mat=False, material=M_WALL, scale=(sw / T, 1.0, zs))

    for course in range(R1_COURSES):
        for cx, cy in ((R1_X0, R1_Y0), (R1_X1, R1_Y0), (R1_X0, R1_Y1), (R1_X1, R1_Y1)):
            place('S10_Pillar_%d_%d_%d' % (course, cx, cy), GEN + 'SM_Bld_Base_Pillar_01', cx, cy,
                  R1_Z + course * STOREY, mat=False, material=M_WALL,
                  scale=(1.0, 1.0, R1_UPPER_SCALE) if course > 0 else None)

    # ---- trims at the floor and the ceiling ----------------------------------------------------
    # The demo dresses every wall junction, and its recipe says exactly how (242 trim pieces,
    # mined offline): the _01 trims sit ON THE FLOOR at z 0 and the _02 trims HANG FROM THE
    # CEILING at storey height, roll and pitch zero, and every piece is 250 (or 125 for a Half)
    # along its own X with its pivot on the wall face and 50 cm of cove running into the room
    # along its own +Y. So a trim is laid like a wall, then turned so +Y points at the room.
    # The lift opening in the north wall gets no trim, and the corners are left to the straight
    # runs meeting -- the kit has Trim_Corner pieces but their frame is unmeasured.
    def trim_run(tag, x, y, yaw, count, half_at_end, z, floor):
        # count full pieces then, if asked, one half piece, marching along the run's own +X.
        piece = 'SM_Bld_Wall_Trim_01' if floor else 'SM_Bld_Wall_Trim_02'
        halfp = 'SM_Bld_Wall_Trim_Half_01' if floor else 'SM_Bld_Wall_Trim_Half_02'
        yaw_r = math.radians(yaw)
        for k in range(count + (1 if half_at_end else 0)):
            mesh = halfp if (half_at_end and k == count) else piece
            px = x + math.cos(yaw_r) * k * T
            py = y + math.sin(yaw_r) * k * T
            place('S10_Trim_%s_%s_%d' % ('F' if floor else 'C', tag, k), HOR + mesh, px, py, z,
                  yaw=yaw, mat=False)
    off = 11.25   # the Base wall's half thickness: the trim's pivot goes on the room-side face
    for floor, tz in ((True, R1_Z), (False, R1_Z + R1_H)):
        # South wall: room is +Y, so yaw 0 puts the cove into the room; run along +X.
        trim_run('S', R1_X0, R1_Y0 + off, 0.0, R1_NX, False, tz, floor)
        # West wall: room is +X. yaw -90 sends +Y to +X; the run's +X goes to -Y, so start at the
        # north end and march south.
        trim_run('W', R1_X0 + off, R1_Y1, -90.0, R1_NY, False, tz, floor)
        # East wall: room is -X. yaw 90 sends +Y to -X; the run marches north from the south end.
        trim_run('E', R1_X1 - off, R1_Y0, 90.0, R1_NY, False, tz, floor)
        # North wall, either side of the lift opening (x 250..750). yaw 180 sends +Y to -Y (into
        # the room) and the run marches -X, so each flank starts at its east end: 375 each side
        # is one full piece and one half.
        trim_run('NE', R1_X1, R1_Y1 - off, 180.0, 1, True, tz, floor)
        trim_run('NW', 250.0, R1_Y1 - off, 180.0, 1, True, tz, floor)

    # ---- the walkways --------------------------------------------------------------------------
    for side, col, inner_x in (('W', 0, R1_X0 + T), ('E', R1_NX - 1, R1_X1 - T)):
        for iy in R1_WALK_ROWS:
            r1_tile('S10_Walk_%s_%d' % (side, iy), GEN + 'SM_Bld_Base_Floor_01', col, iy, R1_WALK_Z, M_GRILL)
            # Railing along the inner edge, except where a flight lands.
            if iy not in R1_STAIR_ROWS:
                place('S10_WalkRail_%s_%d' % (side, iy), HOR + 'SM_Bld_Railing_01',
                      inner_x, R1_Y0 + iy * T, R1_WALK_Z, yaw=90, mat=False)
        # The flights, PERPENDICULAR to the walkway. Stairs_01 climbs toward its own -Y with the
        # bottom step 24 in front of the pivot and the top step 128 behind it, 250 wide along its
        # X. For the west walkway the climb has to run -X (up to the edge at x 125): yaw -90
        # sends -Y to -X and the width onto -Y, so the pivot is at the row's far edge and 128
        # east of the walkway edge. East is the mirror at yaw 90.
        for iy in R1_STAIR_ROWS:
            if side == 'W':
                yaw, px, py = -90.0, inner_x + 128.0, R1_Y0 + (iy + 1) * T
            else:
                yaw, px, py = 90.0, inner_x - 128.0, R1_Y0 + iy * T
            place('S10_Stair_%s_%d' % (side, iy), HOR + 'SM_Bld_Stairs_01', px, py, R1_Z, yaw=yaw, mat=False)
            # Both sides of a perpendicular flight are open, so it gets a rail on each: the rail
            # mesh sits on its own x = 0, so one at the pivot and one a width along.
            for k, off in enumerate((0.0, T)):
                rx = px
                ry = py - off if side == 'W' else py + off
                place('S10_StairRail_%s_%d_%d' % (side, iy, k), HOR + 'SM_Bld_Railing_Stairs_01',
                      rx, ry, R1_Z, yaw=yaw, mat=False)

    # ---- trim, and more of it ------------------------------------------------------------------
    # The kit has a LARGE trim family and the demo uses it: Wall_Trim_Large_01 is a 146-tall,
    # 125-deep base moulding; the cornice is that same moulding upside down (see cornice()). Where the
    # walkways run the base can only be the small trim (the platform is 76 up); the end walls
    # and the ceiling take the large ones, and the joint between the two wall courses at 300 gets
    # the demo's ceiling trim as a belt course. An alcove in place of every third upper tile
    # breaks the long walls up.
    off = 11.25
    def band(tag, mesh, x, y, yaw, count, z, half=None):
        yaw_r = math.radians(yaw)
        for k in range(count):
            px = x + math.cos(yaw_r) * k * T
            py = y + math.sin(yaw_r) * k * T
            place('S10_Trim_%s_%d' % (tag, k), HOR + mesh, px, py, z, yaw=yaw, mat=False)
        if half:
            px = x + math.cos(yaw_r) * count * T
            py = y + math.sin(yaw_r) * count * T
            place('S10_Trim_%s_%d' % (tag, count), HOR + half, px, py, z, yaw=yaw, mat=False)
    # The north wall's floor moulding stops at the door frame: from the frame's edge outward a
    # whole piece, a half, and a half scaled to the 121 that remain to the corner. Pieces at yaw
    # 180 run toward -X from their pivot, so the east flank is laid from the corner in.
    def north_trim(tag, full, half, z):
        rem = (DOOR_X0 - R1_X0 - T - T * 0.5) / (T * 0.5)
        for side, pivots in (('E', ((R1_X1, full, 1.0), (R1_X1 - T, half, 1.0), (R1_X1 - T - T * 0.5, half, rem))),
                             ('W', ((DOOR_X0, full, 1.0), (DOOR_X0 - T, half, 1.0), (DOOR_X0 - T - T * 0.5, half, rem)))):
            for k, (px, mesh, sx) in enumerate(pivots):
                place('S10_Trim_%sN%s_%d' % (tag, side, k), HOR + mesh, px, R1_Y1 - off, z, yaw=180.0, mat=False,
                      scale=(sx, 1.0, 1.0) if abs(sx - 1.0) > 1e-3 else None)
    # Floor: large on the end walls, small under the walkways.
    band('FS', 'SM_Bld_Wall_Trim_Large_01', R1_X0, R1_Y0 + off, 0.0, R1_NX, R1_Z)
    band('FW', 'SM_Bld_Wall_Trim_01', R1_X0 + off, R1_Y1, -90.0, R1_NY, R1_Z)
    band('FE', 'SM_Bld_Wall_Trim_01', R1_X1 - off, R1_Y0, 90.0, R1_NY, R1_Z)
    north_trim('F', 'SM_Bld_Wall_Trim_Large_01', 'SM_Bld_Wall_Trim_Half_01', R1_Z)
    # Belt course at the joint of the two wall courses, and the large cornice at the ceiling:
    # these run the whole north wall, over the door frame's lintel too.
    for tag, mesh, z in (('B', 'SM_Bld_Wall_Trim_02', R1_Z + STOREY),):
        band(tag + 'S', mesh, R1_X0, R1_Y0 + off, 0.0, R1_NX, z)
        band(tag + 'W', mesh, R1_X0 + off, R1_Y1, -90.0, R1_NY, z)
        band(tag + 'E', mesh, R1_X1 - off, R1_Y0, 90.0, R1_NY, z)
        band(tag + 'N', mesh, R1_X1, R1_Y1 - off, 180.0, R1_NX, z)
    # THE CORNICE IS THE FLOOR MOULDING UPSIDE DOWN. Wall_Trim_Large_01 -- 250 along the wall,
    # 125 deep, 146 tall, the sloped base moulding -- rolled 180 and hung from the ceiling gives
    # the ceiling the same profile the floor has, and at 146 it takes the whole upper course
    # (300..450), which is what blends wall into ceiling rather than a lip. The roll sends the
    # piece's depth to local -Y and its height downward, so each piece is yawed 180 from its
    # band and its pivot is the FAR end of its tile: rolled and turned, it then runs back across
    # the tile with its depth into the room. Verified by reading the placed boxes back.
    def cornice(tag, x, y, yaw, count):
        yaw_r = math.radians(yaw)
        for k in range(count):
            px = x + math.cos(yaw_r) * (k + 1) * T
            py = y + math.sin(yaw_r) * (k + 1) * T
            place('S10_Trim_C%s_%d' % (tag, k), HOR + 'SM_Bld_Wall_Trim_Large_01', px, py, R1_Z + R1_H,
                  yaw=yaw + 180.0, roll=180.0, mat=False)
    cornice('S', R1_X0, R1_Y0 + off, 0.0, R1_NX)
    cornice('W', R1_X0 + off, R1_Y1, -90.0, R1_NY)
    cornice('E', R1_X1 - off, R1_Y0, 90.0, R1_NY)
    cornice('N', R1_X1, R1_Y1 - off, 180.0, R1_NX)
    # No pilasters. Pillar_Large_01 was tried on the upper course and read as misplaced; the
    # piece stays in the kit for a later pass that has a use for it.

    # ---- clutter under the walkways, in the demo's own vocabulary ----------------------------
    def r1_prop(label, mesh, ix, iy, yaw=0.0, dx=0.0, dy=0.0):
        place(label, HPR + mesh, R1_X0 + ix * T + 125.0 + dx, R1_Y0 + iy * T + 125.0 + dy, R1_Z,
              yaw=yaw, mat=False)
    # A MINING EQUIPMENT MAINTENANCE AND STORAGE BAY, disused.
    #
    # REAL THINGS DO NOT PASS THROUGH EACH OTHER. Walls, floors and trim overlap by design; a
    # crate half inside a barrel is a mistake, and it was happening because props were dropped
    # on tile centres with no idea how big they were. So every item is placed through prop(),
    # which reads the mesh's bounds, turns them into a world box at the requested pose, and:
    #   - refuses the spot if the box overlaps anything already placed (props, the flights),
    #     trying up to a metre of nudges around it first and saying so if none is clear;
    #   - refuses anything that would stand higher than the walkway it is under (76 cm), or
    #     poke into a wall, rather than clipping it through.
    # The kit's centred pivots (generator, power cells) are lifted onto the floor by dz.
    HALL = (R1_X0 + off, R1_Y0 + off, R1_X1 - off, R1_Y1 - off)
    WALK_TOP = R1_WALK_Z - R1_Z - 2.0
    WALK_COLS = {'W': (R1_X0, R1_X0 + T), 'E': (R1_X1 - T, R1_X1)}
    placed_boxes = []
    # THE BUILDING, as boxes. Everything S10_ standing by now that is not a prop, a light or an
    # effect -- walls, trims, stairs and their rails, pillars, the door wall, the alcoves -- read
    # back from the level, so the boxes are the real ones. Planes (floors, decks, grates) have no
    # thickness and are handled by height: a prop RESTS on the floor and stays under a deck.
    ARCH_NOT = ('S10_Clutter_', 'S10_Fog', 'S10_Steam', 'S10_Dust', 'S10_Sign', 'S10_Lamp', 'S10_Red', 'S10_UnderWalk')
    arch_boxes = []
    for _a in eas.get_all_level_actors():
        _l = _a.get_actor_label()
        if not _l.startswith('S10_') or _l.startswith(ARCH_NOT) or '_Light' in _l:
            continue
        try:
            _o, _e = _a.get_actor_bounds(False)
        except Exception:
            continue
        if _e.z < 0.5:
            continue
        arch_boxes.append((_l, (_o.x - _e.x, _o.y - _e.y, _o.z - _e.z, _o.x + _e.x, _o.y + _e.y, _o.z + _e.z)))
    print('CLUTTER: %d pieces of architecture to keep clear of' % len(arch_boxes))

    def world_box(mesh, x, y, z, yaw, roll, pitch):
        b = mesh.get_bounds(); o, e = b.origin, b.box_extent
        xf = unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw), scale=unreal.Vector(1, 1, 1))
        pts = [xf.transform_location(unreal.Vector(o.x + sx * e.x, o.y + sy * e.y, o.z + sz * e.z))
               for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
        return (min(q.x for q in pts), min(q.y for q in pts), min(q.z for q in pts),
                max(q.x for q in pts), max(q.y for q in pts), max(q.z for q in pts))

    def overlaps(a, b, slack=2.0):
        return not (a[3] <= b[0] + slack or b[3] <= a[0] + slack or a[4] <= b[1] + slack or b[4] <= a[1] + slack
                    or a[5] <= b[2] + slack or b[5] <= a[2] + slack)

    def prop(label, mesh_name, x, y, yaw=0.0, roll=0.0, pitch=0.0, under=None, bottom=None):
        """Places one prop so that it stands ON the floor whatever its pivot (the kit mixes
        base pivots, centred pivots and hanging ones), clear of every piece of the building and
        of every prop before it, nudging up to 130 cm around the asked-for spot first and saying
        so if no spot is clear. under='W'/'E' keeps it beneath that walkway: inside its column,
        no taller than the deck is high, and a hanging cable hangs from the deck's underside.
        bottom= rests its lowest point that far above the floor (a wall-hung rack)."""
        mesh = unreal.load_asset(HPR + mesh_name)
        if not mesh:
            print('CLUTTER MISSING', mesh_name); return
        limit = WALK_TOP if under else R1_H - 2.0
        b = mesh.get_bounds()
        hanging = b.origin.z + b.box_extent.z < 1.0 and b.origin.z - b.box_extent.z < -1.0   # pivot at its top
        why = None
        for dx, dy in [(0, 0)] + [(d * ux, d * uy) for d in (20, 40, 60, 80, 100, 130)
                                  for ux, uy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1))]:
            px, py = x + dx, y + dy
            box0 = world_box(mesh, px, py, 0.0, yaw, roll, pitch)   # about z 0: its own height range
            if bottom is not None:
                lift = bottom - box0[2]
            elif hanging and under:
                lift = WALK_TOP - box0[5]                             # hung from the deck's underside
                if box0[2] + lift < 0.0:
                    why = '%s hangs %.0f, longer than the deck is high' % (mesh_name, box0[5] - box0[2]); break
            else:
                lift = -box0[2]                                       # feet on the floor
            pz = R1_Z + lift
            box = (box0[0], box0[1], box0[2] + pz, box0[3], box0[4], box0[5] + pz)
            if box[5] - R1_Z > limit:
                why = '%s is %.0f tall, too tall for %s' % (mesh_name, box[5] - R1_Z, 'under the walkway' if under else 'the room'); break
            if box[0] < HALL[0] or box[1] < HALL[1] or box[3] > HALL[2] or box[4] > HALL[3]:
                continue   # into a wall
            if under and (box[0] < WALK_COLS[under][0] + 2.0 or box[3] > WALK_COLS[under][1] - 2.0):
                continue   # sticking out from under the deck into the lane
            if any(overlaps(box, ab, 1.0) for _l, ab in arch_boxes):
                continue   # through a trim, a flight, a rail, a pillar, the door frame
            if any(overlaps(box, pb) for _l, pb in placed_boxes):
                continue   # through another prop
            if (dx, dy) != (0, 0):
                print('CLUTTER %s nudged (%+d, %+d)' % (label, dx, dy))
            place(label, HPR + mesh_name, px, py, pz, yaw=yaw, roll=roll, pitch=pitch, mat=False)
            placed_boxes.append((label, box))
            return
        print('CLUTTER %s SKIPPED: %s' % (label, why or ('%s finds no clear floor near (%.0f, %.0f)' % (mesh_name, x, y))))


    def under(col, row, frac=0.5):
        return (R1_X0 + col * T + 125.0, R1_Y0 + row * T + T * frac)

    # UNDER THE WEST WALKWAY: hoses, small boxes, barrels on their sides.
    for k, (mesh, row, yaw, roll) in enumerate((
            ('SM_Prop_Cable_02', 0, 84.0, 0.0), ('SM_Prop_Cable_Pile_07', 1, 0.0, 0.0), ('SM_Prop_Crate_03', 2, 8.0, 0.0),
            ('SM_Prop_Barrel_02', 3, 0.0, 90.0), ('SM_Prop_Cable_10', 4, -30.0, 0.0), ('SM_Prop_Crate_01', 5, 22.0, 0.0),
            ('SM_Prop_Crate_04', 6, -20.0, 0.0), ('SM_Prop_Barrel_01', 7, 20.0, 90.0), ('SM_Prop_Body_Part_01', 8, 120.0, 0.0),
            ('SM_Prop_Crate_02', 9, 60.0, 0.0))):
        x, y = under(0, row)
        prop('S10_Clutter_W%02d' % k, mesh, x, y, yaw=yaw, roll=roll, under='W')
    # UNDER THE EAST WALKWAY.
    for k, (mesh, row, yaw, roll) in enumerate((
            ('SM_Prop_Barrel_01', 0, 0.0, 90.0), ('SM_Prop_Barrel_02', 1, 0.0, 90.0), ('SM_Prop_Cable_Pile_09', 2, 0.0, 0.0),
            ('SM_Prop_Crate_04', 3, -12.0, 0.0), ('SM_Prop_Cable_10', 4, 75.0, 0.0), ('SM_Prop_Cable_01', 5, 95.0, 0.0),
            ('SM_Prop_Barrel_02', 6, 0.0, 90.0), ('SM_Prop_Crate_02', 7, 30.0, 0.0), ('SM_Prop_Cable_03', 8, 100.0, 0.0),
            ('SM_Prop_Crate_04', 9, 25.0, 0.0))):
        x, y = under(4, row)
        prop('S10_Clutter_E%02d' % k, mesh, x, y, yaw=yaw, roll=roll, under='E')

    # THE FLOOR OF THE BAY. Benches flush to the south wall, machinery beside the east walkway
    # with its cells pulled, a tool rack by the lift door, storage in the open bays.
    # The south wall carries the 125-deep moulding, so the benches stand off it by that much;
    # the long bench runs along the lane between the west deck and the trench. The rack hangs
    # on the north wall's east flank, above the moulding and below the belt course.
    prop('S10_Clutter_Bench1', 'SM_Prop_WorkBench_01', 300.0, R1_Y0 + T + 60.0, 90.0)
    prop('S10_Clutter_Bench2', 'SM_Prop_WorkBench_03', 750.0, R1_Y0 + off + 125.0 + 66.0 + 6.0, 0.0)
    prop('S10_Clutter_Gen2',  'SM_Prop_Generator_02', 810.0, R1_Y0 + 4 * T + 125.0, -90.0)
    prop('S10_Clutter_Cell1', 'SM_Prop_Generator_PowerCell_01', 750.0, R1_Y0 + 2 * T + 125.0, 20.0)
    prop('S10_Clutter_Cell2', 'SM_Prop_Generator_PowerCell_01', 750.0, R1_Y0 + 7 * T + 125.0, -35.0)
    prop('S10_Clutter_Rack',  'SM_Prop_Weapon_Rack_02', 760.0, R1_Y1 - off - 1.0, 180.0, bottom=150.0)
    prop('S10_Clutter_Cabinet', 'SM_Prop_Cabinet_03', 750.0, R1_Y0 + T + 125.0, -90.0)
    prop('S10_Clutter_Crate1', 'SM_Prop_Crate_01', 210.0, R1_Y0 + 8 * T + 125.0, 14.0)
    prop('S10_Clutter_Crate2', 'SM_Prop_Crate_02', 330.0, R1_Y0 + 8 * T + 165.0, -31.0)
    prop('S10_Clutter_Crate3', 'SM_Prop_Crate_04', 790.0, R1_Y0 + 1 * T + 250.0, 5.0)
    prop('S10_Clutter_Barrel1', 'SM_Prop_Barrel_01', 720.0, R1_Y0 + 8 * T + 125.0, 0.0)
    prop('S10_Clutter_Barrel2', 'SM_Prop_Barrel_03', 795.0, R1_Y0 + 8 * T + 155.0, 40.0)
    prop('S10_Clutter_Bin1', 'SM_Prop_Bin_Hole_01', 250.0, R1_Y0 + 3 * T + 125.0, 90.0)
    prop('S10_Clutter_Bin2', 'SM_Prop_Bin_Hole_01', 250.0, R1_Y0 + 5 * T + 125.0, 15.0)
    # The sign over the lift door says what this place was. It is thin in Y with its face on -Y,
    # so on the north wall (room to -Y) it goes up as it is, on the door frame's lintel (z 243 to
    # 302, the frame's face 16.6 proud of the plane). Sign_Engineering_01 is 93 wide by 82 tall
    # with its pivot at its top-left corner; at 0.7 it is 65 x 57 and sits in the lintel's band.
    place('S10_Sign', HPR + 'SM_Prop_Sign_Engineering_01', 500.0 - 93.4 * 0.35, R1_Y1 - 16.6 - 8.5 * 0.7 - 1.0, R1_Z + 301.0,
          yaw=0, mat=False, scale=(0.7, 0.7, 0.7))

    # ---- light --------------------------------------------------------------------------------
    # Two white panels over the trench, and one red lamp on each long wall, in the pack's own
    # candela values. The fan is thin in Y so its face is -Y; roll -90 turns that face down.
    for k, iy in enumerate(range(1, R1_NY, 3)):
        lx = R1_X0 + R1_TRENCH_COL * T + 125.0
        if k == 1:
            # The middle lamp is dead: its fitting is there, its light is not. Nobody has
            # changed a tube down here in years.
            place('S10_Lamp_%d' % k, HPR + 'SM_Prop_Light_Grid_01', lx, R1_Y0 + iy * T + 125.0, R1_Z + R1_H - 6.0,
                  roll=-90, mat=False)
            continue
        ly = R1_Y0 + iy * T + 125.0
        place('S10_Lamp_%d' % k, HPR + 'SM_Prop_Light_Grid_01', lx, ly, R1_Z + R1_H - 6.0,
              roll=-90, mat=False)
        light('S10_Lamp_%d_Light' % k, lx, ly, R1_Z + R1_H - 60.0, 22.0, 900.0, (1.0, 0.94, 0.86))
    # Over each deck too, three a side between the flights: the decks were in the dark.
    # In from the deck's centre line: the cornice takes the 125 nearest the wall at the ceiling.
    for side, lx in (('W', R1_X0 + 185.0), ('E', R1_X1 - 185.0)):
        for k, iy in enumerate((0.5, 4.5, 8.5)):
            ly = R1_Y0 + iy * T + 125.0
            place('S10_Lamp%s_%d' % (side, k), HPR + 'SM_Prop_Light_Grid_01', lx, ly, R1_Z + R1_H - 6.0, roll=-90, mat=False)
            light('S10_Lamp%s_%d_Light' % (side, k), lx, ly, R1_Z + R1_H - 60.0, 12.0, 800.0, (1.0, 0.94, 0.86))
    # And under each walkway, or the clutter is in the dark: one small lamp per side, mid-room.
    for side, ux in (('W', R1_X0 + 125.0), ('E', R1_X1 - 125.0)):
        light('S10_UnderWalk_%s_Light' % side, ux, R1_Y0 + R1_NY * T * 0.5, R1_WALK_Z - 18.0,
              3.0, 380.0, (1.0, 0.86, 0.70))
    k = 0
    for iy in (2.0, 4.5, 7.0):   # the two originals, and one on each side midway between them
        for wx, yaw, dx in ((R1_X0 + 14.0, -90.0, 40.0), (R1_X1 - 14.0, 90.0, -40.0)):
            wy = R1_Y0 + iy * T + 125.0
            place('S10_Red_%d' % k, HPR + 'SM_Prop_Light_04', wx, wy, R1_Z + 190.0, yaw=yaw,
                  mat=False)
            light('S10_Red_%d_Light' % k, wx + dx, wy, R1_Z + 190.0, 8.0, 520.0, (1.0, 0.08, 0.06))
            k += 1

    # ---- and one breath of atmosphere ---------------------------------------------------------
    for k, iy in enumerate(range(2, R1_NY, 5)):
        steam('S10_Fog_%d' % k, 'horror_groundfog', R1_X0 + R1_NX * T * 0.5, R1_Y0 + iy * T + 125.0,
              R1_Z + 20.0, scale=0.7)
    # The whole floor, not two patches: a grid of emitters at 400 cm, one in from every wall,
    # so the fog reaches the corners and under the decks. Additive labels: a re-run keeps them.
    # (Eighteen emitters at 0.9 did fill the floor, and halved the frame rate: every one is a
    # sheet of translucent particles, and they all overlapped. SIX at twice the scale cover the
    # same floor -- the particle count is per system, so a larger actor is more fog for the same
    # overdraw. The eighteen are retired through the manifest.)
    k = 0
    for gx in (R1_X0 + 312.0, R1_X1 - 312.0):
        for gy in (R1_Y0 + 700.0, R1_Y0 + 1800.0):
            steam('S10_FogJ_%d' % k, 'horror_groundfog', gx, gy, R1_Z + 20.0, scale=1.0); k += 1
    # OVERHEAD PIPES, the way the horror demo hangs them: two racks of small pipe
    # (Pipe_Straight_Full_03, 94 wide, runs +Y from its pivot) along the room under the
    # ceiling over the lanes between the decks and the trench, a fat main
    # (Pipe_Straight_Full_02, 62 across) beside the west rack with a ceiling flange at each
    # end, and a hanger strut every other tile. Placed clear of the lamps (trench x 468..532,
    # decks x 28..92 and 908..972) and of the cornice (the 125 nearest each wall). Additive
    # labels: a re-run keeps what is there.
    PIPE_Z = R1_Z + R1_H
    for tag, px in (('W', 280.0), ('E', 720.0)):
        for k in range(9):
            place('S10_Pipe_Rack%s_%d' % (tag, k), HPR + 'SM_Prop_Pipe_Straight_Full_03', px, R1_Y0 + 125.0 + k * T, PIPE_Z - 12.0, yaw=0, mat=False)
        for k in range(0, 9, 2):
            place('S10_Pipe_Strut%s_%d' % (tag, k), HPR + 'SM_Prop_Pipe_Strut_01', px, R1_Y0 + 125.0 + k * T + 125.0, PIPE_Z, yaw=0, mat=False)
    for k in range(9):
        place('S10_Pipe_Main_%d' % k, HPR + 'SM_Prop_Pipe_Straight_Full_02', 380.0, R1_Y0 + 125.0 + k * T, PIPE_Z - 36.0, yaw=0, mat=False)
    place('S10_Pipe_Flange_0', HPR + 'SM_Prop_Pipe_Connector_02', 380.0, R1_Y0 + 125.0, PIPE_Z, yaw=0, mat=False)
    place('S10_Pipe_Flange_1', HPR + 'SM_Prop_Pipe_Connector_02', 380.0, R1_Y1 - 125.0, PIPE_Z, yaw=0, mat=False)
    pulsed_steam('S10_Steam_0', 'horror_steam', R1_X1 - 30.0, R1_Y0 + 3 * T + 60.0, R1_Z + 140.0,
                 yaw=180.0, pitch=10.0, burst=2.0, quiet=21.0, sound='steam_burst_1.wav', volume=0.26)
    pulsed_steam('S10_Steam_1', 'horror_steam', R1_X0 + 30.0, R1_Y0 + 8 * T + 60.0, R1_Z + 150.0,
                 yaw=0.0, pitch=12.0, burst=1.7, quiet=29.0, sound='steam_burst_2.wav', volume=0.22)
    for k, iy in enumerate((1, 5, 8)):
        steam('S10_Dust_%d' % k, 'horror_dust', R1_X0 + R1_TRENCH_COL * T + 125.0, R1_Y0 + iy * T + 125.0, R1_Z + 200.0)


build_sub_room()

io.open(os.path.join(unreal.Paths.project_dir(), 'UI', 'SteamVents.json'), 'w', encoding='utf-8', newline='\n').write(
    json.dumps({'_': 'Written by Tools/facility_layout.py. UAmbientPlayer reads it to hang a positional hiss on every vent that has one, so the coordinates live in exactly one place.',
                'vents': STEAM_VENTS}, indent=1))
print('steam vents:', len(STEAM_VENTS), 'audible of', sum(1 for _ in STEAM_VENTS))

json.dump({'placed': sorted(placed), 'removed': sorted(removed)}, open(MANIFEST, 'w', encoding='utf-8'), indent=1)
print('layout: %s; kept deleted: %s; saved: %s' % (stats, skipped, les.save_current_level()))
