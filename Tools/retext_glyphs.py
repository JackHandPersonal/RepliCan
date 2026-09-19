"""The THARSIS wordmark, drawn to the same rules as the SYNCOMM one it replaces.

No system font matches the pack's face. Every one of them was scored against the original S -- render
it, hollow it, count the pixels that differ -- and the best was still three quarters wrong, with
dingbat fonts at the top of the table because an empty glyph beats a mismatched one. So the letters
are not set; they are BUILT, to the construction the original is built to, which is measurable off
the sheet:

  cap height   25 px, every letter, no descenders and no overshoot
  stem         6 px
  outline      the letter's own 2 px boundary -- these are hollow shapes, not filled ones
  corners      square, no radius
  width        22 to 27 px, near enough monospaced
  pitch        the word is 207 px for seven letters, so a shade over 30 px between starts

S is not drawn at all: SYNCOMM has one, so THARSIS's two are lifted from the sheet pixel for pixel.
The rest are polygons on the same grid, filled at eight times size, thresholded back to hard edges
and then hollowed by subtracting an eroded copy, which is the same operation that produced the
original's outline.

Imported by Tools/retext_syncomm; run directly to write a sheet of the glyphs for inspection.
"""
import os

import numpy as np
from PIL import Image, ImageDraw

CAP = 25          # cap height, and the nominal letter width
STEM = 6          # the weight of a stroke in the solid form
STROKE = 2        # the hollow outline's thickness
SS = 8            # supersample before thresholding, so diagonals land cleanly


def erode(m):
    o = m.copy()
    o[1:, :] &= m[:-1, :]
    o[:-1, :] &= m[1:, :]
    o[:, 1:] &= m[:, :-1]
    o[:, :-1] &= m[:, 1:]
    return o


def hollow(solid, stroke=STROKE):
    """The shape's own boundary, `stroke` pixels thick. This is the pack's whole look."""
    t = solid.copy()
    for _ in range(stroke):
        t = erode(t)
    return solid & ~t


# Each glyph is (width, [filled polygons], [holes]), in a box CAP tall. Coordinates are in final
# pixels; they are multiplied up before drawing so the diagonals are not staircased by the grid.
def _rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


C = CAP - 1
S6 = STEM
MID = (CAP - STEM) / 2.0          # a centred stem's left edge
GLYPHS = {
    'T': (25, [_rect(0, 0, 24, S6 - 1), _rect(MID, 0, MID + S6 - 1, C)], []),
    'H': (25, [_rect(0, 0, S6 - 1, C), _rect(25 - S6, 0, 24, C),
               _rect(S6 - 1, MID, 25 - S6, MID + S6 - 1)], []),
    'I': (13, [_rect((13 - S6) / 2.0, 0, (13 + S6) / 2.0 - 1, C)], []),
    # A: two slanted bars and a crossbar, which is how the face builds its M, N and Y -- straight
    # strokes, square ends, no pointed apex. The counter falls out of the three strokes rather than
    # being cut, so it is the right shape by construction.
    'A': (25, [[(0, C), (S6, C), (13, 0), (13 - S6, 0)],
               [(24, C), (24 - S6, C), (12, 0), (12 + S6, 0)],
               _rect(4.5, 14, 20.5, 14 + S6 - 1)], []),
    # R: stem, top bar, a right-hand stem down to the waist, the waist bar, and a straight leg off
    # the waist. The counter is the hole those four leave, six pixels deep like every other counter.
    'R': (25, [_rect(0, 0, S6 - 1, C), _rect(0, 0, 20, S6 - 1),
               _rect(21 - S6, 0, 20, 16), _rect(0, 17 - S6, 20, 16),
               [(13, 16), (19, 16), (25, C), (18.5, C)]], []),
}


def glyph(ch):
    """One hollow letter as a boolean mask, CAP tall."""
    w, shapes, holes = GLYPHS[ch]
    img = Image.new('L', (w * SS, CAP * SS), 0)
    d = ImageDraw.Draw(img)
    for poly in shapes:
        d.polygon([(x * SS, y * SS) for x, y in poly], fill=255)
    for poly in holes:
        d.polygon([(x * SS, y * SS) for x, y in poly], fill=0)
    solid = np.asarray(img.resize((w, CAP), Image.BILINEAR)) > 110
    return hollow(solid)


def word(letters, length, borrowed=None):
    """The whole wordmark as a mask `length` px long and CAP tall.

    borrowed maps a character to a ready-made hollow mask lifted off the sheet, which is how S keeps
    the original's exact pixels instead of being redrawn."""
    borrowed = borrowed or {}
    masks = [borrowed[ch] if ch in borrowed else glyph(ch) for ch in letters]
    out = np.zeros((CAP, length), dtype=bool)
    # Spread the letters so the first starts at 0 and the last ends at the far edge, which is how
    # the original sits in its box.
    span = length - masks[-1].shape[1]
    for i, m in enumerate(masks):
        x = int(round(i * span / float(max(1, len(masks) - 1))))
        w = min(m.shape[1], length - x)
        out[:, x:x + w] |= m[:, :w]
    return out


if __name__ == '__main__':
    sheet = Image.new('RGB', (len(GLYPHS) * 32 + 8, CAP + 16), (24, 24, 26))
    for i, ch in enumerate(sorted(GLYPHS)):
        m = glyph(ch)
        g = Image.fromarray((m * 255).astype('uint8')).convert('RGB')
        sheet.paste(g, (8 + i * 32, 8))
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'glyphs_preview.png')
    sheet.resize((sheet.width * 6, sheet.height * 6), Image.NEAREST).save(out)
    print('wrote ' + out)
