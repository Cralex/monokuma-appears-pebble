# Usami Appears!

A watchface built on the shared engine, like [Monokuma Appears!](../monokuma/README.md). It's a work in progress: the poses are numbered placeholders, and the name in `package.json` is provisional.

## Look

- **Background:** pink (`GColorBrilliantRose`) on color watches and white on black-and-white ones, with a blue gem. Light pinks would look almost white on the real display, so this face uses Brilliant Rose.
- **Clock:** black digits, DorBlue by default (`resources/fonts/dorblue-font/`), with LECO as the alternative in the settings.

The colors are set in two places that must agree: `BG_COLOR` and `CLOCK_COLOR` in `src/c/face_config.h` (for the watch), and `convert.json` (for the image tool, which fills the originals' transparent areas with the background color).

## The gem

`tools/gem_reference.png` is the original 45×45 gem. `tools/make_gem.py` redraws it as a sharp vector replica from its geometry and colors. It writes `resources/images/unscaled/background1.png` (the gem on a transparent 200×228 background) and `resources/images/unscaled/menu_icon.png` (the gem alone, for the switcher icon). To change the gem's shape, size or position, edit the constants at the top of the script and run, from the repo root:

```sh
uv run --with pillow python3 -I faces/usami/tools/make_gem.py
```

Then convert the images (next section).

## Putting in the real poses

1. Replace `front01.png` … `frontNN.png` in `resources/images/unscaled/`. They should have transparent backgrounds, so the tool can set Usami on pink (or white).
2. From the repo root, run:

   ```sh
   uv run --with pillow --with numpy python3 -I shared/tools/convert_images.py faces/usami
   ```

3. If the number of poses changed, list one `IMAGE_FRONT_NN` bitmap per pose in `package.json`, and update `NUM_FG_IMAGES` and `FG_RES_IDS` in `src/c/face_config.h`.

## Build and run

From this folder:

```sh
pebble build
pebble install --emulator emery
```
