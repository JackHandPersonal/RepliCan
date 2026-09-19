"""What space is each weapon mesh actually modelled in? Measured, not assumed.

    python Tools/ue_remote.py --file Tools/measure_weapon_axes.py

Writes Tools/weapon_axes.json: per weapon, the bounding box in its own local space, which
axis it runs along, which way round, and how far the origin sits from each end of that axis.
This is the evidence the Held Asset Standard is built on -- before normalising ninety-six
meshes it is worth knowing what they currently are.
"""
import unreal, json, io, collections

CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'
OUT = r'C:\Dev\Games\RepliCan\Tools\weapon_axes.json'

cat = json.load(io.open(CAT, encoding='utf-8'))['weapons']
rows = {}
tally = collections.Counter()
for key, e in sorted(cat.items()):
    m = unreal.load_asset(e['mesh'])
    if not m:
        tally['missing'] += 1
        continue
    b = m.get_bounds()
    c, x = b.origin, b.box_extent
    lo = unreal.Vector(c.x - x.x, c.y - x.y, c.z - x.z)
    hi = unreal.Vector(c.x + x.x, c.y + x.y, c.z + x.z)
    ext = [hi.x - lo.x, hi.y - lo.y, hi.z - lo.z]
    long_axis = ext.index(max(ext))
    lows = [lo.x, lo.y, lo.z]
    highs = [hi.x, hi.y, hi.z]
    # Which end is the far end? The grip is meant to be at the origin, so the business end is
    # whichever side of zero reaches further.
    sign = 1 if abs(highs[long_axis]) >= abs(lows[long_axis]) else -1
    axis = 'XYZ'[long_axis]
    rows[key] = {
        'kind': e['kind'], 'pack': e['pack'],
        'min': [round(v, 2) for v in lows], 'max': [round(v, 2) for v in highs],
        'extent': [round(v, 2) for v in ext],
        'axis': ('+' if sign > 0 else '-') + axis,
        'length': round(ext[long_axis], 2),
        # How far the origin is from the near end along the long axis. Near zero means the
        # pivot really is at the butt/grip; a big number means it is floating.
        'butt_offset': round(abs(lows[long_axis] if sign > 0 else highs[long_axis]), 2),
        'lateral': [round(abs(lows[i]) + abs(highs[i]), 2) for i in range(3) if i != long_axis],
    }
    tally[rows[key]['axis']] += 1
    tally['kind %s -> %s' % (e['kind'], rows[key]['axis'])] += 1

io.open(OUT, 'w', encoding='utf-8', newline='\n').write(json.dumps(rows, indent=1))
print('MEASURED', len(rows))
for k, v in sorted(tally.items()):
    print('  %-40s %d' % (k, v))
