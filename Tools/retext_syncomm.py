"""Replace the SYNCOMM wordmark on the sci-fi horror sheets with one of our own.

SYNCOMM is Synty's brand, printed three times on every colourway of the pack's main atlas: once in
the orange decal block, once in the tan one and once in the grey one, at the same offset inside each
1024-pixel block. It is set vertically, reading upward, as a HOLLOW wordmark -- each letter is an
outline a couple of pixels thick rather than a solid shape -- in a different colour per block.

THARSIS replaces it. Seven characters, same as SYNCOMM, so the wordmark keeps its length and its
place in the layout; and it is Martian geography, which is where "Amazonis" came from, so the two
read as the same company's naming rather than as two unrelated inventions.

No system font matches the pack's face -- every one on the machine was scored against the original S
and the best was still three quarters wrong -- so the letters are BUILT rather than set, to the
construction measured off the sheet. Tools/retext_glyphs holds the shapes and the reasoning. The S is
not drawn at all: SYNCOMM has one, so THARSIS's two are lifted from the sheet pixel for pixel.

  python Tools/retext_syncomm.py           writes a before-and-after comparison, changes nothing
  python Tools/retext_syncomm.py --apply   also writes the edited sheets ready for import

Nothing here touches the project's content. --apply writes new PNGs beside the exports; importing
them is a separate, deliberate step.
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    raise SystemExit('this needs numpy and pillow (pip install pillow numpy)')

import retext_glyphs

SCRATCH = 'C:/Users/jhand/AppData/Local/Temp/claude/C--Dev-Claude/f0f1126f-2fae-4098-9f9e-f7a1889d2c6d/scratchpad'
TEX = os.path.join(SCRATCH, 'tex')
SHEETS = ['T_PolygonSciFiHorror_%02d_A.png' % n for n in (1, 2, 3, 4)]
NEW_WORD = 'THARSIS'

# The three blocks, by the top-left corner of each 1024 tile. The wordmark sits at the same offset
# inside every one of them, measured off the grey block where the background is flattest.
BLOCKS = [(755, 470), (755, 1494), (1779, 1494)]
OFF_X, OFF_Y = 32, 26          # the wordmark's box within its block
BOX_W, BOX_H = 25, 207         # 25 across the strokes, 207 along the word
FONT_CANDIDATES = ['C:/Windows/Fonts/bahnschrift.ttf', 'C:/Windows/Fonts/AGENCYB.TTF',
                   'C:/Windows/Fonts/tahomabd.ttf']
SS = 4                          # render this many times over, then threshold: hard edges, no fuzz


def erode(mask):
    """True only where all four neighbours are also true -- one pixel off every edge."""
    m = mask
    out = m.copy()
    out[1:, :] &= m[:-1, :]
    out[:-1, :] &= m[1:, :]
    out[:, 1:] &= m[:, :-1]
    out[:, :-1] &= m[:, 1:]
    return out


def find_font():
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return path
    raise SystemExit('no suitable font found; tried ' + ', '.join(FONT_CANDIDATES))


def borrowed_s(arr):
    """The original's own S, hollow, straight off the sheet.

    Taken from the grey block, where the ink is white on a flat ground and the threshold is
    unambiguous. The glyph is identical in every colourway, so one reading serves them all."""
    x, y = BLOCKS[2][0] + OFF_X, BLOCKS[2][1] + OFF_Y
    strip = arr[y:y + BOX_H, x:x + BOX_W]
    lum = strip.astype(np.float32).mean(axis=2)
    horiz = np.rot90(lum > (lum.min() + lum.max()) * 0.5, -1)   # reads left to right
    return horiz[:, 0:25]


def ink_and_ground(arr, x, y):
    """The wordmark's own colour and the flat colour behind it, read off the sheet itself."""
    w = arr[y:y + BOX_H, x:x + BOX_W].reshape(-1, 3)
    cols, counts = np.unique(w, axis=0, return_counts=True)
    bg = cols[counts.argmax()]
    far = w[np.abs(w.astype(np.int16) - bg.astype(np.int16)).sum(axis=1) > 60]
    if len(far) == 0:
        return bg, bg
    fc, fn = np.unique(far, axis=0, return_counts=True)
    return fc[fn.argmax()], bg


def retext(img):
    """Every instance replaced, on a copy. Returns the new image and the boxes it touched."""
    arr = np.asarray(img.convert('RGB')).copy()
    keepS = borrowed_s(arr)
    boxes = []
    for bx, by in BLOCKS:
        x, y = bx + OFF_X, by + OFF_Y
        ink, bg = ink_and_ground(arr, x, y)
        # The word runs UP the sheet, so it is drawn along its length and turned a quarter turn.
        mask = retext_glyphs.word(NEW_WORD, BOX_H, borrowed={'S': keepS})
        mask = np.rot90(mask, 1)                       # now BOX_H tall, BOX_W wide, reading upward
        arr[y:y + BOX_H, x:x + BOX_W] = bg             # out with the old
        region = arr[y:y + BOX_H, x:x + BOX_W]
        region[mask] = ink                             # in with the new
        boxes.append((x, y))
    return Image.fromarray(arr), boxes


def comparison(before, after, boxes, path):
    """One picture: each instance as it was, then as it is, at three times size."""
    pad, zoom = 14, 3
    cw, ch = (BOX_W + 18) * zoom, (BOX_H + 18) * zoom
    out = Image.new('RGB', (len(boxes) * 2 * cw + (len(boxes) * 2 + 1) * pad, ch + 2 * pad + 22), (18, 18, 20))
    d = ImageDraw.Draw(out)
    try:
        label = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 13)
    except Exception:
        label = None
    for i, (x, y) in enumerate(boxes):
        for j, src in enumerate((before, after)):
            crop = src.crop((x - 9, y - 9, x + BOX_W + 9, y + BOX_H + 9)).resize((cw, ch), Image.NEAREST)
            px = pad + (i * 2 + j) * (cw + pad)
            out.paste(crop, (px, pad + 20))
            if label:
                d.text((px, 3), ('BEFORE', 'AFTER')[j], fill=(150, 200, 150) if j else (200, 160, 140), font=label)
    out.save(path)
    return path


src = os.path.join(TEX, SHEETS[0])
if not os.path.exists(src):
    raise SystemExit('export the sheets first: Tools/ue_remote --file Tools/export_texture_png')
before = Image.open(src).convert('RGB')
after, boxes = retext(before)
print('replaced %d instances of SYNCOMM with %s on %s' % (len(boxes), NEW_WORD, SHEETS[0]))
for x, y in boxes:
    print('   at x %4d  y %4d' % (x, y))
print('comparison: ' + comparison(before, after, boxes, os.path.join(TEX, 'syncomm_before_after.png')))

if '--apply' in sys.argv:
    for name in SHEETS:
        p = os.path.join(TEX, name)
        if not os.path.exists(p):
            print('   MISSING ' + name)
            continue
        out, _ = retext(Image.open(p).convert('RGB'))
        edited = os.path.join(TEX, name.replace('.png', '_THARSIS.png'))
        out.save(edited)
        print('   wrote ' + edited)
else:
    print('nothing written to the sheets: re-run with --apply once the comparison is approved')
