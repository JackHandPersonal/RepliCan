"""Reload sounds: a magazine out, a magazine in, the action worked. Three files in RawAudio,
one per stance family, built from filtered noise the way make_weapon_sounds.py builds its
reports. Each is about two seconds, the events spaced to sit under the Lyra reload clips.
    python Tools/make_reload_sounds.py
"""
import math, random, struct, wave, os
OUT = r'C:\Dev\Games\RepliCan\RawAudio'
SR = 22050
random.seed(7)

def lowpass(xs, cutoff):
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff / SR); y = 0.0; out = []
    for v in xs: y += a * (v - y); out.append(y)
    return out
def highpass(xs, cutoff):
    return [v - l for v, l in zip(xs, lowpass(xs, cutoff))]
def noise(n): return [random.uniform(-1.0, 1.0) for _ in range(n)]
def env(n, attack, decay, power=2.0):
    out = []
    for i in range(n):
        t = i / float(SR)
        e = (t / attack) if t < attack else math.exp(-(t - attack) / decay)
        out.append(max(0.0, min(1.0, e)) ** power)
    return out
def burst(seconds, attack, decay, lo, hi, gain):
    n = int(seconds * SR); x = noise(n)
    if lo > 0: x = highpass(x, lo)
    if hi > 0: x = lowpass(x, hi)
    e = env(n, attack, decay); return [v * g * gain for v, g in zip(x, e)]
def thump(seconds, hz, decay, gain):
    n = int(seconds * SR); e = env(n, 0.002, decay, 1.5)
    return [math.sin(2 * math.pi * hz * i / SR) * e[i] * gain for i in range(n)]
def place(track, at, part):
    i0 = int(at * SR)
    for i, v in enumerate(part):
        if i0 + i < len(track): track[i0 + i] += v
def click(track, at, gain=1.0):
    place(track, at, burst(0.05, 0.001, 0.010, 1800, 7000, 0.9 * gain))
def clunk(track, at, gain=1.0):
    place(track, at, thump(0.12, 140, 0.035, 0.8 * gain)); place(track, at, burst(0.06, 0.001, 0.014, 300, 2500, 0.5 * gain))
def scrape(track, at, seconds=0.12, gain=0.5):
    place(track, at, burst(seconds, 0.02, seconds * 0.5, 900, 4000, gain))
def rack(track, at, gain=1.0):
    scrape(track, at, 0.09, 0.45 * gain); click(track, at + 0.09, 0.8 * gain); click(track, at + 0.19, 1.0 * gain); place(track, at + 0.19, thump(0.08, 220, 0.02, 0.35 * gain))
def write(name, xs):
    m = max(1e-6, max(abs(v) for v in xs)); xs = [v * 0.9 / m for v in xs]
    fade = int(0.003 * SR)
    for i in range(min(fade, len(xs))): xs[i] *= i / fade; xs[-1 - i] *= i / fade
    with wave.open(os.path.join(OUT, name), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs))
    print('wrote', name, '%.2fs' % (len(xs) / float(SR)))

t = [0.0] * int(1.9 * SR); click(t, 0.12); scrape(t, 0.20, 0.10, 0.35); clunk(t, 0.78); rack(t, 1.15); write('reload_pistol.wav', t)
t = [0.0] * int(2.1 * SR); click(t, 0.15, 0.8); scrape(t, 0.25, 0.14, 0.4); clunk(t, 0.95, 1.1); click(t, 1.02, 0.5); rack(t, 1.45, 1.1); write('reload_rifle.wav', t)
t = [0.0] * int(2.2 * SR)
for k in range(4): click(t, 0.25 + 0.3 * k, 0.7); place(t, 0.25 + 0.3 * k, thump(0.06, 260, 0.015, 0.4))
rack(t, 1.6, 1.2); write('reload_shotgun.wav', t)
