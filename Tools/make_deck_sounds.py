"""The service deck's own sounds (UAmbientPlayer's "deck" profile, 2026-09-17): four drips into a
puddle, a compressor that cycles up and down (a loopable ten seconds), three distant clanks, and a
cable hum. Files in RawAudio.
    python Tools/make_deck_sounds.py
"""
import math, random, struct, wave
OUT = 'C:/Dev/Games/RepliCan/RawAudio'
SR = 22050

def drip(seed, seconds=0.32):
    # A plink: a short high sine sliding down a fifth, a faint low bloop under it, a touch of ring.
    rnd = random.Random(seed); n = int(seconds * SR); out = []; phase = 0.0
    f0 = rnd.uniform(1500.0, 2400.0)
    for i in range(n):
        t = i / float(SR)
        f = f0 * (0.66 + 0.34 * math.exp(-t * 28.0)); phase += 2.0 * math.pi * f / SR
        plink = math.sin(phase) * math.exp(-t * 26.0)
        bloop = math.sin(2.0 * math.pi * 140.0 * t) * math.exp(-t * 18.0) * 0.35 * (1.0 if t > 0.012 else t / 0.012)
        ring = math.sin(2.0 * math.pi * f0 * 1.5 * t) * math.exp(-t * 40.0) * 0.15
        out.append(plink + bloop + ring)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.7 for v in out]

def compressor(seconds=10.0):
    # Off, then a clunk and the motor spinning up over two seconds, holding, winding down, off: the
    # whole cycle in ten seconds so the loop breathes. Hum at 52 Hz with a second harmonic and a
    # slight wobble; the spin-up sweeps the pitch.
    n = int(seconds * SR); out = []; phase = 0.0; y = 0.0; rnd = random.Random(7)
    for i in range(n):
        t = i / float(SR)
        if t < 0.8: env = 0.0
        elif t < 2.8: env = (t - 0.8) / 2.0
        elif t < 7.2: env = 1.0
        elif t < 9.0: env = 1.0 - (t - 7.2) / 1.8
        else: env = 0.0
        f = 52.0 * (0.55 + 0.45 * min(1.0, env + (0.2 if 0.8 < t < 2.8 else 0.0))) * (1.0 + 0.012 * math.sin(2.0 * math.pi * 1.3 * t))
        phase += 2.0 * math.pi * f / SR
        hum = (math.sin(phase) * 0.7 + math.sin(phase * 2.0) * 0.25 + math.sin(phase * 3.0) * 0.08) * env
        v = rnd.uniform(-1.0, 1.0); y += 0.04 * (v - y); rumble = y * 0.5 * env
        clunk = 0.0
        for at in (0.8, 9.0):
            k = t - at
            if 0.0 <= k < 0.12: clunk += math.sin(2.0 * math.pi * 90.0 * k) * math.exp(-k * 40.0) * 0.8
        out.append(hum + rumble + clunk)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.8 for v in out]

def clank(seed, seconds=1.4):
    # A distant clank: three inharmonic partials with a slow decay, a dull thud under them, a room tail.
    rnd = random.Random(seed); n = int(seconds * SR); out = []; y = 0.0
    p = [rnd.uniform(380.0, 470.0), rnd.uniform(700.0, 900.0), rnd.uniform(1150.0, 1400.0)]
    for i in range(n):
        t = i / float(SR)
        ring = (math.sin(2.0 * math.pi * p[0] * t) * 0.5 + math.sin(2.0 * math.pi * p[1] * t) * 0.3 + math.sin(2.0 * math.pi * p[2] * t) * 0.2) * math.exp(-t * 4.5)
        thud = math.sin(2.0 * math.pi * 70.0 * t) * math.exp(-t * 12.0) * 0.6
        v = rnd.uniform(-1.0, 1.0) * math.exp(-t * 3.0); y += 0.06 * (v - y)
        out.append(ring + thud + y * 0.35)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.6 for v in out]

def cable_hum(seconds=4.0):
    n = int(seconds * SR); out = []
    for i in range(n):
        t = i / float(SR)
        v = math.sin(2.0 * math.pi * 100.0 * t) * 0.6 + math.sin(2.0 * math.pi * 200.0 * t) * 0.3 + math.sin(2.0 * math.pi * 300.0 * t) * 0.1
        v *= 1.0 + 0.08 * math.sin(2.0 * math.pi * 0.5 * t)
        out.append(v)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.5 for v in out]

def write(name, xs):
    w = wave.open(OUT + '/' + name, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs)); w.close(); print('wrote', name, round(len(xs) / SR, 2), 's')

for k in (1, 2, 3, 4): write('drip_%02d.wav' % k, drip(seed=k * 11))
write('deck_compressor_loop.wav', compressor())
for k in (1, 2, 3): write('deck_clank_%02d.wav' % k, clank(seed=k * 5))
write('cable_hum_loop.wav', cable_hum())
