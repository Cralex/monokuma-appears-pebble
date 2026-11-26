"""Draw Usami's gem as a sharp, full-size replica of tools/gem_reference.png.

The reference is a 45x45 pixel-art gem: a parallelogram (the right side
higher than the left) split into four facets that meet at an apex, with a
darker outline. This script redraws it from that geometry at any size and
writes, into resources/images/unscaled/:

- background1.png: 200x228 (emery), the gem on a transparent background.
  shared/tools/convert_images.py fills the transparency with the face's
  background color from convert.json (pink, or white on black-and-white).
- menu_icon.png: the gem alone, which convert_images.py fits into the
  25x25 switcher icon.

shared/tools/page_image.py makes the settings-page gem from menu_icon.png.

Usage (from the repo root):
    uv run --with pillow python3 -I faces/usami/tools/make_gem.py
"""
import os

from PIL import Image, ImageDraw

# Geometry in the reference's 45x45 pixel coordinates.
TL, TR, BR, BL = (12, 17.5), (30.5, 8), (30.5, 28), (12, 37.5)
APEX = (20.5, 22.5)
OUTLINE_W = 1.0           # outline width, in reference pixels

# Colors sampled from the reference.
LEFT = (153, 214, 246)    # lightest facet
TOP = (102, 179, 217)
RIGHT = (71, 139, 173)
BOTTOM = (26, 133, 186)
OUTLINE = (36, 125, 169)

SUPERSAMPLE = 8
BG_SIZE = (200, 228)      # emery screen
GEM_H = 150               # gem height on emery, in px
GEM_CENTER = (100, 140)   # below the 56 px clock band
ICON_H = 100              # drawn large; convert_images.py scales it to 25


def draw_gem(size, height, center):
    """Return an RGBA image of `size` with the gem `height` px tall."""
    ss = SUPERSAMPLE
    x0 = min(p[0] for p in (TL, TR, BR, BL))
    x1 = max(p[0] for p in (TL, TR, BR, BL))
    y0 = min(p[1] for p in (TL, TR, BR, BL))
    y1 = max(p[1] for p in (TL, TR, BR, BL))
    k = height / (y1 - y0) * ss
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2

    def pt(p):
        return ((p[0] - cx) * k + center[0] * ss,
                (p[1] - cy) * k + center[1] * ss)

    img = Image.new("RGBA", (size[0] * ss, size[1] * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for color, a, b in ((TOP, TL, TR), (RIGHT, TR, BR),
                        (BOTTOM, BR, BL), (LEFT, BL, TL)):
        d.polygon([pt(a), pt(b), pt(APEX)], fill=color + (255,))
    width = max(1, round(OUTLINE_W * k))
    d.line([pt(TL), pt(TR), pt(BR), pt(BL), pt(TL)], fill=OUTLINE + (255,),
           width=width, joint="curve")
    # Downscale with premultiplied alpha so the edges don't darken.
    small = img.convert("RGBa").resize(size, Image.LANCZOS)
    return small.convert("RGBA")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "resources", "images", "unscaled")
    draw_gem(BG_SIZE, GEM_H, GEM_CENTER).save(
        os.path.join(out, "background1.png"), optimize=True)
    # Room for the outline around the icon-sized gem.
    side = ICON_H + 8
    draw_gem((side, side), ICON_H, (side / 2, side / 2)).save(
        os.path.join(out, "menu_icon.png"), optimize=True)
    print("wrote background1.png and menu_icon.png")


if __name__ == "__main__":
    main()
