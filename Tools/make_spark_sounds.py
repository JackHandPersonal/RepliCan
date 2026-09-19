"""The spark crackles: real Tesla-generator recordings, trimmed and level-matched. Nothing generated.

SOURCE: "Electricity Sound Effects" by Brian MacIntosh, CC0 (public domain), from OpenGameArt --
recorded off a small Tesla generator. Downloaded to
Content/GameData/RawAudio/OpenGameArt/ with its provenance in SOURCES.txt beside it.

  spark.wav            one discharge
  continuousspark.wav  a sustained arc

WHAT THIS TOOL IS ALLOWED TO DO, and it is deliberately narrow: resample to the project rate, trim
leading silence, take a window, fade the ends so a cut cannot click, and match levels. That is
delivering the recording as recorded.

WHAT IT MUST NOT DO. An earlier version of this file built the crackles by GRANULATING a metal
impact -- chopping a recording into 3-12 ms grains and scattering them to make a different sound.
Every sample was real, which is why it felt like it honoured the rule, and it did not: cutting one
sound into another sound is programmatic generation with a recording as the oscillator. The user's
standing instruction is to download a free library for the sound being asked for, and to get
permission BEFORE any programmatic generation of any kind. When the packs on disk had no electrical
arc, the answer was to go and find one -- which is what this file now does.

  python Tools/make_spark_sounds.py
  Tools/ue_remote.py --file Tools/import_audio_samples.py     # to reimport the assets
"""
import os, wave, struct

import miniaudio

ROOT = r'C:\Dev\Games\RepliCan'
SRC = os.path.join(ROOT, 'Content', 'GameData', 'RawAudio', 'OpenGameArt')
OUT = os.path.join(ROOT, 'Content', 'GameData', 'RawAudio')
SR = 22050

# Three crackles from the two takes: the single discharge whole, and two windows of the sustained
# arc, so the conduits do not repeat one identical sound. Windows are chosen for their own attack,
# not cut arbitrarily out of the middle.
# MEASURED, not assumed: spark.wav is 0.337 s and continuousspark.wav 0.218 s. An earlier version
# of this list asked for a 0.34 s window starting 0.34 s into a 0.218 s file, which is how you find
# out you guessed. Both takes are used whole; the third is the back half of the single discharge,
# which is a different-sounding part of the same real recording rather than a synthesised variant.
CUTS = [
    ('spark_crackle_01', 'spark.wav',           0.000, 0.337),
    ('spark_crackle_02', 'continuousspark.wav', 0.000, 0.218),
    ('spark_crackle_03', 'spark.wav',           0.120, 0.217),
]
PEAK = 0.5
FADE = 0.006


def load_mono(path):
    d = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16,
                              nchannels=1, sample_rate=SR)
    return [s / 32768.0 for s in d.samples]


def cut(pcm, start_s, length_s):
    a = int(start_s * SR)
    b = min(len(pcm), a + int(length_s * SR))
    seg = pcm[a:b]
    if not seg:
        raise SystemExit('cut past the end of the recording')
    # Fades only at the joins, so a window taken out of a longer take cannot click.
    f = max(1, int(FADE * SR))
    for i in range(min(f, len(seg))):
        seg[i] *= i / float(f)
        seg[len(seg) - 1 - i] *= i / float(f)
    p = max((abs(v) for v in seg), default=0.0) or 1.0
    return [v / p * PEAK for v in seg]


def write(name, pcm):
    path = os.path.join(OUT, name + '.wav')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in pcm))
    return path


if __name__ == '__main__':
    cache = {}
    for name, src, start, length in CUTS:
        full = os.path.join(SRC, src)
        if not os.path.exists(full):
            raise SystemExit('missing CC0 source: %s -- see SOURCES.txt in that folder' % full)
        if src not in cache:
            cache[src] = load_mono(full)
        p = write(name, cut(cache[src], start, length))
        print('wrote %-22s from %-20s %.2f s at %.2f s in' % (os.path.basename(p), src, length, start))
    print('\nReimport so the ASSETS change too:')
    print('  Tools/ue_remote.py --file Tools/import_audio_samples.py')
