"""Cut the downloaded pain recordings into one file per grunt, for the player's voice.

The grunts were synthesised until now, and a synthesised grunt cannot carry pain -- it is the one
sound in the game that has to be a person. These are recordings, both CC0 (public domain, no
attribution required, nothing to carry in a credits screen):

  male    OpenGameArt, "grunts of male death and pain"   -- one 50-second take, 44.1 kHz
  female  OpenGameArt, "female hurt grunts and groans"   -- one 13-second take, 48 kHz Ogg

Each arrives as ONE file with every grunt in it end to end, so each is cut on SILENCE. A run of
samples whose peak stays under a threshold for long enough is the gap between takes; everything
between two gaps is one grunt. Each take is written with a few milliseconds of fade at both ends so
the cut cannot click, into the folder the game already reads: ABaseCharacter::PlayVoiceBark globs
"<kind>_*.wav" in Conversations/Voice/PlayerGrunts<Male|Female>, non-recursively, which is why the
old synthesised ones were moved down into _RetiredSynth rather than deleted.

The longest takes become the DEATH cries -- a death runs longer than a hit -- and the rest become
the hurt grunts. Levels are matched between the two voices so a female character is not quieter
than a male one.

An Ogg cannot be read by python's own wave module, so a source that is not a .wav is decoded with
soundfile first; the script says so plainly if it is not installed rather than half-working.

  python Tools/import_voice_grunts.py <source audio> <destination folder> [deaths]

With no arguments it does both packs from the scratchpad downloads.
"""
import wave, os, array, sys

PROJ = 'C:/Dev/Games/RepliCan'
SCRATCH = 'C:/Users/jhand/AppData/Local/Temp/claude/C--Dev-Claude/f0f1126f-2fae-4098-9f9e-f7a1889d2c6d/scratchpad'
GAP_SECONDS = 0.22       # quiet for this long and the take has ended
FLOOR = 0.035            # peak below this fraction of full scale counts as quiet
MIN_TAKE = 0.18          # anything shorter is a breath, not a grunt
MAX_TAKE = 4.0
PEAK = 0.92              # every take normalised to this, so no one grunt jumps out


def load(path):
    """(mono 16-bit samples, rate). Decodes an Ogg if it has to."""
    if path.lower().endswith('.wav'):
        w = wave.open(path, 'rb')
        ch, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        assert width == 2, 'expected 16-bit audio, got %d bytes per sample' % width
        raw = w.readframes(frames)
        w.close()
        s = array.array('h')
        s.frombytes(raw)
        return (array.array('h', s[::ch]) if ch > 1 else s), rate
    try:
        import soundfile as sf
    except ImportError:
        raise SystemExit('%s is not a .wav and soundfile is not installed to decode it '
                         '(pip install soundfile)' % os.path.basename(path))
    d, rate = sf.read(path, dtype='float32', always_2d=True)
    mono = d.mean(axis=1)
    top = float(max(abs(mono.max()), abs(mono.min()))) or 1.0
    return array.array('h', (mono / top * 0.95 * 32767).astype('<i2').tolist()), rate


def takes_in(samples, rate):
    """Every stretch of sound between silences, longest first."""
    n = len(samples)
    win = max(1, int(rate * 0.005))
    loud = []
    for i in range(0, n, win):
        chunk = samples[i:i + win]
        loud.append((max((abs(s) for s in chunk), default=0) / 32768.0) >= FLOOR)
    gap_windows = max(1, int((GAP_SECONDS * rate) / win))
    out, start, quiet = [], None, 0
    for i, is_loud in enumerate(loud):
        if is_loud:
            if start is None:
                start = i
            quiet = 0
        elif start is not None:
            quiet += 1
            if quiet >= gap_windows:
                out.append((start * win, (i - quiet + 1) * win))
                start = None
    if start is not None:
        out.append((start * win, n))
    out = [(a, b) for a, b in out if MIN_TAKE * rate <= (b - a) <= MAX_TAKE * rate]
    out.sort(key=lambda t: t[1] - t[0], reverse=True)
    return out


def write(path, samples, rate, a, b):
    seg = array.array('h', samples[a:b])
    top = max((abs(s) for s in seg), default=1) or 1
    gain = min(8.0, (PEAK * 32767.0) / top)      # matched in level, but never amplifying hiss
    fade = min(int(rate * 0.006), len(seg) // 4)  # no click at either end
    for i in range(len(seg)):
        v = seg[i] * gain
        if i < fade:
            v *= i / float(fade)
        elif i >= len(seg) - fade:
            v *= (len(seg) - 1 - i) / float(fade)
        seg[i] = max(-32768, min(32767, int(v)))
    out = wave.open(path, 'wb')
    out.setnchannels(1)
    out.setsampwidth(2)
    out.setframerate(rate)
    out.writeframes(seg.tobytes())
    out.close()


def cut(src, dst, deaths, label):
    samples, rate = load(src)
    found = takes_in(samples, rate)
    death, hurt = found[:deaths], found[deaths:]
    os.makedirs(dst, exist_ok=True)
    for name in os.listdir(dst):                 # a re-run replaces, it does not pile up
        if name.startswith(('grunt_', 'death_')) and name.endswith('.wav'):
            os.remove(os.path.join(dst, name))
    made = []
    for kind, group in (('death', death), ('grunt', hurt)):
        for i, (a, b) in enumerate(sorted(group, key=lambda t: t[0]), 1):
            p = os.path.join(dst, '%s_%02d.wav' % (kind, i))
            write(p, samples, rate, a, b)
            made.append((os.path.basename(p), (b - a) / float(rate)))
    print('%s: %d takes out of %.1f seconds at %d Hz' % (label, len(made), len(samples) / float(rate), rate))
    for name, secs in made:
        print('   %-16s %.2fs' % (name, secs))
    return len(made)


if len(sys.argv) > 2:
    cut(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 3, os.path.basename(sys.argv[1]))
else:
    cut(os.path.join(SCRATCH, 'male_grunts.wav'),
        os.path.join(PROJ, 'Conversations/Voice/PlayerGruntsMale'), 3, 'male')
    cut(os.path.join(SCRATCH, 'female_hurt.ogg'),
        os.path.join(PROJ, 'Conversations/Voice/PlayerGruntsFemale'), 2, 'female')
    print('sources: OpenGameArt "grunts of male death and pain" and "female hurt grunts and groans", both CC0')
