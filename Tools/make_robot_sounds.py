"""The work bot's noises: a robotic scream for its death (a square wave gliding down through a
ring modulator, crushed, with static rising under it), the clank of its strike, and three short
electrical crackles for the sparks it throws when it is failing. Files in RawAudio.
    python Tools/make_robot_sounds.py
"""
import math, random, struct, wave
OUT = 'C:/Dev/Games/RepliCan/RawAudio'
SR = 22050
random.seed(41)

def scream(seconds=1.35):
    n = int(seconds * SR); out = []; phase = 0.0
    for i in range(n):
        t = i / float(SR); u = t / seconds
        f = 980.0 * math.exp(-1.5 * u) + 130.0                    # glides down
        f *= 1.0 + 0.06 * math.sin(2.0 * math.pi * 26.0 * t)     # a warble
        phase += 2.0 * math.pi * f / SR
        v = (1.0 if math.sin(phase) > 0.0 else -1.0) * 0.6 + math.sin(phase * 2.0) * 0.3
        v *= 0.6 + 0.4 * math.sin(2.0 * math.pi * 47.0 * t)      # ring modulated
        v = math.tanh(v * 2.2)
        v = round(v * 14.0) / 14.0                                 # about four bits
        env = min(1.0, t / 0.02) * (1.0 if u < 0.68 else (1.0 - u) / 0.32)
        static = random.uniform(-1.0, 1.0) * 0.28 * (u ** 2)     # static rising toward the end
        out.append(v * env + static * env)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.85 for v in out]

def strike(seconds=0.38):
    n = int(seconds * SR); out = []; y = 0.0
    for i in range(n):
        t = i / float(SR)
        # a servo whir for the first tenth, then the clank: three metal partials and a burst of noise
        whir = math.sin(2.0 * math.pi * 330.0 * t) * 0.25 * (1.0 if t < 0.11 else math.exp(-(t - 0.11) * 60.0))
        k = t - 0.11
        clank = 0.0
        if k >= 0.0:
            clank = (math.sin(2.0 * math.pi * 620.0 * k) * 0.5 + math.sin(2.0 * math.pi * 1130.0 * k) * 0.35 + math.sin(2.0 * math.pi * 1780.0 * k) * 0.2) * math.exp(-k * 18.0)
            v = random.uniform(-1.0, 1.0) * math.exp(-k * 45.0); y += 0.5 * (v - y); clank += y * 0.7
        out.append(whir + clank)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.8 for v in out]

def crackle(seconds=0.36, seed=0):
    rnd = random.Random(seed)
    n = int(seconds * SR); out = [0.0] * n
    for _ in range(rnd.randint(9, 15)):
        at = int(rnd.uniform(0.0, seconds - 0.012) * SR); length = int(rnd.uniform(0.002, 0.009) * SR); g = rnd.uniform(0.4, 1.0)
        prev = 0.0
        for j in range(length):
            v = rnd.uniform(-1.0, 1.0)
            if at + j < n: out[at + j] += (v - prev) * g * math.exp(-j / float(length) * 3.0)   # the difference: only the sharp part
            prev = v
    hum = [math.sin(2.0 * math.pi * 120.0 * i / SR) * 0.06 * math.exp(-i / float(n) * 4.0) for i in range(n)]
    out = [a + b for a, b in zip(out, hum)]
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.7 for v in out]

def thump(seconds=0.34):
    # A solid thump: a low sine sliding down under a dark slap of noise.
    n = int(seconds * SR); out = []; y = 0.0
    for i in range(n):
        t = i / float(SR)
        f = 70.0 * math.exp(-t * 3.0) + 34.0
        body = math.sin(2.0 * math.pi * f * t) * math.exp(-t * 9.0)
        v = random.uniform(-1.0, 1.0) * math.exp(-t * 70.0); y += 0.18 * (v - y)
        out.append(body * 0.9 + y * 0.8)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.9 for v in out]

def step(seed, seconds=0.3):
    # A footfall: a metal clank (three partials, fast decay) on a short thud, a servo whir under it.
    rnd = random.Random(seed); n = int(seconds * SR); out = []; y = 0.0
    p = [rnd.uniform(430.0, 520.0), rnd.uniform(860.0, 980.0), rnd.uniform(1300.0, 1500.0)]
    for i in range(n):
        t = i / float(SR)
        clank = (math.sin(2.0 * math.pi * p[0] * t) * 0.5 + math.sin(2.0 * math.pi * p[1] * t) * 0.3 + math.sin(2.0 * math.pi * p[2] * t) * 0.2) * math.exp(-t * 22.0)
        thud = math.sin(2.0 * math.pi * (60.0 + 30.0 * math.exp(-t * 20.0)) * t) * math.exp(-t * 14.0) * 0.7
        v = rnd.uniform(-1.0, 1.0) * math.exp(-t * 90.0); y += 0.3 * (v - y)
        whir = math.sin(2.0 * math.pi * 210.0 * t) * 0.12 * (1.0 if t < 0.16 else math.exp(-(t - 0.16) * 40.0))
        out.append(clank + thud + y * 0.5 + whir)
    peak = max(1e-6, max(abs(v) for v in out)); return [v / peak * 0.75 for v in out]

def write(name, xs):
    w = wave.open(OUT + '/' + name, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in xs)); w.close(); print('wrote', name, round(len(xs) / SR, 2), 's')

# NOTHING PLAYS THIS ANY MORE. The bot's death is mixed from real recordings by the robot death
# tool instead: a synthesised falling cry read as comedy, which is not what a machine coming
# apart should sound like. The file is still written, because the synthesis is worth keeping
# readable and it costs nothing, but the game asks for robot_death_1 and robot_death_2.
write('robot_scream.wav', scream())
write('robot_strike.wav', strike())
write('robot_thump.wav', thump())
for k in (1, 2, 3): write('robot_step_%02d.wav' % k, step(seed=k * 7))
for k in (1, 2, 3): write('spark_crackle_%02d.wav' % k, crackle(seed=k))
