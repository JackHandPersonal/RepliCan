"""Make RE_Basement: the echo the service deck answers you with.

The deck is a steel box three storeys down with a grated trench along it and pipe across the
ceiling. A room like that does two things to a sound. It throws back one clear early reflection --
that slap is what "hollow" actually is, not a long tail -- and then it swallows the top end, because
concrete and steel plate are poor reflectors up high and the air path is long.

So the numbers are chosen for those two effects rather than for a big hall:

  a short reflections delay with real gain   the slap off the far wall, which is the hollow part
  decay a shade under three seconds          long enough to hear, short enough to talk over
  decay_hf_ratio well under one              the highs die first, so the tail goes dark
  gain_hf held down                          nothing bright survives the trip
  density a little low                       a bare box rings; a furnished one does not

The layout tool hangs this on an audio volume covering the room (label S10_Reverb), so it applies
while the listener is down there and fades out over a second and a half on the way back up the
lift. Nothing else in the station uses it.

  Tools/ue_remote --file Tools/make_basement_reverb
"""
import unreal

PKG = '/Game/RepliCan/Audio'
NAME = 'RE_Basement'
FULL = PKG + '/' + NAME

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def make():
    # The factory's python name has moved about between engine versions, and for some asset types
    # create_asset answers with no factory at all. Try each in turn and say which one worked,
    # rather than failing on a guess.
    tried = []
    for factory_name in ('ReverbEffectFactory', 'SoundEffectReverbFactory', None):
        try:
            factory = getattr(unreal, factory_name)() if factory_name else None
            a = tools.create_asset(NAME, PKG, unreal.ReverbEffect, factory)
            if a:
                print('created with', factory_name or 'no factory')
                return a
        except Exception as ex:
            tried.append('%s: %s' % (factory_name, ex))
    raise RuntimeError('could not create %s -- %s' % (FULL, ' | '.join(tried)))


effect = unreal.load_asset(FULL) if eal.does_asset_exist(FULL) else make()
if not effect:
    raise RuntimeError('could not create or load ' + FULL)

SETTINGS = (
    ('density',                0.58),   # a bare-ish box: it rings rather than absorbs
    ('diffusion',              0.72),   # pipes and railings scatter what comes back
    ('gain',                   0.34),   # the whole effect kept under the dry sound
    ('gain_hf',                0.30),   # the top end does not survive the room
    ('decay_time',             2.9),    # seconds: audible, not a cathedral
    ('decay_hf_ratio',         0.55),   # highs die well before the lows, so the tail darkens
    ('reflections_gain',       0.58),   # THE SLAP. This is the hollow.
    ('reflections_delay',      0.028),  # about nine metres away, which is the width of the deck
    ('late_gain',              1.10),
    ('late_delay',             0.036),
    ('air_absorption_gain_hf', 0.985),
    ('room_rolloff_factor',    0.0),
)

applied, refused = [], []
for key, value in SETTINGS:
    try:
        effect.set_editor_property(key, value)
        applied.append('%s %s' % (key, value))
    except Exception as ex:
        refused.append('%s (%s)' % (key, ex))

unreal.EditorLoadingAndSavingUtils.save_packages([effect.get_outermost()], False)
print('%s: %d settings applied' % (NAME, len(applied)))
for line in applied:
    print('   ' + line)
if refused:
    print('NOT APPLIED (property name wrong for this engine):')
    for line in refused:
        print('   ' + line)
print('saved:', eal.does_asset_exist(FULL))
