"""Write a face's settings-page image as src/pkjs/page_image.js.

The settings page is a single data: URL, so the image goes in as a PNG data
URI. It's made from, in order of preference:
- resources/images/unscaled/menu_icon.png (any RGBA image), trimmed to its
  visible part, or
- the strong red shape in resources/images/unscaled/background1.png (the
  same shape convert_images.py makes the menu icon from), cut out with soft
  edges.
It's scaled to fit MAX_W x MAX_H CSS px, at 2x for sharp phone screens.

Usage (from the repo root):
    uv run --with pillow --with numpy python3 -I shared/tools/page_image.py faces/<face>
"""
import base64
import io
import os
import sys

import numpy as np
from PIL import Image

MAX_W, MAX_H = 96, 52   # CSS px
SCALE = 2


def from_icon(path):
    img = Image.open(path).convert("RGBA")
    return img.crop(img.getchannel("A").getbbox())


def from_red_shape(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.float64)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mask = (r > 2 * g + 40) & (r > 2 * b + 40)   # convert_images.red_mask
    if not mask.any():
        sys.exit(f"{path} has no red shape (and there's no menu_icon.png)")
    shape = np.median(a[mask], axis=0)
    bg = np.median(a[~mask], axis=0)
    # Edge pixels are a blend of the shape and the background; how far R-G
    # is from the background's toward the shape's gives the coverage.
    redness = r - g
    lo, hi = bg[0] - bg[1], shape[0] - shape[1]
    alpha = np.clip((redness - lo) / (hi - lo), 0, 1)
    out = np.zeros(a.shape[:2] + (4,), dtype=np.uint8)
    out[..., :3] = np.round(shape).astype(np.uint8)
    out[..., 3] = np.round(alpha * 255).astype(np.uint8)
    img = Image.fromarray(out, "RGBA")
    return img.crop(img.getchannel("A").getbbox())


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    face = sys.argv[1]
    unscaled = os.path.join(face, "resources", "images", "unscaled")
    icon = os.path.join(unscaled, "menu_icon.png")
    if os.path.exists(icon):
        img, source = from_icon(icon), "menu_icon.png"
    else:
        img = from_red_shape(os.path.join(unscaled, "background1.png"))
        source = "background1.png's red shape"
    zoom = min(MAX_W / img.width, MAX_H / img.height)
    css_w, css_h = round(img.width * zoom), round(img.height * zoom)
    # Scale with premultiplied alpha so the edges don't darken.
    img = img.convert("RGBa").resize((css_w * SCALE, css_h * SCALE),
                                     Image.LANCZOS).convert("RGBA")
    buf = io.BytesIO()
    img.save(buf, "PNG", optimize=True)
    uri = "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()
    out = os.path.join(face, "src", "pkjs", "page_image.js")
    with open(out, "w") as f:
        f.write("// Written by shared/tools/page_image.py from "
                f"{source}: the settings-page image.\n")
        f.write(f"module.exports = {{ uri: '{uri}', width: {css_w}, "
                f"height: {css_h} }};\n")
    print(f"wrote {out} ({css_w}x{css_h} CSS px, {len(uri)} bytes)")


if __name__ == "__main__":
    main()
