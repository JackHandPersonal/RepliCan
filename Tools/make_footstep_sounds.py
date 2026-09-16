"""Footsteps for RawAudio: a boot on the facility's steel deck, plus landings and quiet
crouched steps. Several variants of each so a walk cycle is not one sample on repeat.
Pure python, no dependencies, same conventions as Tools/make_weapon_sounds.py.

    python Tools/make_footstep_sounds.py
    python Tools/analyse_footsteps.py     (spectral check: tonality and decay)

Writes RawAudio/step_metal_<n>.wav, step_soft_<n>.wav, step_land.wav, step_land_soft.wav.
ABaseCharacter picks one per foot plant; see PlayFootstep there.

WHY IT IS BUILT THIS WAY
The first version used sine tones for the boot's thump and for the deck's answer. A sine is a
pure pitch, so the steps rang like a struck bell: "pingy and hollow", and closer to a bare foot
slapping than to a boot. Nothing here is a sine any more. A real boot on plate is:

  1. a hard, very short broadband transient as the heel arrives,
  2. a low broadband thud from the boot's mass, no pitch to it at all,
  3. the plate answering in a band rather than at a note, dying inside a few hundredths,
  4. a short, quiet tail of room.

The plate is modelled with two-pole resonators driven by noise. Several of them at inharmonic
frequencies, each heavily damped, read as metal without ever landing on a pitch. Damping is set
by a decay time in seconds rather than a Q, so the values below are readable.

Boot, not bare foot: the weight sits in the low thud and the hardness in the transient. Bare
skin is slappy between 1 and 3 kHz, so that band is deliberately kept down.
"""
import math, random, struct, wave, os
OUT = r'C:\Dev\Games\RepliCan\RawAudio'
SR = 22050
random.seed(41)

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

def noise(n): return [random.uniform(-1.0, 1.0) for _ in range(n)]

def env(n, attack, decay, power=2.0):
    a = max(1, int(attack * SR)); out = []
    for i in range(n):
        if i < a: out.append((i / a) ** 0.5)
        else:
            t = (i - a) / float(SR)
            out.append(math.exp(-t / max(1e-4, decay)) ** power)
    return out

def resonate(xs, hz, decay):
    """One two-pole resonator. decay is the time the ring falls to about a third.

    Driven with noise and damped this hard it colours the noise rather than singing: that is
    what makes it read as a plate being struck instead of a bell."""
    r = math.exp(-1.0 / max(1e-4, decay * SR))
    w = 2.0 * math.pi * hz / SR
    a1 = 2.0 * r * math.cos(w); a2 = -r * r
    out = []; y1 = 0.0; y2 = 0.0
    for x in xs:
        y = x + a1 * y1 + a2 * y2
        y2 = y1; y1 = y
        out.append(y)
    return out

def plate(n, modes, drive_decay=0.004):
    """A struck deck plate: a short noise burst through several damped resonators at
    inharmonic frequencies. No two modes are related by a simple ratio, so no pitch emerges."""
    drive = [v * e for v, e in zip(noise(n), env(n, 0.0002, drive_decay, 1.0))]
    out = [0.0] * n
    for hz, decay, amp in modes:
        r = resonate(drive, hz, decay)
        m = max(1e-6, max(abs(v) for v in r))
        for i in range(n): out[i] += r[i] * amp / m
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
    fade = int(0.002 * SR)
    for i in range(min(fade, len(xs))):
        xs[i] *= i / fade; xs[-1 - i] *= i / fade
    path = os.path.join(OUT, name)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs))
    print('wrote', name, '%.3fs' % (len(xs) / float(SR)))

def boot_on_deck(length, modes, weight, hardness, scuff):
    """One boot arriving on plate. Every layer is noise based; none of it has a pitch.

    Tuned away from "plastic banged on metal". Plastic is a short mid-frequency resonance with
    a hard click on the front, which is exactly what a lightly damped resonator bank gives you,
    so the resonators are now a seasoning rather than the substance. The substance is a heavy
    low thud and a broad mid crunch: mass arriving on a deck, not a shell being tapped."""
    n = int(length * SR)
    # 1. the heel arriving. Softened and dropped in frequency: a bright click on the front is
    #    most of what reads as plastic.
    strike = [v * e for v, e in zip(bandpass(noise(n), 900.0, 3000.0), env(n, 0.0004, 0.0045, 1.4))]
    # 2. the weight. The loudest thing here by a wide margin, and the reason a boot sounds like
    #    a boot: broadband, low, and with no resonance in it at all.
    thud = [v * e for v, e in zip(lowpass(noise(n), 200.0), env(n, 0.0010, 0.045, 0.8))]
    # 3. the deck taking the load: a broad mid crunch rather than a ring. This replaces most of
    #    what the resonators used to carry.
    crunch = [v * e for v, e in zip(bandpass(noise(n), 180.0, 900.0), env(n, 0.0009, 0.026, 1.0))]
    # 4. the plate's own answer, now only a trace: enough to say metal, not enough to say shell
    ring = [v * e for v, e in zip(plate(n, modes), env(n, 0.0006, 0.010, 1.4))]
    # 5. the sole rolling, and a little grit. Both kept low: this is the band bare skin and
    #    plastic both live in.
    sole = [v * e for v, e in zip(bandpass(noise(n), 700.0, 1800.0), env(n, 0.0020, 0.010, 1.4))]
    grit = [v * e for v, e in zip(bandpass(noise(n), 2000.0, 5000.0), env(n, 0.004, 0.010, 1.2))]
    return mix([s * hardness * 0.55 for s in strike], [t * weight * 3.2 for t in thud],
               [c * 1.1 for c in crunch], [r * 0.10 for r in ring],
               [s * 0.07 for s in sole], [g * scuff * 0.7 for g in grit])

