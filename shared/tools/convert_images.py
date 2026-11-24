"""Scale and color-convert the watchface images for each color platform.

Reads a face's full-color originals in faces/<face>/resources/images/unscaled/
(background1.png and front01.png..frontNN.png) and writes into that face's
resources/images/, for every platform in PLATFORMS, frontNN.png (the
character's poses), background1.png
and, once, menu_icon.png. Emery files have no suffix; other platforms use
Pebble's file tags (frontNN~basalt.png), which the SDK picks per platform.

Every size is made straight from the originals, never from another
platform's output. Layouts follow emery's: the foreground keeps emery's
200:172 shape, sits on the bottom edge, and the background is scaled by
the same factor and anchored the same way.

Scaling: each foreground image is zoomed on its own so the character is as large
as possible while staying fully visible (4 px of headroom on emery, scaled
on other platforms), centered horizontally and anchored to the bottom edge,
where it slides in.

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
    uv run --with pillow --with numpy python3 -I shared/tools/convert_images.py faces/<face>

A face can have a faces/<face>/convert.json; every key is optional:
    "background_color": [r, g, b]  the face's background, a Pebble color
        (the face's BG_COLOR). Transparent parts of background1.png and of
        the foregrounds are filled with how that color looks on the display,
        so it comes out as that exact color. Default: black.
    "bw_background_gray": 0-255  the same for black-and-white watches.
        Default: black.
    "crops": {"front01.png": {"center_x": x, "top": y0, "bottom": y1}, ...}
        a fixed view for each listed foreground, in the original's pixels:
        the rows from top to bottom, centered on center_x, as wide as the
        platform's foreground shape needs. It replaces the automatic zoom,
        so a face can show its character at the same size in every pose;
        anything outside the view is cut off.
    "extend_into_clock": true  with "crops": a pose with more of the
        character above its view (a raised wand) keeps it, up to the height
        of the clock band. That image is taller than the foreground area,
        and the engine draws the extra rows over the band. The band must be
        plain background color, so the extra rows can be opaque.
    "full_width": true  with "crops": on the round platforms, whose
        foreground areas are narrower than the screen, the views come out
        as wide as the screen (same zoom, more of the sides), so wings and
        wands aren't cut where the screen still has room. Set FG_X 0 and
        FG_W to the screen width for chalk and gabbro in face_config.h.
Without background1.png's red shape, the menu icon comes from
unscaled/menu_icon.png (an RGBA image of any size) instead.

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
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

# Screen size, foreground size and file suffix per platform. Keep in sync
# with the layout constants in each face's src/c/face_config.h.
PLATFORMS = {
    "emery":  {"screen": (200, 228), "fg": (200, 172), "suffix": ""},
    "basalt": {"screen": (144, 168), "fg": (144, 124), "suffix": "~basalt"},
    "chalk":  {"screen": (180, 180), "fg": (142, 123), "suffix": "~chalk"},
    "gabbro": {"screen": (260, 260), "fg": (212, 182), "suffix": "~gabbro"},
    # aplite and diorite: 144x168 black and white, basalt's layout.
    "bw":     {"screen": (144, 168), "fg": (144, 124), "suffix": "~bw",
               "bw": True},
}
EMERY_FG_W = 200
BG_W, BG_H = 200, 228   # the background original is emery-sized
BW_SHAPE_GRAY = 128     # B&W screens: gray the red shape is drawn as
ICON_SIZE = 25          # SDK maximum for menuIcon resources
ICON_OUTLINE = True     # ring the switcher icon in black for contrast
TOP_MARGIN = 4          # px of black kept above the character's head
SUBJECT_THRESHOLD = 24  # pixels brighter than this count as the character

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


def crop_view(src, fg_w, fg_h, fill, view, max_extra=0):
    """Scale a fixed view of src (see "crops" above) to fg_w x fg_h.

    With max_extra > 0, the result grows upward by up to max_extra rows to
    keep any part of the character above the view.
    """
    flat = flatten(src, fill)
    top, bottom = view["top"], view["bottom"]
    zoom = fg_h / (bottom - top)
    crop_w = (bottom - top) * fg_w / fg_h
    left = view["center_x"] - crop_w / 2
    out_h = fg_h
    if max_extra > 0:
        # Highest visible pixel of the character within the view's columns.
        alpha = np.asarray(src.convert("RGBA").getchannel("A"))
        x0, x1 = max(0, int(left)), min(src.width, int(left + crop_w) + 1)
        rows = np.nonzero((alpha[:, x0:x1] > 64).any(axis=1))[0]
        if len(rows) and rows[0] < top:
            extra = min(max_extra, int(np.ceil((top - rows[0]) * zoom)) + 1)
            out_h = fg_h + extra
            top -= extra / zoom
    # Pad so a view past the image edges still works.
    pad = int(max(-left, -top, left + crop_w - flat.width,
                  bottom - flat.height, 0)) + 1
    padded = Image.new("RGB", (flat.width + 2 * pad, flat.height + 2 * pad),
                       fill)
    padded.paste(flat, (pad, pad))
    box = (left + pad, top + pad, left + crop_w + pad, bottom + pad)
    return padded.resize((fg_w, out_h), Image.LANCZOS, box=box)


def scale(src, fg_w, fg_h, fill=(0, 0, 0)):
    """Flatten onto `fill` and zoom each image so the character fills the frame.

    Finds the box around everything brighter than the black background,
    then picks the largest zoom that fits that box in fg_w x fg_h with
    TOP_MARGIN px (scaled from emery) above it, centered on the character horizontally and anchored
    to the bottom edge. Areas outside the original are filled with `fill`.
    The character is found on a black background whatever `fill` is.
    """
    mask = np.asarray(flatten(src)).max(axis=2) > SUBJECT_THRESHOLD
    flat = flatten(src, fill)
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        sys.exit("image has no visible subject")
    x0, x1 = xs.min(), xs.max() + 1
    y0, y1 = ys.min(), ys.max() + 1

    margin = max(2, round(TOP_MARGIN * fg_w / EMERY_FG_W))
    zoom = min(fg_w / (x1 - x0), (fg_h - margin) / (y1 - y0))
    crop_w, crop_h = fg_w / zoom, fg_h / zoom
    if crop_w <= flat.width:
        # Center on the character, but don't pan past the image's sides.
        left = min(max((x0 + x1) / 2 - crop_w / 2, 0), flat.width - crop_w)
    else:
        left = (flat.width - crop_w) / 2
    top = y1 - crop_h

    # Pad so a crop box past the image edges still works.
    pad = int(max(-left, -top, 0)) + 1
    padded = Image.new("RGB", (flat.width + 2 * pad, flat.height + 2 * pad),
                       fill)
    padded.paste(flat, (pad, pad))
    box = (left + pad, top + pad, left + crop_w + pad, y1 + pad)
    return padded.resize((fg_w, fg_h), Image.LANCZOS, box=box)


def convert_color(img):
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


def convert_bw(img):
    """Black-and-white version for aplite and diorite.

    Grayscale with the converter's saturation formula at -100 (its luminance
    weights), then Atkinson-dither to pure black and white, the only two
    shades those screens show.
    """
    rgb = np.asarray(img, dtype=np.float64)
    work = rgb @ np.array([0.2989, 0.5870, 0.1140])
    h, w = work.shape
    out = np.zeros((h, w), dtype=np.uint8)
    for y in range(h):
        for x in range(w):
            v = work[y, x]
            level = 255.0 if v >= 128 else 0.0
            out[y, x] = 1 if level else 0
            err = (v - level) * 0.125
            for dx, dy in ATKINSON:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and ny < h:
                    work[ny, nx] += err
    img = Image.fromarray(out, mode="P")
    img.putpalette([0, 0, 0, 255, 255, 255])
    return img


def flatten(src, fill=(0, 0, 0)):
    rgba = src.convert("RGBA")
    flat = Image.new("RGB", rgba.size, fill)
    flat.paste(rgba, mask=rgba.getchannel("A"))
    return flat


def red_mask(img):
    """Boolean mask of the background's red shape."""
    a = np.asarray(img, dtype=np.int16)
    return (a[..., 0] > 2 * a[..., 1] + 40) & (a[..., 0] > 2 * a[..., 2] + 40)


