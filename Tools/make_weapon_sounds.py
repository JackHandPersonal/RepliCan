"""Weapon report sounds for RawAudio, one per weapon family: a transient crack over a low
body thump, then a filtered tail for the room. Blades and blunts get a swing instead of a
report. Pure python, no dependencies.

    python Tools/make_weapon_sounds.py

Writes RawAudio/wep_<family>.wav; UI/Weapons.json names the family per weapon (see
scratchpad make_weapons_json.py, which fills the "sound" field).
"""
import math, random, struct, wave, os
OUT = r'C:\Dev\Games\RepliCan\RawAudio'
SR = 22050
random.seed(7)

def lowpass(xs, cutoff):
    rc = 1.0 / (2 * math.pi * cutoff); dt = 1.0 / SR; a = dt / (rc + dt)
    out = []; y = 0.0
    for x in xs:
        y += a * (x - y); out.append(y)
    return out

def highpass(xs, cutoff):
    rc = 1.0 / (2 * math.pi * cutoff); dt = 1.0 / SR; a = rc / (rc + dt)
    out = []; y = 0.0; px = 0.0
    for x in xs:
        y = a * (y + x - px); px = x; out.append(y)
    return out

def noise(n): return [random.uniform(-1.0, 1.0) for _ in range(n)]
def env(n, attack, decay, power=2.0):
    """A fast rise then an exponential fall, both in seconds."""
    a = max(1, int(attack * SR)); out = []
    for i in range(n):
        if i < a: out.append((i / a) ** 0.5)
        else:
            t = (i - a) / float(SR)
            out.append(math.exp(-t / max(1e-4, decay)) ** power)
    return out

def mix(*layers):
    n = max(len(l) for l in layers)
    out = [0.0] * n
    for l in layers:
        for i, v in enumerate(l): out[i] += v
    return out

def norm(xs, peak=0.92):
    m = max(1e-6, max(abs(v) for v in xs))
    return [v * peak / m for v in xs]

def write(name, xs):
    xs = norm(xs)
    # a couple of milliseconds of fade at each end so nothing clicks
    fade = int(0.003 * SR)
    for i in range(min(fade, len(xs))):
        xs[i] *= i / fade; xs[-1 - i] *= i / fade
    path = os.path.join(OUT, name)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs))
    print('wrote', name, '%.2fs' % (len(xs) / float(SR)))

def report(length, crack_hz, body_hz, decay, body_decay, tail, grit=1.0):
    """A gun: a bright crack, a low body, and a tail of room noise."""
    n = int(length * SR)
    crack = [v * e for v, e in zip(highpass(noise(n), crack_hz), env(n, 0.0004, decay, 1.6))]
    body = [math.sin(2 * math.pi * body_hz * (i / float(SR)) * (1.0 - 0.6 * i / n)) * e
            for i, e in enumerate(env(n, 0.0008, body_decay, 1.2))]
    room = [v * e for v, e in zip(lowpass(noise(n), 1800.0), env(n, 0.004, tail, 1.0))]
    return mix([c * grit for c in crack], [b * 0.8 for b in body], [r * 0.25 for r in room])

def softclip(xs, drive):
    """Saturation: what makes a gunshot dense rather than a thin click. tanh keeps the peaks
    from clipping hard while pushing everything under them up."""
    return [math.tanh(v * drive) / math.tanh(drive) for v in xs]


def echoes(xs, taps):
    """Early reflections, as (delay seconds, gain, cutoff Hz): a shot in a room is the shot and
    the room answering it a few times, each answer duller than the last."""
    n = len(xs); out = list(xs)
    for delay, gain, cutoff in taps:
        d = int(delay * SR); dull = lowpass(xs, cutoff)
        for i in range(n - d):
            out[i + d] += dull[i] * gain
    return out


def gunshot(length, crack_hz, body_hz, body_decay, bark, tail, drive=2.2, room=1.0, mech=0.0):
    """A firearm, in layers, each doing one job:
      1. the crack   -- a millisecond of full-band noise, the supersonic arrival, brighter than
                        anything else in the mix and over before anything else starts
      2. the blast   -- the muzzle: LOW noise (not a sine, which rings like a drum) through a
                        band around body_hz, opening hard and dying in a few hundredths, its
                        centre sliding down as it decays the way a real report drops
      3. the bark    -- damped resonators in the low mids, the weapon's own body answering;
                        inharmonic, so no note is heard
      4. the mech    -- the action cycling, a small bright click a few hundredths later
      5. the room    -- early reflections, then a long low-passed noise tail
    then everything is driven through a soft clip, which is where the weight comes from."""
    n = int(length * SR)
    crack = [v * e for v, e in zip(highpass(noise(n), crack_hz), env(n, 0.0002, 0.006, 1.4))]
    raw = noise(n); blast = []
    lo_env = env(n, 0.0006, body_decay, 1.1)
    # the band slides down: filter twice with the cutoff falling over the decay
    b1 = lowpass(highpass(raw, body_hz * 0.45), body_hz * 2.6)
    b2 = lowpass(highpass(raw, body_hz * 0.25), body_hz * 0.9)
    for i in range(n):
        t = min(1.0, i / max(1.0, body_decay * SR * 2.5))
        blast.append((b1[i] * (1.0 - t) + b2[i] * t) * lo_env[i])
    barks = [0.0] * n
    for hz, dec, amp in bark:
        r = resonate([v * e for v, e in zip(noise(n), env(n, 0.0002, 0.004, 1.0))], hz, dec)
        m = max(1e-6, max(abs(v) for v in r))
        for i in range(n): barks[i] += r[i] * amp / m
    click = [0.0] * n
    if mech > 0.0:
        at = int(0.045 * SR); ln = int(0.012 * SR)
        bit = bandpass(noise(ln), 2500.0, 8000.0); e = env(ln, 0.0001, 0.003, 1.0)
        for i in range(ln):
            if at + i < n: click[at + i] = bit[i] * e[i] * mech
    dry = mix([c * 1.0 for c in crack], [b * 1.6 for b in blast], [b * 0.7 for b in barks], click)
    wet = echoes(dry, [(0.031, 0.35 * room, 3200.0), (0.074, 0.22 * room, 1600.0), (0.128, 0.14 * room, 900.0)])
    rm = [v * e * 0.22 * room for v, e in zip(lowpass(noise(n), 700.0), env(n, 0.010, tail, 1.0))]
    return softclip(mix(wet, rm), drive)


