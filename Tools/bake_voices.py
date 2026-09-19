"""Bake NPC voice lines with Piper TTS.

Runs as a pre-build step (see Source/*.Target.cs -> Tools/bake_voices.cmd) and
can be run by hand:

    <piper-venv>/Scripts/python.exe Tools/bake_voices.py [--force] [--only Orvan]

Input:  <Project>/Conversations/<Character>.json and <Project>/Sequences/<Name>.json
        (a sequence's "voices" block maps its actor ids to voice settings;
        its "say" steps bake to Voice/Sequences/<Name>/<stepIndex>.wav)
        A tree may carry a "voice" block (per character):
            "voice": { "model": "en_US-ryan-medium", "length_scale": 1.0,
                       "noise_scale": 0.667, "noise_w_scale": 0.8, "speaker": 0,
                       "volume": 1.0 }
        Trees without a "voice" block are skipped.
        Line text may contain performance markup (stripped for display by the
        game, see ConversationData.h):
            [pause 0.6]   exact silence in seconds ([pause] = 0.5)
            *emphasis*    spoken slower and a touch more varied
            ~aside~       spoken quicker and flatter
        "..." and "--" also read as short natural pauses.
Output: <Project>/Conversations/Voice/<Character>/<node>_<lineIndex>.wav
        (16-bit mono PCM at the model's sample rate) and
        <Project>/Conversations/Voice/manifest.json
        One manifest entry per line: the text and voice settings hash it was
        baked from, when it was baked, and the source file's modification
        time. A line is rebaked only when that hash changes (or the WAV is
        missing), so touching one line rebakes one file.
"""
import argparse
import hashlib
import json
import os
import re
import struct
import sys
import time
import wave
from datetime import datetime, timezone

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONV_DIR = os.path.join(PROJECT_DIR, "Conversations")
SEQ_DIR = os.path.join(PROJECT_DIR, "Sequences")
VOICE_DIR = os.path.join(CONV_DIR, "Voice")
MANIFEST_PATH = os.path.join(VOICE_DIR, "manifest.json")
VOICES_DIR = os.environ.get("PIPER_VOICES", r"C:\Dev\Tools\piper-voices")

DEFAULT_VOICE = {
    "model": "en_US-lessac-medium",
    "length_scale": 1.0,
    "noise_scale": 0.667,
    "noise_w_scale": 0.8,
    "speaker": 0,
    "volume": 1.0,
}
# How the markup bends the base settings.
EMPHASIS = {"length_scale": 1.18, "noise_scale": +0.08, "noise_w_scale": +0.15}
ASIDE = {"length_scale": 0.86, "noise_scale": -0.15, "noise_w_scale": -0.2}
SEGMENT_GAP_SECONDS = 0.06      # breath between adjacent spoken segments
TRIM_THRESHOLD = 0.004          # leading/trailing silence trim (float amplitude)
TRIM_KEEP_SECONDS = 0.04        # keep this much of the trimmed silence

TOKEN_RE = re.compile(r"\[pause(?:\s+([0-9]*\.?[0-9]+))?\]|\*([^*]+)\*|~([^~]+)~")


def strip_markup(text):
    """What the player reads: markup removed, whitespace tidied."""
    out = TOKEN_RE.sub(lambda m: (m.group(2) or m.group(3) or ""), text)
    return re.sub(r"\s{2,}", " ", out).strip()


def parse_segments(text):
    """Split a marked-up line into [(kind, payload)] where kind is
    'say' (payload: (text, style)) or 'pause' (payload: seconds)."""
    segments = []
    pos = 0
    for m in TOKEN_RE.finditer(text):
        plain = text[pos:m.start()].strip()
        if plain:
            segments.append(("say", (plain, "normal")))
        if m.group(0).startswith("[pause"):
            segments.append(("pause", float(m.group(1)) if m.group(1) else 0.5))
        elif m.group(2) is not None:
            segments.append(("say", (m.group(2).strip(), "emphasis")))
        else:
            segments.append(("say", (m.group(3).strip(), "aside")))
        pos = m.end()
    tail = text[pos:].strip()
    if tail:
        segments.append(("say", (tail, "normal")))
    return segments