def menu_icon(bg, outline=ICON_OUTLINE):
    """Build the launcher icon from the background's red shape.

    The shape is cropped, scaled to fit ICON_SIZE x ICON_SIZE, and centered
    on a transparent square. Edges are anti-aliased with Pebble's four alpha
    levels. The fill is the Pebble color the converter picks for the
    original's average red. With outline=True the shape is shrunk by 1 px on
    each side and ringed in black so it stays visible on the switcher's
    colored highlight.
    """
    flat = flatten(bg)
    mask = red_mask(flat)
    if not mask.any():
        sys.exit("background1.png has no red shape to make the menu icon from "
                 "(see red_mask)")
    ys, xs = np.nonzero(mask)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1

    sun = np.array(SUN, dtype=np.float64)
    avg = np.asarray(flat, dtype=np.float64)[mask].mean(axis=0)
    fill = DEFINED[int(np.argmin(((sun - avg) ** 2).sum(axis=1)))]

    room = ICON_SIZE - 2 if outline else ICON_SIZE
    zoom = room / max(x1 - x0, y1 - y0)
    w = max(1, round((x1 - x0) * zoom))
    h = max(1, round((y1 - y0) * zoom))
    shape = Image.fromarray((mask * 255).astype(np.uint8)).crop((x0, y0, x1, y1))
    alpha = np.asarray(shape.resize((w, h), Image.LANCZOS), dtype=np.float64)
    alpha = (np.round(alpha / 85) * 85).astype(np.uint8)  # 0, 85, 170, 255

    icon = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), fill + (0,))
    layer = Image.new("RGBA", (w, h), fill + (255,))
    layer.putalpha(Image.fromarray(alpha))
    pos = ((ICON_SIZE - w) // 2, (ICON_SIZE - h) // 2)
    if outline:
        # Black ring: the shape's alpha grown by 1 px in every direction.
        ring = Image.fromarray(alpha).filter(ImageFilter.MaxFilter(3))
        black = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
        full = Image.new("L", (ICON_SIZE, ICON_SIZE), 0)
        full.paste(ring, pos)
        black.putalpha(full)
        icon = black
    icon.alpha_composite(layer, pos)
    # Round to the four alpha levels Pebble supports.
    quantized = (np.round(np.asarray(icon.getchannel("A")) / 85) * 85)
    icon.putalpha(Image.fromarray(quantized.astype(np.uint8)))
    return icon


def image_icon(src):
    """Build the launcher icons from an RGBA image (unscaled/menu_icon.png).

    Returns (color, bw). The color icon is the image fitted into
    ICON_SIZE x ICON_SIZE, each pixel set to the Pebble color that looks
    closest on the display (no dithering at this size), with Pebble's four
    alpha levels. The black-and-white icon is the image's silhouette in
    black, 1 px smaller on each side, ringed in white so it shows on both
    the white rows and the black highlight.
    """
    src = src.convert("RGBA")
    box = src.getbbox()
    if box is None:
        sys.exit("menu_icon.png is fully transparent")
    src = src.crop(box)

    def fit(room):
        zoom = room / max(src.size)
        w = max(1, round(src.width * zoom))
        h = max(1, round(src.height * zoom))
        small = src.convert("RGBa").resize((w, h), Image.LANCZOS).convert("RGBA")
        icon = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
        icon.paste(small, ((ICON_SIZE - w) // 2, (ICON_SIZE - h) // 2))
        return np.asarray(icon)

    a = fit(ICON_SIZE)
    sun = np.array(SUN, dtype=np.float64)
    rgb = a[..., :3].reshape(-1, 1, 3).astype(np.float64)
    idx = ((rgb - sun) ** 2).sum(axis=2).argmin(axis=1)
    out = np.zeros(a.shape, dtype=np.uint8)
    out[..., :3] = np.array(DEFINED, dtype=np.uint8)[idx].reshape(a.shape[:2] + (3,))
    out[..., 3] = (np.round(a[..., 3] / 85) * 85).astype(np.uint8)
    color = Image.fromarray(out, mode="RGBA")

    solid = fit(ICON_SIZE - 2)[..., 3] >= 128
    ring = np.asarray(Image.fromarray(solid.astype(np.uint8) * 255)
                      .filter(ImageFilter.MaxFilter(3))) > 0
    bw = np.zeros(a.shape, dtype=np.uint8)
    bw[ring & ~solid, :3] = 255
    bw[..., 3] = ring * 255
    return color, Image.fromarray(bw, mode="RGBA")


def place_background(bg, screen, fg_w):
    """Scale the emery background like the foreground and fit it to a screen.

    It shrinks or grows by the same factor as the foreground (fg_w / 200),
    sits on the bottom edge, is centered, and any leftover space is filled
    with the original's background color.
    """
    flat = flatten(bg)
    if screen == flat.size:
        return flat
    factor = fg_w / EMERY_FG_W
    size = (round(flat.width * factor), round(flat.height * factor))
    scaled = flat.resize(size, Image.LANCZOS)
    canvas = Image.new("RGB", screen, flat.getpixel((0, 0)))
    canvas.paste(scaled, ((screen[0] - size[0]) // 2, screen[1] - size[1]))
    return canvas


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: convert_images.py faces/<face>")
    face = sys.argv[1]
    images = os.path.join(face, "resources", "images")
    unscaled = os.path.join(images, "unscaled")
    if not os.path.isdir(unscaled):
        sys.exit(f"no originals folder: {unscaled}")
    # front01.png, front02.png, ... with no gaps.
    num_images = 0
    while os.path.exists(os.path.join(unscaled, f"front{num_images + 1:02d}.png")):
        num_images += 1
    if num_images == 0:
        sys.exit(f"no front01.png in {unscaled}")

    config = {}
    config_path = os.path.join(face, "convert.json")
    if os.path.exists(config_path):
        with open(config_path) as f:
            config = json.load(f)
    color_fill = (0, 0, 0)
    if "background_color" in config:
        c = tuple(config["background_color"])
        if c not in DEFINED:
            sys.exit(f"background_color {list(c)} is not a Pebble color")
        # How it looks on the display, so it converts back to exactly c.
        color_fill = SUN[DEFINED.index(c)]
    gray = config.get("bw_background_gray", 0)
    crops = config.get("crops", {})
    bw_fill = (gray, gray, gray)

    bg = Image.open(os.path.join(unscaled, "background1.png"))
    if bg.size != (BG_W, BG_H):
        sys.exit(f"background1.png must be {BG_W}x{BG_H}, got {bg.size}")

    for platform, spec in PLATFORMS.items():
        fg_w, fg_h = spec["fg"]
        suffix = spec["suffix"]
        convert = convert_bw if spec.get("bw") else convert_color
        fill = bw_fill if spec.get("bw") else color_fill
        for n in range(1, num_images + 1):
            name = f"front{n:02d}.png"
            src = Image.open(os.path.join(unscaled, name))
            if name in crops:
                max_extra = (spec["screen"][1] - fg_h
                             if config.get("extend_into_clock") else 0)
                # "full_width": span the whole screen (the round ones'
                # foregrounds are narrower) at the same zoom, so the view
                # just widens instead of cutting the sides.
                view_w = (spec["screen"][0] if config.get("full_width")
                          else fg_w)
                scaled = crop_view(src, view_w, fg_h, fill, crops[name],
                                   max_extra)
            else:
                scaled = scale(src, fg_w, fg_h, fill)
            out = f"front{n:02d}{suffix}.png"
            convert(scaled).save(os.path.join(images, out), optimize=True)
        out = f"background1{suffix}.png"
        placed = place_background(flatten(bg, fill), spec["screen"], fg_w)
        if spec.get("bw"):
            # The dark red would dither to a few faint dots; draw the shape
            # as mid-gray instead so it reads clearly.
            pixels = np.asarray(placed).copy()
            pixels[red_mask(placed)] = BW_SHAPE_GRAY
            placed = Image.fromarray(pixels)
        convert(placed).save(
            os.path.join(images, out), optimize=True)
        print(f"wrote {platform}: {num_images} foregrounds and {out}")

    icon_src = os.path.join(unscaled, "menu_icon.png")
    if os.path.exists(icon_src):
        color, bw_icon = image_icon(Image.open(icon_src))
        color.save(os.path.join(images, "menu_icon.png"), optimize=True)
        bw_icon.save(os.path.join(images, "menu_icon~bw.png"), optimize=True)
        print("wrote menu_icon.png and menu_icon~bw.png from unscaled/menu_icon.png")
        return

    menu_icon(bg).save(os.path.join(images, "menu_icon.png"), optimize=True)
    print("wrote menu_icon.png")

    # Black-and-white screens: black shape with a white outline and hard
    # edges, so it shows on both the white rows and the black highlight.
    icon = np.asarray(menu_icon(bg, outline=True))
    solid = icon[..., 3] >= 128
    ring = solid & (icon[..., 0] < 64)       # the outline is black in color
    bw = np.zeros(icon.shape, dtype=np.uint8)
    bw[ring, :3] = 255
    bw[..., 3] = solid * 255
    bw_icon = Image.fromarray(bw, mode="RGBA")
    bw_icon.save(os.path.join(images, "menu_icon~bw.png"), optimize=True)
    print("wrote menu_icon~bw.png")


if __name__ == "__main__":
    main()
