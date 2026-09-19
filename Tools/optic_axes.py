"""Which optic meshes are authored along the wrong axis, measured rather than eyeballed.

A sight is a long thin thing you look THROUGH, so in the weapon's frame its longest dimension has to
run along X -- the direction of aim. Synty's scope parts are not all authored that way: some run
along Y, which is what "rotated by 90" looks like once mounted. The bounding box says which is which
without anyone having to squint at a viewport.

Reports the long axis and the yaw that would bring it onto X. Writes nothing; the number it prints
is what goes in the optic's `rot` field (or gets nudged in the hand tuning page).

  Tools/ue_remote --file Tools/optic_axes
"""
import unreal

REG = unreal.AssetRegistryHelpers.get_asset_registry()
FOLDERS = ['/Game/RepliCan/Optics', '/Game/RepliCan/Weapons/Optics']

rows = []
for folder in FOLDERS:
    for data in REG.get_assets_by_path(folder, recursive=True):
        a = data.get_asset()
        if not isinstance(a, unreal.StaticMesh):
            continue
        b = a.get_bounds()
        e = b.box_extent
        dims = {'X': e.x, 'Y': e.y, 'Z': e.z}
        long_axis = max(dims, key=dims.get)
        # How square it is: a stubby red dot is genuinely ambiguous and should not be "corrected".
        ordered = sorted(dims.values(), reverse=True)
        ratio = ordered[0] / max(0.001, ordered[1])
        yaw = 0.0 if long_axis == 'X' else (90.0 if long_axis == 'Y' else 0.0)
        rows.append((a.get_name(), e.x, e.y, e.z, long_axis, ratio, yaw))

rows.sort(key=lambda r: r[0])
print('%-28s %7s %7s %7s  %4s %6s %6s' % ('optic', 'ext X', 'ext Y', 'ext Z', 'long', 'ratio', 'yaw?'))
for n, x, y, z, ax, ratio, yaw in rows:
    note = ''
    if ax == 'Y' and ratio > 1.25:
        note = '   <-- authored across the aim: yaw %.0f' % yaw
    elif ratio <= 1.25:
        note = '   (too square to call from the box alone)'
    print('%-28s %7.2f %7.2f %7.2f  %4s %6.2f %6.0f%s' % (n, x, y, z, ax, ratio, yaw, note))
