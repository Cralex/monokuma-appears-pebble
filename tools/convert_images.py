"""Scale and color-convert the foreground images for emery.

Reads resources/images/unscaled/frontNN.png (680x680 full color) and writes
resources/images/frontNN.png (200x172, Pebble 64-color palette).

Scaling: each image is zoomed on its own so Monokuma is as large as
possible while staying fully visible (4 px of headroom), centered
horizontally and anchored to the bottom edge, where it slides in.

Color: a port of the default path of the TomHol PhotoWatch converter
(https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html),
a later version of Pebble Color Optimizer
(https://github.com/czmanix/pebble-color-optimizer).
With default settings (gamma 1, brightness/contrast/saturation/sharpen 0)
no adjustments apply; each pixel is matched to the nearest color in the
"sun" palette (how the Pebble Time 2 display actually renders each color),
Atkinson error diffusion spreads the error, and the output gets the
matching Pebble color at the same palette index.

Usage (from the repo root):
    uv run --with pillow --with numpy python3 -I tools/convert_images.py

The DEFINED and SUN palettes and the dithering method come from
Pebble Color Optimizer, used under the MIT License:

    Copyright (c) 2026 czmanix

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
"""
import os
import sys

import numpy as np
from PIL import Image

FG_W, FG_H = 200, 172
NUM_IMAGES = 16
BG_W, BG_H = 200, 228   # background is full screen, used unscaled
ICON_SIZE = 25          # SDK maximum for menuIcon resources
TOP_MARGIN = 4          # px of black kept above Monokuma's head
SUBJECT_THRESHOLD = 24  # pixels brighter than this count as Monokuma

# Pebble 64-color palette, in the converter's order ("defined").
DEFINED = [
    (0, 0, 0), (0, 0, 85), (0, 0, 170), (85, 0, 0),
    (0, 0, 255), (85, 0, 85), (85, 0, 170), (0, 85, 0),
    (170, 0, 85), (0, 85, 170), (170, 0, 170), (85, 85, 0),
    (255, 0, 0), (0, 85, 255), (170, 0, 255), (85, 85, 85),
    (0, 170, 0), (170, 85, 0), (85, 85, 255), (255, 0, 255),
    (0, 170, 85), (170, 85, 85), (0, 170, 170), (170, 85, 170),
    (170, 85, 255), (85, 170, 85), (255, 85, 85), (85, 170, 170),
    (255, 85, 170), (0, 255, 0), (170, 170, 0), (85, 170, 255),
    (0, 255, 170), (170, 170, 170), (85, 255, 0), (255, 170, 0),
    (0, 255, 255), (170, 170, 255), (85, 255, 85), (255, 170, 85),
    (85, 255, 255), (255, 170, 255), (170, 255, 85), (170, 255, 170),
    (255, 255, 0), (170, 255, 255), (255, 255, 85), (255, 255, 170),
    (170, 255, 0), (170, 170, 85), (255, 255, 255), (85, 255, 170),
    (255, 85, 255), (85, 170, 0), (255, 0, 85), (170, 0, 0),
    (0, 170, 255), (255, 0, 170), (0, 85, 85), (255, 170, 170),
    (0, 255, 85), (255, 85, 0), (85, 85, 170), (85, 0, 255),
]

# How each DEFINED color looks on the real display ("sun"), same order.
SUN = [
    (39, 32, 32), (38, 52, 74), (27, 65, 104), (91, 36, 35),
    (3, 72, 122), (88, 52, 71), (86, 69, 99), (63, 73, 50),
    (136, 63, 81), (62, 108, 128), (136, 80, 112), (108, 82, 62),
    (158, 45, 50), (44, 111, 141), (131, 89, 128), (103, 91, 90),
    (87, 109, 69), (156, 95, 80), (108, 117, 140), (163, 95, 132),
    (82, 123, 105), (146, 99, 97), (83, 132, 134), (141, 107, 118),
    (149, 123, 142), (128, 130, 113), (169, 104, 103), (122, 138, 138),
    (174, 111, 124), (97, 134, 91), (157, 121, 92), (114, 136, 148),
    (95, 154, 143), (151, 141, 136), (139, 144, 107), (186, 130, 100),
    (101, 163, 162), (151, 141, 136), (131, 141, 122), (181, 135, 117),
    (137, 163, 158), (184, 157, 148), (162, 139, 119), (167, 164, 155),
    (194, 151, 116), (167, 164, 155), (185, 154, 131), (184, 157, 148),
    (175, 145, 107), (162, 139, 119), (188, 169, 166), (133, 155, 141),
    (170, 124, 145), (126, 117, 84), (158, 66, 83), (128, 40, 44),
    (79, 142, 155), (164, 86, 117), (65, 92, 95), (182, 145, 141),
    (98, 145, 120), (174, 91, 75), (107, 107, 120), (85, 83, 125),
]

# Atkinson: 1/8 of the error to each of these (dx, dy) neighbors.
ATKINSON = [(1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)]