def speakable(text):
    """Normalise punctuation espeak handles poorly."""
    return (text.replace("\u2014", ", ").replace("--", ", ")
                .replace("\u2019", "'").replace("\u201c", '"').replace("\u201d", '"'))


def line_key(character, node, index):
    return "%s/%s/%d" % (character, node, index)


def line_hash(voice, text):
    payload = json.dumps({"voice": voice, "text": text}, sort_keys=True)
    return hashlib.sha1(payload.encode("utf-8")).hexdigest()[:16]


def iso(ts):
    return datetime.fromtimestamp(ts, timezone.utc).astimezone().isoformat(timespec="seconds")


def load_manifest():
    try:
        with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_manifest(manifest):
    os.makedirs(VOICE_DIR, exist_ok=True)
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")


def collect_lines():
    """-> {key: (character, node, index, raw_text, voice, source_path, source_mtime)}"""
    lines = {}
    if not os.path.isdir(CONV_DIR):
        return lines
    for name in sorted(os.listdir(CONV_DIR)):
        if not name.lower().endswith(".json"):
            continue
        path = os.path.join(CONV_DIR, name)
        character = os.path.splitext(name)[0]
        with open(path, "r", encoding="utf-8") as f:
            tree = json.load(f)
        voice_cfg = tree.get("voice")
        if not isinstance(voice_cfg, dict):
            print("bake_voices: %s has no \"voice\" block, skipped" % name)
            continue
        voice = dict(DEFAULT_VOICE)
        voice.update(voice_cfg)
        mtime = os.path.getmtime(path)
        for node_id, node in (tree.get("nodes") or {}).items():
            text = node.get("text")
            texts = text if isinstance(text, list) else [text]
            for i, t in enumerate(texts):
                if not isinstance(t, str) or not t.strip():
                    continue
                lines[line_key(character, node_id, i)] = (character, node_id, i, t, voice, path, mtime, "%s/%s_%d.wav" % (character, node_id, i))
    # Sequences: every "say" step with a voiced actor.
    if os.path.isdir(SEQ_DIR):
        for name in sorted(os.listdir(SEQ_DIR)):
            if not name.lower().endswith(".json"):
                continue
            path = os.path.join(SEQ_DIR, name)
            seq = os.path.splitext(name)[0]
            with open(path, "r", encoding="utf-8") as f:
                tree = json.load(f)
            voices = tree.get("voices") or {}
            mtime = os.path.getmtime(path)
            for i, step in enumerate(tree.get("steps") or []):
                say = step.get("say") if isinstance(step, dict) else None
                if not isinstance(say, dict):
                    continue
                who, text = say.get("who"), say.get("text")
                if who not in voices or not isinstance(text, str) or not text.strip():
                    continue
                voice = dict(DEFAULT_VOICE)
                voice.update(voices[who])
                key = "Sequences/%s/%d" % (seq, i)
                lines[key] = ("Sequences/" + seq, str(i), 0, text, voice, path, mtime, "Sequences/%s/%d.wav" % (seq, i))
    return lines


