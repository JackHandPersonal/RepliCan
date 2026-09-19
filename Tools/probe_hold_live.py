"""Read the player's weapon hold out of the RUNNING game, without touching it.

The hand tuner and the game are meant to solve the same hold, and they do not look the same, so the
question is which INPUT differs. This reads the live pawn's reflected properties and the bones and
transforms that the solve works from, and prints them. It writes nothing and changes nothing: it is
safe to run while someone is playing, though it does run on the game thread, so expect one hitched
frame.

  Tools/ue_remote --file Tools/probe_hold_live
"""
import unreal

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_game_world()
if world is None:
    raise SystemExit('the game is not running: start Play, then run this again')

pawn = None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BaseCharacter):
    pc = a.get_instigator_controller()
    if isinstance(pc, unreal.PlayerController) or a.get_name().lower().startswith('bp_player') or pawn is None:
        pawn = a
        if isinstance(pc, unreal.PlayerController):
            break
if not pawn:
    raise SystemExit('no BaseCharacter in the running world')

print('pawn: %s  class %s' % (pawn.get_name(), pawn.get_class().get_name()))

# Every reflected property whose name touches the hold. Plain C++ members are invisible here, so
# what comes back is what UPROPERTY exposed -- which is exactly the set worth comparing anyway.
WORDS = ('aim', 'sight', 'weapon', 'hand', 'carry', 'eye', 'first_person', 'lean', 'hunch',
         'elbow', 'support', 'trigger', 'grip', 'fore', 'finger', 'reach', 'freelook', 'spine')
seen = []
for name in sorted(dir(pawn)):
    if name.startswith('_') or '(' in name:
        continue
    if not any(w in name for w in WORDS):
        continue
    try:
        v = pawn.get_editor_property(name)
    except Exception:
        continue
    if callable(v):
        continue
    seen.append((name, v))

print('--- reflected state (%d) ---' % len(seen))
for name, v in seen:
    s = str(v)
    print('   %-38s %s' % (name, s[:90]))

# Where things actually are. The hold is a geometric claim: the weapon's sight should sit on the
# line from the eye along the aim, and the hands should be at the weapon, not reaching for it.
mesh = pawn.get_editor_property('mesh') if 'mesh' in dir(pawn) else None
try:
    mesh = pawn.mesh
except Exception:
    pass
print('--- bones and transforms ---')
if mesh:
    for bone in ('head', 'eyes', 'hand_r', 'hand_l', 'WeaponGrip_R', 'WeaponGrip_L', 'clavicle_r', 'upperarm_r', 'lowerarm_r'):
        try:
            if mesh.does_socket_exist(bone):
                p = mesh.get_socket_location(bone)
                print('   %-16s %8.1f %8.1f %8.1f' % (bone, p.x, p.y, p.z))
            else:
                print('   %-16s (no socket)' % bone)
        except Exception as ex:
            print('   %-16s ERROR %s' % (bone, ex))
for comp in pawn.get_components_by_class(unreal.StaticMeshComponent):
    m = comp.static_mesh
    if not m:
        continue
    t = comp.get_world_transform()
    loc, rot = t.translation, t.rotation.rotator()
    print('   component %-22s mesh %-28s at %8.1f %8.1f %8.1f  rot %6.1f %6.1f %6.1f'
          % (comp.get_name(), m.get_name(), loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll))

pc = unreal.GameplayStatics.get_player_controller(world, 0)
if pc:
    try:
        cr = pc.get_control_rotation()
        print('control rotation   pitch %6.1f yaw %6.1f roll %6.1f' % (cr.pitch, cr.yaw, cr.roll))
    except Exception as ex:
        print('control rotation unavailable:', ex)
    try:
        cm = pc.player_camera_manager
        cl, crot = cm.get_camera_location(), cm.get_camera_rotation()
        print('camera             %8.1f %8.1f %8.1f  pitch %6.1f yaw %6.1f roll %6.1f'
              % (cl.x, cl.y, cl.z, crot.pitch, crot.yaw, crot.roll))
    except Exception as ex:
        print('camera unavailable:', ex)
