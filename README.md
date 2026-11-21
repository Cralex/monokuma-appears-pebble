# Monokuma Appears

A Pebble watchface for **emery** (Pebble Time 2). Monokuma appears over a background in one of 16 poses. On launch, the opening image stays on screen for a random 1–3 seconds before it fades and the clock appears. Shaking or tapping the watch switches to a different image.

## Settings

Settings use [Clay](https://github.com/pebble/clay) and open from the watchface's gear icon in the Pebble phone app:

- **Change image on shake** (`SHAKE_ENABLED`, on by default): whether shaking or tapping the watch switches the image.

To try the settings page in the emulator, run `pebble emu-app-config --emulator emery`.

## Build and run

```sh
pebble build
pebble install --emulator emery
```

## Layout

- `src/c/main.c`: the watchface
- `src/pkjs/index.js`: phone-side JS (starts Clay)
- `src/pkjs/config.js`: Clay settings page definition
- `resources/images/`: background and the 16 `front01`–`front16` images (200×172, Pebble colors)
- `resources/images/menu_icon.png`: 25×25 icon for the watchface switcher, made from the background's red shape
- `resources/images/unscaled/`: full-color originals (the 680×680 foreground images and the 200×228 background)
- `tools/convert_images.py`: regenerates `front01`–`front16`, `background1.png` and `menu_icon.png` from the originals

## Updating the images

Edit or replace the originals in `resources/images/unscaled/`, then run:

```sh
uv run --with pillow --with numpy python3 -I tools/convert_images.py
```

The script zooms each image on its own so Monokuma is as large as possible while staying fully visible: centered, anchored to the bottom edge, with 4 px of space above the head. It then converts the colors the same way as the [TomHol PhotoWatch converter](https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html) does with its default settings: Atkinson dithering against how the Pebble Time 2 display actually renders each color.

## Credits

- **Color conversion:** the method in `tools/convert_images.py` comes from TomHol ([czmanix](https://github.com/czmanix)). That includes the display-simulated color palette and the Atkinson dithering.
  - [Pebble Color Optimizer](https://github.com/czmanix/pebble-color-optimizer) ([try it live](https://czmanix.github.io/pebble-color-optimizer/)) is the original tool, MIT licensed, © 2026 czmanix.
  - The [PhotoWatch image converter](https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html), the settings page of TomHol's PhotoWatch watchface, is a later version with more adjustment options. The script is a Python port of that converter's default settings.
- **Settings page:** [Clay](https://github.com/pebble/clay) by Pebble.
