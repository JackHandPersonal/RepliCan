"""Copy one weapon's HAND TUNING onto every weapon that is held the same way.

A hold is mostly about the BODY, not the gun: how the wrist sits on a pistol grip, how far the
weapon rides off the face at each carry, where the elbows go, how far the head sinks at the sights.
Those numbers are worth having once and sharing. What is NOT shared is anything measured on the
weapon's own geometry -- above all fore_grip, the point on the handguard the support hand takes,
which runs from 4 cm to 56 cm along the barrel across the rifles here. Copying that would put half
the support hands in mid-air.

So: everything but fore_grip's X and Z, which each weapon keeps. fore_grip's Y comes across,
because it is a correction of where the hand sits ACROSS the weapon and every other rifle has it at
zero, which is the untuned value rather than a measured one.

  Tools/ue_remote.py --file Tools/copy_hand_tune.py      (or just run it: it only edits JSON)

Edit SOURCE and MATCH below to copy a different hold to a different family.
"""
import io, json, collections, os

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(HERE, 'UI', 'Weapons.json')

SOURCE = 'Worlds/Wep_Assault_01'
def MATCH(key, w):
    return str(w.get('stance', '')).lower() == 'rifle' and int(w.get('hands', 1) or 1) == 2

# The body's half of the hold: copied whole.
SHARED = ('grip', 'hand_rot', 'fingers_r', 'fore_hand_rot', 'fingers_l',
          'hunch', 'lean', 'pull', 'lateral', 'low_ready', 'elbow_main', 'elbow_support')

d = json.load(io.open(PATH, encoding='utf-8'), object_pairs_hook=collections.OrderedDict)
ws = d['weapons']
src = ws[SOURCE]
src_fore_y = src.get('fore_grip', [0, 0, 0])[1] if isinstance(src.get('fore_grip'), list) and len(src['fore_grip']) > 1 else 0

# A FIELD THE SOURCE DOES NOT HAVE IS SILENTLY NOT COPIED, and that is how twenty-four rifles ended
# up with a support elbow 112 degrees off the tuned one: elbow_main and elbow_support were added to
# the catalogue after this last ran, so "if f in src" was false for both and every target kept an
# older default. The gun looked right in the tuner, which shows the source, and wrong in the hands of
# anything else. Now the skipped fields are named.
absent = [f for f in SHARED if f not in src]

changed = []
for key, w in ws.items():
    if key == SOURCE or not MATCH(key, w):
        continue
    for f in SHARED:
        if f in src:
            w[f] = json.loads(json.dumps(src[f]))   # a copy, not a shared reference
    fg = w.get('fore_grip')
    if isinstance(fg, list) and len(fg) >= 3:
        fg[1] = src_fore_y      # across the weapon: the tuned correction travels
    changed.append(key)

io.open(PATH, 'w', encoding='utf-8', newline='\r\n').write(json.dumps(d, indent='\t', ensure_ascii=False))
print('copied the hold from %s to %d weapons:' % (SOURCE, len(changed)))
for k in changed:
    print('   ' + k)
print('kept per weapon: fore_grip x and z (its own handguard), sight, muzzle, optic mount, eject')
if absent:
    print('NOT ON THE SOURCE, so not copied -- tune these on %s and run again: %s' % (SOURCE, ', '.join(absent)))
