"""Bullet impacts and steam vents for RawAudio. Pure python, no dependencies.

    python Tools/make_impact_sounds.py

Writes:
    impact_metal_<n>.wav   a round into station plate: crack, spark, a short ring
    impact_soft_<n>.wav    into crate, panel or composite: a thud with no ring
    impact_glass_<n>.wav   into glass or a hologram: bright, sharp, fast
    impact_flesh_<n>.wav   into a person: dull, wet, no metal at all
    steam_burst_<n>.wav    a valve letting go: a hiss that opens hard and tails off
    steam_hiss_<n>.wav     a continuous leak, seamlessly loopable
    pest_skitter_<n>.wav   small claws on deck plate: a burst of tiny irregular ticks

SAME PRINCIPLE AS THE FOOTSTEPS, AND FOR THE SAME REASON. Nothing here is a sine. An impact
that uses a tone rings like a triangle being struck; what makes a round hitting plate sound
like a round hitting plate is broadband noise shaped in time, plus heavily damped resonators
at frequencies that are not related by simple ratios, so the ear hears "metal" and never hears
a note. The structure of every impact is:

  1. the crack -- a couple of milliseconds of full-band noise, the supersonic arrival,
  2. the strike -- the material's own answer, noise through resonators, decaying in hundredths,
  3. the debris -- a sparse scatter of tiny clicks, which is what reads as spall and sparks,
  4. a short tail of room.

Steam is the opposite shape: no transient at all, just filtered noise with a slow swell, a long
body and a longer tail. Making it loopable means crossfading the end into the start, which is
why steam_hiss is built at double length and folded in half.
"""
import math, random, struct, wave, os

OUT = r'C:\Dev\Games\RepliCan\RawAudio'
SR = 22050
random.seed(1971)


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


def bandpass(xs, lo, hi):
    return highpass(lowpass(xs, hi), lo)


def noise(n):
    return [random.uniform(-1.0, 1.0) for _ in range(n)]


def env(n, attack, decay, power=2.0):
    a = max(1, int(attack * SR)); out = []
    for i in range(n):
        if i < a:
            out.append((i / a) ** 0.5)
        else:
            t = (i - a) / float(SR)
            out.append(math.exp(-t / max(1e-4, decay)) ** power)
    return out


def resonate(xs, hz, decay):
    """One two-pole resonator, damped by a decay time rather than a Q so the numbers read."""
    r = math.exp(-1.0 / max(1e-4, decay * SR))
    w = 2.0 * math.pi * hz / SR
    a1 = 2.0 * r * math.cos(w); a2 = -r * r
    out = []; y1 = 0.0; y2 = 0.0
    for x in xs:
        y = x + a1 * y1 + a2 * y2
        y2 = y1; y1 = y
        out.append(y)
    return out


def struck(n, modes, drive_decay=0.0025):
    """A struck surface: a very short noise burst through inharmonic damped resonators."""
    drive = [v * e for v, e in zip(noise(n), env(n, 0.0002, drive_decay, 1.0))]
    out = [0.0] * n
    for hz, decay, amp in modes:
        r = resonate(drive, hz, decay)
        m = max(1e-6, max(abs(v) for v in r))
        for i in range(n):
            out[i] += r[i] * amp / m
    return out


def debris(n, count, spread, brightness):
    """Spall: a sparse scatter of tiny clicks after the strike. This is what the ear reads as
    sparks and fragments -- a smooth decay alone sounds like a drum, not like something
    shattering off a wall."""
    out = [0.0] * n
    for _ in range(count):
        at = int(random.uniform(0.004, spread) * SR)
        if at >= n:
            continue
        length = int(random.uniform(0.0008, 0.004) * SR)
        amp = random.uniform(0.05, 0.28)
        bit = bandpass(noise(length), brightness * 0.5, min(brightness * 2.2, SR * 0.45))
        e = env(length, 0.0001, random.uniform(0.0006, 0.003), 1.0)
        for i in range(length):
            if at + i < n:
                out[at + i] += bit[i] * e[i] * amp
    return out


def mix(*layers):
    n = max(len(l) for l in layers)
    out = [0.0] * n
    for l in layers:
        for i, v in enumerate(l):
            out[i] += v
    return out


