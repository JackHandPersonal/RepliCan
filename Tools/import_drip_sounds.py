"""Cut real water drips out of a basement recording, to replace the synthesised ones.

The deck's drips were synthesised -- a sine sliding down a fifth with a bloop under it -- and they
sounded electronic, because that is what they were. These are a recording: OpenGameArt's "dripping
water loop" by Independent.nu, CC0 (public domain, nothing to credit), twenty seconds of a damp
cellar with water falling into a puddle. It is exactly the room this deck is meant to be.

The recording is a ROOM TONE with drips in it, not drips on silence, so the cut cannot be the
silence split used for the voice packs. Instead each drip is found as an ONSET: a point where the
short-window peak jumps well clear of the room's own noise floor, having been quiet for long enough
before it that it is a new drip and not the tail of the last one. A window of just under half a
second is taken from slightly before each onset, which is long enough for the plink and its ring
and short enough that little of the room tone comes with it.

They are level-matched to each other, because UAmbientPlayer picks from the pool at random and one
drip twice as loud as the rest reads as a different sound entirely.

Writes drip_NN.wav into RawAudio, which is where the deck profile already looks; nothing in the
C++ changes. Needs soundfile to read FLAC (pip install soundfile numpy).

  python Tools/import_drip_sounds.py [source audio]
"""
import os, sys, wave

try:
    import numpy as np
    import soundfile as sf
except ImportError:
    raise SystemExit('this needs numpy and soundfile to read the recording (pip install soundfile numpy)')

SCRATCH = 'C:/Users/jhand/AppData/Local/Temp/claude/C--Dev-Claude/f0f1126f-2fae-4098-9f9e-f7a1889d2c6d/scratchpad'
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SCRATCH, 'drips.flac')
DST = 'C:/Dev/Games/RepliCan/RawAudio'
TAKE = 0.46          # seconds kept per drip: the plink, its ring, and little else
LEAD = 0.015         # start slightly before the onset so the attack is not clipped
LOOKBACK = 0.20      # the stretch of room tone a drip has to rise above
RISE = 3.0           # times that background, which is what makes it an onset and not just loud
ABSOLUTE = 4.0       # and at least this many times the noise floor, so hiss cannot trigger it
REFRACTORY = 0.35    # seconds before another drip is allowed: the ring is not a second drip
WANT = 6             # the pool the deck plays from
PEAK = 0.80

d, sr = sf.read(SRC, dtype='float64', always_2d=True)
mono = d.mean(axis=1)

win = max(1, int(sr * 0.005))
usable = len(mono) // win * win
env = np.abs(mono[:usable]).reshape(-1, win).max(axis=1)
floor = float(np.percentile(env, 20)) or 1e-6

back = int(LOOKBACK / 0.005)
gap = int(REFRACTORY / 0.005)
onsets = []
last = -gap
for i in range(back, len(env)):
    if i - last < gap:
        continue                                    # inside the last drip's ring
    if env[i] < floor * ABSOLUTE:
        continue
    # A drip is a RISE, not a level: the recording has a constant room tone, so "loud" is common
    # and "suddenly louder than the last fifth of a second" is what a falling drop actually is.
    background = float(np.median(env[i - back:i - 4]))
    if env[i] < max(background * RISE, floor * ABSOLUTE):
        continue
    onsets.append(i * win)
    last = i

# The strongest ones first: a recording like this has a few clear drips and a lot of half-heard ones.
onsets.sort(key=lambda a: -float(np.abs(mono[a:a + int(sr * 0.12)]).max()))
picked = sorted(onsets[:WANT])


def save(name, seg):
    seg = seg.copy()
    top = float(np.abs(seg).max()) or 1.0
    seg *= PEAK / top                                # matched, so no one drip jumps out of the pool
    a = int(sr * 0.003)
    b = int(sr * 0.08)
    seg[:a] *= np.linspace(0.0, 1.0, a)
    seg[-b:] *= np.linspace(1.0, 0.0, b)             # fade the room tone out rather than cutting it
    pcm = np.clip(seg * 32767.0, -32768, 32767).astype('<i2')
    w = wave.open(os.path.join(DST, name), 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(int(sr))
    w.writeframes(pcm.tobytes())
    w.close()
    print('   %-14s %.2fs' % (name, len(pcm) / float(sr)))


print('cut %d drips out of %.1f seconds (noise floor %.4f)' % (len(picked), len(mono) / float(sr), floor))
for i, at in enumerate(picked, 1):
    a = max(0, at - int(sr * LEAD))
    save('drip_%02d.wav' % i, mono[a:a + int(sr * TAKE)])
if len(picked) < WANT:
    print('WANTED %d, found %d -- lower ONSET or raise QUIET_BEFORE' % (WANT, len(picked)))
print('source: OpenGameArt "dripping water loop" by Independent.nu, CC0')
