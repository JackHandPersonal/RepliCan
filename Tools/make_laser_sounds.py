"""Laser weapon sounds: the beam's buzz (a short loop the controller re-lights before it ends, so
it reads as one continuous note) and the dull click of a flat battery. Two files in RawAudio.
    python Tools/make_laser_sounds.py
"""
import math, random, struct, wave
OUT = 'C:/Dev/Games/RepliCan/RawAudio'
SR = 22050
random.seed(23)

def buzz(seconds):
    # More hiss than buzz: band-passed noise (the beam boiling the air) carries it, with a low saw
    # underneath (the power stage) and a faint wobbling whine, all chopped a little at mains rate.
    n = int(seconds * SR); out = []; y1 = 0.0; y2 = 0.0
    a = 1.0 - math.exp(-2.0 * math.pi * 5200.0 / SR)   # the noise band's top
    b = 1.0 - math.exp(-2.0 * math.pi * 1400.0 / SR)   # and its bottom
    for i in range(n):
        t = i / float(SR)
        v = random.uniform(-1.0, 1.0)
        y1 += a * (v - y1); y2 += b * (y1 - y2)
        hiss = (y1 - y2) * 0.9 * (0.85 + 0.15 * math.sin(2.0 * math.pi * 11.0 * t))
        saw = sum(math.sin(2.0 * math.pi * 118.0 * h * t) / h for h in range(1, 7)) * 0.16
        whine = math.sin(2.0 * math.pi * (2400.0 + 60.0 * math.sin(2.0 * math.pi * 7.0 * t)) * t) * 0.08
        chop = 0.85 + 0.15 * (1.0 if math.sin(2.0 * math.pi * 60.0 * t) > 0.0 else -0.6)
        env = min(1.0, t / 0.03, (seconds - t) / 0.06)   # soft ends, so two overlapping copies cross without a click
        out.append((hiss + (saw + whine) * chop) * max(0.0, env))
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.75 for v in out]

def flat(seconds):
    n = int(seconds * SR); out = []; y = 0.0
    for i in range(n):
        t = i / float(SR)
        v = random.uniform(-1.0, 1.0) * math.exp(-t * 60.0)     # a dull thock
        y += 0.25 * (v - y)
        out.append(y + math.sin(2.0 * math.pi * 210.0 * t) * math.exp(-t * 40.0) * 0.5)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.6 for v in out]

def write(name, xs):
    w = wave.open(OUT + '/' + name, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs)); w.close(); print('wrote', name, round(len(xs) / SR, 2), 's')

write('laser_buzz.wav', buzz(0.6))
write('laser_flat.wav', flat(0.14))
