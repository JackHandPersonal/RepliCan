import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if l in ('Lift_Car', 'Lift_Shaft_+00', 'Sign_Door_Lift', 'Bay_Wall_S1', 'Foyer_Wall_N0', 'Foyer_Wall_N2'):
        o, e = a.get_actor_bounds(False)
        print('%-18s pivot (%8.1f,%8.1f,%7.1f) yaw %6.1f  spans x %7.1f..%-7.1f y %7.1f..%-7.1f'
              % (l, a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z,
                 a.get_actor_rotation().yaw, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y))
