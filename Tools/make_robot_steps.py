"""The work bot's footstep: a quiet plastic-on-metal clack, cut from a real recording.

THE PROBLEM WAS NOT SYNTHESIS. The steps were already a genuine CC0 recording --
impactMetal_medium_* from Kenney's Impact Sounds, imported as assets. They sounded like steel drums
because that is honestly what they are: a struck metal plate, and a struck plate RINGS. The ring is
a decaying resonance lasting a few hundred milliseconds, and a bot taking a step every 0.6 s lays
one ring on top of the next, which is how you build a steel drum.

A plastic-on-metal clack is the same strike with the resonance taken off: the attack transient
survives, the tail does not. So this keeps the real recording and damps it, rather than replacing
it with something synthesised -- there is nothing synthetic in the output, only less of the input.

  impactMetal_light_*  (lighter strike than the medium the steps used)
    -> trim to the transient
    -> exponential damping so the ring dies in ~70 ms instead of ringing on
    -> a gentle high shelf off, because plastic has less top than bare steel
    -> quiet: peak well under the old level, since these fire twice a second

    python Tools/make_robot_steps.py
    Tools/ue_remote.py --file Tools/import_audio_samples.py     # to reimport the assets

Source: Kenney Impact Sounds 1.0, CC0 -- public domain, no attribution required. Licence text sits
beside the files in Content/GameData/RawAudio/Kenney/License.txt.
"""
import os, wave, struct, math

import miniaudio

ROOT = r'C:\Dev\Games\RepliCan'
SRC = os.path.join(ROOT, 'Content', 'GameData', 'RawAudio', 'Kenney')
OUT = os.path.join(ROOT, 'Content', 'GameData', 'RawAudio')
SR = 22050

# The lighter strike, not the medium: a work bot places a foot, it does not drop an anvil.
SOURCES = ['impactMetal_light_00%d.ogg' % i for i in range(5)]

HOLD = 0.006      # seconds of the attack left completely alone
DECAY = 0.070     # the ring is 60 dB down by here -- a clack, not a note
LENGTH = 0.130    # nothing after this is audible anyway
PEAK = 0.34       # quiet: these fire about twice a second while it walks
TOP = 0.55        # how much of the brightest content survives (plastic has less top than steel)


def load_mono(path):
    d = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16,
                              nchannels=1, sample_rate=SR)
    return [s / 32768.0 for s in d.samples]


def clack(pcm):
    # Start at the real attack rather than whatever silence the file opens with.
    peak = max((abs(v) for v in pcm), default=0.0) or 1.0
    start = next((i for i, v in enumerate(pcm) if abs(v) > peak * 0.18), 0)
    pcm = pcm[start:start + int(LENGTH * SR)]

    out = []
    prev_in = prev_out = 0.0
    for i, v in enumerate(pcm):
        t = i / float(SR)
        # ONE-POLE HIGH SHELF DOWN: keep the click, lose some of the steel brightness. A plain
        # low-pass would muffle the attack too and give a thud; this only leans on the top.
        hp = v - prev_in + 0.86 * prev_out
        prev_in, prev_out = v, hp
        v = (v - hp) + hp * TOP
        # THE DAMPING, which is the whole point: flat through the attack, then a fast exponential.
        env = 1.0 if t < HOLD else math.exp(-(t - HOLD) * (6.9078 / DECAY))
        out.append(v * env)

    # A short fade-out so the file cannot end on a step, which clicks.
    tail = max(1, int(0.004 * SR))
    for i in range(tail):
        out[len(out) - tail + i] *= 1.0 - i / float(tail)

    p = max((abs(v) for v in out), default=0.0) or 1.0
    return [v / p * PEAK for v in out]


def write(name, pcm):
    path = os.path.join(OUT, name + '.wav')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in pcm))
    return path, len(pcm) / float(SR)


if __name__ == '__main__':
    for i, src in enumerate(SOURCES, start=1):
        full = os.path.join(SRC, src)
        if not os.path.exists(full):
            raise SystemExit('missing CC0 source: %s' % full)
        path, secs = write('robot_step_%02d' % i, clack(load_mono(full)))
        print('wrote %-22s from %-26s %.0f ms' % (os.path.basename(path), src, secs * 1000))
    print('\nNow reimport so the ASSET changes too -- PlayAt prefers the imported sample over the')
    print('loose .wav, so rewriting the .wav alone changes nothing you can hear:')
    print('  Tools/ue_remote.py --file Tools/import_audio_samples.py')
