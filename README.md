# Pebble character watchfaces

Watchfaces that share one engine: a character slides up over a background in a random pose, the clock sits in the band above, and the pose changes every minute or when you shake the watch. Each face has its own art, name and UUID, so they install side by side.

| Face | Folder |
|---|---|
| Monokuma Appears! | [`faces/monokuma/`](faces/monokuma/README.md) |
| Usami Appears! (in progress, placeholder art) | [`faces/usami/`](faces/usami/README.md) |

## Layout

- `shared/src/c/main.c`: the watchface engine (fades, slides, clock, shake and settings), compiled into every face
- `shared/tools/convert_images.py`: scales and color-converts a face's original images for every platform
- `faces/<name>/`: one complete Pebble project per face
  - `package.json`: UUID, display name, resources and message keys
  - `wscript`: builds `src/c/` plus `../../shared/src/c/`, with `src/c/` on the include path
  - `src/c/face_config.h`: what the engine needs from the face: `NUM_FG_IMAGES`, `FG_RES_IDS`, the colors (`BG_COLOR` behind the images, `CLOCK_COLOR` for the digits), and the per-platform layout and clock font constants
  - `convert.json` (optional): the background colors for the image tool (see "Adding a face")
  - `src/pkjs/`: the Clay settings page (`config.js`) and the phone-side JS that starts it
  - `resources/`: images (originals in `resources/images/unscaled/`) and fonts

## Build

Each face builds on its own, from its folder:

```sh
cd faces/monokuma
pebble build
pebble install --emulator emery
```

## Adding a face

1. Copy `faces/usami/` to `faces/<name>/` and give `package.json` a new `name`, `displayName` and `uuid` (`uuidgen | tr A-Z a-z`).
2. Put `background1.png` (200×228) and `front01.png`, `front02.png`, … in `resources/images/unscaled/`, plus `menu_icon.png` (any size, with transparency) for the switcher icon. Without `menu_icon.png`, the tool makes the icon from a strong red shape in the background, the way Monokuma's is made. Then run `uv run --with pillow --with numpy python3 -I shared/tools/convert_images.py faces/<name>`.
3. List one `IMAGE_FRONT_NN` bitmap per image in `package.json`, and set `NUM_FG_IMAGES` and `FG_RES_IDS` in `src/c/face_config.h` to match.
4. For a background other than black, set `BG_COLOR` (and `CLOCK_COLOR`) in `face_config.h`, and the same colors in `convert.json`: `"background_color"` as the Pebble color's `[r, g, b]`, and `"bw_background_gray"` (0–255) for black-and-white watches. The tool fills transparent parts of the originals with them, so the images blend into the screen. Pick a strong color: on the real display, light pinks and pastels look almost white.
5. Change the text in `src/pkjs/config.js`.

A change to `shared/` affects every face, so rebuild and check each one.
