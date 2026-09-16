import unreal
print([n for n in dir(unreal) if 'AnimPose' in n])
print(unreal.AnimPoseExtensions.get_bone_pose.__doc__)
