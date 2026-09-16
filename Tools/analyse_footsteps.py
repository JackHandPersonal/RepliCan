"""Objective check on the footstep samples: is anything ringing at a pitch, and how fast does it die.

    python Tools/analyse_footsteps.py

Reports per file:
  peak_ratio  how far the loudest spectral bin stands above the median of its neighbourhood.
              A sine tone gives a huge number. Noise through a damped resonator gives a small
              one. This is the number that says "pingy" or not; under about 8 reads as a knock
              rather than a note.
  centroid    spectral centre of mass in Hz. A boot sits low; a bare-foot slap sits higher.
  t60_ms      time to fall 60 dB below the peak. A boot on plate is short.
"""
import math, wave, struct, os, sys, cmath
RAW = r'C:\Dev\Games\RepliCan\RawAudio'

def read(path):
    with wave.open(path, 'rb') as w:
        n = w.getnframes(); sr = w.getframerate()
        data = w.readframes(n)
    xs = [v / 32768.0 for (v,) in struct.iter_unpack('<h', data)]
    return xs, sr

def dft_mag(xs, sr, bins=512):
    """Coarse magnitude spectrum of the first 60 ms, which is where the character lives."""
    n = min(len(xs), int(0.060 * sr))
    seg = xs[:n]
    # Hann window so a tone does not smear across bins
    seg = [v * (0.5 - 0.5 * math.cos(2 * math.pi * i / max(1, n - 1))) for i, v in enumerate(seg)]
    out = []
    for k in range(bins):
        hz = (k + 1) * (sr * 0.5) / bins
        w = 2.0 * math.pi * hz / sr
        re = sum(v * math.cos(w * i) for i, v in enumerate(seg))
        im = sum(v * math.sin(w * i) for i, v in enumerate(seg))
        out.append((hz, math.hypot(re, im)))
    return out

def analyse(path):
    xs, sr = read(path)
    spec = dft_mag(xs, sr)
    mags = [m for (_, m) in spec]
    peak = max(mags); pi = mags.index(peak)
    # median of everything outside a narrow band around the peak
    others = sorted(m for i, m in enumerate(mags) if abs(i - pi) > 12)
    med = others[len(others) // 2] if others else 1e-9
    peak_ratio = peak / max(1e-9, med)
    total = sum(mags) or 1e-9
    centroid = sum(hz * m for (hz, m) in spec) / total
    # t60 from the envelope of the rectified signal
    env = []
    y = 0.0
    for v in xs:
        y = max(abs(v), y * 0.9995)
        env.append(y)
    top = max(env) or 1e-9
    t60 = len(xs)
    for i, v in enumerate(env):
        if i > 10 and v < top * 0.001:
            t60 = i; break
    return peak_ratio, centroid, 1000.0 * t60 / sr, spec[pi][0]

names = sorted(f for f in os.listdir(RAW) if f.startswith('step_') and f.endswith('.wav'))
if not names:
    print('no step_*.wav in', RAW); sys.exit(1)
print('%-22s %10s %10s %9s %9s' % ('file', 'peak_ratio', 'peak_hz', 'centroid', 't60_ms'))
for f in names:
    pr, c, t, ph = analyse(os.path.join(RAW, f))
    flag = '  <-- tonal' if pr > 8.0 else ''
    print('%-22s %10.1f %10.0f %9.0f %9.0f%s' % (f, pr, ph, c, t, flag))
