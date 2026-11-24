# Usami Appears!

A watchface built on the shared engine, like [Monokuma Appears!](../monokuma/README.md). Usami appears in one of 12 poses. The name in `package.json` is provisional.

## Look

- **Background:** pink (`GColorBrilliantRose`) on color watches and white on black-and-white ones, with a blue gem. Light pinks would look almost white on the real display, so this face uses Brilliant Rose.
- **Clock:** black digits, DorBlue by default (`resources/fonts/dorblue-font/`), with LECO as the alternative in the settings. Where the digits cross a pose (the raised wand), they get a white outline (`CLOCK_OUTLINE_COLOR`, 2 px on emery and gabbro, 1 px on the smaller screens); over the plain background there's none.

The colors are set in two places that must agree: `BG_COLOR` and `CLOCK_COLOR` in `src/c/face_config.h` (for the watch), and `convert.json` (for the image tool, which fills the originals' transparent areas with the background color).

## The gem

`tools/gem_reference.png` is the original 45×45 gem. `tools/make_gem.py` redraws it as a sharp vector replica from its geometry and colors. It writes `resources/images/unscaled/background1.png` (the gem on a transparent 200×228 background) and `resources/images/unscaled/menu_icon.png` (the gem alone, for the switcher icon). To change the gem's shape, size or position, edit the constants at the top of the script and run, from the repo root:

```sh
uv run --with pillow python3 -I faces/usami/tools/make_gem.py
```

Then convert the images (next section).

## The poses

The originals in `resources/images/unscaled/` (`front01.png` … `front12.png`) are full-body sprites with transparent backgrounds, all drawn at the same scale. Unlike Monokuma's, they aren't zoomed one by one: each gets a fixed crop window, the same size for every pose, so Usami is the same size in all of them. The window is placed by the heart brooch on Usami's collar. It runs from just above the ear tips to the top of the skirt, so the skirt is cut off the way Monokuma's originals stop at his belly. Anything above the window, usually the wand, isn't cut off: with `"extend_into_clock": true` in `convert.json`, those poses (01, 02, 04, 09, 10 and 12) come out taller and reach up into the pink clock band, up to its full height, and the digits are drawn over them. On the round watches (chalk and gabbro), whose foreground areas are narrower than the screen, `"full_width": true` widens each window to the full screen at the same zoom (with `FG_X 0` and `FG_W` set to the screen width in `src/c/face_config.h`), so the wings and wand reach the edge of the circle instead of stopping at a straight edge. Wing tips can still be cut at the sides of the rectangular screens.

`tools/find_crops.py` finds each brooch and writes the windows to `convert.json` (`"crops"`). To show more or less of Usami, change `ABOVE` and `BELOW` at the top of the script.

After adding or changing poses, run from the repo root:

```sh
uv run --with pillow --with numpy --with scipy python3 -I faces/usami/tools/find_crops.py
uv run --with pillow --with numpy python3 -I shared/tools/convert_images.py faces/usami
```

If the number of poses changed, list one `IMAGE_FRONT_NN` bitmap per pose in `package.json`, and update `NUM_FG_IMAGES` and `FG_RES_IDS` in `src/c/face_config.h`.

## Build and run

From this folder:

```sh
pebble build
pebble install --emulator emery
```