class Baker:
    def __init__(self):
        self.voices = {}

    def voice(self, model):
        if model not in self.voices:
            from piper import PiperVoice
            onnx = os.path.join(VOICES_DIR, model + ".onnx")
            if not os.path.isfile(onnx):
                raise FileNotFoundError("voice model not found: %s (set PIPER_VOICES or run "
                                        "python -m piper.download_voices --data-dir <dir> %s)" % (onnx, model))
            t0 = time.time()
            self.voices[model] = PiperVoice.load(onnx)
            print("bake_voices: loaded %s (%.1fs)" % (model, time.time() - t0))
        return self.voices[model]

    def synth(self, voice_cfg, text, style):
        import numpy as np
        from piper.config import SynthesisConfig
        v = self.voice(voice_cfg["model"])
        ls, ns, nw = voice_cfg["length_scale"], voice_cfg["noise_scale"], voice_cfg["noise_w_scale"]
        bend = EMPHASIS if style == "emphasis" else ASIDE if style == "aside" else None
        if bend:
            ls *= bend["length_scale"]
            ns = max(0.05, ns + bend["noise_scale"])
            nw = max(0.05, nw + bend["noise_w_scale"])
        cfg = SynthesisConfig(speaker_id=voice_cfg.get("speaker") or None, length_scale=ls,
                              noise_scale=ns, noise_w_scale=nw, volume=voice_cfg["volume"])
        chunks = list(v.synthesize(speakable(text), syn_config=cfg))
        if not chunks:
            return np.zeros(0, dtype=np.float32), v.config.sample_rate
        rate = chunks[0].sample_rate
        pieces = []
        for c in chunks:
            a = c.audio_float_array.astype(np.float32)
            if c.sample_channels > 1:
                a = a.reshape(-1, c.sample_channels).mean(axis=1)
            pieces.append(trim(a, rate))
        gap = np.zeros(int(rate * 0.18), dtype=np.float32)   # sentence boundary
        out = pieces[0]
        for p in pieces[1:]:
            out = np.concatenate([out, gap, p])
        return out, rate

    def bake_line(self, voice_cfg, raw_text):
        import numpy as np
        rate = None
        out = []
        for kind, payload in parse_segments(raw_text):
            if kind == "pause":
                if rate is None:
                    rate = self.voice(voice_cfg["model"]).config.sample_rate
                out.append(np.zeros(int(rate * payload), dtype=np.float32))
            else:
                audio, rate = self.synth(voice_cfg, *payload)
                if out and out[-1].size and not is_silence(out[-1]):
                    out.append(np.zeros(int(rate * SEGMENT_GAP_SECONDS), dtype=np.float32))
                out.append(audio)
        audio = np.concatenate(out) if out else np.zeros(0, dtype=np.float32)
        # Tail so the last word isn't clipped by an eager stop.
        audio = np.concatenate([audio, np.zeros(int(rate * 0.12), dtype=np.float32)])
        return audio, rate


def pitch_shift(a, rate, factor, droop=0.0):
    """Resample ("pitch": 0.8 in a voice block plays the line at four fifths speed: a fifth of an
    octave down, and slower with it). "droop" lowers it further across the line -- 0.06 is six
    per cent lower at the end than the start -- for a voice that sinks as it speaks."""
    import numpy as np
    if a.size == 0 or factor <= 0.0:
        return a
    n = a.size
    steps = np.linspace(factor, factor * (1.0 - droop), int(n / factor) + 1)
    pos = np.cumsum(steps)
    pos = pos[pos < n - 1]
    return np.interp(pos, np.arange(n), a).astype(np.float32)


def robotize(a, rate):
    """A bad speaker on a worse voice board ("fx": "robot" in a voice block): ring-modulated at
    52 Hz, held down to about nine kilohertz, five bits deep, band-limited like a small cone,
    overdriven, with a squelch of static at each end. The point is that it should not sound good."""
    import numpy as np
    if a.size == 0:
        return a
    t = np.arange(a.size) / float(rate)
    y = a * (0.55 + 0.45 * np.sign(np.sin(2.0 * np.pi * 38.0 * t)))   # the chop: the classic robot flutter, slow enough to mourn
    hold = max(1, int(rate / 9000))                                     # sample-and-hold: aliasing grit
    y = np.repeat(y[::hold], hold)[:a.size]
    y = np.round(y * 24.0) / 24.0                                       # about five bits
    lp = np.convolve(y, np.ones(5) / 5.0, mode="same")                  # a small speaker: no top
    hp = lp - np.convolve(lp, np.ones(64) / 64.0, mode="same")          # and no bottom
    y = np.tanh(hp * 2.6) * 0.8                                         # overdriven
    n = int(rate * 0.09)
    rng = np.random.default_rng(7)
    burst = rng.uniform(-1.0, 1.0, n) * np.linspace(1.0, 0.0, n) * 0.35   # the squelch opening and closing
    return np.concatenate([burst, y, burst[::-1]]).astype(np.float32)


