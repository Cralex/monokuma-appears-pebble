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
  - `src/c/face_config.h`: what the engine needs from the face: `NUM_FG_IMAGES`, `FG_RES_IDS`, and the per-platform layout and clock font constants
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
2. Put `background1.png` (200×228, with a strong red shape for the menu icon) and `front01.png`, `front02.png`, … in `resources/images/unscaled/`, then run `uv run --with pillow --with numpy python3 -I shared/tools/convert_images.py faces/<name>`.
3. List one `IMAGE_FRONT_NN` bitmap per image in `package.json`, and set `NUM_FG_IMAGES` and `FG_RES_IDS` in `src/c/face_config.h` to match.
4. Change the text in `src/pkjs/config.js`.

A change to `shared/` affects every face, so rebuild and check each one.
