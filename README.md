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
- `resources/images/unscaled/`: full-color 680×680 originals of the 16 images
- `tools/convert_images.py`: regenerates `front01`–`front16` from the originals

## Updating the images

Edit or replace the originals in `resources/images/unscaled/`, then run:

```sh
uv run --with pillow --with numpy python3 -I tools/convert_images.py
```

The script scales each image to 200 px wide and keeps the bottom 172 px. It then converts the colors the same way as the [TomHol PhotoWatch converter](https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html) does with its default settings: Atkinson dithering against how the Pebble Time 2 display actually renders each color.

## Credits

- **Color conversion:** the method in `tools/convert_images.py` comes from the [PhotoWatch image converter](https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html), the settings page of the PhotoWatch watchface by TomHol ([czmanix](https://github.com/czmanix)). That includes the display-simulated color palette and the Atkinson dithering. The script is a Python port of the converter's default settings.
- **Settings page:** [Clay](https://github.com/pebble/clay) by Pebble.