def norm(xs, peak=0.92):
    m = max(1e-6, max(abs(v) for v in xs))
    return [v * peak / m for v in xs]


def write(name, xs, peak=0.92, fade_in=0.001):
    xs = norm(xs, peak)
    fi = max(1, int(fade_in * SR))
    fo = max(1, int(0.004 * SR))
    for i in range(min(fi, len(xs))):
        xs[i] *= i / fi
    for i in range(min(fo, len(xs))):
        xs[-1 - i] *= i / fo
    with wave.open(os.path.join(OUT, name), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs))
    print('wrote %-24s %.3fs' % (name, len(xs) / float(SR)))


# ---- Impacts ----------------------------------------------------------------------------

def impact(length, modes, crack_amp, crack_band, debris_count, debris_bright, body_amp,
           thud_hz=140.0, thud_amp=0.0):
    n = int(length * SR)
    # 1. the crack: the arrival itself, almost no duration, full band.
    crack = [v * e * crack_amp for v, e in
             zip(bandpass(noise(n), crack_band[0], crack_band[1]), env(n, 0.00005, 0.0016, 1.0))]
    # 2. the strike: the material answering.
    body = [v * body_amp for v in struck(n, modes)]
    # 3. spall.
    bits = debris(n, debris_count, min(0.09, length * 0.55), debris_bright)
    layers = [crack, body, bits]
    # 4. an optional low thud, for anything with mass behind it. Noise, not a tone.
    if thud_amp > 0.0:
        low = [v * e * thud_amp for v, e in zip(lowpass(noise(n), thud_hz), env(n, 0.0004, 0.030, 1.4))]
        layers.append(low)
    return mix(*layers)


# Station plate: bright, ringing briefly, lots of spall. Modes deliberately unrelated.
for i in range(1, 5):
    write('impact_metal_%d.wav' % i, impact(
        0.26,
        modes=[(random.uniform(1180, 1320), 0.055, 1.0),
               (random.uniform(2270, 2520), 0.032, 0.72),
               (random.uniform(3810, 4260), 0.018, 0.45),
               (random.uniform(6100, 6900), 0.009, 0.24)],
        crack_amp=0.95, crack_band=(900, 9000),
        debris_count=14, debris_bright=3600, body_amp=0.62, thud_amp=0.22))

# Crates, panels, composite: a thud, almost no ring, dull debris.
for i in range(1, 4):
    write('impact_soft_%d.wav' % i, impact(
        0.20,
        modes=[(random.uniform(300, 380), 0.030, 1.0),
               (random.uniform(690, 820), 0.016, 0.5)],
        crack_amp=0.55, crack_band=(400, 4200),
        debris_count=6, debris_bright=1400, body_amp=0.85, thud_amp=0.55))

# Glass and holograms: bright, sharp, over fast, plenty of fragments.
for i in range(1, 4):
    write('impact_glass_%d.wav' % i, impact(
        0.22,
        modes=[(random.uniform(3300, 3900), 0.030, 1.0),
               (random.uniform(5400, 6300), 0.020, 0.8),
               (random.uniform(8200, 9400), 0.012, 0.55)],
        crack_amp=1.0, crack_band=(2200, 10000),
        debris_count=26, debris_bright=6200, body_amp=0.7))

# A person: mass and no metal whatever. Low band only, and no spall.
for i in range(1, 4):
    write('impact_flesh_%d.wav' % i, impact(
        0.16,
        modes=[(random.uniform(150, 210), 0.022, 1.0),
               (random.uniform(390, 470), 0.012, 0.4)],
        crack_amp=0.30, crack_band=(200, 1800),
        debris_count=0, debris_bright=800, body_amp=1.0, thud_amp=0.8))


# ---- Steam ------------------------------------------------------------------------------
# No transient at all: steam is pressure escaping, so it swells rather than strikes. The
# character is entirely in the filter -- a narrow high band is a pinhole leak, a wide band with
# some low end in it is a pipe letting go.