def scale(src):
    """Flatten onto black and zoom each image so Monokuma fills the frame.

    Finds the box around everything brighter than the black background,
    then picks the largest zoom that fits that box in FG_W x FG_H with
    TOP_MARGIN px above it, centered on Monokuma horizontally and anchored
    to the bottom edge. Areas outside the original are filled with black.
    """
    rgba = src.convert("RGBA")
    flat = Image.new("RGB", rgba.size, (0, 0, 0))
    flat.paste(rgba, mask=rgba.getchannel("A"))

    mask = np.asarray(flat).max(axis=2) > SUBJECT_THRESHOLD
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        sys.exit("image has no visible subject")
    x0, x1 = xs.min(), xs.max() + 1
    y0, y1 = ys.min(), ys.max() + 1

    zoom = min(FG_W / (x1 - x0), (FG_H - TOP_MARGIN) / (y1 - y0))
    crop_w, crop_h = FG_W / zoom, FG_H / zoom
    if crop_w <= flat.width:
        # Center on Monokuma, but don't pan past the image's sides.
        left = min(max((x0 + x1) / 2 - crop_w / 2, 0), flat.width - crop_w)
    else:
        left = (flat.width - crop_w) / 2
    top = y1 - crop_h

    # Pad with black so a crop box past the image edges still works.
    pad = int(max(-left, -top, 0)) + 1
    padded = Image.new("RGB", (flat.width + 2 * pad, flat.height + 2 * pad))
    padded.paste(flat, (pad, pad))
    box = (left + pad, top + pad, left + crop_w + pad, y1 + pad)
    return padded.resize((FG_W, FG_H), Image.LANCZOS, box=box)


def convert(img):
    """Atkinson-dither against SUN, output DEFINED colors (palette mode)."""
    sun = np.array(SUN, dtype=np.float64)
    work = np.asarray(img, dtype=np.float64).copy()
    h, w, _ = work.shape
    idx = np.zeros((h, w), dtype=np.uint8)
    for y in range(h):
        for x in range(w):
            px = work[y, x]
            # First minimum wins, like the converter's strict "<" search.
            best = int(np.argmin(((sun - px) ** 2).sum(axis=1)))
            idx[y, x] = best
            err = (px - sun[best]) * 0.125
            for dx, dy in ATKINSON:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and ny < h:
                    work[ny, nx] += err
    out = Image.fromarray(idx, mode="P")
    out.putpalette([c for rgb in DEFINED for c in rgb])
    return out


def flatten(src):
    rgba = src.convert("RGBA")
    flat = Image.new("RGB", rgba.size, (0, 0, 0))
    flat.paste(rgba, mask=rgba.getchannel("A"))
    return flat


def red_mask(img):
    """Boolean mask of the background's red shape."""
    a = np.asarray(img, dtype=np.int16)
    return (a[..., 0] > 2 * a[..., 1] + 40) & (a[..., 0] > 2 * a[..., 2] + 40)


def menu_icon(bg):
    """Build the launcher icon from the background's red shape.

    The shape is cropped, scaled to fit ICON_SIZE x ICON_SIZE, and centered
    on a transparent square. Edges are anti-aliased with Pebble's four alpha
    levels. The fill is the Pebble color the converter picks for the
    original's average red.
    """
    flat = flatten(bg)
    mask = red_mask(flat)
    ys, xs = np.nonzero(mask)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1

    sun = np.array(SUN, dtype=np.float64)
    avg = np.asarray(flat, dtype=np.float64)[mask].mean(axis=0)
    fill = DEFINED[int(np.argmin(((sun - avg) ** 2).sum(axis=1)))]

    zoom = ICON_SIZE / max(x1 - x0, y1 - y0)
    w = max(1, round((x1 - x0) * zoom))
    h = max(1, round((y1 - y0) * zoom))
    shape = Image.fromarray((mask * 255).astype(np.uint8)).crop((x0, y0, x1, y1))
    alpha = np.asarray(shape.resize((w, h), Image.LANCZOS), dtype=np.float64)
    alpha = (np.round(alpha / 85) * 85).astype(np.uint8)  # 0, 85, 170, 255

    icon = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), fill + (0,))
    layer = Image.new("RGBA", (w, h), fill + (255,))
    layer.putalpha(Image.fromarray(alpha))
    icon.paste(layer, ((ICON_SIZE - w) // 2, (ICON_SIZE - h) // 2))
    return icon


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    images = os.path.join(root, "resources", "images")
    for n in range(1, NUM_IMAGES + 1):
        name = f"front{n:02d}.png"
        src = Image.open(os.path.join(images, "unscaled", name))
        convert(scale(src)).save(os.path.join(images, name), optimize=True)
        print("wrote", name)

    bg = Image.open(os.path.join(images, "unscaled", "background1.png"))
    if bg.size != (BG_W, BG_H):
        sys.exit(f"background1.png must be {BG_W}x{BG_H}, got {bg.size}")
    convert(flatten(bg)).save(os.path.join(images, "background1.png"),
                              optimize=True)
    print("wrote background1.png")

    menu_icon(bg).save(os.path.join(images, "menu_icon.png"), optimize=True)
    print("wrote menu_icon.png")


if __name__ == "__main__":
    main()
