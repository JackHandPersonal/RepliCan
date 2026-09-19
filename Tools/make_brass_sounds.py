"""Spent brass on a deck plate (BrassFx, 2026-09-17): three tinks -- a bright, short metallic ring with a
second bounce -- and one for a shotgun hull, duller and plastic. Files in RawAudio.
    python Tools/make_brass_sounds.py
"""
import math, random, struct, wave
OUT = 'C:/Dev/Games/RepliCan/RawAudio'
SR = 22050

def tink(seed, seconds=0.28, f0=None, dull=False):
    rnd = random.Random(seed); n = int(seconds * SR); out = []
    f = f0 or rnd.uniform(3200.0, 4600.0)
    parts = [(f, 0.6), (f * 1.53, 0.25), (f * 2.31, 0.12)] if not dull else [(f * 0.35, 0.6), (f * 0.55, 0.3)]
    decay = 26.0 if not dull else 40.0
    for i in range(n):
        t = i / float(SR)
        v = sum(math.sin(2.0 * math.pi * p * t) * a for p, a in parts) * math.exp(-t * decay)
        k = t - rnd.uniform(0.09, 0.13) if i == 0 else k   # a second, smaller bounce
        if t > 0.1: v += sum(math.sin(2.0 * math.pi * p * (t - 0.1)) * a for p, a in parts) * math.exp(-(t - 0.1) * decay * 1.4) * 0.45
        out.append(v)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.55 for v in out]

def write(name, xs):
    w = wave.open(OUT + '/' + name, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs)); w.close(); print('wrote', name, round(len(xs) / SR, 2), 's')

for k in (1, 2, 3): write('brass_%02d.wav' % k, tink(seed=k * 13))
write('hull_01.wav', tink(seed=99, seconds=0.22, f0=2600.0, dull=True))
