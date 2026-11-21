# Monokuma Appears!

A Pebble watchface for the color and black-and-white Pebbles. Monokuma appears over a background in one of 16 poses. On launch, the opening image stays on screen for a random 1–3 seconds before it fades and the clock appears. Shaking or tapping the watch switches to a different image.

## Supported watches

| Platform | Watch | Screen | Monokuma image | Clock band | DorBlue | LECO |
|---|---|---|---|---|---|---|
| emery | Pebble Time 2 | 200×228 | 200×172 | 56 px | 48 px | 42 |
| basalt | Pebble Time, Time Steel | 144×168 | 144×124 | 44 px | 34 px | 32 Bold |
| chalk | Pebble Time Round | 180×180 round | 142×123 | 57 px | 32 px | 32 Bold |
| gabbro | Pebble Round 2 | 260×260 round | 212×182 | 78 px | 46 px | 42 |
| aplite | Pebble, Pebble Steel | 144×168 black and white | 144×124 | 44 px | 34 px | 32 Bold |
| diorite | Pebble 2 | 144×168 black and white | 144×124 | 44 px | 34 px | 32 Bold |

Every layout is scaled from emery's. Monokuma keeps emery's 200:172 shape on the bottom edge, and the background is scaled by the same factor. On the round watches the clock band is taller, so the time fits inside the circle. The layout constants are in `src/c/main.c`.

## Settings

Settings use [Clay](https://github.com/pebble-dev/clay) (the `@rebble/clay` package) and open from the watchface's gear icon in the Pebble phone app:

- **Clock font** (`CLOCK_FONT`, DorBlue by default): DorBlue, or LECO, Pebble's built-in numeric font.
- **Change image on shake** (`SHAKE_ENABLED`, on by default): whether shaking or tapping the watch switches the image.

To try the settings page in the emulator, run `pebble emu-app-config --emulator emery`.

## Build and run

```sh
pebble build
pebble install --emulator emery   # or basalt, chalk, gabbro, aplite, diorite
```

## Layout

- `src/c/main.c`: the watchface
- `src/pkjs/index.js`: phone-side JS (starts Clay)
- `src/pkjs/config.js`: Clay settings page definition
- `resources/fonts/dorblue-font/`: the DorBlue clock font, unchanged from the download, with its license file
- `resources/images/`: the background and the 16 `front01`–`front16` images in Pebble colors. Emery's files have no suffix; the other platforms' use Pebble's file tags (`front01~basalt.png`); aplite and diorite share the `~bw` files
- `resources/images/menu_icon.png`: 25×25 icon for the watchface switcher, made from the background's red shape with a black outline
- `resources/images/unscaled/`: full-color originals (the 680×680 foreground images and the 200×228 background)
- `screenshots/<platform>/`: emulator screenshots for each platform (the opening screen, then three poses at 10:09), color-corrected to match the real screens; `raw/` has the exact pixels
- `tools/convert_images.py`: regenerates every platform's `front01`–`front16` and `background1`, plus `menu_icon.png`, straight from the originals

## Updating the images

Edit or replace the originals in `resources/images/unscaled/`, then run:

```sh
uv run --with pillow --with numpy python3 -I tools/convert_images.py
```

For each platform, the script zooms each original on its own so Monokuma is as large as possible while staying fully visible: centered, anchored to the bottom edge, with 4 px of space above the head (scaled down on smaller screens). Every size is made from the originals, never from another platform's output. It then converts the colors the same way as the [TomHol PhotoWatch converter](https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html) does with its default settings: Atkinson dithering against how the Pebble Time 2 display actually renders each color. For the black-and-white watches it instead turns the image gray using the converter's saturation formula at −100, then applies the same Atkinson dithering to pure black and white, the only two shades those screens show. The background's red shape is drawn as 50% gray there (`BW_SHAPE_GRAY`), since its dark red would dither to just a few faint dots. Their switcher icon is the black shape with a white outline, so it shows on both white rows and the black highlight.

## Credits

- **Color conversion:** the method in `tools/convert_images.py` comes from TomHol ([czmanix](https://github.com/czmanix)). That includes the display-simulated color palette and the Atkinson dithering.
  - [Pebble Color Optimizer](https://github.com/czmanix/pebble-color-optimizer) ([try it live](https://czmanix.github.io/pebble-color-optimizer/)) is the original tool, MIT licensed, © 2026 czmanix.
  - The [PhotoWatch image converter](https://github.com/czmanix/tomhol-configs/blob/main/photowatch/index.html), the settings page of TomHol's PhotoWatch watchface, is a later version with more adjustment options. The script is a Python port of that converter's default settings.
- **Clock font:** DorBlue by Bartek Nowak (Barmee), [www.nowak.tv/fontoholic](http://www.nowak.tv/fontoholic). It's free for non-commercial use and redistributed unchanged under the terms in [`barmefonts - licence.txt`](resources/fonts/dorblue-font/misc/barmefonts%20-%20licence.txt). Using it commercially, or distributing it as part of software, requires the author's permission.
- **Settings page:** [Clay](https://github.com/pebble/clay) by Pebble, maintained by the Rebble community as [`@rebble/clay`](https://github.com/pebble-dev/clay).
