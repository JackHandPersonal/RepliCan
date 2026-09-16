"""Turn downloaded gunshot samples into the game's report files.

    python Tools/convert_samples.py

Reads RawAudio/samples/*.mp3|wav|ogg|flac (decoded with the miniaudio module), mixes to mono,
resamples to 22050 Hz, cuts to the first shot (the first sample over a tenth of the peak,
five milliseconds of run-in kept), caps the length, fades the tail, normalises, and writes
16-bit WAVs into RawAudio under the names UI/Weapons.json plays. Originals stay in samples/.

The mapping is here, by BigSoundBank id (all CC0 -- "free and royalty-free, commercial or not"):
    0438  .357 Magnum revolver        -> wep_pistol
    0437  Beretta M12 9 mm (an SMG)   -> wep_smg
    2853  rifle shot 1                -> wep_rifle
    0397  Winchester Magnum XTR       -> wep_sniper
    0532  shotgun, several shots      -> wep_shotgun (the first shot)
wep_heavy and wep_launcher stay synthesised until a sample turns up.
"""
import os, struct, wave, array, math
DRIVE = 2.4   # soft-clip drive: 1 = untouched, 3 = brick
import miniaudio

SRC = r'C:\Dev\Games\RepliCan\RawAudio\samples'
OUT = r'C:\Dev\Games\RepliCan\RawAudio'
SR = 22050
MAP = {'bsb_0438': ('wep_pistol', 0.70), 'bsb_0437': ('wep_smg', 0.55), 'bsb_2853': ('wep_rifle', 1.00),
       'bsb_0397': ('wep_sniper', 1.30), 'bsb_0532': ('wep_shotgun', 1.10)}


def decode(path):
    d = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16, nchannels=1, sample_rate=SR)
    return array.array('h', d.samples)


for stem, (name, max_seconds) in MAP.items():
    src = None
    for ext in ('.mp3', '.wav', '.ogg', '.flac'):
        if os.path.exists(os.path.join(SRC, stem + ext)): src = os.path.join(SRC, stem + ext); break
    if not src:
        print('missing', stem); continue
    pcm = decode(src)
    peak = max(1, max(abs(v) for v in pcm))
    # the first shot: the first sample over a tenth of the peak, with 5 ms of run-in
    start = next((i for i, v in enumerate(pcm) if abs(v) > peak * 0.1), 0)
    start = max(0, start - int(0.005 * SR))
    end = min(len(pcm), start + int(max_seconds * SR))
    cut = pcm[start:end]
    # a second shot inside the window (the shotgun file has several) is cut before it lands:
    # look for a new rise above 60% of peak after the first 150 ms of decay
    quiet_after = int(0.15 * SR)
    for i in range(quiet_after, len(cut)):
        if abs(cut[i]) > peak * 0.6 and max(abs(v) for v in cut[i - int(0.05 * SR):i]) < peak * 0.12:
            cut = cut[:i - int(0.01 * SR)]; break
    n = len(cut)
    out = [v / 32768.0 for v in cut]
    fade = min(n, int(0.12 * SR))
    for i in range(fade):
        out[n - 1 - i] *= i / float(fade)
    # LOUDER: a real shot is one huge transient over a quiet tail, so a peak-normalised take is
    # mostly quiet. Driving it through a soft clip lifts the body and tail toward the peak
    # (density, the way a limiter would) without the peak going anywhere.
    m = max(1e-6, max(abs(v) for v in out))
    out = [math.tanh((v / m) * DRIVE) / math.tanh(DRIVE) * 0.95 for v in out]
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767)) for v in out))
    print('wrote %-16s from %-14s %.2f s (of %.2f s decoded)' % (name + '.wav', os.path.basename(src), n / float(SR), len(pcm) / float(SR)))
