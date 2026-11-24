"""Work out a crop window for each of Usami's poses and save it in convert.json.

The originals are full-body sprites drawn at the same scale, but not all at
the same place in the frame (front04 has the wand and arms up, so Usami sits
lower). To show Usami at the same size in every pose, each image gets the
same size of window, placed relative to a landmark every pose has: the heart
brooch on the collar (the lowest cluster of red-pink pixels; the wand's gem
is always higher up).

The window runs from ABOVE px above the brooch (just over the ear tips) to
BELOW px under it (the top of the skirt, so the bottom is cropped off the way
Monokuma's originals stop at his belly), centered on the brooch, give or take
a pose's SHIFT_X. Anything outside it, such as the top of a raised wand or the
wing tips, is cut off.
shared/tools/convert_images.py scales the window to each platform's
foreground size.

Usage (from the repo root):
    uv run --with pillow --with numpy --with scipy python3 -I faces/usami/tools/find_crops.py
"""
import json
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

ABOVE = 440        # window top, in source px above the brooch
BELOW = 170        # window bottom, in source px below the brooch
MIN_CLUSTER = 40   # ignore red specks smaller than this (px)

# Per-pose nudges, in source px: positive moves Usami right in the frame
# (the window left). front05 leans to our left, leaving the right side empty;
# 53 px is about 15 px on emery, just short of cutting the right wing tip.
SHIFT_X = {"front05.png": 53}


def find_brooch(path):
    a = np.asarray(Image.open(path).convert("RGBA")).astype(int)
    r, g, b, alpha = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    red = ((alpha > 200) & (r > 170) & (g < 110) & (b > 50) & (b < 170)
           & (r - g > 100))
    labels, count = ndimage.label(red)
    best = None
    for i in range(1, count + 1):
        ys, xs = np.nonzero(labels == i)
        if len(xs) >= MIN_CLUSTER and (best is None or ys.mean() > best[1]):
            best = (xs.mean(), ys.mean())
    if best is None:
        sys.exit(f"no brooch found in {path}")
    return round(best[0]), round(best[1])


def main():
    face = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    unscaled = os.path.join(face, "resources", "images", "unscaled")
    config_path = os.path.join(face, "convert.json")
    with open(config_path) as f:
        config = json.load(f)

    crops = {}
    n = 1
    while os.path.exists(os.path.join(unscaled, f"front{n:02d}.png")):
        name = f"front{n:02d}.png"
        x, y = find_brooch(os.path.join(unscaled, name))
        crops[name] = {"center_x": x - SHIFT_X.get(name, 0),
                       "top": y - ABOVE, "bottom": y + BELOW}
        print(f"{name}: brooch at ({x}, {y})")
        n += 1
    config["crops"] = crops
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")
    print(f"wrote {len(crops)} crops to convert.json")


if __name__ == "__main__":
    main()