def steam(length, lo, hi, attack, body, tail, rasp=0.0):
    n = int(length * SR)
    src = bandpass(noise(n), lo, hi)
    a = int(attack * SR)
    b = int(body * SR)
    out = []
    for i in range(n):
        if i < a:
            g = (i / max(1, a)) ** 0.7
        elif i < a + b:
            g = 1.0
        else:
            g = math.exp(-(i - a - b) / float(SR) / max(1e-3, tail))
        out.append(src[i] * g)
    if rasp > 0.0:
        # A slow wander in the low end: a vent does not hiss evenly, it flutters.
        slow = lowpass(noise(n), 7.0)
        m = max(1e-6, max(abs(v) for v in slow))
        out = [v * (1.0 + rasp * (slow[i] / m)) for i, v in enumerate(out)]
    return out


# A valve letting go: hard onset, short body, long tail.
for i, (lo, hi) in enumerate([(700, 7000), (1100, 9000), (420, 5200)], start=1):
    write('steam_burst_%d.wav' % i, steam(1.5, lo, hi, attack=0.02, body=0.20, tail=0.45, rasp=0.35),
          peak=0.8, fade_in=0.004)

# A continuous leak. Built at double length and folded so the end crossfades into the start:
# a loop with a seam is worse than no loop at all, and a vent runs for minutes.
for i, (lo, hi, r) in enumerate([(1600, 9500, 0.25), (500, 4200, 0.45)], start=1):
    n = int(3.0 * SR)
    raw = bandpass(noise(n * 2), lo, hi)
    if r > 0.0:
        slow = lowpass(noise(n * 2), 5.0)
        m = max(1e-6, max(abs(v) for v in slow))
        raw = [v * (1.0 + r * (slow[j] / m)) for j, v in enumerate(raw)]
    folded = []
    for j in range(n):
        t = j / float(n)
        folded.append(raw[j] * (1.0 - t) + raw[j + n] * t)
    write('steam_hiss_%d.wav' % i, folded, peak=0.55, fade_in=0.0)

print('IMPACT + STEAM AUDIO written to', OUT)


# ---- Pests ------------------------------------------------------------------------------
# Claws on plate. Not a rhythm: a real scuttle is a ragged burst where the feet are close
# enough together to overlap, and the irregularity is the whole character of it -- evenly
# spaced ticks sound like a clock and instantly stop being an animal.

def skitter(length, ticks, bright, spread_start=0.01):
    n = int(length * SR)
    out = [0.0] * n
    t = spread_start
    for _ in range(ticks):
        at = int(t * SR)
        if at >= n:
            break
        ln = int(random.uniform(0.0010, 0.0035) * SR)
        bit = bandpass(noise(ln), bright * 0.45, min(bright * 2.6, SR * 0.45))
        e = env(ln, 0.00008, random.uniform(0.0007, 0.0022), 1.0)
        amp = random.uniform(0.25, 1.0)
        for i in range(ln):
            if at + i < n:
                out[at + i] += bit[i] * e[i] * amp
        # The gap between footfalls, jittered hard. Occasionally two feet land almost together.
        t += random.uniform(0.018, 0.055) if random.random() > 0.22 else random.uniform(0.004, 0.012)
    # A whisper of body dragging over the plate underneath it all.
    drag = [v * e * 0.12 for v, e in zip(bandpass(noise(n), 1800, 7000), env(n, 0.02, length * 0.6, 1.0))]
    return mix(out, drag)


for i, (ln, ticks, bright) in enumerate([(0.55, 16, 5200), (0.40, 11, 6400), (0.72, 22, 4300)], start=1):
    write('pest_skitter_%d.wav' % i, skitter(ln, ticks, bright), peak=0.6, fade_in=0.0005)

print('PEST AUDIO written to', OUT)


# ---- The crack -------------------------------------------------------------------------
# A conventional round on a hard surface: the arrival, one hard short strike, a spark or two,
# and nothing after it. Nine hundredths of a second. The ring the plate impacts have is what
# made them read as long; here the modes are damped in single-digit milliseconds.
for i in range(1, 5):
    write('impact_crack_%d.wav' % i, impact(
        0.09,
        modes=[(random.uniform(1500, 1750), 0.011, 1.0),
               (random.uniform(2900, 3300), 0.007, 0.7),
               (random.uniform(5200, 5900), 0.004, 0.4)],
        crack_amp=1.0, crack_band=(1200, 10000),
        debris_count=5, debris_bright=4500, body_amp=0.5, thud_amp=0.3))

print('CRACK AUDIO written to', OUT)
