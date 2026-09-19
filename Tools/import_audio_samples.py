"""Import the real recorded samples as sound assets, and say which game sound each one becomes.

The station's audio has been synthesised up to now -- tones and noise written to loose .wav files
in RawAudio/ and played at runtime through USoundWaveProcedural. Two things are wrong with that.
It sounds synthesised, which is what a robot's footsteps sounded like. And a procedural wave never
ends by itself, so every one-shot depends on a timer to stop it; a timer that does not fire (a
paused game, for one) leaves the wave starved, and a starved procedural wave CLICKS. A real
imported sound has a duration, ends on its own and needs no timer at all.

The samples are Kenney's Impact Sounds 1.0, CC0 (public domain): free for any use, no attribution
required, no licence to carry. The .ogg files live in RawAudio/Kenney with their licence text, and
this imports them as assets under /Game/RepliCan/Audio named for the GAME sound they stand in for,
so the code can ask for "robot_step_01.wav" and be handed the recording instead.

  Tools/ue_remote.py --file Tools/import_audio_samples.py
"""
import unreal, os

SRC = os.path.join('C:/Dev/Games/RepliCan', 'RawAudio', 'Kenney')
PKG = '/Game/RepliCan/Audio'

# game sound -> the sample that plays instead. The name on the left is the loose .wav the code
# already asks for, so nothing at the call sites has to change.
MAP = [
    # A WORKBOT IS A HEAVY THING ON A STEEL DECK. Medium metal impacts: struck plate, not a tap.
    ('robot_step_01', 'impactMetal_medium_000.ogg'),
    ('robot_step_02', 'impactMetal_medium_001.ogg'),
    ('robot_step_03', 'impactMetal_medium_002.ogg'),
    ('robot_step_04', 'impactMetal_medium_003.ogg'),
    ('robot_step_05', 'impactMetal_medium_004.ogg'),
    # Its heavier noises: a fall, a strike.
    ('robot_thump',   'impactMetal_heavy_000.ogg'),
    ('robot_strike',  'impactMetal_heavy_002.ogg'),
    # A boot on deck plate, for anyone walking on metal.
    ('step_metal_1',  'impactPlate_medium_000.ogg'),
    ('step_metal_2',  'impactPlate_medium_001.ogg'),
    # THE BODY BEING HIT. Not the voice -- the grunt is a voice line and needs a voice; this is
    # the thud that lands with it.
    ('body_hit_01',   'impactSoft_heavy_000.ogg'),
    ('body_hit_02',   'impactSoft_heavy_001.ogg'),
    ('body_hit_03',   'impactPunch_medium_000.ogg'),
    ('body_hit_04',   'impactPunch_medium_001.ogg'),
]

# THE WORK BOT DYING, and the arcs that go on popping afterwards. These are not raw pack files:
# they are mixed and processed by the robot death tool and written straight into RawAudio, so they
# are imported from one directory up rather than from the Kenney folder. Importing them matters
# more than it does for the rest -- the death cry runs four and a half seconds, and four and a half
# seconds is a long time for a procedural wave to be relying on a timer that a paused game will not
# fire. An imported sound knows its own length and ends on its own.
EXTRA_SRC = os.path.join('C:/Dev/Games/RepliCan', 'RawAudio')

# EVERY LOOSE WAV, not a hand-written list. This sweeps RawAudio because a hand-written list is how
# the clicking survived twice: the beds were named and fixed, and the steam vents' two hiss loops
# were not, so every vent in the station went on refilling a procedural wave from an underflow
# callback -- which fires only once the queue has already run dry, leaving a gap at every lap. Six
# continuous vents on the deck alone is six clicks a lap. An imported sound has none of that.
EXTRA = [(os.path.splitext(f)[0], f) for f in sorted(os.listdir(EXTRA_SRC))
         if f.lower().endswith('.wav') and not f.lower().endswith('_preview.wav')]

# What has to say it LOOPS, or the mixer plays it once and stops: the ambient beds, and the two
# steam hisses, which UAmbientPlayer hangs on every vent through UI/SteamVents.json.
LOOPS = [n for n, _f in EXTRA if n.endswith('_loop')] + ['steam_hiss_1', 'steam_hiss_2']

tools = unreal.AssetToolsHelpers.get_asset_tools()
made, missing = [], []
for game_name, sample, src_dir in [(a, b, SRC) for a, b in MAP] + [(a, b, EXTRA_SRC) for a, b in EXTRA]:
    path = os.path.join(src_dir, sample)
    if not os.path.exists(path):
        missing.append(sample)
        continue
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = PKG
    t.destination_name = 'A_' + game_name      # the code looks for A_<the wav it wanted>
    t.automated = True
    t.replace_existing = True
    t.save = True
    tools.import_asset_tasks([t])
    a = unreal.load_asset('%s/A_%s' % (PKG, game_name))
    if a:
        # A bed has to say it loops, or the mixer plays it once and stops.
        if game_name in LOOPS:
            try: a.set_editor_property('looping', True)
            except Exception as ex: print('could not mark %s looping: %s' % (game_name, ex))
            unreal.EditorAssetLibrary.save_loaded_asset(a, False)
        made.append((game_name, sample, a.get_editor_property('duration')))
    else:
        missing.append(sample)

print('imported %d samples as assets under %s' % (len(made), PKG))
for game_name, sample, dur in made:
    print('   %-16s <- %-28s %.2fs' % (game_name, sample, dur))
if missing:
    print('MISSING (not imported): ' + ', '.join(missing))
print('licence: Kenney Impact Sounds 1.0, CC0 -- see RawAudio/Kenney/License.txt')