def is_silence(a):
    import numpy as np
    return a.size == 0 or float(np.max(np.abs(a))) < TRIM_THRESHOLD


def trim(a, rate):
    import numpy as np
    loud = np.nonzero(np.abs(a) > TRIM_THRESHOLD)[0]
    if loud.size == 0:
        return a[:0]
    keep = int(rate * TRIM_KEEP_SECONDS)
    start = max(0, int(loud[0]) - keep)
    end = min(a.size, int(loud[-1]) + keep)
    return a[start:end]


def write_wav(path, audio, rate):
    import numpy as np
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pcm = np.clip(audio * 32767.0, -32768, 32767).astype("<i2").tobytes()
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="rebake every line")
    ap.add_argument("--only", help="only this character")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    t0 = time.time()
    lines = collect_lines()
    manifest = load_manifest()
    todo = []
    for key, (character, node, index, raw, voice, src, mtime, rel) in lines.items():
        if args.only and character != args.only:
            continue
        h = line_hash(voice, raw)
        entry = manifest.get(key)
        wav_path = os.path.join(VOICE_DIR, rel.replace("/", os.sep))
        if not args.force and entry and entry.get("hash") == h and os.path.isfile(wav_path):
            continue
        todo.append((key, character, node, index, raw, voice, src, mtime, rel, h, wav_path))

    # Stale entries: lines that no longer exist.
    stale = [k for k in manifest if k not in lines and (not args.only or k.startswith(args.only + "/"))]
    for k in stale:
        wav_path = os.path.join(VOICE_DIR, manifest[k].get("wav", "").replace("/", os.sep))
        if wav_path and os.path.isfile(wav_path):
            if not args.dry_run:
                os.remove(wav_path)
        print("bake_voices: removed stale %s" % k)
        if not args.dry_run:
            del manifest[k]

    if not todo:
        if stale and not args.dry_run:
            save_manifest(manifest)
        print("bake_voices: %d lines up to date (%.2fs)" % (len(lines), time.time() - t0))
        return 0

    print("bake_voices: %d of %d lines need baking" % (len(todo), len(lines)))
    if args.dry_run:
        for item in todo:
            print("  would bake %s" % item[0])
        return 0

    baker = Baker()
    for key, character, node, index, raw, voice, src, mtime, rel, h, wav_path in todo:
        t1 = time.time()
        audio, rate = baker.bake_line(voice, raw)
        if voice.get("pitch"):
            audio = pitch_shift(audio, rate, float(voice["pitch"]), float(voice.get("droop", 0.0)))
        if voice.get("fx") == "robot":
            audio = robotize(audio, rate)
        write_wav(wav_path, audio, rate)
        seconds = audio.size / float(rate)
        manifest[key] = {
            "text": raw,
            "display": strip_markup(raw),
            "hash": h,
            "wav": rel,
            "seconds": round(seconds, 3),
            "sample_rate": rate,
            "voice": voice["model"],
            "baked": iso(time.time()),
            "source": os.path.basename(src),
            "source_modified": iso(mtime),
        }
        print("  baked %-28s %5.2fs audio  (%.1fs)" % (key, seconds, time.time() - t1))
        save_manifest(manifest)   # incremental: a crash mid-run keeps what's done
    print("bake_voices: done, %d baked (%.1fs)" % (len(todo), time.time() - t0))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # never break the build over a voice line
        print("bake_voices: FAILED: %s" % e)
        sys.exit(0)
