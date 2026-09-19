"""Build the work bot's death from real recordings instead of synthesised tones.

The old robot_scream.wav was synthesised and it read as comical -- a falling whistle is what a
cartoon does when it falls off a cliff. A machine this size dying should be heavy and mechanical,
and it should take a moment: the thing comes apart, and then the parts that were still turning
spin down.

So the death is MIXED here, from Kenney's Sci-Fi Sounds (CC0, public domain, no attribution
required), into one file per variant so the layers cannot drift apart at runtime:

  0.00  a low-frequency explosion            the weight, felt more than heard
  0.02  an explosion crunch                  the casing tearing
  0.06  a metal impact                       the body letting go
  0.14  a motor, WOUND DOWN                  the servos losing power over three and a half seconds
  0.8+  two force-field arcs                 the electrics shorting after the fact

The wind-down is the part that does the work, and it is not a synthesised sweep: it is the pack's
own running motor, resampled with a read rate that falls away, so the recording itself slows and
drops in pitch the way a rotor does when its supply is cut. An amplitude curve takes it to nothing.

Also cut here: three short electrical arcs (robot_arc_*) for the sparks that go on popping after
the bot is down. They are real crackle rather than the synthesised spark_crackle_* files, which are
another generator's and are left alone.

Everything is written as 16-bit mono WAV into RawAudio, so the game can play it with no import
step; Tools/import_audio_samples.py then brings the same files in as assets, which is what makes
them stop by themselves.

Needs soundfile to read Ogg (pip install soundfile numpy).

  python Tools/make_robot_death.py
"""
import os, wave, sys

try:
    import numpy as np
    import soundfile as sf
except ImportError:
    raise SystemExit('this needs numpy and soundfile to read the Ogg samples (pip install soundfile numpy)')

SRC = 'C:/Dev/Games/RepliCan/RawAudio/Kenney_SciFi'
DST = 'C:/Dev/Games/RepliCan/RawAudio'
SR = 44100


def read(name):
    """One sample, mono, at SR."""
    d, sr = sf.read(os.path.join(SRC, name), dtype='float64', always_2d=True)
    m = d.mean(axis=1)
    if sr != SR:                                   # nothing in the pack needs this, but be honest
        idx = np.arange(0, len(m), sr / float(SR))
        m = np.interp(idx, np.arange(len(m)), m)
    return m


def winddown(name, seconds, rate_from=1.0, rate_to=0.22, curve=1.5):
    """A running machine losing its supply.

    Reading the recording at a rate that falls away slows it and drops its pitch together, which
    is what actually happens to a rotor -- a pitch shift alone sounds like a tape effect. The
    amplitude follows the speed down so it arrives at silence rather than stopping dead."""
    src = read(name)
    n = int(seconds * SR)
    t = np.linspace(0.0, 1.0, n)
    rate = rate_from + (rate_to - rate_from) * (t ** 0.8)
    pos = np.cumsum(rate)
    pos = pos[pos < len(src) - 2]
    out = np.interp(pos, np.arange(len(src)), src)
    env = (1.0 - np.linspace(0.0, 1.0, len(out))) ** curve
    return out * env


def mix(layers, seconds):
    """layers: (samples, start seconds, gain). Summed, then brought to a common peak."""
    buf = np.zeros(int(seconds * SR))
    for s, at, gain in layers:
        i = int(at * SR)
        n = min(len(s), len(buf) - i)
        if n > 0:
            buf[i:i + n] += s[:n] * gain
    peak = float(np.abs(buf).max()) or 1.0
    return buf / peak * 0.94


def save(name, samples):
    fade = int(SR * 0.008)
    s = samples.copy()
    s[-fade:] *= np.linspace(1.0, 0.0, fade)       # never end on a step: that is the click
    pcm = np.clip(s * 32767.0, -32768, 32767).astype('<i2')
    w = wave.open(os.path.join(DST, name), 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(pcm.tobytes())
    w.close()
    print('   %-24s %.2fs' % (name, len(pcm) / float(SR)))


print('the work bot dies:')
# Two variants, so a second kill does not sound like a replay of the first.
save('robot_death_1.wav', mix([
    (read('lowFrequency_explosion_000.ogg'), 0.00, 1.00),
    (read('explosionCrunch_003.ogg'),        0.02, 0.80),
    (read('impactMetal_001.ogg'),            0.06, 0.65),
    (winddown('engineCircular_000.ogg', 3.6), 0.14, 0.60),
    (read('forceField_002.ogg'),             0.85, 0.34),
    (read('forceField_000.ogg'),             1.70, 0.26),
], 4.4))
save('robot_death_2.wav', mix([
    (read('lowFrequency_explosion_001.ogg'), 0.00, 1.00),
    (read('explosionCrunch_004.ogg'),        0.03, 0.82),
    (read('impactMetal_003.ogg'),            0.08, 0.60),
    (winddown('spaceEngineLow_002.ogg', 4.0, rate_to=0.18, curve=1.8), 0.16, 0.62),
    (read('forceField_003.ogg'),             1.05, 0.32),
    (read('forceField_001.ogg'),             2.20, 0.24),
], 4.8))

# The arcs that keep popping while it lies there: the loudest half-second of each force field.
print('and goes on arcing:')
for i, name in enumerate(('forceField_000.ogg', 'forceField_002.ogg', 'forceField_004.ogg'), 1):
    s = read(name)
    win = int(SR * 0.5)
    # Start at the loudest half second rather than at the file's head, which is often the run-up.
    energy = np.convolve(s * s, np.ones(win) / win, mode='valid')
    a = int(np.argmax(energy))
    seg = s[a:a + win].copy()
    seg[:int(SR * 0.004)] *= np.linspace(0.0, 1.0, int(SR * 0.004))
    peak = float(np.abs(seg).max()) or 1.0
    save('robot_arc_%d.wav' % i, seg / peak * 0.9)

print('source: Kenney Sci-Fi Sounds, CC0 -- see RawAudio/Kenney_SciFi/License.txt')
