"""Grime for the floors (Tools/make_grime_decals imports these): a 512 tileable value-noise sheet the
whole-floor stain decal reads through world position, and six 256 RGBA stain stamps for the scatter --
an oil pool, a dried-puddle ring, scuff streaks, a rust bleed, drag marks, a dust patch. Alpha is the
stain's shape; colour is its own. Seeded, so a re-run gives the same art. Plain Python: the PNGs are
written by hand (zlib + struct), no imaging library needed.
    python Tools/make_grime_textures.py
"""
import math, random, struct, zlib
OUT = 'C:/Dev/Games/RepliCan/RawArt'
rnd = random.Random(4173)

def png(path, w, h, rows):
    """rows: list of bytes, each 4*w RGBA."""
    raw = b''.join(b'\x00' + r for r in rows)
    def chunk(tag, data): return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def lattice(n): return [[rnd.random() for _ in range(n)] for _ in range(n)]
def smooth(t): return t * t * (3.0 - 2.0 * t)
def sample(a, n, u, v):
    x = u * n; y = v * n; x0 = math.floor(x); y0 = math.floor(y)
    fx = smooth(x - x0); fy = smooth(y - y0)
    i0 = int(x0) % n; j0 = int(y0) % n; i1 = (i0 + 1) % n; j1 = (j0 + 1) % n
    top = a[j0][i0] + (a[j0][i1] - a[j0][i0]) * fx; bot = a[j1][i0] + (a[j1][i1] - a[j1][i0]) * fx
    return top + (bot - top) * fy
def clamp01(v): return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)

# ---- the floor noise: 512, tileable, three octaves, low contrast
S = 512; L1, L2, L3 = lattice(5), lattice(11), lattice(23)
rows = []
for y in range(S):
    r = bytearray()
    for x in range(S):
        u = x / S; v = y / S
        n = 0.5 * sample(L1, 5, u, v) + 0.32 * sample(L2, 11, u, v) + 0.18 * sample(L3, 23, u, v)
        g = int(255 * clamp01(n)); r += bytes((g, g, g, 255))
    rows.append(bytes(r))
png(OUT + '/T_FloorNoise.png', S, S, rows); print('wrote T_FloorNoise.png')

# ---- the stamps: 256 RGBA. Each takes (u, v) in -0.5..0.5 about the centre and an edge wobble e in 0..1.
S = 256
def stamp(name, fn):
    E = lattice(6); rows = []
    for y in range(S):
        r = bytearray()
        for x in range(S):
            u = (x + 0.5) / S - 0.5; v = (y + 0.5) / S - 0.5
            e = sample(E, 6, (x + 0.5) / S, (y + 0.5) / S)
            cr, cg, cb, a = fn(u, v, e)
            r += bytes((int(cr), int(cg), int(cb), int(255 * clamp01(a))))
        rows.append(bytes(r))
    png(OUT + '/' + name + '.png', S, S, rows); print('wrote', name + '.png')

def oil(u, v, e):        # a dark pool, its edge broken by the noise, densest in the middle
    r = math.sqrt(u * u * 1.3 + v * v) * 2.0 + (e - 0.5) * 0.45; a = clamp01((1.0 - r) * 2.2); return (16, 14, 12, a * a * 0.95)
def puddle(u, v, e):     # a dried ring, the middle nearly clear
    r = math.sqrt(u * u + v * v * 1.15) * 2.0 + (e - 0.5) * 0.3; ring = clamp01(1.0 - abs(r - 0.78) * 6.0); inner = clamp01((0.7 - r) * 1.5) * 0.18; return (40, 36, 30, ring * 0.75 + inner)
def scuffs(u, v, e):     # soft streaks along x
    a = 0.0
    for k in (-0.28, -0.1, 0.06, 0.22, 0.36):
        d = abs(v - k - (e - 0.5) * 0.08); ln = clamp01(1.0 - abs(u * 2.0) * (0.9 + e * 0.6)); a += clamp01(1.0 - d * 22.0) * ln * 0.35
    return (62, 60, 56, clamp01(a))
def rust(u, v, e):       # an orange-brown patch with dribbles running down (+v)
    body = clamp01((1.0 - (math.sqrt(u * u * 2.2 + (v + 0.15) ** 2 * 3.0) * 2.0 + (e - 0.5) * 0.4)) * 2.0); drib = 0.0
    for k in (-0.14, -0.02, 0.11):
        d = abs(u - k - (e - 0.5) * 0.05); drib += clamp01(1.0 - d * 30.0) * clamp01((v + 0.1) * 2.2) * clamp01((0.5 - v) * 2.0) * 0.7
    return (96, 52, 22, clamp01(body + drib) * 0.85)
def drag(u, v, e):       # two long parallel lines, worn through in places
    a = 0.0
    for k in (-0.07, 0.07):
        d = abs(v - k); a += clamp01(1.0 - d * 40.0) * clamp01(1.0 - abs(u * 2.0) * 1.05) * (0.45 + e * 0.5)
    return (30, 28, 26, clamp01(a) * 0.7)
def dust(u, v, e):       # a big soft light patch
    r = math.sqrt(u * u + v * v) * 2.0 + (e - 0.5) * 0.5; a = clamp01((1.0 - r) * 1.4); return (122, 116, 104, a * 0.32)

for name, fn in (('T_Grime_01', oil), ('T_Grime_02', puddle), ('T_Grime_03', scuffs), ('T_Grime_04', rust), ('T_Grime_05', drag), ('T_Grime_06', dust)):
    stamp(name, fn)