def bandpass(xs, lo, hi):
    return highpass(lowpass(xs, hi), lo)


def resonate(xs, hz, decay):
    r = math.exp(-1.0 / max(1e-4, decay * SR)); w = 2.0 * math.pi * hz / SR
    a1 = 2.0 * r * math.cos(w); a2 = -r * r
    out = []; y1 = 0.0; y2 = 0.0
    for x in xs:
        y = x + a1 * y1 + a2 * y2; y2 = y1; y1 = y; out.append(y)
    return out


def swing(length, hz, decay):
    """A blade or a club through the air: filtered noise swelling and falling away."""
    n = int(length * SR)
    out = []
    for i in range(n):
        t = i / float(n)
        amp = math.sin(math.pi * min(1.0, t * 1.25)) ** 2
        out.append(random.uniform(-1.0, 1.0) * amp)
    return [v * e for v, e in zip(lowpass(highpass(out, hz * 0.5), hz * 3.0), env(n, 0.02, decay, 1.0))]

def beam(length, start_hz, end_hz, decay):
    """An energy weapon: a falling tone with a bright edge on it."""
    n = int(length * SR); out = []; phase = 0.0
    for i in range(n):
        t = i / float(n)
        f = start_hz * (end_hz / start_hz) ** t
        phase += 2 * math.pi * f / SR
        out.append(math.sin(phase) + 0.35 * math.sin(phase * 2.01))
    e = env(n, 0.001, decay, 1.3)
    edge = [v * x for v, x in zip(highpass(noise(n), 3000.0), env(n, 0.0005, decay * 0.4, 2.0))]
    return mix([o * x * 0.8 for o, x in zip(out, e)], [x * 0.3 for x in edge])

os.makedirs(OUT, exist_ok=True)
# The firearms: a blast with real low end, the weapon's bark, the action, and a room around it.
# (The earlier one-crack-one-sine 'report' read as a cap gun; kept above for the record.)
write('wep_pistol.wav',   gunshot(0.60, 1600.0, 170.0, 0.055, [(310.0, 0.030, 1.0), (520.0, 0.022, 0.6), (900.0, 0.012, 0.35)], 0.30, drive=2.4, mech=0.5))
write('wep_smg.wav',      gunshot(0.45, 1900.0, 200.0, 0.040, [(360.0, 0.024, 1.0), (640.0, 0.016, 0.5)], 0.22, drive=2.2, mech=0.35))
write('wep_rifle.wav',    gunshot(0.85, 1200.0, 130.0, 0.075, [(240.0, 0.045, 1.0), (410.0, 0.030, 0.7), (760.0, 0.016, 0.4)], 0.45, drive=2.8, mech=0.45))
write('wep_sniper.wav',   gunshot(1.20, 1000.0, 105.0, 0.095, [(190.0, 0.060, 1.0), (330.0, 0.040, 0.7), (610.0, 0.020, 0.4)], 0.70, drive=3.0, mech=0.3))
write('wep_shotgun.wav',  gunshot(1.00, 800.0, 90.0, 0.110, [(160.0, 0.070, 1.0), (290.0, 0.045, 0.8), (540.0, 0.022, 0.4)], 0.55, drive=3.2, mech=0.4))
write('wep_heavy.wav',    gunshot(1.10, 700.0, 70.0, 0.130, [(130.0, 0.080, 1.0), (240.0, 0.050, 0.8), (450.0, 0.025, 0.5)], 0.65, drive=3.2))
write('wep_launcher.wav', gunshot(1.20, 500.0, 55.0, 0.160, [(95.0, 0.100, 1.0), (180.0, 0.060, 0.7)], 0.80, drive=2.6))
write('wep_laser.wav',    beam(0.34, 2400.0, 420.0, 0.070))
write('wep_alien.wav',    beam(0.48, 620.0, 2600.0, 0.100))
write('wep_blade.wav',    swing(0.34, 900.0, 0.090))
write('wep_blunt.wav',    swing(0.42, 420.0, 0.130))
print('done')
