"""Swing sounds: a whoosh for a light blade and a heavier one for an axe or a hammer, filtered
noise with a band that rises and falls over the swing. Two files in RawAudio.
    python Tools/make_swing_sounds.py
"""
import math, random, struct, wave
OUT = 'C:/Dev/Games/RepliCan/RawAudio'
SR = 22050
random.seed(11)
def whoosh(seconds, lo_hz, hi_hz, gain):
    n = int(seconds * SR); out = []; y1 = 0.0; y2 = 0.0
    for i in range(n):
        t = i / float(n)
        centre = lo_hz + (hi_hz - lo_hz) * math.sin(math.pi * t)          # the band sweeps up and back down
        a = 1.0 - math.exp(-2.0 * math.pi * centre / SR)
        b = 1.0 - math.exp(-2.0 * math.pi * (centre * 0.35) / SR)
        v = random.uniform(-1.0, 1.0)
        y1 += a * (v - y1); y2 += b * (y1 - y2)
        env = math.sin(math.pi * t) ** 1.6                                  # in and out with the arm
        out.append((y1 - y2) * env * gain)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.8 for v in out]
def write(name, xs):
    w = wave.open(OUT + '/' + name, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs)); w.close(); print('wrote', name, round(len(xs) / SR, 2), 's')
write('swing_light.wav', whoosh(0.28, 900.0, 3200.0, 1.0))
write('swing_heavy.wav', whoosh(0.42, 300.0, 1400.0, 1.0))
