# Usami Appears!

A watchface built on the shared engine, like [Monokuma Appears!](../monokuma/README.md). It's a work in progress: the images are numbered placeholders (4 poses and a red circle background), and the name in `package.json` is provisional.

## Putting in the real art

1. Replace the originals in `resources/images/unscaled/`: `background1.png` (200×228) and `front01.png` … `frontNN.png`. The menu icon is made from the background's red shape, so the background needs one (see `red_mask` in the tool).
2. From the repo root, run:

   ```sh
   uv run --with pillow --with numpy python3 -I shared/tools/convert_images.py faces/usami
   ```

3. If the number of poses changed, list one `IMAGE_FRONT_NN` bitmap per pose in `package.json`, and update `NUM_FG_IMAGES` and `FG_RES_IDS` in `src/c/face_config.h`.
4. Adjust the clock font and layout in `src/c/face_config.h` if needed. It uses DorBlue (`resources/fonts/dorblue-font/`) for now.

## Build and run

From this folder:

```sh
pebble build
pebble install --emulator emery
```