def boot_quiet(length, weight, scuff):
    """Crouch-walking: a boot rolled down deliberately, with the weight taken slowly.

    Not simply the loud step turned down -- that still sounds like a boot hitting plate, just
    further away. Muffling means losing the top end and the transient: no deck resonance at all,
    a slow attack instead of a crack, and the whole thing low-passed so what survives is the
    dull thump of mass and a little gear moving. Distinct enough that the player can hear the
    difference between sneaking and walking without looking at the screen."""
    n = int(length * SR)
    # The weight, arriving slowly. A 4 ms attack is the difference between placing a boot and
    # dropping one, and it is most of what "muffled" means here.
    thud = [v * e for v, e in zip(lowpass(noise(n), 160.0), env(n, 0.0040, 0.030, 0.9))]
    # The sole compressing under the roll: soft, broad, and well below the band a hard step
    # lives in.
    roll = [v * e for v, e in zip(bandpass(noise(n), 120.0, 520.0), env(n, 0.0055, 0.024, 1.0))]
    # Webbing and knee armour shifting. The only high content left, and it is barely there.
    gear = [v * e for v, e in zip(bandpass(noise(n), 1400.0, 3200.0), env(n, 0.0070, 0.013, 1.2))]
    mixed = mix([t * weight * 2.6 for t in thud], [r * 0.85 for r in roll], [g * scuff * 0.35 for g in gear])
    # One more pass over the whole thing: nothing above this survives a crouch.
    return lowpass(mixed, 900.0)

os.makedirs(OUT, exist_ok=True)

# Six boots. The variation is in which plate modes answer and how hard the heel lands, not in
# any pitch: the frequencies below are deliberately unrelated to each other.
VARIANTS = [
    ([(318.0, 0.0090, 1.00), (547.0, 0.0060, 0.62), (923.0, 0.0040, 0.34), (1487.0, 0.0025, 0.18)], 1.00, 0.17, 0.05),
    ([(286.0, 0.0075, 1.00), (611.0, 0.0052, 0.55), (1042.0, 0.0035, 0.30), (1733.0, 0.0020, 0.15)], 0.92, 0.20, 0.07),
    ([(352.0, 0.0105, 1.00), (593.0, 0.0068, 0.58), (871.0, 0.0044, 0.36), (1394.0, 0.0028, 0.20)], 1.08, 0.15, 0.04),
    ([(264.0, 0.0084, 1.00), (509.0, 0.0056, 0.66), (998.0, 0.0031, 0.28), (1621.0, 0.0020, 0.16)], 0.96, 0.22, 0.08),
    ([(337.0, 0.0068, 1.00), (628.0, 0.0047, 0.52), (1104.0, 0.0038, 0.32), (1822.0, 0.0024, 0.14)], 1.04, 0.18, 0.06),
    ([(301.0, 0.0098, 1.00), (566.0, 0.0064, 0.60), (949.0, 0.0035, 0.38), (1538.0, 0.0028, 0.17)], 0.88, 0.19, 0.06),
]
for i, (modes, weight, hardness, scuff) in enumerate(VARIANTS, start=1):
    write('step_metal_%d.wav' % i, boot_on_deck(0.20, modes, weight, hardness, scuff))

for i, (weight, scuff) in enumerate([(0.95, 0.30), (1.05, 0.22), (0.88, 0.38), (1.00, 0.26)], start=1):
    write('step_soft_%d.wav' % i, boot_quiet(0.16, weight, scuff))

# Landing: both boots together. More weight, the plate given a real hit, and a short room tail.
n = int(0.38 * SR)
land = mix(
    [v * e * 0.5 for v, e in zip(highpass(noise(n), 1900.0), env(n, 0.0002, 0.006, 1.6))],
    [v * e * 2.6 for v, e in zip(lowpass(noise(n), 180.0), env(n, 0.001, 0.055, 0.85))],
    [v * e * 0.30 for v, e in zip(plate(n, [(214.0, 0.020, 1.00), (398.0, 0.014, 0.66),
                                            (703.0, 0.009, 0.40), (1201.0, 0.006, 0.22)], 0.006),
                                  env(n, 0.0005, 0.030, 1.1))],
    [v * e * 0.18 for v, e in zip(lowpass(noise(n), 800.0), env(n, 0.008, 0.10, 1.0))])
write('step_land.wav', land)

n = int(0.24 * SR)
land_soft = mix(
    [v * e for v, e in zip(lowpass(noise(n), 200.0), env(n, 0.0015, 0.034, 1.0))],
    [v * e * 0.22 for v, e in zip(plate(n, [(392.0, 0.012, 1.0), (688.0, 0.008, 0.5)]), env(n, 0.001, 0.018, 1.1))],
    [v * e * 0.14 for v, e in zip(bandpass(noise(n), 700.0, 2200.0), env(n, 0.003, 0.016, 1.1))])
# Landing out of a crouch is muffled the same way the crouched steps are, or the contrast
# breaks the moment the player drops off anything.
write('step_land_soft.wav', lowpass(land_soft, 1000.0))
print('footsteps written')
